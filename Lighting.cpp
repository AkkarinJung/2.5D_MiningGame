// Lighting.cpp
// Fixed-size arrays only (no allocation per frame): up to 128 requested lights and 16
// flashes; the nearest 16 to the focus point are uploaded with one UpdateSubresource.

#include "main.h"
#include "renderer.h"
#include "Lighting.h"
#include <algorithm>
#include <cmath>

namespace
{
    const int kMaxShaderLights = 16;        // must match MAX_LIGHTS in pointLitPS.hlsl
    const int kMaxRequested = 128;
    const int kMaxFlashes = 16;

    struct Light
    {
        float x, y, z, radius;
        float r, g, b;          // colour x intensity
        float sortKey;
    };

    struct FlashLight
    {
        float x, y, z, radius;
        float r, g, b;
        float intensity;
    };

    // Matches cbuffer PointLightBuffer in pointLitPS.hlsl (560 bytes, a multiple of 16)
    struct PointLightBuffer
    {
        XMFLOAT4 posRadius[kMaxShaderLights];
        XMFLOAT4 color[kMaxShaderLights];
        XMFLOAT4 ambientCount;
        XMFLOAT4 fogColorStart;
        XMFLOAT4 fogEndEnabled;
    };

    Lighting::Settings g_Settings;
    ID3D11Buffer* g_Buffer = nullptr;

    Light g_Lights[kMaxRequested];
    int   g_LightCount = 0;
    int   g_Requested = 0;
    int   g_Used = 0;

    FlashLight g_Flashes[kMaxFlashes];
    int   g_FlashCount = 0;

    float g_FocusX = 0.0f, g_FocusZ = 0.0f;
    float g_Time = 0.0f;

    void Rgb(uint32_t rgb, float& r, float& g, float& b)
    {
        r = ((rgb >> 16) & 0xFF) / 255.0f;
        g = ((rgb >> 8) & 0xFF) / 255.0f;
        b = (rgb & 0xFF) / 255.0f;
    }
}

namespace Lighting
{
    void Init()
    {
        D3D11_BUFFER_DESC bd{};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(PointLightBuffer);
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        Renderer::GetDevice()->CreateBuffer(&bd, nullptr, &g_Buffer);
        assert(g_Buffer);

        ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
        ctx->VSSetConstantBuffers(6, 1, &g_Buffer);
        ctx->PSSetConstantBuffers(6, 1, &g_Buffer);
        Upload();                           // valid contents before the first draw
    }

    void Uninit()
    {
        if (g_Buffer) { g_Buffer->Release(); g_Buffer = nullptr; }
    }

    void BeginFrame()
    {
        g_LightCount = 0;
        g_Requested = 0;
    }

    void SetFocus(float x, float z)
    {
        g_FocusX = x;
        g_FocusZ = z;
    }

    void AddLight(float x, float y, float z, float radius, float r, float g, float b, float intensity)
    {
        g_Requested++;
        if (g_LightCount >= kMaxRequested || radius <= 0.0f || intensity <= 0.0f)
            return;
        Light& l = g_Lights[g_LightCount++];
        l.x = x; l.y = y; l.z = z; l.radius = radius;
        l.r = r * intensity; l.g = g * intensity; l.b = b * intensity;
        l.sortKey = 0.0f;
    }

    void AddLight(float x, float y, float z, float radius, uint32_t rgb, float intensity)
    {
        float r, g, b;
        Rgb(rgb, r, g, b);
        AddLight(x, y, z, radius, r, g, b, intensity);
    }

    void Flash(float x, float y, float z, float radius, uint32_t rgb, float intensity)
    {
        int slot = g_FlashCount;
        if (g_FlashCount < kMaxFlashes)
            g_FlashCount++;
        else
        {
            slot = 0;                       // full: replace the weakest flash
            for (int i = 1; i < kMaxFlashes; i++)
                if (g_Flashes[i].intensity < g_Flashes[slot].intensity)
                    slot = i;
        }
        FlashLight& f = g_Flashes[slot];
        f.x = x; f.y = y; f.z = z; f.radius = radius;
        Rgb(rgb, f.r, f.g, f.b);
        f.intensity = intensity;
    }

    float Flicker(float seed)
    {
        if (!g_Settings.flicker)
            return 1.0f;
        const float wobble = sinf(g_Time * 7.7f + seed) * 0.04f + sinf(g_Time * 21.3f + seed * 1.7f) * 0.025f;
        return 1.0f + wobble * g_Settings.flickerStrength;
    }

    void Upload()
    {
        const float dt = 1.0f / 60.0f;
        g_Time += dt;

        // Flashes fade quickly (x0.004 per second) and join this frame's lights
        const float fade = powf(0.004f, dt);
        for (int i = 0; i < g_FlashCount;)
        {
            FlashLight& f = g_Flashes[i];
            AddLight(f.x, f.y, f.z, f.radius, f.r, f.g, f.b, f.intensity);
            f.intensity *= fade;
            if (f.intensity < 0.05f)
                g_Flashes[i] = g_Flashes[--g_FlashCount];   // swap-and-pop
            else
                i++;
        }

        // Keep the 16 lights nearest the player (minus their radius, so big lights reach further)
        for (int i = 0; i < g_LightCount; i++)
        {
            const float dx = g_Lights[i].x - g_FocusX, dz = g_Lights[i].z - g_FocusZ;
            g_Lights[i].sortKey = sqrtf(dx * dx + dz * dz) - g_Lights[i].radius;
        }
        g_Used = std::min(g_LightCount, kMaxShaderLights);
        if (g_LightCount > kMaxShaderLights)
            std::nth_element(g_Lights, g_Lights + kMaxShaderLights, g_Lights + g_LightCount,
                [](const Light& a, const Light& b) { return a.sortKey < b.sortKey; });

        PointLightBuffer cb{};
        for (int i = 0; i < g_Used; i++)
        {
            const Light& l = g_Lights[i];
            cb.posRadius[i] = XMFLOAT4(l.x, l.y, l.z, l.radius);
            cb.color[i] = XMFLOAT4(l.r, l.g, l.b, 1.0f);
        }
        const float* clear = Renderer::GetClearColor();
        cb.ambientCount = XMFLOAT4(g_Settings.ambient[0], g_Settings.ambient[1], g_Settings.ambient[2], (float)g_Used);
        cb.fogColorStart = XMFLOAT4(clear[0], clear[1], clear[2], g_Settings.fog ? g_Settings.fogStart : 1000.0f);
        cb.fogEndEnabled = XMFLOAT4(g_Settings.fog ? g_Settings.fogEnd : 2000.0f, g_Settings.enabled ? 1.0f : 0.0f, 0.0f, 0.0f);

        if (g_Buffer)
            Renderer::GetDeviceContext()->UpdateSubresource(g_Buffer, 0, nullptr, &cb, 0, 0);
    }

    Settings& GetSettings() { return g_Settings; }
    int LightsRequested() { return g_Requested; }
    int LightsUsed() { return g_Used; }
}
