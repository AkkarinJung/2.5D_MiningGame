// TileMap.cpp
// Floor generation (border, 14 wall pieces, seal unreachable tiles, stairs on the
// farthest tile) and drawing: the floor is one textured quad, walls are one cube
// drawn per wall tile that touches open floor and is inside the camera view.

#include "main.h"
#include "renderer.h"
#include "manager.h"
#include "Camera.h"
#include "TileMap.h"
#include "OreData.h"
#include <random>
#include <cmath>
#include <algorithm>

namespace
{
    const int kDX[4] = { 0, 1, 0, -1 };
    const int kDZ[4] = { -1, 0, 1, 0 };

    VERTEX_3D MakeVertex(XMFLOAT3 p, XMFLOAT3 n, float shade, XMFLOAT2 uv)
    {
        VERTEX_3D v;
        v.Position = p;
        v.Normal = n;
        v.Diffuse = XMFLOAT4(shade, shade, shade, 1.0f);
        v.TexCoord = uv;
        return v;
    }

    // One face of a unit cube (-0.5..0.5). r and u are the face's right and up
    // directions as seen from outside. Triangles are clockwise = front faces.
    void AddFace(std::vector<VERTEX_3D>& out, XMFLOAT3 n, XMFLOAT3 r, XMFLOAT3 u, float shade)
    {
        auto P = [&](float sr, float su)
            {
                return XMFLOAT3(n.x * 0.5f + r.x * sr + u.x * su,
                    n.y * 0.5f + r.y * sr + u.y * su,
                    n.z * 0.5f + r.z * sr + u.z * su);
            };
        const VERTEX_3D tl = MakeVertex(P(-0.5f, 0.5f), n, shade, XMFLOAT2(0.0f, 0.0f));
        const VERTEX_3D tr = MakeVertex(P(0.5f, 0.5f), n, shade, XMFLOAT2(1.0f, 0.0f));
        const VERTEX_3D bl = MakeVertex(P(-0.5f, -0.5f), n, shade, XMFLOAT2(0.0f, 1.0f));
        const VERTEX_3D br = MakeVertex(P(0.5f, -0.5f), n, shade, XMFLOAT2(1.0f, 1.0f));
        out.push_back(tl); out.push_back(tr); out.push_back(bl);
        out.push_back(tr); out.push_back(br); out.push_back(bl);
    }

    ID3D11Buffer* CreateVB(const VERTEX_3D* vertices, UINT count)
    {
        D3D11_BUFFER_DESC bd{};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(VERTEX_3D) * count;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

        D3D11_SUBRESOURCE_DATA sd{};
        sd.pSysMem = vertices;

        ID3D11Buffer* buffer = nullptr;
        Renderer::GetDevice()->CreateBuffer(&bd, &sd, &buffer);
        assert(buffer);
        return buffer;
    }

    ID3D11ShaderResourceView* LoadTexture(const wchar_t* path)
    {
        TexMetadata metadata;
        ScratchImage image;
        LoadFromWICFile(path, WIC_FLAGS_NONE, &metadata, image);

        ID3D11ShaderResourceView* srv = nullptr;
        CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
            image.GetImageCount(), metadata, &srv);
        assert(srv);
        return srv;
    }

    template<typename T>
    void SafeRelease(T*& p)
    {
        if (p) { p->Release(); p = nullptr; }
    }
}

void TileMap::Init()
{
    m_Layer = 1;

    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "shader\\unlitTextureVS.cso");
    Renderer::CreatePixelShader(&m_PixelShader, "shader\\unlitTexturePS.cso");

    m_WallTexture = LoadTexture(L"asset\\texture\\Brick.jpg");
    m_FloorTexture = LoadTexture(L"asset\\texture\\block_field.png");

    // Cube without a bottom face; each side has its own shade so edges read clearly.
    std::vector<VERTEX_3D> cube;
    cube.reserve(30);
    AddFace(cube, XMFLOAT3(0, 1, 0), XMFLOAT3(1, 0, 0), XMFLOAT3(0, 0, 1), 1.00f); // top
    AddFace(cube, XMFLOAT3(0, 0, -1), XMFLOAT3(1, 0, 0), XMFLOAT3(0, 1, 0), 0.80f); // front (toward camera)
    AddFace(cube, XMFLOAT3(0, 0, 1), XMFLOAT3(-1, 0, 0), XMFLOAT3(0, 1, 0), 0.55f); // back
    AddFace(cube, XMFLOAT3(1, 0, 0), XMFLOAT3(0, 0, 1), XMFLOAT3(0, 1, 0), 0.70f); // right
    AddFace(cube, XMFLOAT3(-1, 0, 0), XMFLOAT3(0, 0, -1), XMFLOAT3(0, 1, 0), 0.70f); // left
    m_CubeVertexCount = (UINT)cube.size();
    m_CubeVB = CreateVB(cube.data(), m_CubeVertexCount);

    // Floor: one quad over the whole map, texture repeats once per tile.
    const XMFLOAT3 up(0, 1, 0);
    const float w = (float)W, h = (float)H;
    VERTEX_3D floorV[4] =
    {
        MakeVertex(XMFLOAT3(0, 0, h), up, 1.0f, XMFLOAT2(0, 0)),
        MakeVertex(XMFLOAT3(w, 0, h), up, 1.0f, XMFLOAT2(w, 0)),
        MakeVertex(XMFLOAT3(0, 0, 0), up, 1.0f, XMFLOAT2(0, h)),
        MakeVertex(XMFLOAT3(w, 0, 0), up, 1.0f, XMFLOAT2(w, h)),
    };
    m_FloorVB = CreateVB(floorV, 4);

    // Unit quad on the floor (-0.5..0.5), used for the stairs marker.
    VERTEX_3D quadV[4] =
    {
        MakeVertex(XMFLOAT3(-0.5f, 0,  0.5f), up, 1.0f, XMFLOAT2(0, 0)),
        MakeVertex(XMFLOAT3(0.5f, 0,  0.5f), up, 1.0f, XMFLOAT2(1, 0)),
        MakeVertex(XMFLOAT3(-0.5f, 0, -0.5f), up, 1.0f, XMFLOAT2(0, 1)),
        MakeVertex(XMFLOAT3(0.5f, 0, -0.5f), up, 1.0f, XMFLOAT2(1, 1)),
    };
    m_QuadVB = CreateVB(quadV, 4);

    m_Tiles.reserve(W * H);
    m_Dist.reserve(W * H);
    m_VisibleWalls.reserve(W * H);

    Generate(std::random_device{}(), 1);
}

void TileMap::Uninit()
{
    SafeRelease(m_CubeVB);
    SafeRelease(m_FloorVB);
    SafeRelease(m_QuadVB);
    SafeRelease(m_VertexLayout);
    SafeRelease(m_VertexShader);
    SafeRelease(m_PixelShader);
    SafeRelease(m_WallTexture);
    SafeRelease(m_FloorTexture);

    GameObject::Uninit();
}

void TileMap::Update()
{
    GameObject::Update();
}

//--------------------- generation--------------------------

void TileMap::Set(int x, int z, Tile t)
{
    if (x >= 0 && z >= 0 && x < W && z < H)
        m_Tiles[z * W + x] = t;
}

Tile TileMap::Get(int x, int z) const
{
    if (x < 0 || z < 0 || x >= W || z >= H)
        return Tile::Wall;
    return m_Tiles[z * W + x];
}

bool TileMap::Blocks(int x, int z) const
{
    const Tile t = Get(x, z);
    return t == Tile::Wall || t == Tile::Block;
}

bool TileMap::IsStairs(const Vector3& pos) const
{
    return Get((int)floorf(pos.x), (int)floorf(pos.z)) == Tile::Stairs;
}

void TileMap::Generate(uint32_t seed, int floorNo)
{
    m_Seed = seed;
    m_FloorNo = floorNo;

    // Own generator per floor: the same seed + floor always gives the same layout.
    std::mt19937 rng(seed ^ (uint32_t)(floorNo * 0x9E3779B9u));
    auto randInt = [&](int n) { return std::uniform_int_distribution<int>(0, n - 1)(rng); };
    auto chance = [&](float p) { return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) < p; };

    // 1. Border is wall, inside is floor.
    m_Tiles.assign(W * H, Tile::Floor);
    for (int z = 0; z < H; z++)
        for (int x = 0; x < W; x++)
            if (x == 0 || z == 0 || x == W - 1 || z == H - 1)
                Set(x, z, Tile::Wall);

    // 2. 14 wall pieces: half pillars (1x1 or 2x2), half broken runs (4-10 long, 85% per tile).
    for (int k = 0; k < 14; k++)
    {
        const int x = 2 + randInt(W - 6);
        const int z = 2 + randInt(H - 6);
        if (chance(0.5f))
        {
            const int size = 1 + randInt(2);
            for (int j = 0; j < size; j++)
                for (int i = 0; i < size; i++)
                    Set(x + i, z + j, Tile::Wall);
        }
        else
        {
            const bool horizontal = chance(0.5f);
            const int length = 4 + randInt(7);
            for (int s = 0; s < length; s++)
            {
                const int xx = horizontal ? x + s : x;
                const int zz = horizontal ? z : z + s;
                if (xx < W - 1 && zz < H - 1 && chance(0.85f))
                    Set(xx, zz, Tile::Wall);
            }
        }
    }

    // 3. Start on a random floor tile.
    std::vector<int> floors;
    floors.reserve(W * H);
    for (int i = 0; i < W * H; i++)
        if (m_Tiles[i] == Tile::Floor)
            floors.push_back(i);
    const int start = floors[randInt((int)floors.size())];
    m_StartX = start % W;
    m_StartZ = start / W;

    // 4. Breadth-first search (4 directions) from the start.
    m_Dist.assign(W * H, -1);
    std::vector<int> queue;
    queue.reserve(W * H);
    queue.push_back(start);
    m_Dist[start] = 0;
    for (size_t head = 0; head < queue.size(); head++)
    {
        const int c = queue[head];
        const int cx = c % W, cz = c / W;
        for (int d = 0; d < 4; d++)
        {
            const int nx = cx + kDX[d], nz = cz + kDZ[d];
            if (Get(nx, nz) == Tile::Wall)
                continue;
            const int j = nz * W + nx;
            if (m_Dist[j] < 0)
            {
                m_Dist[j] = m_Dist[c] + 1;
                queue.push_back(j);
            }
        }
    }

    // 5. Seal floor the search never reached, so every floor tile is reachable.
    for (int i = 0; i < W * H; i++)
        if (m_Tiles[i] == Tile::Floor && m_Dist[i] < 0)
            m_Tiles[i] = Tile::Wall;

    // 6. Stairs on the last tile reached = the farthest walk from the start.
    const int stairs = queue.back();
    m_Tiles[stairs] = Tile::Stairs;
    m_StairsX = stairs % W;
    m_StairsZ = stairs / W;
    assert(m_Dist[stairs] >= 0);

    // Only walls next to open space can ever be seen.
    m_VisibleWalls.clear();
    for (int z = 0; z < H; z++)
        for (int x = 0; x < W; x++)
        {
            if (Get(x, z) != Tile::Wall)
                continue;
            bool nearOpen = false;
            for (int j = -1; j <= 1 && !nearOpen; j++)
                for (int i = -1; i <= 1; i++)
                    if (Get(x + i, z + j) != Tile::Wall) { nearOpen = true; break; }
            if (nearOpen)
                m_VisibleWalls.push_back(z * W + x);
        }
}

// 7. Ore veins on tiles more than 2 steps from the start; rarer grades deeper.
void TileMap::PlaceOre(uint32_t seed, int floorNo)
{
    std::mt19937 rng(seed ^ (uint32_t)(floorNo * 0x85EBCA6Bu) ^ 0x0BE5u);
    auto randInt = [&](int n) { return std::uniform_int_distribution<int>(0, n - 1)(rng); };
    auto chance = [&](float p) { return std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) < p; };

    m_OreGrade.assign(W * H, 0);
    m_BlockHp.assign(W * H, 0.0f);
    m_BlockDamage.assign(W * H, 0.0f);
    m_BlocksLeft = 0;

    std::vector<int> open;
    open.reserve(W * H);
    for (int i = 0; i < W * H; i++)
        if (m_Tiles[i] == Tile::Floor && m_Dist[i] > 2)
            open.push_back(i);

    auto takeAt = [&](size_t k) { const int i = open[k]; open[k] = open.back(); open.pop_back(); return i; };

    const float f = (float)floorNo;
    const float weights[kOreCount] =
    {
        std::max(14.0f, 62.0f - 6.0f * f),   // copper
        22.0f + 2.0f * f,                    // silver
        8.0f + 2.5f * f,                     // gold
        3.0f + 1.6f * f,                     // crystal
    };
    const float total = weights[0] + weights[1] + weights[2] + weights[3];

    const int veins = 12 + std::min(6, floorNo);
    for (int v = 0; v < veins && !open.empty(); v++)
    {
        // Pick a grade by weight
        float roll = std::uniform_real_distribution<float>(0.0f, total)(rng);
        int grade = 0;
        for (int g = 0; g < kOreCount; g++)
        {
            if (roll < weights[g]) { grade = g; break; }
            roll -= weights[g];
        }

        // 65% single block, 35% grows to 2-3 blocks into free neighbours
        const int size = chance(0.35f) ? 2 + randInt(2) : 1;
        int i = takeAt(randInt((int)open.size()));
        for (int c = 0; c < size; c++)
        {
            m_Tiles[i] = Tile::Block;
            m_OreGrade[i] = (uint8_t)grade;
            m_BlockHp[i] = kOre[grade].baseHp + f * (2.0f + grade);
            m_BlocksLeft++;

            if (c == size - 1)
                break;
            const int d = randInt(4);
            const int nx = i % W + kDX[d], nz = i / W + kDZ[d];
            if (Get(nx, nz) != Tile::Floor)
                break;
            auto it = std::find(open.begin(), open.end(), nz * W + nx);
            if (it == open.end())
                break;
            i = takeAt((size_t)(it - open.begin()));
        }
    }
}

// ---------------------------------------------------------------- mining

int TileMap::BlocksInArc(float ox, float oz, float facing, float range, float arc, int* out, int maxOut) const
{
    int count = 0;
    const int rays = std::max(3, (int)lroundf(arc * 4.0f));
    for (int k = 0; k <= rays; k++)
    {
        const float a = facing - arc * 0.5f + arc * k / rays;
        const float ca = cosf(a), sa = sinf(a);
        for (float r = 0.4f; r <= range + 0.05f; r += 0.25f)
        {
            const int tx = (int)floorf(ox + ca * r);
            const int tz = (int)floorf(oz + sa * r);
            const Tile t = Get(tx, tz);
            if (t == Tile::Wall)
                break;
            if (t == Tile::Block)
            {
                const int index = tz * W + tx;
                bool already = false;
                for (int n = 0; n < count; n++)
                    if (out[n] == index) { already = true; break; }
                if (!already && count < maxOut)
                    out[count++] = index;
                break;
            }
        }
    }
    return count;
}

MineResult TileMap::MineBlock(int x, int z, float power, int mineLevel)
{
    MineResult result;
    if (Get(x, z) != Tile::Block)
        return result;

    const int i = z * W + x;
    result.hit = true;
    result.grade = m_OreGrade[i];

    if (mineLevel < kOre[result.grade].requiredLevel)
    {
        power *= 0.25f;
        result.tooHard = true;
    }

    m_BlockDamage[i] += power;
    if (m_BlockDamage[i] >= m_BlockHp[i])
    {
        m_Tiles[i] = Tile::Floor;
        m_BlockDamage[i] = 0.0f;
        m_BlocksLeft--;
        result.broken = true;
    }
    return result;
}


// ---------------------------------------------------------------- collision

bool TileMap::BoxHits(float x, float z, float half) const
{
    const float eps = 0.001f;   // touching a tile edge is not overlapping it
    const int x0 = (int)floorf(x - half), x1 = (int)floorf(x + half - eps);
    const int z0 = (int)floorf(z - half), z1 = (int)floorf(z + half - eps);
    for (int tz = z0; tz <= z1; tz++)
        for (int tx = x0; tx <= x1; tx++)
            if (Blocks(tx, tz))
                return true;
    return false;
}

void TileMap::MoveAndSlide(Vector3& pos, Vector3& vel, float dt, float half) const
{
    const float dx = vel.x * dt;
    const float dz = vel.z * dt;

    if (!BoxHits(pos.x + dx, pos.z, half)) pos.x += dx; else vel.x = 0.0f;
    if (!BoxHits(pos.x, pos.z + dz, half)) pos.z += dz; else vel.z = 0.0f;

    pos.y += vel.y * dt;
}

// ----------------------------------drawing------------------------------
void TileMap::Draw()
{
    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
    ctx->IASetInputLayout(m_VertexLayout);
    ctx->VSSetShader(m_VertexShader, NULL, 0);
    ctx->PSSetShader(m_PixelShader, NULL, 0);

    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;

    MATERIAL material{};
    material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.TextureEnable = TRUE;
    Renderer::SetMaterial(material);

    // Floor
    Renderer::SetWorldMatrix(XMMatrixIdentity());
    ctx->PSSetShaderResources(0, 1, &m_FloorTexture);
    ctx->IASetVertexBuffers(0, 1, &m_FloorVB, &stride, &offset);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->Draw(4, 0);

    // Walls: skip the ones outside the camera view (tile culling)
    Camera* camera = Manager::GetGameObj<Camera>();
    ctx->PSSetShaderResources(0, 1, &m_WallTexture);
    ctx->IASetVertexBuffers(0, 1, &m_CubeVB, &stride, &offset);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    m_DrawnWalls = 0;
    for (int index : m_VisibleWalls)
    {
        const int x = index % W, z = index / W;
        const float height = 1.05f + ((x * 7 + z * 13) % 5) * 0.04f;   // slight height variation
        const Vector3 center(x + 0.5f, height * 0.5f, z + 0.5f);

        if (camera && !camera->CheckView(center, 1.0f))
            continue;

        Renderer::SetWorldMatrix(XMMatrixScaling(1.0f, height, 1.0f) *
            XMMatrixTranslation(center.x, center.y, center.z));
        ctx->Draw(m_CubeVertexCount, 0);
        m_DrawnWalls++;
    }

    // Ore blocks: tinted cubes that shrink a little as they take damage
    for (int i = 0; i < W * H; i++)
    {
        if (m_Tiles[i] != Tile::Block)
            continue;
        const int x = i % W, z = i / W;
        const float damage = std::min(1.0f, m_BlockDamage[i] / m_BlockHp[i]);
        const float size = 0.8f * (1.0f - damage * 0.18f);
        const Vector3 center(x + 0.5f, size * 0.5f, z + 0.5f);

        if (camera && !camera->CheckView(center, 1.0f))
            continue;

        const OreDef& ore = kOre[m_OreGrade[i]];
        material.Diffuse = XMFLOAT4(ore.r, ore.g, ore.b, 1.0f);
        Renderer::SetMaterial(material);
        Renderer::SetWorldMatrix(XMMatrixScaling(size, size, size) *
            XMMatrixTranslation(center.x, center.y, center.z));
        ctx->Draw(m_CubeVertexCount, 0);
    }

    // Stairs marker: a flat gold square
    material.TextureEnable = FALSE;
    material.Diffuse = XMFLOAT4(0.88f, 0.66f, 0.23f, 1.0f);
    Renderer::SetMaterial(material);
    Renderer::SetWorldMatrix(XMMatrixScaling(0.8f, 1.0f, 0.8f) *
        XMMatrixTranslation(m_StairsX + 0.5f, 0.02f, m_StairsZ + 0.5f));
    ctx->IASetVertexBuffers(0, 1, &m_QuadVB, &stride, &offset);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->Draw(4, 0);
}