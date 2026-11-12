
#include "common.hlsl"


Texture2D		g_Texture : register(t0);
SamplerState	g_SamplerState : register(s0);

static float PI = 3.141592653589;

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    float3 eyev = In.WorldPosition.xyz - CameraPosition.xyz;
    eyev = normalize(eyev);
	
    float3 normal = normalize(In.Normal.xyz);
    float3 refv = reflect(eyev, normal);
    float3 refracv = refract(eyev, normal,0.5);
	
	//環境マッピングテクスチャ座標（パノラマ）
    float2 envTexCoord;
    envTexCoord.x = -atan2(refv.x, refv.z) / (PI * 2) - 0.25f;
    envTexCoord.y = acos(refv.y) / PI;
	
    float d = dot(normal, refv);
    float f0 = 0.1f;
    float f = f0 + (1.0f - f0) * pow(1.0 - d, 25.0);
    outDiffuse = g_Texture.SampleLevel(g_SamplerState, envTexCoord, 4);
	outDiffuse.a = f;
}
