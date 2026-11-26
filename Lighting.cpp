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
        XMFLOAT4 fogOfWar;                  // x = on, y = 1 / map width, z = 1 / map height
    };

    Lighting::Settings g_Settings;
    ID3D11Buffer* g_Buffer = nullptr;

    Light g_Lights[kMaxRequested];
    int   g_LightCount = 0;
    int   g_Requested = 0;
    int   g_Used = 0;

    FlashLight g_Flashes[kMaxFlashes];
    int   g_FlashCount = 0;

    int   g_FogMapW = 0, g_FogMapH = 0;     // 0 = no fog-of-war texture this frame

    Light g_Lamp{};
    bool  g_HasLamp = false;

    bool  g_Town = false;                   // floor 0: daylight preset (C8)
    float g_DungeonClear[3] = { 0.04f, 0.03f, 0.03f };   // clear colour to restore when leaving town

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
        g_HasLamp = false;
        g_FogMapW = g_FogMapH = 0;
    }

    void SetFogMap(int width, int height)
    {
        g_FogMapW = width;
        g_FogMapH = height;
    }

    void SetLamp(float x, float y, float z, float radius, uint32_t rgb, float intensity)
    {
        float r, g, b;
        Rgb(rgb, r, g, b);
        g_Lamp.x = x; g_Lamp.y = y; g_Lamp.z = z; g_Lamp.radius = radius;
        g_Lamp.r = r * intensity; g_Lamp.g = g * intensity; g_Lamp.b = b * intensity;
        g_HasLamp = radius > 0.0f;
        g_Requested++;
    }

    bool GetLamp(float& x, float& y, float& z, float& radius)
    {
        if (!g_HasLamp)
            return false;
        x = g_Lamp.x; y = g_Lamp.y; z = g_Lamp.z; radius = g_Lamp.radius;
        return true;
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
        // The lamp takes slot 0 (the shader knows light 0 is the shadow-casting lamp)
        const int lampSlots = g_HasLamp ? 1 : 0;
        const int others = std::min(g_LightCount, kMaxShaderLights - lampSlots);
        g_Used = others + lampSlots;
        if (g_LightCount > others)
            std::nth_element(g_Lights, g_Lights + others, g_Lights + g_LightCount,
                [](const Light& a, const Light& b) { return a.sortKey < b.sortKey; });

        PointLightBuffer cb{};
        for (int i = 0; i < g_Used; i++)
        {
            const Light& l = (g_HasLamp && i == 0) ? g_Lamp : g_Lights[i - lampSlots];
            cb.posRadius[i] = XMFLOAT4(l.x, l.y, l.z, l.radius);
            cb.color[i] = XMFLOAT4(l.r, l.g, l.b, 1.0f);
        }
        float* clear = Renderer::GetClearColor();
        const float* ambient = g_Settings.ambient;
        float fogStart = g_Settings.fogStart, fogEnd = g_Settings.fogEnd;
        if (g_Town)
        {
            ambient = g_Settings.townAmbient;
            fogStart = g_Settings.townFogStart;
            fogEnd = g_Settings.townFogEnd;
            for (int i = 0; i < 3; i++)
                clear[i] = g_Settings.townFogColor[i];      // sky = fog colour (live from the F1 panel)
        }
        cb.ambientCount = XMFLOAT4(ambient[0], ambient[1], ambient[2], (float)g_Used);
        cb.fogColorStart = XMFLOAT4(clear[0], clear[1], clear[2], g_Settings.fog ? fogStart : 1000.0f);
        cb.fogEndEnabled = XMFLOAT4(g_Settings.fog ? fogEnd : 2000.0f, g_Settings.enabled ? 1.0f : 0.0f, 0.0f, 0.0f);
        // The town is fully revealed: no fog of war on floor 0
        const bool fogOfWar = g_Settings.fogOfWar && !g_Town && g_FogMapW > 0 && g_FogMapH > 0;
        cb.fogOfWar = XMFLOAT4(fogOfWar ? 1.0f : 0.0f, fogOfWar ? 1.0f / g_FogMapW : 0.0f,
                               fogOfWar ? 1.0f / g_FogMapH : 0.0f, 0.0f);

        if (g_Buffer)
            Renderer::GetDeviceContext()->UpdateSubresource(g_Buffer, 0, nullptr, &cb, 0, 0);
    }

    void ApplyFloorPreset(int floor)
    {
        const bool town = floor <= 0;
        if (town == g_Town)
            return;
        float* clear = Renderer::GetClearColor();
        if (town)
            for (int i = 0; i < 3; i++)
                g_DungeonClear[i] = clear[i];               // remember the cave colour (F1 may have changed it)
        else
            for (int i = 0; i < 3; i++)
                clear[i] = g_DungeonClear[i];
        g_Town = town;
    }

    bool IsTown() { return g_Town; }

    Settings& GetSettings() { return g_Settings; }
    float FocusX() { return g_FocusX; }
    float FocusZ() { return g_FocusZ; }
    int LightsRequested() { return g_Requested; }
    int LightsUsed() { return g_Used; }
}
