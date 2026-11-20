// Projectiles.cpp
// Bolt flight (3 substeps per tick against the grid, then monster circles), bomb arcs
// and explosions (monsters and blocks in the radius), and the effect rings.
// Mining goes through Player::MineTile so XP and level-ups work the same as swings.

#include "main.h"
#include "renderer.h"
#include "manager.h"
#include "Camera.h"
#include "Player.h"
#include "TileMap.h"
#include "Monsters.h"
#include "Projectiles.h"
#include "SpriteAtlas.h"
#include "GameState.h"
#include <cmath>
#include <algorithm>

namespace
{
    const SpriteDef kBoltSprite =
    {
        { "........", "...ww...", "..wppw..", ".wppppw.", ".wppppw.", "..wppw..", "...ww...", "........" },
        { { 'w', 0xf0e8ff },{ 'p', 0x8f6be0 },{ 0, 0 },{ 0, 0 } }
    };
    const SpriteDef kBombSprite =
    {
        { "......f.", ".....k..", "..rrrk..", ".rrrrr..", ".rdrrr..", ".rrrrr..", "..rrr...", "........" },
        { { 'r', 0xb8443c },{ 'd', 0x7a2a24 },{ 'f', 0xfff3b0 },{ 'k', 0x3b3337 } }
    };

    template<typename T>
    void SafeRelease(T*& p)
    {
        if (p) { p->Release(); p = nullptr; }
    }

    template<typename T>
    void SwapRemove(std::vector<T>& list, size_t i)
    {
        list[i] = list.back();
        list.pop_back();
    }
}

void Projectiles::Init()
{
    m_Layer = 1;
    m_Bolts.reserve(32);
    m_Waves.reserve(16);
    m_Bombs.reserve(8);
    m_Rings.reserve(16);

    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "shader\\unlitTextureVS.cso");
    Renderer::CreatePixelShader(&m_PixelShader, "shader\\unlitTexturePS.cso");

    const SpriteDef* sprites[2] = { &kBoltSprite, &kBombSprite };
    m_Atlas = SpriteAtlas::Build(sprites, 2);
    m_BoltVB = SpriteAtlas::CreateQuad(0, 2);
    m_BombVB = SpriteAtlas::CreateQuad(1, 2);
    m_PointSampler = SpriteAtlas::CreatePointSampler();

    // Ring texture: a soft white circle outline; the material colour tints it.
    const int size = 64;
    std::vector<uint32_t> pixels(size * size);
    for (int y = 0; y < size; y++)
        for (int x = 0; x < size; x++)
        {
            const float dx = (x + 0.5f) / (size * 0.5f) - 1.0f, dy = (y + 0.5f) / (size * 0.5f) - 1.0f;
            const float d = sqrtf(dx * dx + dy * dy);
            const float a = std::max(0.0f, 1.0f - fabsf(d - 0.88f) / 0.1f);
            pixels[y * size + x] = ((uint32_t)(a * 255.0f) << 24) | 0x00FFFFFFu;
        }
    D3D11_TEXTURE2D_DESC td{};
    td.Width = td.Height = size;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_IMMUTABLE;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = pixels.data();
    init.SysMemPitch = size * 4;
    ID3D11Texture2D* texture = nullptr;
    Renderer::GetDevice()->CreateTexture2D(&td, &init, &texture);
    assert(texture);
    Renderer::GetDevice()->CreateShaderResourceView(texture, nullptr, &m_RingTexture);
    texture->Release();

    // Flat unit quad on the floor, centred on the origin, facing up
    const XMFLOAT3 up(0, 1, 0);
    const XMFLOAT4 white(1, 1, 1, 1);
    VERTEX_3D v[4];
    v[0] = { XMFLOAT3(-0.5f, 0,  0.5f), up, white, XMFLOAT2(0, 0) };
    v[1] = { XMFLOAT3(0.5f, 0,  0.5f), up, white, XMFLOAT2(1, 0) };
    v[2] = { XMFLOAT3(-0.5f, 0, -0.5f), up, white, XMFLOAT2(0, 1) };
    v[3] = { XMFLOAT3(0.5f, 0, -0.5f), up, white, XMFLOAT2(1, 1) };
    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(v);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vd{};
    vd.pSysMem = v;
    Renderer::GetDevice()->CreateBuffer(&bd, &vd, &m_RingVB);
}

void Projectiles::Uninit()
{
    SafeRelease(m_BoltVB);
    SafeRelease(m_BombVB);
    SafeRelease(m_RingVB);
    SafeRelease(m_Atlas);
    SafeRelease(m_RingTexture);
    SafeRelease(m_PointSampler);
    SafeRelease(m_VertexLayout);
    SafeRelease(m_VertexShader);
    SafeRelease(m_PixelShader);

    GameObject::Uninit();
}

void Projectiles::FireBolt(float x, float z, float angle, float damage, float mine)
{
    Bolt b;
    b.x = x + cosf(angle) * 0.3f;
    b.z = z + sinf(angle) * 0.3f;
    b.vx = cosf(angle) * 11.0f;
    b.vz = sinf(angle) * 11.0f;
    b.life = 1.1f;
    b.damage = damage;
    b.mine = mine;
    m_Bolts.push_back(b);
}

void Projectiles::ThrowBomb(float x, float z, float targetX, float targetZ, float radius, float damage, float mine)
{
    Bomb b;
    b.startX = b.x = x;
    b.startZ = b.z = z;
    b.targetX = targetX;
    b.targetZ = targetZ;
    b.y = 0.0f;
    b.t = 0.0f;
    b.duration = 0.65f;
    b.radius = radius;
    b.damage = damage;
    b.mine = mine;
    b.visible = true;
    m_Bombs.push_back(b);
}

void Projectiles::QueueExplosion(float x, float z, float delay, float radius, float damage, float mine)
{
    ThrowBomb(x, z, x, z, radius, damage, mine);
    m_Bombs.back().duration = delay;
    m_Bombs.back().visible = false;
}

void Projectiles::FireWave(float x, float z, float angle, float damage, float mine)
{
    Monsters* monsters = Manager::GetGameObj<Monsters>();
    Wave w;
    w.x = x;
    w.z = z;
    w.vx = cosf(angle) * 9.0f;
    w.vz = sinf(angle) * 9.0f;
    w.life = 0.5f;
    w.damage = damage;
    w.mine = mine;
    w.hitId = monsters ? monsters->NewHitId() : 0;
    m_Waves.push_back(w);
}

void Projectiles::AddRing(float x, float z, float radius, uint32_t rgb, float life)
{
    Ring r;
    r.x = x;
    r.z = z;
    r.radius = radius;
    r.t = 0.0f;
    r.life = life;
    r.r = ((rgb >> 16) & 0xFF) / 255.0f;
    r.g = ((rgb >> 8) & 0xFF) / 255.0f;
    r.b = (rgb & 0xFF) / 255.0f;
    m_Rings.push_back(r);
}

void Projectiles::Explode(float x, float z, float radius, float damage, float mine)
{
    AddRing(x, z, radius, 0xffb35a);

    Camera* camera = Manager::GetGameObj<Camera>();
    if (camera)
        camera->Shake(Vector3(0.25f, 0.0f, 0.25f));

    Monsters* monsters = Manager::GetGameObj<Monsters>();
    if (monsters)
        monsters->HitInRadius(x, z, radius, damage, 6.0f);

    Player* player = Manager::GetGameObj<Player>();
    if (!player)
        return;
    for (int tz = (int)floorf(z - radius); tz <= (int)floorf(z + radius); tz++)
        for (int tx = (int)floorf(x - radius); tx <= (int)floorf(x + radius); tx++)
        {
            if (tx < 1 || tz < 1 || tx >= TileMap::W - 1 || tz >= TileMap::H - 1)
                continue;
            const float dx = tx + 0.5f - x, dz = tz + 0.5f - z;
            if (sqrtf(dx * dx + dz * dz) <= radius + 0.3f)
                player->MineTile(tx, tz, mine);
        }
}

// ---------------------------------------------------------------- update
void Projectiles::Update()
{
    TileMap* map = Manager::GetGameObj<TileMap>();
    Monsters* monsters = Manager::GetGameObj<Monsters>();
    Player* player = Manager::GetGameObj<Player>();
    if (!map)
        return;

    const float dt = 1.0f / 60.0f;
    if (GamePaused())
        return;                     // card pick open
    m_Time += dt;

    // Bolts: 3 substeps per tick so they never skip through a one-tile wall
    for (size_t i = 0; i < m_Bolts.size();)
    {
        Bolt& b = m_Bolts[i];
        b.life -= dt;
        bool dead = b.life <= 0.0f;
        for (int s = 0; s < 3 && !dead; s++)
        {
            const float nx = b.x + b.vx * dt / 3.0f, nz = b.z + b.vz * dt / 3.0f;
            const int tx = (int)floorf(nx), tz = (int)floorf(nz);
            const Tile t = map->Get(tx, tz);
            if (t == Tile::Wall)
            {
                dead = true; break;
            }
            if (t == Tile::Block)
            {
                if (player)
                    player->MineTile(tx, tz, b.mine);
                dead = true;
                break;
            }
            b.x = nx;
            b.z = nz;
            if (monsters && monsters->HitAtPoint(b.x, b.z, 0.2f, b.damage, b.x - b.vx * 0.05f, b.z - b.vz * 0.05f))
                dead = true;
        }
        if (dead)
            SwapRemove(m_Bolts, i);
        else
            i++;
    }

    // Sword waves: like bolts, but they pierce monsters (each hit once)
    for (size_t i = 0; i < m_Waves.size();)
    {
        Wave& w = m_Waves[i];
        w.life -= dt;
        bool dead = w.life <= 0.0f;
        for (int s = 0; s < 3 && !dead; s++)
        {
            const float nx = w.x + w.vx * dt / 3.0f, nz = w.z + w.vz * dt / 3.0f;
            const int tx = (int)floorf(nx), tz = (int)floorf(nz);
            const Tile t = map->Get(tx, tz);
            if (t == Tile::Wall || t == Tile::Block)
            {
                if (t == Tile::Block && player)
                    player->MineTile(tx, tz, w.mine);
                dead = true;
                break;
            }
            w.x = nx;
            w.z = nz;
            if (monsters)
                monsters->HitOnce(w.x, w.z, 0.2f, w.damage, w.hitId, 3.0f);
        }
        if (dead)
            SwapRemove(m_Waves, i);
        else
            i++;
    }


    // Bombs: fly along an arc, stop short of walls, explode on landing
    for (size_t i = 0; i < m_Bombs.size();)
    {
        Bomb& b = m_Bombs[i];
        b.t += dt;
        const float k = std::min(1.0f, b.t / b.duration);
        const float nx = b.startX + (b.targetX - b.startX) * k;
        const float nz = b.startZ + (b.targetZ - b.startZ) * k;
        if (!map->Blocks((int)floorf(nx), (int)floorf(nz)))
        {
            b.x = nx;
            b.z = nz;
        }
        b.y = sinf(k * XM_PI) * 1.3f;
        if (k >= 1.0f)
        {
            const Bomb landed = b;          // copy: Explode may add rings, and we remove b
            SwapRemove(m_Bombs, i);
            Explode(landed.x, landed.z, landed.radius, landed.damage, landed.mine);
        }
        else
            i++;
    }

    // Rings
    for (size_t i = 0; i < m_Rings.size();)
    {
        m_Rings[i].t += dt;
        if (m_Rings[i].t >= m_Rings[i].life)
            SwapRemove(m_Rings, i);
        else
            i++;
    }
}

// ---------------------------------------------------------------- drawing

void Projectiles::Draw()
{
    Camera* camera = Manager::GetGameObj<Camera>();
    if (!camera || (m_Bolts.empty() && m_Waves.empty() && m_Bombs.empty() && m_Rings.empty()))
        return;

    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
    ctx->IASetInputLayout(m_VertexLayout);
    ctx->VSSetShader(m_VertexShader, NULL, 0);
    ctx->PSSetShader(m_PixelShader, NULL, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;

    MATERIAL material{};
    material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.TextureEnable = TRUE;
    Renderer::SetMaterial(material);

    XMMATRIX invView = XMMatrixInverse(nullptr, camera->GetViewMatrix());
    invView.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

    // Sprites (bolts, bombs): point-sampled, cut-out billboards
    ID3D11SamplerState* oldSampler = nullptr;
    ctx->PSGetSamplers(0, 1, &oldSampler);
    ctx->PSSetSamplers(0, 1, &m_PointSampler);
    ctx->PSSetShaderResources(0, 1, &m_Atlas);
    Renderer::SetATCEnable(true);

    ctx->IASetVertexBuffers(0, 1, &m_BoltVB, &stride, &offset);
    for (const Bolt& b : m_Bolts)
    {
        Renderer::SetWorldMatrix(XMMatrixScaling(0.4f, 0.4f, 1.0f) * invView *
            XMMatrixTranslation(b.x, 0.35f, b.z));
        ctx->Draw(4, 0);
    }

    // Sword waves: the bolt sprite, stretched and tinted pale gold
    material.Diffuse = XMFLOAT4(1.0f, 0.95f, 0.6f, 1.0f);
    Renderer::SetMaterial(material);
    for (const Wave& w : m_Waves)
    {
        Renderer::SetWorldMatrix(XMMatrixScaling(0.9f, 0.35f, 1.0f) * invView *
            XMMatrixTranslation(w.x, 0.25f, w.z));
        ctx->Draw(4, 0);
    }
    material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    Renderer::SetMaterial(material);

    ctx->IASetVertexBuffers(0, 1, &m_BombVB, &stride, &offset);
    for (const Bomb& b : m_Bombs)
    {
        if (!b.visible)
            continue;
        Renderer::SetWorldMatrix(XMMatrixScaling(0.5f, 0.5f, 1.0f) * XMMatrixRotationZ(m_Time * 12.0f) *
            invView * XMMatrixTranslation(b.x, b.y, b.z));
        ctx->Draw(4, 0);
    }

    Renderer::SetATCEnable(false);
    ctx->PSSetSamplers(0, 1, &oldSampler);
    if (oldSampler)
        oldSampler->Release();

    // Rings: flat on the floor, grow from 30% to full radius while fading out
    ctx->PSSetShaderResources(0, 1, &m_RingTexture);
    ctx->IASetVertexBuffers(0, 1, &m_RingVB, &stride, &offset);
    for (const Ring& r : m_Rings)
    {
        const float k = r.t / r.life;
        const float d = r.radius * 2.0f * (0.3f + 0.7f * k);
        material.Diffuse = XMFLOAT4(r.r, r.g, r.b, 1.0f - k);
        Renderer::SetMaterial(material);
        Renderer::SetWorldMatrix(XMMatrixScaling(d, 1.0f, d) * XMMatrixTranslation(r.x, 0.04f, r.z));
        ctx->Draw(4, 0);
    }
}