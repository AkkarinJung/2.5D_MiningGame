// Juice.cpp
// Fixed-size pools (no allocation while playing): 320 debris particles and 48 floating
// numbers. When a pool is full, the oldest entry is reused. Debris are small lit cubes;
// numbers are drawn with ImGui's background draw list (over the 3D scene, under the UI).

#include "main.h"
#include "renderer.h"
#include "manager.h"
#include "Juice.h"
#include "Camera.h"
#include "TileMap.h"
#include "Projectiles.h"
#include "Lighting.h"
#include "GameState.h"
#include "Text.h"
#include <random>
#include <cmath>
#include <algorithm>
#include <cfloat>

#include "imgui/imgui.h"

namespace
{
    const int kMaxParticles = 320;
    const int kMaxNumbers = 48;

    struct Particle
    {
        float x, y, z, vx, vy, vz;
        float life;
        float angle, spin;
        float r, g, b;
    };

    struct FloatingNumber
    {
        float x, y, z;
        float life;                 // 1 -> 0
        float r, g, b;
        bool  big;
        char  text[40];
    };

    Juice::Settings g_Settings;
    Juice* g_Instance = nullptr;

    Particle g_Particles[kMaxParticles];
    int g_ParticleCount = 0;
    int g_ParticleNext = 0;         // oldest slot to reuse when full

    FloatingNumber g_Numbers[kMaxNumbers];
    int g_NumberCount = 0;
    int g_NumberNext = 0;

    float g_Time = 0.0f;
    float g_TooHardTime = -10.0f;   // "Too hard" popups at most every 0.9 s

    std::mt19937 g_Rng(1234);       // cosmetic only: never affects gameplay

    ID3D11Buffer* g_CubeVB = nullptr;
    ID3D11InputLayout* g_Layout = nullptr;
    ID3D11VertexShader* g_VS = nullptr;
    ID3D11PixelShader* g_PS = nullptr;

    float Rand(float a, float b)
    {
        return std::uniform_real_distribution<float>(a, b)(g_Rng);
    }

    void Rgb(uint32_t rgb, float& r, float& g, float& b)
    {
        r = ((rgb >> 16) & 0xFF) / 255.0f;
        g = ((rgb >> 8) & 0xFF) / 255.0f;
        b = (rgb & 0xFF) / 255.0f;
    }

    uint32_t Lighten(uint32_t rgb)
    {
        float r, g, b;
        Rgb(rgb, r, g, b);
        auto up = [](float c) { return (uint32_t)(std::min(1.0f, c * 0.5f + 0.5f) * 255.0f); };
        return (up(r) << 16) | (up(g) << 8) | up(b);
    }

    // Unit cube (-0.5..0.5), all 6 faces, clockwise triangles (front faces)
    void AddFace(std::vector<VERTEX_3D>& out, XMFLOAT3 n, XMFLOAT3 r, XMFLOAT3 u)
    {
        auto P = [&](float sr, float su)
            {
                return XMFLOAT3(n.x * 0.5f + r.x * sr + u.x * su, n.y * 0.5f + r.y * sr + u.y * su,
                    n.z * 0.5f + r.z * sr + u.z * su);
            };
        const XMFLOAT4 white(1, 1, 1, 1);
        const VERTEX_3D tl = { P(-0.5f, 0.5f), n, white, XMFLOAT2(0, 0) };
        const VERTEX_3D tr = { P(0.5f, 0.5f), n, white, XMFLOAT2(1, 0) };
        const VERTEX_3D bl = { P(-0.5f, -0.5f), n, white, XMFLOAT2(0, 1) };
        const VERTEX_3D br = { P(0.5f, -0.5f), n, white, XMFLOAT2(1, 1) };
        out.push_back(tl); out.push_back(tr); out.push_back(bl);
        out.push_back(tr); out.push_back(br); out.push_back(bl);
    }

    template<typename T>
    void SafeRelease(T*& p)
    {
        if (p) { p->Release(); p = nullptr; }
    }

    void DrawParticles(bool shadowPass)
    {
        if (g_ParticleCount == 0 || !g_CubeVB)
            return;
        ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
        ctx->IASetInputLayout(g_Layout);
        ctx->VSSetShader(g_VS, NULL, 0);
        ctx->PSSetShader(shadowPass ? nullptr : g_PS, NULL, 0);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        UINT stride = sizeof(VERTEX_3D), offset = 0;
        ctx->IASetVertexBuffers(0, 1, &g_CubeVB, &stride, &offset);

        MATERIAL material{};
        material.Ambient = XMFLOAT4(1, 1, 1, 1);
        material.TextureEnable = FALSE;
        for (int i = 0; i < g_ParticleCount; i++)
        {
            const Particle& p = g_Particles[i];
            const float size = g_Settings.debrisSize * std::min(1.0f, p.life / 0.3f);   // shrink away at the end
            if (!shadowPass)
            {
                material.Diffuse = XMFLOAT4(p.r, p.g, p.b, 1.0f);
                Renderer::SetMaterial(material);
            }
            Renderer::SetWorldMatrix(XMMatrixScaling(size, size, size) *
                XMMatrixRotationRollPitchYaw(p.angle, p.angle * 0.7f, 0.0f) *
                XMMatrixTranslation(p.x, p.y, p.z));
            ctx->Draw(36, 0);
        }
    }
}

// ---------------------------------------------------------------- static API

Juice::Settings& Juice::GetSettings() { return g_Settings; }
int Juice::ParticleCount() { return g_ParticleCount; }

void Juice::HitStop(float seconds)
{
    if (!g_Instance || !g_Settings.hitStop)
        return;
    const float s = std::min(0.06f, seconds) * g_Settings.hitStopStrength;
    HitStopTimer() = std::max(HitStopTimer(), s);
}

void Juice::Shake(float amount)
{
    if (!g_Instance || !g_Settings.shake)
        return;
    Camera* camera = Manager::GetGameObj<Camera>();
    if (camera)
    {
        const float a = amount * g_Settings.shakeStrength;
        camera->Shake(Vector3(a, 0.0f, a));
    }
}

void Juice::Number(float x, float y, float z, const char* text, uint32_t rgb, bool big)
{
    if (!g_Instance || !g_Settings.numbers)
        return;
    int slot = g_NumberCount;
    if (g_NumberCount < kMaxNumbers)
        g_NumberCount++;
    else
        slot = g_NumberNext = (g_NumberNext + 1) % kMaxNumbers;     // full: reuse the oldest
    FloatingNumber& n = g_Numbers[slot];
    n.x = x; n.y = y; n.z = z;
    n.life = 1.0f;
    Rgb(rgb, n.r, n.g, n.b);
    n.big = big;
    snprintf(n.text, sizeof(n.text), "%s", text);
}

void Juice::Debris(float x, float y, float z, uint32_t rgb, int count, float speed)
{
    if (!g_Instance || !g_Settings.debris)
        return;
    const int n = (int)roundf(count * g_Settings.debrisAmount);
    for (int k = 0; k < n; k++)
    {
        int slot = g_ParticleCount;
        if (g_ParticleCount < kMaxParticles)
            g_ParticleCount++;
        else
            slot = g_ParticleNext = (g_ParticleNext + 1) % kMaxParticles;
        Particle& p = g_Particles[slot];
        p.x = x; p.y = y; p.z = z;
        p.vx = Rand(-2.5f, 2.5f) * speed;
        p.vy = Rand(2.0f, 5.5f) * speed;
        p.vz = Rand(-2.5f, 2.5f) * speed;
        p.life = Rand(0.8f, 1.6f);
        p.angle = Rand(0.0f, 6.28f);
        p.spin = Rand(-12.0f, 12.0f);
        Rgb(rgb, p.r, p.g, p.b);
    }
}

void Juice::BlockHit(int tileX, int tileZ, uint32_t oreRgb)
{
    const float x = tileX + 0.5f, z = tileZ + 0.5f;
    Debris(x, 0.6f, z, oreRgb, 4, 0.7f);
    Debris(x, 0.6f, z, 0x6e6b71, 3, 0.6f);         // stone chips
    Shake(0.03f);
}

void Juice::TooHard(int tileX, int tileZ, int requiredLevel)
{
    if (g_Time - g_TooHardTime < 0.9f)
        return;
    g_TooHardTime = g_Time;
    char text[40];
    snprintf(text, sizeof(text), Tr("Too hard! Lv%d"), requiredLevel);
    Number(tileX + 0.5f, 1.1f, tileZ + 0.5f, text, 0xff8a7a);
}

void Juice::BlockBreak(int tileX, int tileZ, uint32_t oreRgb, int grade, int xp)
{
    const float x = tileX + 0.5f, z = tileZ + 0.5f;
    Debris(x, 0.4f, z, oreRgb, 18 + grade * 8, 1.1f);
    Debris(x, 0.4f, z, Lighten(oreRgb), 6 + grade * 4, 1.3f);
    Debris(x, 0.4f, z, 0x5d5a5f, 10, 1.0f);
    Shake(0.06f + grade * 0.04f);                   // rarer ore, bigger shake
    if (grade >= 2)
        HitStop(0.04f);                             // gold and crystal feel weighty
    if (g_Instance && g_Settings.dustRing)
    {
        Projectiles* projectiles = Manager::GetGameObj<Projectiles>();
        if (projectiles)
            projectiles->AddRing(x, z, 0.9f, 0x9b8b7a, 0.35f);
    }
    char text[24];
    snprintf(text, sizeof(text), Tr("+%d XP"), xp);
    Number(x, 0.9f, z, text, 0xb9e07a);
}

void Juice::MonsterHit(float x, float height, float z, int damage, bool crit, bool executed, bool direct)
{
    char text[16];
    snprintf(text, sizeof(text), "%d", damage);
    const uint32_t color = executed ? 0xd58cf0 : (crit ? 0xffd166 : (direct ? 0x9fbf5a : 0xffffff));
    Number(x, height * 0.9f, z, text, color, crit || executed);
    Debris(x, 0.5f, z, 0xd0554f, 4, 0.6f);
    if (crit || executed)
        HitStop(0.05f);
}

void Juice::MonsterKilled(float x, float z)
{
    Debris(x, 0.5f, z, 0xd0554f, 12, 0.9f);
    HitStop(0.045f);
}

void Juice::PlayerHurt(float x, float z, int damage)
{
    char text[16];
    snprintf(text, sizeof(text), "-%d", damage);
    Number(x, 1.3f, z, text, 0xff6b5e, true);
    Debris(x, 0.6f, z, 0xd0554f, 8, 0.8f);
    Shake(0.12f);
}

void Juice::Heal(float x, float z, int amount)
{
    char text[16];
    snprintf(text, sizeof(text), "+%d", amount);
    Number(x, 1.3f, z, text, 0xffb3a8, true);
}

void Juice::Explosion(float x, float z)
{
    Debris(x, 0.3f, z, 0xffb35a, 20, 1.4f);
    Debris(x, 0.3f, z, 0x5d5a5f, 14, 1.2f);
    Shake(0.25f);
    HitStop(0.05f);
}

void Juice::LevelUp(float x, float z, int level)
{
    char text[32];
    snprintf(text, sizeof(text), Tr("Mining Lv%d!"), level);
    Number(x, 1.5f, z, text, 0xb9e07a, true);
    if (!g_Instance || !g_Settings.levelUpBurst)
        return;
    Debris(x, 0.3f, z, 0xb9e07a, 22, 1.0f);
    Projectiles* projectiles = Manager::GetGameObj<Projectiles>();
    if (projectiles)
        projectiles->AddRing(x, z, 1.4f, 0xb9e07a, 0.4f);
    Lighting::Flash(x, 1.2f, z, 5.0f, 0xb9e07a, 2.5f);
}

// ---------------------------------------------------------------- object

void Juice::Init()
{
    m_Layer = 1;
    g_Instance = this;
    g_ParticleCount = g_NumberCount = 0;
    HitStopTimer() = 0.0f;

    Renderer::CreateVertexShader(&g_VS, &g_Layout, "shader\\pointLitVS.cso");
    Renderer::CreatePixelShader(&g_PS, "shader\\pointLitPS.cso");

    std::vector<VERTEX_3D> cube;
    cube.reserve(36);
    AddFace(cube, XMFLOAT3(0, 1, 0), XMFLOAT3(1, 0, 0), XMFLOAT3(0, 0, 1));
    AddFace(cube, XMFLOAT3(0, -1, 0), XMFLOAT3(1, 0, 0), XMFLOAT3(0, 0, -1));
    AddFace(cube, XMFLOAT3(0, 0, -1), XMFLOAT3(1, 0, 0), XMFLOAT3(0, 1, 0));
    AddFace(cube, XMFLOAT3(0, 0, 1), XMFLOAT3(-1, 0, 0), XMFLOAT3(0, 1, 0));
    AddFace(cube, XMFLOAT3(1, 0, 0), XMFLOAT3(0, 0, 1), XMFLOAT3(0, 1, 0));
    AddFace(cube, XMFLOAT3(-1, 0, 0), XMFLOAT3(0, 0, -1), XMFLOAT3(0, 1, 0));

    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(VERTEX_3D) * (UINT)cube.size();
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = cube.data();
    Renderer::GetDevice()->CreateBuffer(&bd, &sd, &g_CubeVB);
}

void Juice::Uninit()
{
    SafeRelease(g_CubeVB);
    SafeRelease(g_Layout);
    SafeRelease(g_VS);
    SafeRelease(g_PS);
    if (g_Instance == this)
        g_Instance = nullptr;
    HitStopTimer() = 0.0f;
    GameObject::Uninit();
}

void Juice::Update()
{
    const float dt = 1.0f / 60.0f;
    g_Time += dt;

    // Hit-stop counts down here (this object keeps updating while the game is frozen)
    if (HitStopTimer() > 0.0f)
        HitStopTimer() = std::max(0.0f, HitStopTimer() - dt);

    // Debris: gravity, bounce on the floor, lose speed, shrink and vanish
    for (int i = 0; i < g_ParticleCount;)
    {
        Particle& p = g_Particles[i];
        p.life -= dt;
        if (p.life <= 0.0f)
        {
            g_Particles[i] = g_Particles[--g_ParticleCount];    // swap-and-pop
            continue;
        }
        p.vy -= 16.0f * dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.z += p.vz * dt;
        p.angle += p.spin * dt;
        const float halfSize = g_Settings.debrisSize * 0.5f;
        if (p.y < halfSize)                         // bounce when the chunk touches the floor
        {
            p.y = halfSize;
            p.vy *= -0.35f;
            p.vx *= 0.55f;
            p.vz *= 0.55f;
            p.spin *= 0.5f;
        }
        i++;
    }
    if (g_ParticleNext >= g_ParticleCount)
        g_ParticleNext = 0;

    // Floating numbers rise and fade (about 0.8 s); drawn here because the ImGui frame is open
    Camera* camera = Manager::GetGameObj<Camera>();
    TileMap* map = Manager::GetGameObj<TileMap>();
    const XMMATRIX viewProj = camera ? camera->GetViewMatrix() * camera->GetProjectionMatrix() : XMMatrixIdentity();
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    for (int i = 0; i < g_NumberCount;)
    {
        FloatingNumber& n = g_Numbers[i];
        n.life -= dt * 1.2f;
        n.y += dt * 0.8f;
        if (n.life <= 0.0f)
        {
            g_Numbers[i] = g_Numbers[--g_NumberCount];
            continue;
        }
        i++;
        if (!camera || (map && !map->IsVisible((int)floorf(n.x), (int)floorf(n.z))))
            continue;                               // fog of war hides numbers out of sight

        const XMVECTOR clip = XMVector3TransformCoord(XMVectorSet(n.x, n.y, n.z, 1.0f), viewProj);
        const float cz = XMVectorGetZ(clip);
        if (cz < 0.0f || cz > 1.0f)
            continue;
        const float sx = (XMVectorGetX(clip) + 1.0f) * 0.5f * SCREEN_WIDTH;
        const float sy = (1.0f - XMVectorGetY(clip)) * 0.5f * SCREEN_HEIGHT;
        const float size = (n.big ? 24.0f : 19.0f) * g_Settings.numberSize;
        const float alpha = std::min(1.0f, n.life * 2.0f);
        const ImVec2 textSize = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, n.text);
        const ImVec2 pos(sx - textSize.x * 0.5f, sy - textSize.y * 0.5f);
        draw->AddText(ImGui::GetFont(), size, ImVec2(pos.x + 1.5f, pos.y + 1.5f), IM_COL32(0, 0, 0, (int)(alpha * 200)), n.text);
        draw->AddText(ImGui::GetFont(), size, pos,
            IM_COL32((int)(n.r * 255), (int)(n.g * 255), (int)(n.b * 255), (int)(alpha * 255)), n.text);
    }
    if (g_NumberNext >= g_NumberCount)
        g_NumberNext = 0;

    GameObject::Update();
}

void Juice::Draw()
{
    DrawParticles(false);
}

void Juice::DrawShadow()
{
    DrawParticles(true);                // debris cast small shadows (moon and lamp)
}
