// ProjectionShadow0PS.hlsl  影を描く用（シャドウマップに影の色を書き込む）
#include "common.hlsl"

Texture2D    g_Texture      : register(t0); //テクスチャ0番
SamplerState g_SamplerState : register(s0); //サンプラー0番

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    outDiffuse = float4(0.5f, 0.5f, 0.5f, 1.0f); //影の輝度値
}
