// pointLitPS: texture x vertex colour, lit by ambient + up to 16 point lights with a
// smooth distance falloff (M7), a shadow-casting moon light with 3x3 PCF (M8),
// a shadow-casting lamp (light 0, cube map), plus emissive glow and linear distance fog.
// Material.Emission.rgb = glow added after lighting (crystals, hit flash).
// Material.Emission.w   = 1 for camera-facing sprites: they use an upward normal,
//                         so they pick up the lamp like the floor around them.
#include "common.hlsl"

#define MAX_LIGHTS 16

cbuffer PointLightBuffer : register(b6)
{
    float4 LightPosRadius[MAX_LIGHTS]; // xyz = position, w = radius
    float4 LightColor[MAX_LIGHTS]; // rgb = colour x intensity
    float4 AmbientCount; // rgb = ambient, w = number of lights
    float4 FogColorStart; // rgb = fog colour, w = fog start distance
    float4 FogEndEnabled; // x = fog end distance, y = lighting on (1) / off (0)
}

cbuffer ShadowBuffer : register(b7)
{
    matrix LightViewProj; // world -> moon clip space
    float4 MoonDirIntensity; // xyz = direction the light travels, w = intensity
    float4 MoonColorShadowAmbient; // rgb = moon colour, w = ambient kept inside shadows
    float4 ShadowParams; // x = texel size, y = bias, z = PCF on, w = shadows on
    float4 LampPosFar; // xyz = lamp position, w = far plane (lamp radius)
    float4 LampParams; // x = lamp shadows on, y = bias, z = 2 / face size, w = near plane
}

Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);
Texture2D<float> g_ShadowMap : register(t1);
SamplerComparisonState g_ShadowSampler : register(s1);
TextureCube<float> g_LampShadow : register(t2); // the lamp's 6-face shadow cube

// 1 = lit by the lamp, 0 = a wall or block is between this pixel and the lamp.
// Each cube face stored normal depth from a 90-degree camera at the lamp. The face this
// pixel falls on is the largest axis of (pixel - lamp), and that axis is its distance along
// that face's view, so it converts to the same depth value the face's projection wrote.
float LampShadow(float3 P)
{
    if (LampParams.x < 0.5f)
        return 1.0f;
    const float3 d = P - LampPosFar.xyz;
    const float3 a = abs(d);
    const float z = max(a.x, max(a.y, a.z));
    const float n = LampParams.w, f = LampPosFar.w;
    if (z >= f || z <= n)
        return 1.0f;
    const float zb = z * (1.0f - LampParams.y); // bias: pull slightly toward the lamp
    const float depth = f / (f - n) - (f * n / (f - n)) / zb;

    // 8 taps around the direction (about one texel apart at this distance) for soft edges
    const float o = LampParams.z * z;
    float sum = 0.0f;
    [unroll]
    for (int i = 0; i < 8; i++)
    {
        const float3 offset = float3((i & 1) ? o : -o, (i & 2) ? o : -o, (i & 4) ? o : -o);
        sum += g_LampShadow.SampleCmpLevelZero(g_ShadowSampler, d + offset, depth);
    }
    return sum / 8.0f;
}

// 1 = fully lit by the moon, 0 = fully in shadow
float ShadowFactor(float3 P)
{
    if (ShadowParams.w < 0.5f)
        return 1.0f;
    const float4 lp = mul(float4(P, 1.0f), LightViewProj);
    const float3 ndc = lp.xyz / lp.w;
    const float2 uv = float2(ndc.x * 0.5f + 0.5f, -ndc.y * 0.5f + 0.5f);
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f || ndc.z > 1.0f)
        return 1.0f; // outside the light's box: lit
    const float depth = ndc.z - ShadowParams.y;
    if (ShadowParams.z < 0.5f)
        return g_ShadowMap.SampleCmpLevelZero(g_ShadowSampler, uv, depth);

    float sum = 0.0f; // 3x3 PCF: soft edges
    [unroll]
    for (int y = -1; y <= 1; y++)
        [unroll]
        for (int x = -1; x <= 1; x++)
            sum += g_ShadowMap.SampleCmpLevelZero(g_ShadowSampler, uv + float2(x, y) * ShadowParams.x, depth);
    return sum / 9.0f;
}

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float4 base = In.Diffuse;
    if (Material.TextureEnable)
        base *= g_Texture.Sample(g_SamplerState, In.TexCoord);

    if (FogEndEnabled.y < 0.5f)
    {
        outDiffuse = base; // lighting switched off in the debug panel
        return;
    }

    const bool sprite = Material.Emission.w > 0.5f;
    const float3 N = sprite ? float3(0.0f, 1.0f, 0.0f) : normalize(In.Normal.xyz);
    const float3 P = In.WorldPosition.xyz;

    const float shadow = ShadowFactor(P);
    const float lampShadow = LampShadow(P); // only light 0 (the lamp) uses it
    float3 light = AmbientCount.rgb * lerp(MoonColorShadowAmbient.w, 1.0f, shadow);

    // Moon (M8): directional light, blocked by the shadow map
    float moonNdl = saturate(dot(N, -MoonDirIntensity.xyz));
    if (sprite)
        moonNdl = 0.6f;
    light += MoonColorShadowAmbient.rgb * MoonDirIntensity.w * moonNdl * shadow;
    const int count = (int) AmbientCount.w;
    [loop]
    for (int i = 0; i < count; i++)
    {
        const float3 toLight = LightPosRadius[i].xyz - P;
        const float dist = length(toLight);
        float falloff = saturate(1.0f - dist / LightPosRadius[i].w);
        falloff *= falloff; // smooth edge
        float ndl = saturate(dot(N, toLight / max(dist, 0.001f)));
        if (sprite)
            ndl = 0.5f + 0.5f * ndl; // flat sprites: softer
        const float blocked = (i == 0) ? lampShadow : 1.0f; // light 0 is the lamp when lamp shadows are on
        light += LightColor[i].rgb * falloff * ndl * blocked;
    }

    float3 color = base.rgb * light + Material.Emission.rgb;

    const float d = length(P - CameraPosition.xyz);
    const float fog = saturate((d - FogColorStart.w) / max(FogEndEnabled.x - FogColorStart.w, 0.001f));
    outDiffuse = float4(lerp(color, FogColorStart.rgb, fog), base.a);
}
