// Pickups.cpp
// Pool update (pop-up arc with gravity 14, bounce off walls and blocks at x -0.4, stop on
// landing, magnet pull within 1.7 tiles, collect within 0.45), and drawing as lit, camera-
// facing 8x8 sprites. Grey sprites (ore chunk, gel, fang) are tinted with the material colour.

#include "main.h"
#include "renderer.h"
#include "manager.h"
#include "Camera.h"
#include "Player.h"
#include "TileMap.h"
#include "Pickups.h"
#include "SpriteAtlas.h"
#include "GameState.h"
#include "Juice.h"
#include "DebugUI.h"
#include "Text.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace
{
    Pickups::Settings g_Settings;

    // 8x8 sprites, feet on the bottom row. Grey ones (w/l/d) get the material colour as a tint.
    const SpriteDef kChunkSprite =
    {
        { "........", "........", "........", "...wl...", "..wlll..", ".wllldd.", ".lllddd.", "..dddd.." },
        { { 'w', 0xffffff }, { 'l', 0xc8c8c8 }, { 'd', 0x8c8c8c }, { 0, 0 } }
    };
    const SpriteDef kGelSprite =
    {
        { "........", "........", "........", "...ww...", "..wwlw..", ".wlllll.", ".llllll.", "..dddd.." },
        { { 'w', 0xffffff }, { 'l', 0xc8c8c8 }, { 'd', 0x8c8c8c }, { 0, 0 } }
    };
    const SpriteDef kFangSprite =
    {
        { "........", "........", "....w...", "...wl...", "...wl...", "..wll...", "..lld...", ".ddd...." },
        { { 'w', 0xffffff }, { 'l', 0xc8c8c8 }, { 'd', 0x8c8c8c }, { 0, 0 } }
    };
    const SpriteDef kBombSprite =
    {
        { "........", "......f.", ".....k..", "..kkkk..", ".kwkkkk.", ".kkkkkk.", ".kkkkkk.", "..kkkk.." },
        { { 'k', 0x3a3a44 }, { 'w', 0x8a8a9a }, { 'f', 0xffcc44 }, { 0, 0 } }
    };
    const SpriteDef kTorchSprite =
    {
        { "...f....", "..fyf...", "..fff...", "...b....", "...b....", "...b....", "...b....", "...b...." },
        { { 'f', 0xff8a30 }, { 'y', 0xffe080 }, { 'b', 0x7a5030 }, { 0, 0 } }
    };

    // Atlas order: chunk, potion, gel, fang, bomb, torch
    const SpriteDef* const kSprites[] = { &kChunkSprite, &kPotionSprite, &kGelSprite, &kFangSprite, &kBombSprite, &kTorchSprite };

    int SpriteFor(MatId id)
    {
        switch (id)
        {
        case MatId::Potion: return 1;
        case MatId::Gel:    return 2;
        case MatId::Fang:   return 3;
        case MatId::Bomb:   return 4;
        case MatId::Torch:  return 5;
        default:            return 0;       // stone, metals, crystal, core: tinted chunk
        }
    }

    bool Tinted(MatId id)
    {
        const int s = SpriteFor(id);
        return s == 0 || s == 2 || s == 3;
    }

    float SpriteHeight(MatId id)
    {
        return (id == MatId::Potion || id == MatId::Torch) ? 0.55f : 0.42f;
    }

    void Rgb(uint32_t rgb, float& r, float& g, float& b)
    {
        r = ((rgb >> 16) & 0xFF) / 255.0f;
        g = ((rgb >> 8) & 0xFF) / 255.0f;
        b = (rgb & 0xFF) / 255.0f;
    }

    template<typename T>
    void SafeRelease(T*& p)
    {
        if (p) { p->Release(); p = nullptr; }
    }
}

Pickups::Settings& Pickups::GetSettings() { return g_Settings; }

void Pickups::Init()
{
    m_Layer = 1;
    m_Rng.seed(std::random_device{}());

    Renderer::CreateVertexShader(&m_VS, &m_Layout, "shader\\pointLitVS.cso");
    Renderer::CreatePixelShader(&m_PS, "shader\\pointLitPS.cso");
    Renderer::CreatePixelShader(&m_ShadowPS, "shader\\shadowAlphaPS.cso");

    m_Atlas = SpriteAtlas::Build(kSprites, kSpriteCount);
    for (int k = 0; k < kSpriteCount; k++)
        m_QuadVB[k] = SpriteAtlas::CreateQuad(k, kSpriteCount);
    m_PointSampler = SpriteAtlas::CreatePointSampler();
}

void Pickups::Uninit()
{
    for (ID3D11Buffer*& vb : m_QuadVB)
        SafeRelease(vb);
    SafeRelease(m_Layout);
    SafeRelease(m_VS);
    SafeRelease(m_PS);
    SafeRelease(m_ShadowPS);
    SafeRelease(m_Atlas);
    SafeRelease(m_PointSampler);
    GameObject::Uninit();
}

// ---------------------------------------------------------------- spawning

Pickups::Item* Pickups::NewItem()
{
    for (Item& item : m_Items)
        if (!item.active)
        {
            item = Item();
            item.active = true;
            return &item;
        }
    return nullptr;                         // pool full: the drop is lost (128 is plenty)
}

void Pickups::Spawn(MatId id, int count, float x, float z)
{
    if (count <= 0)
        return;
    Item* item = NewItem();
    if (!item)
        return;
    const float angle = std::uniform_real_distribution<float>(0.0f, XM_2PI)(m_Rng);
    const float speed = std::uniform_real_distribution<float>(1.0f, 2.2f)(m_Rng);
    item->id = id;
    item->count = count;
    item->x = x;
    item->z = z;
    item->y = 0.3f;
    item->vx = cosf(angle) * speed;
    item->vz = sinf(angle) * speed;
    item->vy = std::uniform_real_distribution<float>(2.6f, 4.0f)(m_Rng);
}

void Pickups::Place(MatId id, int count, float x, float z)
{
    if (count <= 0)
        return;
    Item* item = NewItem();
    if (!item)
        return;
    item->id = id;
    item->count = count;
    item->x = x;
    item->z = z;
    item->onGround = true;
}

void Pickups::DropFromPlayer(MatId id, int count, float x, float z)
{
    if (count <= 0)
        return;
    Item* item = NewItem();
    if (!item)
        return;
    item->id = id;
    item->count = count;
    item->x = x;
    item->z = z;
    item->y = 0.3f;
    item->vy = 2.6f;
    item->waitAway = true;
    item->dropX = x;
    item->dropZ = z;
}

void Pickups::Clear()
{
    for (Item& item : m_Items)
        item.active = false;
}

int Pickups::Count() const
{
    int n = 0;
    for (const Item& item : m_Items)
        if (item.active)
            n++;
    return n;
}

int Pickups::CountOf(MatId id) const
{
    int n = 0;
    for (const Item& item : m_Items)
        if (item.active && item.id == id)
            n += item.count;
    return n;
}

// ---------------------------------------------------------------- update

void Pickups::Collect(Item& item)
{
    Player* player = Manager::GetGameObj<Player>();
    if (!player)
        return;
    const int taken = player->GetBag().Add(item.id, item.count);
    const Vector3 p = player->GetPosition();
    if (taken > 0)
    {
        item.count -= taken;
        if (item.id == MatId::Potion)
            DebugUI::Log("Picked up a red potion. (%d)", player->GetPotions());
        char text[48];
        snprintf(text, sizeof(text), "+%d %s", taken, Tr(kMat[(int)item.id].name));
        Juice::Number(p.x, 1.5f, p.z, text, kMat[(int)item.id].color);
    }
    if (item.count <= 0)
    {
        item.active = false;
        return;
    }
    // Some or all did not fit: leave the rest and try again a little later
    item.retry = g_Settings.retryDelay;
    if (m_BagFullTimer <= 0.0f)
    {
        Juice::Number(p.x, 1.8f, p.z, Tr("Bag full"), 0xe05a4a);
        m_BagFullTimer = g_Settings.bagFullMessageGap;
    }
}

void Pickups::Update()
{
    if (GamePaused())
        return;
    const float dt = 1.0f / 60.0f;
    m_Time += dt;
    if (m_BagFullTimer > 0.0f)
        m_BagFullTimer -= dt;

    Player* player = Manager::GetGameObj<Player>();
    TileMap* map = Manager::GetGameObj<TileMap>();
    if (!player || !map)
        return;
    const Vector3 p = player->GetPosition();
    const bool canCollect = !player->IsDead();

    for (Item& item : m_Items)
    {
        if (!item.active)
            continue;

        if (!item.onGround)
        {
            // Flying: gravity 14, bounce off walls and blocks (velocity x -0.4), stop on landing
            item.vy -= 14.0f * dt;
            const float nx = item.x + item.vx * dt;
            if (map->Blocks((int)floorf(nx), (int)floorf(item.z)))
                item.vx *= -0.4f;
            else
                item.x = nx;
            const float nz = item.z + item.vz * dt;
            if (map->Blocks((int)floorf(item.x), (int)floorf(nz)))
                item.vz *= -0.4f;
            else
                item.z = nz;
            item.y += item.vy * dt;
            if (item.y <= 0.0f)
            {
                item.y = 0.0f;
                item.onGround = true;
                item.vx = item.vy = item.vz = 0.0f;
            }
            continue;
        }

        if (item.retry > 0.0f)
        {
            item.retry -= dt;
            continue;
        }
        if (item.waitAway)
        {
            const float ax = p.x - item.dropX, az = p.z - item.dropZ;
            if (ax * ax + az * az < 1.1f * 1.1f)
                continue;
            item.waitAway = false;
        }
        if (!canCollect)
            continue;

        // On the ground: pulled toward the player when close, collected on touch
        const float dx = p.x - item.x, dz = p.z - item.z;
        const float d = sqrtf(dx * dx + dz * dz);
        if (d < g_Settings.collectRadius)
        {
            Collect(item);
            continue;
        }
        const float magnet = g_Settings.magnetRadius;
        if (d < magnet && d > 0.0f)
        {
            const float speed = 2.5f + 6.0f * (1.0f - d / magnet);
            const float step = std::min(d, speed * dt);
            item.x += dx / d * step;
            item.z += dz / d * step;
        }
    }
}

// ---------------------------------------------------------------- drawing

void Pickups::Draw()
{
    Camera* camera = Manager::GetGameObj<Camera>();
    const TileMap* map = Manager::GetGameObj<TileMap>();
    if (!camera || Count() == 0)
        return;

    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
    ctx->IASetInputLayout(m_Layout);
    ctx->VSSetShader(m_VS, NULL, 0);
    ctx->PSSetShader(m_PS, NULL, 0);
    ctx->PSSetShaderResources(0, 1, &m_Atlas);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    ID3D11SamplerState* oldSampler = nullptr;
    ctx->PSGetSamplers(0, 1, &oldSampler);
    ctx->PSSetSamplers(0, 1, &m_PointSampler);
    Renderer::SetATCEnable(true);

    XMMATRIX invView = XMMatrixInverse(nullptr, camera->GetViewMatrix());
    invView.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;

    MATERIAL material{};
    material.Ambient = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    material.TextureEnable = TRUE;

    for (const Item& item : m_Items)
    {
        if (!item.active)
            continue;
        if (map && !map->IsVisible((int)floorf(item.x), (int)floorf(item.z)))
            continue;                       // fog of war hides pickups out of sight

        float r = 1.0f, g = 1.0f, b = 1.0f;
        if (Tinted(item.id))
            Rgb(kMat[(int)item.id].color, r, g, b);
        material.Diffuse = XMFLOAT4(r, g, b, 1.0f);
        // A faint glow so loot can be found in the dark; w = 1: camera-facing sprite
        material.Emission = XMFLOAT4(r * 0.12f, g * 0.12f, b * 0.12f, 1.0f);
        Renderer::SetMaterial(material);

        const float bob = item.onGround ? 0.04f + sinf(m_Time * 4.5f + item.x * 3.0f) * 0.04f : 0.0f;
        const float h = SpriteHeight(item.id);
        Renderer::SetWorldMatrix(XMMatrixScaling(h, h, 1.0f) * invView *
            XMMatrixTranslation(item.x, item.y + bob, item.z));
        ctx->IASetVertexBuffers(0, 1, &m_QuadVB[SpriteFor(item.id)], &stride, &offset);
        ctx->Draw(4, 0);
    }

    Renderer::SetATCEnable(false);
    ctx->PSSetSamplers(0, 1, &oldSampler);
    if (oldSampler)
        oldSampler->Release();              // PSGetSamplers added a reference
}

// Shadow pass: alpha-tested silhouettes, like the monsters
void Pickups::DrawShadow()
{
    Camera* camera = Manager::GetGameObj<Camera>();
    const TileMap* map = Manager::GetGameObj<TileMap>();
    if (!camera || Count() == 0)
        return;

    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
    ctx->IASetInputLayout(m_Layout);
    ctx->VSSetShader(m_VS, NULL, 0);
    ctx->PSSetShader(m_ShadowPS, NULL, 0);
    ctx->PSSetShaderResources(0, 1, &m_Atlas);
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    ID3D11SamplerState* oldSampler = nullptr;
    ctx->PSGetSamplers(0, 1, &oldSampler);
    ctx->PSSetSamplers(0, 1, &m_PointSampler);

    XMMATRIX invView = XMMatrixInverse(nullptr, camera->GetViewMatrix());
    invView.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;

    for (const Item& item : m_Items)
    {
        if (!item.active)
            continue;
        if (map && !map->IsVisible((int)floorf(item.x), (int)floorf(item.z)))
            continue;
        const float h = SpriteHeight(item.id);
        Renderer::SetWorldMatrix(XMMatrixScaling(h, h, 1.0f) * invView *
            XMMatrixTranslation(item.x, item.y, item.z));
        ctx->IASetVertexBuffers(0, 1, &m_QuadVB[SpriteFor(item.id)], &stride, &offset);
        ctx->Draw(4, 0);
    }

    ctx->PSSetSamplers(0, 1, &oldSampler);
    if (oldSampler)
        oldSampler->Release();
}
