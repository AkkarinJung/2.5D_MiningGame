// Monsters.cpp
// Monster AI (wake/sleep, distance-map steering, slime hops, bat zigzag, goblin chase),
// Floor pool: slimes and bats; goblins from floor 2, brutes from floor 3, more of both from 5.
// brute wind-up and charge), contact damage, melee hits from the player, spawning,
// potions (floor loot, drops, pickup), and drawing everything as camera-facing
// pixel-art billboards built from MonsterData.h, with HP bars over hurt monsters.

#include "main.h"
#include "renderer.h"
#include "manager.h"
#include "Camera.h"
#include "Player.h"
#include "TileMap.h"
#include "Monsters.h"
#include "DebugUI.h"
#include <cmath>
#include <algorithm>
#include <climits>

namespace
{
    const int kDX[4] = { 0, 1, 0, -1 };
    const int kDZ[4] = { -1, 0, 1, 0 };
    const float kPlayerRadius = 0.3f;

    enum BruteState : uint8_t { BruteWalk, BruteWindUp, BruteCharge, BruteRest };

    const SpriteDef& SpriteFor(int k)
    {
        return k < kMonsterTypeCount ? kMonster[k].sprite : kPotionSprite;
    }

    float AngleDiff(float a, float b)
    {
        float d = a - b;
        while (d > XM_PI)  d -= XM_2PI;
        while (d < -XM_PI) d += XM_2PI;
        return fabsf(d);
    }

    // Line of sight sampled every half tile; walls block it, ore blocks do not.
    bool ClearLine(const TileMap& map, float ax, float az, float bx, float bz)
    {
        const float dx = bx - ax, dz = bz - az;
        const int n = (int)ceilf(std::max(fabsf(dx), fabsf(dz)) * 2.0f);
        for (int s = 1; s < n; s++)
        {
            const float t = (float)s / n;
            if (map.Get((int)floorf(ax + dx * t), (int)floorf(az + dz * t)) == Tile::Wall)
                return false;
        }
        return true;
    }

    template<typename T>
    void SafeRelease(T*& p)
    {
        if (p) { p->Release(); p = nullptr; }
    }
}

// ---------------------------------------------------------------- setup

void Monsters::Init()
{
    m_Layer = 1;
    m_Rng.seed(std::random_device{}());
    m_List.reserve(64);
    m_Flow.reserve(TileMap::W * TileMap::H);

    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "shader\\unlitTextureVS.cso");
    Renderer::CreatePixelShader(&m_PixelShader, "shader\\unlitTexturePS.cso");

    // Sprite atlas: every 8x8 sprite (monsters, then the potion) side by side in one texture.
    const int atlasW = 8 * kSpriteCount, atlasH = 8;
    std::vector<uint32_t> pixels(atlasW * atlasH, 0);
    for (int k = 0; k < kSpriteCount; k++)
        for (int y = 0; y < 8; y++)
            for (int x = 0; x < 8; x++)
            {
                const char key = SpriteFor(k).rows[y][x];
                for (const PaletteEntry& p : SpriteFor(k).palette)
                    if (p.key != 0 && p.key == key)
                    {
                        const uint32_t r = (p.rgb >> 16) & 0xFF, g = (p.rgb >> 8) & 0xFF, b = p.rgb & 0xFF;
                        pixels[y * atlasW + k * 8 + x] = 0xFF000000u | (b << 16) | (g << 8) | r;   // RGBA in memory
                    }
            }

    D3D11_TEXTURE2D_DESC td{};
    td.Width = atlasW;
    td.Height = atlasH;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = pixels.data();
    init.SysMemPitch = atlasW * 4;

    ID3D11Texture2D* texture = nullptr;
    Renderer::GetDevice()->CreateTexture2D(&td, &init, &texture);
    assert(texture);
    Renderer::GetDevice()->CreateShaderResourceView(texture, nullptr, &m_Atlas);
    texture->Release();
    assert(m_Atlas);

    // Point sampling keeps the pixel art sharp.
    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    Renderer::GetDevice()->CreateSamplerState(&sd, &m_PointSampler);

    // One quad per sprite, anchored at the feet (y 0..1), facing -Z, UVs pointing into the atlas.
    for (int k = 0; k < kSpriteCount; k++)
    {
        const float u0 = (k * 8 + 0.01f) / atlasW, u1 = ((k + 1) * 8 - 0.01f) / atlasW;
        VERTEX_3D v[4];
        const XMFLOAT3 n(0, 0, -1);
        const XMFLOAT4 white(1, 1, 1, 1);
        v[0] = { XMFLOAT3(-0.5f, 1, 0), n, white, XMFLOAT2(u0, 0) };
        v[1] = { XMFLOAT3(0.5f, 1, 0), n, white, XMFLOAT2(u1, 0) };
        v[2] = { XMFLOAT3(-0.5f, 0, 0), n, white, XMFLOAT2(u0, 1) };
        v[3] = { XMFLOAT3(0.5f, 0, 0), n, white, XMFLOAT2(u1, 1) };

        D3D11_BUFFER_DESC bd{};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(v);
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA vd{};
        vd.pSysMem = v;
        Renderer::GetDevice()->CreateBuffer(&bd, &vd, &m_QuadVB[k]);
    }
}

void Monsters::Uninit()
{
    for (ID3D11Buffer*& vb : m_QuadVB)
        SafeRelease(vb);
    SafeRelease(m_VertexLayout);
    SafeRelease(m_VertexShader);
    SafeRelease(m_PixelShader);
    SafeRelease(m_Atlas);
    SafeRelease(m_PointSampler);

    GameObject::Uninit();
}

// ---------------------------------------------------------------- spawning

MonsterType Monsters::RandomType()
{
    // Floor pool: slimes and bats; goblins join from floor 2 (brutes come in a later step).
    MonsterType pool[10];
    int n = 0;
    pool[n++] = MonsterType::Slime;
    pool[n++] = MonsterType::Bat;
    pool[n++] = MonsterType::Slime;
    pool[n++] = MonsterType::Bat;
    if (m_FloorNo >= 2) { pool[n++] = MonsterType::Goblin; pool[n++] = MonsterType::Goblin; }
    if (m_FloorNo >= 3) { pool[n++] = MonsterType::Brute; }
    if (m_FloorNo >= 5) { pool[n++] = MonsterType::Goblin; pool[n++] = MonsterType::Brute; }
    return pool[std::uniform_int_distribution<int>(0, n - 1)(m_Rng)];
}

void Monsters::Spawn(MonsterType type, float x, float z)
{
    const MonsterDef& def = kMonster[(int)type];
    const int f = std::max(1, m_FloorNo);

    Monster m;
    m.type = type;
    m.x = x;
    m.z = z;
    m.maxHp = m.hp = roundf(def.hp * (1.0f + 0.22f * (f - 1)));
    m.attack = def.attack + (f - 1) / 2;
    m.timer = std::uniform_real_distribution<float>(0.0f, 1.0f)(m_Rng);
    m.phase = std::uniform_real_distribution<float>(0.0f, 9.0f)(m_Rng);
    m_List.push_back(m);
}

void Monsters::PopulateFloor(const TileMap& map)
{
    m_Seed = map.GetSeed();
    m_FloorNo = map.GetFloor();
    m_List.clear();
    m_Potions.clear();
    m_FlowTile = -1;
    m_SpawnTimer = (float)std::max(7, 17 - m_FloorNo);

    // Free floor tiles more than 9 steps from the start
    std::vector<int> spots;
    spots.reserve(TileMap::W * TileMap::H);
    for (int z = 0; z < TileMap::H; z++)
        for (int x = 0; x < TileMap::W; x++)
            if (map.Get(x, z) == Tile::Floor && map.GetStartDistance(x, z) > 9)
                spots.push_back(z * TileMap::W + x);

    const int count = 6 + 2 * m_FloorNo;
    for (int k = 0; k < count && !spots.empty(); k++)
    {
        const int pick = std::uniform_int_distribution<int>(0, (int)spots.size() - 1)(m_Rng);
        const int i = spots[pick];
        spots[pick] = spots.back();
        spots.pop_back();
        Spawn(RandomType(), i % TileMap::W + 0.5f, i / TileMap::W + 0.5f);
    }

    // 1-2 potions lying around, also far from the start
    const int potions = 1 + std::uniform_int_distribution<int>(0, 1)(m_Rng);
    for (int k = 0; k < potions && !spots.empty(); k++)
    {
        const int pick = std::uniform_int_distribution<int>(0, (int)spots.size() - 1)(m_Rng);
        const int i = spots[pick];
        spots[pick] = spots.back();
        spots.pop_back();
        DropPotion(i % TileMap::W + 0.5f, i / TileMap::W + 0.5f);
    }
}

// ---------------------------------------------------------------- distance map

int Monsters::Flow(int x, int z) const
{
    if (x < 0 || z < 0 || x >= TileMap::W || z >= TileMap::H || m_Flow.empty())
        return -1;
    return m_Flow[z * TileMap::W + x];
}

void Monsters::BuildFlow(const TileMap& map, int playerTile)
{
    static std::vector<int> queue;      // reused every rebuild: no allocation after the first
    queue.clear();
    queue.reserve(TileMap::W * TileMap::H);

    m_Flow.assign(TileMap::W * TileMap::H, -1);
    m_Flow[playerTile] = 0;
    queue.push_back(playerTile);
    for (size_t head = 0; head < queue.size(); head++)
    {
        const int c = queue[head];
        const int cx = c % TileMap::W, cz = c / TileMap::W;
        for (int d = 0; d < 4; d++)
        {
            const int nx = cx + kDX[d], nz = cz + kDZ[d];
            if (map.Blocks(nx, nz))
                continue;
            const int j = nz * TileMap::W + nx;
            if (m_Flow[j] < 0)
            {
                m_Flow[j] = m_Flow[c] + 1;
                queue.push_back(j);
            }
        }
    }
}

// Head straight at a close or visible player; otherwise follow the distance map downhill.
void Monsters::Steer(const Monster& m, const TileMap& map, float px, float pz, float& sx, float& sz) const
{
    const float dx = px - m.x, dz = pz - m.z;
    const float d = std::max(1e-6f, sqrtf(dx * dx + dz * dz));
    sx = dx / d;
    sz = dz / d;
    if (d < 1.6f || (d < 5.0f && ClearLine(map, m.x, m.z, px, pz)))
        return;

    const int tx = (int)floorf(m.x), tz = (int)floorf(m.z);
    const int cur = Flow(tx, tz);
    int best = cur < 0 ? INT_MAX : cur;
    float bx = 0.0f, bz = 0.0f;
    bool found = false;
    for (int k = 0; k < 4; k++)
    {
        const int v = Flow(tx + kDX[k], tz + kDZ[k]);
        if (v >= 0 && v < best)
        {
            best = v;
            bx = tx + kDX[k] + 0.5f;
            bz = tz + kDZ[k] + 0.5f;
            found = true;
        }
    }
    if (!found)
        return;     // unreachable tile: keep heading straight at the player

    const float ex = bx - m.x, ez = bz - m.z;
    const float el = std::max(1e-6f, sqrtf(ex * ex + ez * ez));
    sx = ex / el;
    sz = ez / el;
}

// ---------------------------------------------------------------- update

void Monsters::Update()
{
    TileMap* map = Manager::GetGameObj<TileMap>();
    Player* player = Manager::GetGameObj<Player>();
    if (!map || !player)
        return;

    const float dt = 1.0f / 60.0f;
    m_Time += dt;

    // A new floor was generated (stairs, G key or debug panel): repopulate it.
    if (map->GetSeed() != m_Seed || map->GetFloor() != m_FloorNo)
        PopulateFloor(*map);

    const Vector3 p = player->GetPosition();
    const int ptx = std::min(std::max((int)floorf(p.x), 0), TileMap::W - 1);
    const int ptz = std::min(std::max((int)floorf(p.z), 0), TileMap::H - 1);
    const int playerTile = ptz * TileMap::W + ptx;

    // Rebuild the distance map only when the player changes tile or a block breaks.
    if (playerTile != m_FlowTile || map->GetBlocksLeft() != m_FlowBlocks)
    {
        BuildFlow(*map, playerTile);
        m_FlowTile = playerTile;
        m_FlowBlocks = map->GetBlocksLeft();
    }

    if (m_Frozen)
        return;

    for (Monster& m : m_List)
        UpdateOne(m, *map, p.x, p.z, dt);

    // Push overlapping monsters apart, half the overlap each (through MoveAndSlide, never into walls).
    for (size_t a = 0; a < m_List.size(); a++)
        for (size_t b = a + 1; b < m_List.size(); b++)
        {
            Monster& ma = m_List[a];
            Monster& mb = m_List[b];
            const float dx = ma.x - mb.x, dz = ma.z - mb.z;
            const float d = sqrtf(dx * dx + dz * dz);
            const float minD = kMonster[(int)ma.type].radius + kMonster[(int)mb.type].radius;
            if (d <= 0.0f || d >= minD)
                continue;
            const float push = (minD - d) * 0.5f;
            Vector3 pa(ma.x, 0.0f, ma.z), va(dx / d * push, 0.0f, dz / d * push);
            Vector3 pb(mb.x, 0.0f, mb.z), vb(-dx / d * push, 0.0f, -dz / d * push);
            map->MoveAndSlide(pa, va, 1.0f, kMonster[(int)ma.type].radius);
            map->MoveAndSlide(pb, vb, 1.0f, kMonster[(int)mb.type].radius);
            ma.x = pa.x; ma.z = pa.z;
            mb.x = pb.x; mb.z = pb.z;
        }

    // Contact damage (the player handles armor, invincibility and knockback)
    for (const Monster& m : m_List)
    {
        const float dx = p.x - m.x, dz = p.z - m.z;
        const float r = kPlayerRadius + kMonster[(int)m.type].radius;
        if (dx * dx + dz * dz < r * r)
            player->TakeDamage(m.attack, m.x, m.z, kMonster[(int)m.type].name);
    }

    // Potion pickup
    for (size_t i = 0; i < m_Potions.size();)
    {
        const float dx = p.x - m_Potions[i].x, dz = p.z - m_Potions[i].z;
        if (dx * dx + dz * dz < 0.6f * 0.6f && !player->IsDead())
        {
            player->AddPotion();
            DebugUI::Log("Picked up a red potion. (%d)", player->GetPotions());
            m_Potions[i] = m_Potions.back();
            m_Potions.pop_back();
        }
        else
            i++;
    }

    // New monsters trickle in out of sight
    m_SpawnTimer -= dt;
    if (m_SpawnTimer <= 0.0f)
    {
        m_SpawnTimer = (float)std::max(7, 17 - m_FloorNo);
        if ((int)m_List.size() < 10 + 3 * m_FloorNo)
        {
            int candidates = 0, chosen = -1;
            for (int i = 0; i < TileMap::W * TileMap::H; i++)
            {
                if (map->Get(i % TileMap::W, i / TileMap::W) != Tile::Floor || m_Flow[i] <= 10)
                    continue;
                // reservoir sampling: pick uniformly without building a list
                candidates++;
                if (std::uniform_int_distribution<int>(1, candidates)(m_Rng) == 1)
                    chosen = i;
            }
            if (chosen >= 0)
                Spawn(RandomType(), chosen % TileMap::W + 0.5f, chosen / TileMap::W + 0.5f);
        }
    }
}

void Monsters::UpdateOne(Monster& m, const TileMap& map, float px, float pz, float dt)
{
    const MonsterDef& def = kMonster[(int)m.type];
    m.flash -= dt;

    const float dx = px - m.x, dz = pz - m.z;
    const float d = sqrtf(dx * dx + dz * dz);

    // Wake on sight, fall asleep again far away
    if (!m.awake && d < def.sight && ClearLine(map, m.x, m.z, px, pz))
        m.awake = true;
    if (m.awake && d > def.sight * 2.5f)
        m.awake = false;

    float vx = 0.0f, vz = 0.0f;
    if (m.awake)
    {
        float sx, sz;
        Steer(m, map, px, pz, sx, sz);
        switch (m.type)
        {
        case MonsterType::Slime:            // hop toward the target every 0.9-1.3 s
        {
            m.timer -= dt;
            if (m.timer <= 0.0f)
            {
                m.timer = std::uniform_real_distribution<float>(0.9f, 1.3f)(m_Rng);
                m.velX = sx * 4.6f;
                m.velZ = sz * 4.6f;
            }
            const float slow = powf(0.02f, dt);
            m.velX *= slow;
            m.velZ *= slow;
            vx = m.velX;
            vz = m.velZ;
            break;
        }
        case MonsterType::Bat:              // zigzag: sideways swing on top of the steering
        {
            m.phase += dt * 7.0f;
            const float swing = sinf(m.phase) * 0.9f;
            vx = (sx - sz * swing) * def.speed;
            vz = (sz + sx * swing) * def.speed;
            break;
        }
        case MonsterType::Brute:            // walk, then wind up, charge and rest
            UpdateBrute(m, map, px, pz, sx, sz, dt, vx, vz);
            break;
        default:                            // goblin: run straight along the steering
            vx = sx * def.speed;
            vz = sz * def.speed;
            break;
        }
    }
    else
    {
        m.state = BruteWalk;
        // Asleep: wander, picking a new direction about 60% of seconds
        if (std::uniform_real_distribution<float>(0.0f, 1.0f)(m_Rng) < dt * 0.6f)
        {
            const float a = std::uniform_real_distribution<float>(0.0f, XM_2PI)(m_Rng);
            m.velX = cosf(a) * 0.7f;
            m.velZ = sinf(a) * 0.7f;
        }
        vx = m.velX;
        vz = m.velZ;
    }

    Vector3 pos(m.x, 0.0f, m.z);
    Vector3 vel(vx + m.knockX, 0.0f, vz + m.knockZ);
    map.MoveAndSlide(pos, vel, dt, def.radius);
    if (vel.x == 0.0f) m.knockX = 0.0f;
    if (vel.z == 0.0f) m.knockZ = 0.0f;
    m.x = pos.x;
    m.z = pos.z;

    const float decay = powf(0.003f, dt);
    m.knockX *= decay;
    m.knockZ *= decay;
}

// Brute cycle: walk toward the player; when close (4.5) and in sight, wind up for 0.6 s
// (flashing), charge in a straight line at 7.5 tiles/s for 0.5 s, rest 0.9 s, walk again.
void Monsters::UpdateBrute(Monster& m, const TileMap& map, float px, float pz, float sx, float sz,
    float dt, float& vx, float& vz)
{
    const MonsterDef& def = kMonster[(int)m.type];
    const float dx = px - m.x, dz = pz - m.z;
    const float d = sqrtf(dx * dx + dz * dz);
    m.stateTimer -= dt;

    switch (m.state)
    {
    case BruteWalk:
        vx = sx * def.speed;
        vz = sz * def.speed;
        if (d < 4.5f && m.stateTimer <= 0.0f && ClearLine(map, m.x, m.z, px, pz))
        {
            m.state = BruteWindUp;
            m.stateTimer = 0.6f;
            m.chargeAngle = atan2f(dz, dx);     // locked in: sidestep to dodge
        }
        break;
    case BruteWindUp:
        if (m.stateTimer <= 0.0f) { m.state = BruteCharge; m.stateTimer = 0.5f; }
        break;
    case BruteCharge:
        vx = cosf(m.chargeAngle) * 7.5f;
        vz = sinf(m.chargeAngle) * 7.5f;
        if (m.stateTimer <= 0.0f) { m.state = BruteRest; m.stateTimer = 0.9f; }
        break;
    default:    // BruteRest
        if (m.stateTimer <= 0.0f) { m.state = BruteWalk; m.stateTimer = 1.0f; }
        break;
    }
}

// ---------------------------------------------------------------- combat

int Monsters::HitInArc(float ox, float oz, float facing, float range, float arc, float damage)
{
    int kills = 0;
    for (Monster& m : m_List)
    {
        const MonsterDef& def = kMonster[(int)m.type];
        const float dx = m.x - ox, dz = m.z - oz;
        const float dist = sqrtf(dx * dx + dz * dz);
        if (dist >= range + def.radius)
            continue;
        if (dist >= 0.45f && AngleDiff(atan2f(dz, dx), facing) > arc * 0.5f + 0.25f)
            continue;   // outside the swing (anything closer than 0.45 always gets hit)

        const int dealt = std::max(1, (int)roundf(damage));
        m.hp -= dealt;
        m.flash = 0.1f;
        m.awake = true;
        if (dist > 0.0f)
        {
            m.knockX += dx / dist * 3.0f * def.knockback;
            m.knockZ += dz / dist * 3.0f * def.knockback;
        }
        if (m.hp <= 0.0f)
        {
            kills++;
            DebugUI::Log("You defeated the %s.", def.name);
            if (std::uniform_real_distribution<float>(0.0f, 1.0f)(m_Rng) < 0.12f)
                DropPotion(m.x, m.z);           // 12% potion drop
        }
    }
    RemoveDead();
    return kills;
}

void Monsters::RemoveDead()
{
    for (size_t i = 0; i < m_List.size();)
    {
        if (m_List[i].hp <= 0.0f)
        {
            m_List[i] = m_List.back();      // swap-and-pop
            m_List.pop_back();
        }
        else
            i++;
    }
}

void Monsters::KillAll()
{
    m_List.clear();
}

int Monsters::AwakeCount() const
{
    int n = 0;
    for (const Monster& m : m_List)
        if (m.awake)
            n++;
    return n;
}

// ---------------------------------------------------------------- drawing

void Monsters::Draw()
{
    Camera* camera = Manager::GetGameObj<Camera>();
    if (!camera || (m_List.empty() && m_Potions.empty()))
        return;

    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
    ctx->IASetInputLayout(m_VertexLayout);
    ctx->VSSetShader(m_VertexShader, NULL, 0);
    ctx->PSSetShader(m_PixelShader, NULL, 0);
    ctx->PSSetShaderResources(0, 1, &m_Atlas);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // Swap in the point sampler, remember the renderer's one to put back afterwards.
    ID3D11SamplerState* oldSampler = nullptr;
    ctx->PSGetSamplers(0, 1, &oldSampler);
    ctx->PSSetSamplers(0, 1, &m_PointSampler);
    Renderer::SetATCEnable(true);       // transparent pixels are cut out, so depth sorting just works

    // Billboard: copy the camera's rotation so sprites always face it.
    XMMATRIX invView = XMMatrixInverse(nullptr, camera->GetViewMatrix());
    invView.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    MATERIAL material{};
    material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.TextureEnable = TRUE;

    for (const Monster& m : m_List)
    {
        const MonsterDef& def = kMonster[(int)m.type];
        float y = 0.0f, squash = 1.0f;
        if (m.type == MonsterType::Bat)
            y = 0.45f + sinf(m_Time * 11.0f + m.phase) * 0.08f;
        if (m.type == MonsterType::Slime)
        {
            const float hop = std::min(1.0f, sqrtf(m.velX * m.velX + m.velZ * m.velZ) / 4.6f);
            y = sinf(hop * XM_PI) * 0.18f;
            squash = 1.0f - hop * 0.15f;
        }

        // Brute wind-up: shakes and flickers white as a warning
        const bool windUp = m.type == MonsterType::Brute && m.state == BruteWindUp;
        const float shakeX = windUp ? sinf(m_Time * 90.0f) * 0.04f : 0.0f;
        const bool flicker = windUp && ((int)(m_Time / 0.07f) % 2) == 0;

        const float bright = (m.flash > 0.0f || flicker) ? 4.0f : 1.0f;     // hit flash
        material.Diffuse = XMFLOAT4(bright, bright, bright, 1.0f);
        Renderer::SetMaterial(material);
        Renderer::SetWorldMatrix(XMMatrixScaling(def.height, def.height * squash, 1.0f) *
            invView * XMMatrixTranslation(m.x + shakeX, y, m.z));
        ctx->IASetVertexBuffers(0, 1, &m_QuadVB[(int)m.type], &stride, &offset);
        ctx->Draw(4, 0);
    }
    // Potions bob gently on the floor
    material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    Renderer::SetMaterial(material);
    ctx->IASetVertexBuffers(0, 1, &m_QuadVB[kMonsterTypeCount], &stride, &offset);
    for (const Potion& potion : m_Potions)
    {
        const float y = 0.05f + sinf(m_Time * 4.5f + potion.x) * 0.05f;
        Renderer::SetWorldMatrix(XMMatrixScaling(0.55f, 0.55f, 1.0f) *
            invView * XMMatrixTranslation(potion.x, y, potion.z));
        ctx->Draw(4, 0);
    }
    // HP bars over hurt monsters: dark background, red fill nudged toward the camera
    material.TextureEnable = FALSE;
    for (const Monster& m : m_List)
    {
        if (m.hp >= m.maxHp)
            continue;
        const MonsterDef& def = kMonster[(int)m.type];
        const float barY = def.height + 0.15f + (m.type == MonsterType::Bat ? 0.45f : 0.0f);
        const float w = 0.6f, h = 0.07f;
        const float f = std::max(0.0f, m.hp / m.maxHp);
        ctx->IASetVertexBuffers(0, 1, &m_QuadVB[(int)m.type], &stride, &offset);

        material.Diffuse = XMFLOAT4(0.05f, 0.03f, 0.03f, 1.0f);
        Renderer::SetMaterial(material);
        Renderer::SetWorldMatrix(XMMatrixScaling(w, h, 1.0f) *
            invView * XMMatrixTranslation(m.x, barY, m.z));
        ctx->Draw(4, 0);

        material.Diffuse = XMFLOAT4(0.82f, 0.33f, 0.31f, 1.0f);
        Renderer::SetMaterial(material);
        Renderer::SetWorldMatrix(XMMatrixScaling(w * f, h, 1.0f) *
            XMMatrixTranslation(-(1.0f - f) * w * 0.5f, 0.0f, -0.01f) *
            invView * XMMatrixTranslation(m.x, barY, m.z));
        ctx->Draw(4, 0);
    }

    Renderer::SetATCEnable(false);
    ctx->PSSetSamplers(0, 1, &oldSampler);
    if (oldSampler)
        oldSampler->Release();          // PSGetSamplers added a reference
}
