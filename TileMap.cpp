// TileMap.cpp
// Floor generation (border, 14 wall pieces, seal unreachable tiles, stairs on the
// farthest tile, ore veins), mining, and drawing: the floor is one textured quad,
// all walls are baked into one mesh per floor (one draw call), ore blocks are cubes.
// Floor 0 (C8) is the town: an open grass field fenced by hedges, a ring of trees,
// and the dungeon gate at the far (north) end, which works like the stairs.

#include "main.h"
#include "renderer.h"
#include "manager.h"
#include "TileMap.h"
#include "OreData.h"
#include "Lighting.h"
#include "Juice.h"
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

    // A small RGBA texture made from pixels in memory (the town's grass and hedge).
    ID3D11ShaderResourceView* CreateTextureRGBA(int w, int h, const std::vector<uint32_t>& pixels)
    {
        D3D11_TEXTURE2D_DESC td{};
        td.Width = w;
        td.Height = h;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA sd{};
        sd.pSysMem = pixels.data();
        sd.SysMemPitch = w * 4;

        ID3D11Texture2D* texture = nullptr;
        Renderer::GetDevice()->CreateTexture2D(&td, &sd, &texture);
        assert(texture);
        ID3D11ShaderResourceView* srv = nullptr;
        Renderer::GetDevice()->CreateShaderResourceView(texture, nullptr, &srv);
        texture->Release();                 // the view keeps the texture alive
        assert(srv);
        return srv;
    }

    // R8G8B8A8 in memory is bytes r, g, b, a = 0xAABBGGRR as a little-endian uint32
    uint32_t Pixel(int r, int g, int b)
    {
        auto c = [](int v) { return (uint32_t)std::min(std::max(v, 0), 255); };
        return 0xFF000000u | (c(b) << 16) | (c(g) << 8) | c(r);
    }

    // 64x64 grass: speckled greens and a few small flowers (fixed seed: same every run)
    std::vector<uint32_t> MakeGrassPixels()
    {
        const int n = 64;
        std::mt19937 rng(1234u);
        std::uniform_int_distribution<int> jitter(-14, 14);
        std::vector<uint32_t> px(n * n);
        for (int i = 0; i < n * n; i++)
        {
            const int v = jitter(rng);
            const bool dark = (rng() % 7) == 0;
            px[i] = dark ? Pixel(70 + v, 128 + v, 52 + v) : Pixel(96 + v, 164 + v, 70 + v);
        }
        const uint32_t petals[3] = { Pixel(250, 248, 236), Pixel(250, 222, 90), Pixel(240, 150, 190) };
        for (int f = 0; f < 9; f++)
        {
            const int x = rng() % (n - 1), y = rng() % (n - 1);
            const uint32_t c = petals[f % 3];
            px[y * n + x] = c; px[y * n + x + 1] = c; px[(y + 1) * n + x] = c; px[(y + 1) * n + x + 1] = c;
        }
        return px;
    }

    // 32x32 hedge: dark leafy greens
    std::vector<uint32_t> MakeHedgePixels()
    {
        const int n = 32;
        std::mt19937 rng(4321u);
        std::uniform_int_distribution<int> jitter(-12, 12);
        std::vector<uint32_t> px(n * n);
        for (int i = 0; i < n * n; i++)
        {
            const int v = jitter(rng);
            const int k = rng() % 5;
            px[i] = k == 0 ? Pixel(36 + v, 78 + v, 34 + v)          // shadowed leaves
                  : k == 1 ? Pixel(88 + v, 150 + v, 66 + v)         // lit leaves
                           : Pixel(56 + v, 112 + v, 46 + v);
        }
        return px;
    }

    // One triangle, wound so it faces along n (clockwise as seen from outside = front face).
    void AddTri(std::vector<VERTEX_3D>& out, XMFLOAT3 a, XMFLOAT3 b, XMFLOAT3 c, XMFLOAT3 n, XMFLOAT4 color)
    {
        const XMVECTOR va = XMLoadFloat3(&a), vb = XMLoadFloat3(&b), vc = XMLoadFloat3(&c);
        const float facing = XMVectorGetX(XMVector3Dot(XMVector3Cross(vb - va, vc - va), XMLoadFloat3(&n)));
        if (facing < 0.0f)
            std::swap(b, c);
        VERTEX_3D v{};
        v.Normal = n;
        v.Diffuse = color;
        v.Position = a; out.push_back(v);
        v.Position = b; out.push_back(v);
        v.Position = c; out.push_back(v);
    }

    // The town's layout (prototype values, with the rows flipped so the gate is at the far end)
    const int kTownX0 = 5, kTownX1 = 30;    // open tiles x 5..30
    const int kTownZ0 = 5, kTownZ1 = 22;    // open tiles z 5..22 (26 x 18)
    const int kGateX = 17, kGateZ = 22;     // gate = the stairs, far (north) row
    const int kTownStartX = 17, kTownStartZ = 9;
}

void TileMap::Init()
{
    m_Layer = 1;

    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "shader\\pointLitVS.cso");   // M7: lit by point lights
    Renderer::CreatePixelShader(&m_PixelShader, "shader\\pointLitPS.cso");

    m_WallTexture = LoadTexture(L"asset\\texture\\Brick.jpg");
    m_FloorTexture = LoadTexture(L"asset\\texture\\Ground.png");

    // Cube without a bottom face; each side has its own shade so edges read clearly.
    std::vector<VERTEX_3D>& cube = m_CubeVertices;
    cube.clear();
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

    // Town (C8): grass floor repeating every 4 tiles, hedge walls, tree mesh
    VERTEX_3D townV[4] =
    {
        MakeVertex(XMFLOAT3(0, 0, h), up, 1.0f, XMFLOAT2(0, 0)),
        MakeVertex(XMFLOAT3(w, 0, h), up, 1.0f, XMFLOAT2(w / 4.0f, 0)),
        MakeVertex(XMFLOAT3(0, 0, 0), up, 1.0f, XMFLOAT2(0, h / 4.0f)),
        MakeVertex(XMFLOAT3(w, 0, 0), up, 1.0f, XMFLOAT2(w / 4.0f, h / 4.0f)),
    };
    m_TownFloorVB = CreateVB(townV, 4);
    m_GrassTexture = CreateTextureRGBA(64, 64, MakeGrassPixels());
    m_HedgeTexture = CreateTextureRGBA(32, 32, MakeHedgePixels());
    BuildTreeMesh();

    m_Tiles.reserve(W * H);
    m_Dist.reserve(W * H);
    m_VisibleWalls.reserve(W * H);
    m_OreGrade.reserve(W * H);
    m_BlockHp.reserve(W * H);
    m_BlockDamage.reserve(W * H);

    Generate(std::random_device{}(), 0);    // every run starts in the town (C8)
}

void TileMap::Uninit()
{
    SafeRelease(m_CubeVB);
    SafeRelease(m_WallVB);
    SafeRelease(m_FloorVB);
    SafeRelease(m_QuadVB);
    SafeRelease(m_VertexLayout);
    SafeRelease(m_VertexShader);
    SafeRelease(m_PixelShader);
    SafeRelease(m_WallTexture);
    SafeRelease(m_FloorTexture);
    SafeRelease(m_FogTexture);
    SafeRelease(m_FogSRV);
    SafeRelease(m_FogSampler);
    SafeRelease(m_TownFloorVB);
    SafeRelease(m_TreeVB);
    SafeRelease(m_GrassTexture);
    SafeRelease(m_HedgeTexture);

    GameObject::Uninit();
}

void TileMap::Update()
{
    // Juice: hit blocks wobble for a moment
    const float dt = 1.0f / 60.0f;
    m_Time += dt;
    for (float& s : m_BlockShake)
        if (s > 0.0f)
            s -= dt;

    // Fog of war: upload the visibility worked out last frame and bind it (t3, s2) for pointLitPS
    UploadFog();
    if (m_FogSRV)
    {
        ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
        ctx->PSSetShaderResources(3, 1, &m_FogSRV);
        ctx->PSSetSamplers(2, 1, &m_FogSampler);
        Lighting::SetFogMap(W, H);
    }

    if (IsTown())
    {
        // Town: the two torches beside the gate
        for (int side = -1; side <= 1; side += 2)
            Lighting::AddLight(m_StairsX + 0.5f + side * 0.85f, 1.4f, m_StairsZ + 0.3f, 4.0f, 0xff9a40,
                               1.3f * Lighting::Flicker(side * 3.0f));
    }
    else
    {
        // M7: every crystal glows and lights the floor around it; the stairs give a gold glow
        for (int i = 0; i < W * H; i++)
            if (m_Tiles[i] == Tile::Block && m_OreGrade[i] == 3)
                Lighting::AddLight(i % W + 0.5f, 0.8f, i / W + 0.5f, 4.5f, 0x6fdcea,
                                   1.1f * Lighting::Flicker((float)i));
        Lighting::AddLight(m_StairsX + 0.5f, 0.6f, m_StairsZ + 0.5f, 3.0f, 0xe0a83a, 0.8f);
    }

    GameObject::Update();
}

// ---------------------------------------------------------------- generation

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
    Lighting::ApplyFloorPreset(floorNo);    // town daylight or dungeon dark (C8)

    if (floorNo <= 0)
    {
        m_FloorNo = 0;
        GenerateTown(seed);
        return;
    }
    m_Trees.clear();

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
    const int farthest = ComputeDistances(start);

    // 5. Seal floor the search never reached, so every floor tile is reachable.
    for (int i = 0; i < W * H; i++)
        if (m_Tiles[i] == Tile::Floor && m_Dist[i] < 0)
            m_Tiles[i] = Tile::Wall;

    // 6. Stairs on the last tile reached = the farthest walk from the start.
    const int stairs = farthest;
    m_Tiles[stairs] = Tile::Stairs;
    m_StairsX = stairs % W;
    m_StairsZ = stairs / W;
    assert(m_Dist[stairs] >= 0);

    FindVisibleWalls();
    BuildWallMesh();
    PlaceOre(seed, floorNo);

    // New floor: nothing seen yet
    m_Visible.assign(W * H, 0);
    m_Seen.assign(W * H, 0);
    m_ViewTile = -1;
    m_FogDirty = true;
}

// Breadth-first search (4 directions) from the start tile: m_Dist = steps, -1 = unreachable.
// Returns the last tile reached (the farthest walk from the start).
int TileMap::ComputeDistances(int start)
{
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
    return queue.back();
}

// Only walls next to open space can ever be seen.
void TileMap::FindVisibleWalls()
{
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

// Floor 0 (C8): a safe 26x18 grass field fenced by hedges, trees around it, the dungeon
// gate in the far row. No ore, no monsters; fully revealed (no fog of war).
void TileMap::GenerateTown(uint32_t seed)
{
    m_Tiles.assign(W * H, Tile::Wall);
    for (int z = kTownZ0; z <= kTownZ1; z++)
        for (int x = kTownX0; x <= kTownX1; x++)
            Set(x, z, Tile::Floor);

    m_StartX = kTownStartX;
    m_StartZ = kTownStartZ;
    m_StairsX = kGateX;
    m_StairsZ = kGateZ;
    Set(kGateX, kGateZ, Tile::Stairs);
    ComputeDistances(m_StartZ * W + m_StartX);

    FindVisibleWalls();
    BuildWallMesh();                        // hedges (lower than cave walls)
    PlaceOre(seed, 0);                      // clears the block arrays; places nothing on floor 0

    // A ring of trees 2 tiles outside the field, every 3rd tile, slightly jittered
    std::mt19937 rng(seed ^ 0x7EE5u);
    std::uniform_real_distribution<float> scale(0.9f, 1.4f), jitter(-0.25f, 0.25f);
    m_Trees.clear();
    auto addTree = [&](int x, int z) { m_Trees.push_back({ x + 0.5f + jitter(rng), z + 0.5f + jitter(rng), scale(rng) }); };
    for (int x = kTownX0 - 2; x <= kTownX1 + 2; x += 3)
    {
        addTree(x, kTownZ0 - 2);
        addTree(x, kTownZ1 + 2);
    }
    for (int z = kTownZ0 + 1; z <= kTownZ1 - 1; z += 3)
    {
        addTree(kTownX0 - 2, z);
        addTree(kTownX1 + 2, z);
    }

    // Fully revealed: everything counts as seen and visible
    m_Visible.assign(W * H, 1);
    m_Seen.assign(W * H, 1);
    m_ViewTile = -1;
    m_FogDirty = true;
}

// A low-poly tree: 8-sided trunk (0.8 high) and a 7-sided cone of leaves (1.5 high).
void TileMap::BuildTreeMesh()
{
    std::vector<VERTEX_3D> v;
    const XMFLOAT4 bark(0.45f, 0.30f, 0.18f, 1.0f);
    const float pi2 = XM_2PI;

    const int trunkSides = 8;
    const float trunkR = 0.11f, trunkH = 0.8f;
    for (int s = 0; s < trunkSides; s++)
    {
        const float a0 = pi2 * s / trunkSides, a1 = pi2 * (s + 1) / trunkSides, am = (a0 + a1) * 0.5f;
        const XMFLOAT3 n(cosf(am), 0.0f, sinf(am));
        const XMFLOAT3 b0(cosf(a0) * trunkR, 0.0f, sinf(a0) * trunkR), b1(cosf(a1) * trunkR, 0.0f, sinf(a1) * trunkR);
        const XMFLOAT3 t0(b0.x, trunkH, b0.z), t1(b1.x, trunkH, b1.z);
        AddTri(v, b0, t0, t1, n, bark);
        AddTri(v, b0, t1, b1, n, bark);
    }

    const int leafSides = 7;
    const float leafR = 0.62f, leafBase = 0.55f, leafTop = leafBase + 1.5f;
    const float slope = leafR / (leafTop - leafBase);          // tilts the side normals upward
    for (int s = 0; s < leafSides; s++)
    {
        const float a0 = pi2 * s / leafSides, a1 = pi2 * (s + 1) / leafSides, am = (a0 + a1) * 0.5f;
        XMFLOAT3 n(cosf(am), slope, sinf(am));
        XMStoreFloat3(&n, XMVector3Normalize(XMLoadFloat3(&n)));
        const float shade = (s % 2) ? 0.9f : 1.0f;            // alternate faces read as facets
        const XMFLOAT4 leaf(0.22f * shade, 0.52f * shade, 0.24f * shade, 1.0f);
        const XMFLOAT3 b0(cosf(a0) * leafR, leafBase, sinf(a0) * leafR), b1(cosf(a1) * leafR, leafBase, sinf(a1) * leafR);
        AddTri(v, b0, XMFLOAT3(0.0f, leafTop, 0.0f), b1, n, leaf);
        AddTri(v, b0, b1, XMFLOAT3(0.0f, leafBase, 0.0f), XMFLOAT3(0.0f, -1.0f, 0.0f), XMFLOAT4(0.16f, 0.36f, 0.17f, 1.0f));
    }

    m_TreeVertexCount = (UINT)v.size();
    m_TreeVB = CreateVB(v.data(), m_TreeVertexCount);
}

// Bake every visible wall into one vertex buffer. Walls never change during a floor,
// so this runs once per Generate and the whole dungeon's walls cost one draw call.
void TileMap::BuildWallMesh()
{
    std::vector<VERTEX_3D> vertices;
    vertices.reserve(m_VisibleWalls.size() * m_CubeVertices.size());

    for (int index : m_VisibleWalls)
    {
        const int x = index % W, z = index / W;
        const float height = IsTown()
            ? 0.70f + ((x * 7 + z * 13) % 5) * 0.05f                    // hedges 0.7-0.9
            : 1.05f + ((x * 7 + z * 13) % 5) * 0.04f;                   // slight height variation

        for (VERTEX_3D v : m_CubeVertices)
        {
            v.Position.x = v.Position.x + x + 0.5f;
            v.Position.y = (v.Position.y + 0.5f) * height;              // cube sits on the floor
            v.Position.z = v.Position.z + z + 0.5f;
            vertices.push_back(v);
        }
    }

    SafeRelease(m_WallVB);
    m_WallVertexCount = (UINT)vertices.size();
    if (m_WallVertexCount > 0)
        m_WallVB = CreateVB(vertices.data(), m_WallVertexCount);
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
    m_BlockShake.assign(W * H, 0.0f);
    m_BlocksLeft = 0;
    if (floorNo <= 0)
        return;                             // the town has no ore

    std::vector<int> open;
    open.reserve(W * H);
    for (int i = 0; i < W * H; i++)
        if (m_Tiles[i] == Tile::Floor && m_Dist[i] > 2)
            open.push_back(i);

    auto takeAt = [&](size_t k) { const int i = open[k]; open[k] = open.back(); open.pop_back(); return i; };

    const float f = (float)floorNo;
    float weights[kOreCount] =
    {
        std::max(14.0f, 62.0f - 6.0f * f),   // copper
        22.0f + 2.0f * f,                    // silver
        8.0f + 2.5f * f,                     // gold
        3.0f + 1.6f * f,                     // crystal
    };
    if (m_Motherlode)
    {
        weights[2] += 18.0f;                 // Motherlode card
        weights[3] += 14.0f;
    }
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
    m_BlockShake[i] = 0.15f;            // juice: wobble
    if (m_BlockDamage[i] >= m_BlockHp[i])
    {
        m_Tiles[i] = Tile::Floor;
        m_BlockDamage[i] = 0.0f;
        m_BlocksLeft--;
        result.broken = true;

        // M7: a short flash in the ore's colour, stronger for gold and crystal
        const OreDef& ore = kOre[result.grade];
        const uint32_t rgb = ((uint32_t)(ore.r * 255.0f) << 16) | ((uint32_t)(ore.g * 255.0f) << 8) | (uint32_t)(ore.b * 255.0f);
        Lighting::Flash(x + 0.5f, 1.0f, z + 0.5f, 5.0f, rgb, 1.0f + result.grade * 0.6f);
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

// ---------------------------------------------------------------- drawing

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

    const bool town = IsTown();

    // Floor (grass in the town)
    Renderer::SetWorldMatrix(XMMatrixIdentity());
    ctx->PSSetShaderResources(0, 1, town ? &m_GrassTexture : &m_FloorTexture);
    ctx->IASetVertexBuffers(0, 1, town ? &m_TownFloorVB : &m_FloorVB, &stride, &offset);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->Draw(4, 0);

    // Walls: one draw call for the whole floor (hedges in the town)
    ctx->PSSetShaderResources(0, 1, town ? &m_HedgeTexture : &m_WallTexture);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    if (m_WallVB)
    {
        ctx->IASetVertexBuffers(0, 1, &m_WallVB, &stride, &offset);
        ctx->Draw(m_WallVertexCount, 0);
    }
    m_DrawnWalls = (int)m_VisibleWalls.size();

    // Ore blocks: tinted cubes that shrink a little as they take damage
    ctx->IASetVertexBuffers(0, 1, &m_CubeVB, &stride, &offset);
    for (int i = 0; i < W * H; i++)
    {
        if (m_Tiles[i] != Tile::Block)
            continue;
        const int x = i % W, z = i / W;
        const float damage = std::min(1.0f, m_BlockDamage[i] / m_BlockHp[i]);
        const float size = 0.8f * (1.0f - damage * 0.18f);
        const float wobble = WobbleOffset(i);
        const Vector3 center(x + 0.5f + wobble, size * 0.5f, z + 0.5f - wobble * 0.6f);

        const OreDef& ore = kOre[m_OreGrade[i]];
        material.Diffuse = XMFLOAT4(ore.r, ore.g, ore.b, 1.0f);
        // Glow (M7): crystal shines on its own, gold a little
        if (m_OreGrade[i] == 3)
            material.Emission = XMFLOAT4(0.12f, 0.35f, 0.40f, 0.0f);
        else if (m_OreGrade[i] == 2)
            material.Emission = XMFLOAT4(0.12f, 0.08f, 0.0f, 0.0f);
        else
            material.Emission = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
        Renderer::SetMaterial(material);
        Renderer::SetWorldMatrix(XMMatrixScaling(size, size, size) *
            XMMatrixTranslation(center.x, center.y, center.z));
        ctx->Draw(m_CubeVertexCount, 0);
    }

    if (town)
    {
        DrawTown(material);                 // the gate replaces the stairs marker
        return;
    }

    // Stairs marker: a flat gold square
    material.TextureEnable = FALSE;
    material.Diffuse = XMFLOAT4(0.88f, 0.66f, 0.23f, 1.0f);
    material.Emission = XMFLOAT4(0.35f, 0.25f, 0.08f, 0.0f);    // visible even in the dark
    Renderer::SetMaterial(material);
    Renderer::SetWorldMatrix(XMMatrixScaling(0.8f, 1.0f, 0.8f) *
        XMMatrixTranslation(m_StairsX + 0.5f, 0.02f, m_StairsZ + 0.5f));
    ctx->IASetVertexBuffers(0, 1, &m_QuadVB, &stride, &offset);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->Draw(4, 0);
}

// Town (C8): the dungeon gate (two stone pillars, a lintel, a dark doorway, two torches)
// and the ring of trees.
void TileMap::DrawTown(MATERIAL& material)
{
    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    const float gx = m_StairsX + 0.5f, gz = m_StairsZ + 0.5f;

    // Stone pillars and lintel: brick-textured cubes
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx->IASetVertexBuffers(0, 1, &m_CubeVB, &stride, &offset);
    ctx->PSSetShaderResources(0, 1, &m_WallTexture);
    material.TextureEnable = TRUE;
    material.Diffuse = XMFLOAT4(0.70f, 0.68f, 0.66f, 1.0f);
    material.Emission = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    Renderer::SetMaterial(material);
    for (int side = -1; side <= 1; side += 2)
    {
        Renderer::SetWorldMatrix(XMMatrixScaling(0.32f, 1.7f, 0.32f) *
            XMMatrixTranslation(gx + side * 0.55f, 0.85f, gz));
        ctx->Draw(m_CubeVertexCount, 0);
    }
    Renderer::SetWorldMatrix(XMMatrixScaling(1.46f, 0.24f, 0.38f) * XMMatrixTranslation(gx, 1.82f, gz));
    ctx->Draw(m_CubeVertexCount, 0);

    // Torches: a thin stick and a small glowing flame beside each pillar
    material.TextureEnable = FALSE;
    for (int side = -1; side <= 1; side += 2)
    {
        const float tx = gx + side * 0.85f, tz = gz - 0.2f;
        material.Diffuse = XMFLOAT4(0.40f, 0.26f, 0.14f, 1.0f);
        material.Emission = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
        Renderer::SetMaterial(material);
        Renderer::SetWorldMatrix(XMMatrixScaling(0.06f, 1.1f, 0.06f) * XMMatrixTranslation(tx, 0.55f, tz));
        ctx->Draw(m_CubeVertexCount, 0);
        const float flicker = Lighting::Flicker(side * 3.0f);
        material.Diffuse = XMFLOAT4(1.0f, 0.6f, 0.25f, 1.0f);
        material.Emission = XMFLOAT4(0.9f * flicker, 0.5f * flicker, 0.15f, 0.0f);
        Renderer::SetMaterial(material);
        Renderer::SetWorldMatrix(XMMatrixScaling(0.13f, 0.18f, 0.13f) * XMMatrixTranslation(tx, 1.18f, tz));
        ctx->Draw(m_CubeVertexCount, 0);
    }

    // Doorway: a dark upright quad between the pillars, facing the camera
    material.Diffuse = XMFLOAT4(0.03f, 0.02f, 0.02f, 1.0f);
    material.Emission = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
    Renderer::SetMaterial(material);
    Renderer::SetWorldMatrix(XMMatrixScaling(0.78f, 1.0f, 1.7f) * XMMatrixRotationX(-XM_PIDIV2) *
        XMMatrixTranslation(gx, 0.85f, gz + 0.1f));
    ctx->IASetVertexBuffers(0, 1, &m_QuadVB, &stride, &offset);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->Draw(4, 0);

    // Trees: colours are in the vertices
    material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    Renderer::SetMaterial(material);
    ctx->IASetVertexBuffers(0, 1, &m_TreeVB, &stride, &offset);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    for (const TreeInstance& t : m_Trees)
    {
        Renderer::SetWorldMatrix(XMMatrixScaling(t.scale, t.scale, t.scale) * XMMatrixTranslation(t.x, 0.0f, t.z));
        ctx->Draw(m_TreeVertexCount, 0);
    }
}

// Shadow pass (M8): walls and ore blocks, depth only (no pixel shader).
void TileMap::DrawShadow()
{
    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
    ctx->IASetInputLayout(m_VertexLayout);
    ctx->VSSetShader(m_VertexShader, NULL, 0);
    ctx->PSSetShader(nullptr, NULL, 0);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;

    if (m_WallVB)
    {
        Renderer::SetWorldMatrix(XMMatrixIdentity());
        ctx->IASetVertexBuffers(0, 1, &m_WallVB, &stride, &offset);
        ctx->Draw(m_WallVertexCount, 0);
    }

    ctx->IASetVertexBuffers(0, 1, &m_CubeVB, &stride, &offset);
    for (int i = 0; i < W * H; i++)
    {
        if (m_Tiles[i] != Tile::Block)
            continue;
        const float damage = std::min(1.0f, m_BlockDamage[i] / m_BlockHp[i]);
        const float size = 0.8f * (1.0f - damage * 0.18f);
        Renderer::SetWorldMatrix(XMMatrixScaling(size, size, size) *
            XMMatrixTranslation(i % W + 0.5f + WobbleOffset(i), size * 0.5f, i / W + 0.5f));
        ctx->Draw(m_CubeVertexCount, 0);
    }

    if (IsTown())
    {
        // The gate and the trees cast sun shadows too
        const float gx = m_StairsX + 0.5f, gz = m_StairsZ + 0.5f;
        for (int side = -1; side <= 1; side += 2)
        {
            Renderer::SetWorldMatrix(XMMatrixScaling(0.32f, 1.7f, 0.32f) * XMMatrixTranslation(gx + side * 0.55f, 0.85f, gz));
            ctx->Draw(m_CubeVertexCount, 0);
        }
        Renderer::SetWorldMatrix(XMMatrixScaling(1.46f, 0.24f, 0.38f) * XMMatrixTranslation(gx, 1.82f, gz));
        ctx->Draw(m_CubeVertexCount, 0);

        ctx->IASetVertexBuffers(0, 1, &m_TreeVB, &stride, &offset);
        for (const TreeInstance& t : m_Trees)
        {
            Renderer::SetWorldMatrix(XMMatrixScaling(t.scale, t.scale, t.scale) * XMMatrixTranslation(t.x, 0.0f, t.z));
            ctx->Draw(m_TreeVertexCount, 0);
        }
    }
}

// ---------------------------------------------------------------- fog of war

// Line of sight from a point to the centre of tile (tx, tz), sampled every half tile.
// Walls block it (ore blocks do not); the target tile itself never blocks, so walls are seen.
bool TileMap::CanSee(float ax, float az, int tx, int tz) const
{
    const float bx = tx + 0.5f, bz = tz + 0.5f;
    const float dx = bx - ax, dz = bz - az;
    const int n = (int)ceilf(std::max(fabsf(dx), fabsf(dz)) * 2.0f);
    for (int s = 1; s < n; s++)
    {
        const float t = (float)s / n;
        const int sx = (int)floorf(ax + dx * t), sz = (int)floorf(az + dz * t);
        if (sx == tx && sz == tz)
            break;                          // reached the target tile
        if (Get(sx, sz) == Tile::Wall)
            return false;
    }
    return true;
}

void TileMap::UpdateVisibility(float x, float z, float radius)
{
    const int px = std::min(std::max((int)floorf(x), 0), W - 1);
    const int pz = std::min(std::max((int)floorf(z), 0), H - 1);
    const int tile = pz * W + px;
    if (tile == m_ViewTile && radius == m_ViewRadius && m_BlocksLeft == m_ViewBlocks)
        return;                             // nothing changed since last time
    m_ViewTile = tile;
    m_ViewRadius = radius;
    m_ViewBlocks = m_BlocksLeft;

    std::fill(m_Visible.begin(), m_Visible.end(), (uint8_t)0);
    const int r = (int)ceilf(radius);
    const float cx = px + 0.5f, cz = pz + 0.5f;
    for (int tz = pz - r; tz <= pz + r; tz++)
        for (int tx = px - r; tx <= px + r; tx++)
        {
            if (tx < 0 || tz < 0 || tx >= W || tz >= H)
                continue;
            const float dx = tx + 0.5f - cx, dz = tz + 0.5f - cz;
            if (dx * dx + dz * dz > radius * radius)
                continue;
            if (!CanSee(cx, cz, tx, tz))
                continue;
            m_Visible[tz * W + tx] = 1;
            m_Seen[tz * W + tx] = 1;
        }
    m_FogDirty = true;
}

bool TileMap::IsVisible(int x, int z) const
{
    if (!Lighting::GetSettings().fogOfWar || IsTown())
        return true;                        // the town is always fully visible
    if (x < 0 || z < 0 || x >= W || z >= H || m_Visible.empty())
        return false;
    return m_Visible[z * W + x] != 0;
}

void TileMap::RevealAll()
{
    std::fill(m_Seen.begin(), m_Seen.end(), (uint8_t)1);
    m_FogDirty = true;
}

// Build the 48x34 brightness texture (created on first use) and upload it when it changed.
void TileMap::UploadFog()
{
    if (m_Visible.empty())
        return;
    ID3D11Device* device = Renderer::GetDevice();
    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();

    if (!m_FogTexture)
    {
        D3D11_TEXTURE2D_DESC td{};
        td.Width = W;
        td.Height = H;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DYNAMIC;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateTexture2D(&td, nullptr, &m_FogTexture);
        assert(m_FogTexture);
        device->CreateShaderResourceView(m_FogTexture, nullptr, &m_FogSRV);

        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;   // soft edges between visible / seen / unseen
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        device->CreateSamplerState(&sd, &m_FogSampler);
        m_FogDirty = true;
    }

    const float seenLevel = Lighting::GetSettings().seenBrightness;
    if (!m_FogDirty && seenLevel == m_FogSeenLevel)
        return;
    m_FogDirty = false;
    m_FogSeenLevel = seenLevel;

    const uint8_t seen = (uint8_t)(std::min(std::max(seenLevel, 0.0f), 1.0f) * 255.0f);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(ctx->Map(m_FogTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
    {
        for (int z = 0; z < H; z++)
        {
            uint8_t* row = (uint8_t*)mapped.pData + z * mapped.RowPitch;     // rows may be padded
            for (int x = 0; x < W; x++)
            {
                const int i = z * W + x;
                row[x] = m_Visible[i] ? 255 : (m_Seen[i] ? seen : 0);
            }
        }
        ctx->Unmap(m_FogTexture, 0);
    }
}

// Juice: a block that was just hit shakes sideways for 0.15 s (strength from the F1 panel).
float TileMap::WobbleOffset(int i) const
{
    const Juice::Settings& js = Juice::GetSettings();
    if (!js.wobble || m_BlockShake.empty() || m_BlockShake[i] <= 0.0f)
        return 0.0f;
    return sinf(m_Time * 90.0f + i) * 0.05f * js.wobbleStrength * (m_BlockShake[i] / 0.15f);
}
