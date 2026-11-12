
#include "common.hlsl"


Texture2D g_Texture : register(t0);
SamplerState g_SamplerState : register(s0);

static float PI = 3.141592653589;

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float3 eyev = In.WorldPosition.xyz - CameraPosition.xyz;
    eyev = normalize(eyev);
	
    float3 normal = normalize(In.Normal.xyz);
    
// 反射ベクトル
    float3 refv = reflect(eyev, normal);

// 屈折ベクトル
    float3 refracv = refract(eyev, normal, 0.5);

// 環境マッピングテクスチャ座標（パノラマ）
    float2 envTexCoord;
    envTexCoord.x = -atan2(refv.x, refv.z) / (PI * 2) - 0.25f;
    envTexCoord.y = acos(refv.y) / PI;

// 屈折マッピングテクスチャ座標（パノラマ）
    float2 refTexCoord;
    refTexCoord.x = -atan2(refracv.x, refracv.z) / (PI * 2) - 0.25f;
    refTexCoord.y = acos(refracv.y) / PI;

// フレネル反射率
    float d = dot(normal, refv);
    float f0 = 0.01;
    float f = f0 + (1.0 - f0) * pow(1.0 - d, 3.0);

    float3 envColor = g_Texture.SampleLevel(g_SamplerState, envTexCoord, 4);
    float3 refColor = g_Texture.SampleLevel(g_SamplerState, refTexCoord, 4);
    
    
    
    outDiffuse.rgb = lerp(In.Diffuse.rgb, envColor, f);
    outDiffuse.a = 1.0;
}

