// pointLitPS: texture x vertex colour, lit by ambient + up to 16 point lights with a
// smooth distance falloff, plus emissive glow and linear distance fog (M7).
// Material.Emission.rgb = glow added after lighting (crystals, hit flash).
// Material.Emission.w   = 1 for camera-facing sprites: they use an upward normal,
//                         so they pick up the lamp like the floor around them.
#include "common.hlsl"

#define MAX_LIGHTS 16

cbuffer PointLightBuffer : register(b6)
{
    float4 LightPosRadius[MAX_LIGHTS];  // xyz = position, w = radius
    float4 LightColor[MAX_LIGHTS];      // rgb = colour x intensity
    float4 AmbientCount;                // rgb = ambient, w = number of lights
    float4 FogColorStart;               // rgb = fog colour, w = fog start distance
    float4 FogEndEnabled;               // x = fog end distance, y = lighting on (1) / off (0)
}

Texture2D    g_Texture      : register(t0);
SamplerState g_SamplerState : register(s0);

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float4 base = In.Diffuse;
    if (Material.TextureEnable)
        base *= g_Texture.Sample(g_SamplerState, In.TexCoord);

    if (FogEndEnabled.y < 0.5f)
    {
        outDiffuse = base;              // lighting switched off in the debug panel
        return;
    }

    const bool sprite = Material.Emission.w > 0.5f;
    const float3 N = sprite ? float3(0.0f, 1.0f, 0.0f) : normalize(In.Normal.xyz);
    const float3 P = In.WorldPosition.xyz;

    float3 light = AmbientCount.rgb;
    const int count = (int)AmbientCount.w;
    [loop]
    for (int i = 0; i < count; i++)
    {
        const float3 toLight = LightPosRadius[i].xyz - P;
        const float dist = length(toLight);
        float falloff = saturate(1.0f - dist / LightPosRadius[i].w);
        falloff *= falloff;                                     // smooth edge
        float ndl = saturate(dot(N, toLight / max(dist, 0.001f)));
        if (sprite)
            ndl = 0.5f + 0.5f * ndl;                            // flat sprites: softer
        light += LightColor[i].rgb * falloff * ndl;
    }

    float3 color = base.rgb * light + Material.Emission.rgb;

    const float d = length(P - CameraPosition.xyz);
    const float fog = saturate((d - FogColorStart.w) / max(FogEndEnabled.x - FogColorStart.w, 0.001f));
    outDiffuse = float4(lerp(color, FogColorStart.rgb, fog), base.a);
}
