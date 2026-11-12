// ProjectionShadow1PS.hlsl  影を受ける側（ブリンフォン + 投影シャドウ）
#include "common.hlsl"

Texture2D    g_Texture       : register(t0); //通常のテクスチャ
Texture2D    g_Texture1      : register(t1); //シャドウマップ
SamplerState g_SamplerState  : register(s0);
SamplerState g_SamplerState1 : register(s1); //シャドウマップ用（BORDER設定）

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    //物体から光源へのベクトル
    float4 lv = In.WorldPosition - Light.Position;
    //物体と光源の距離
    float ld = length(lv);
    //ベクトルの正規化
    lv = normalize(lv);

    //減衰の計算
    float ofs = 1.0f - (1.0f / Light.PointLightparam.x) * ld;
    //減衰率0未満は0にする。
    ofs = max(0, ofs);

    //ピクセルの法線を正規化
    float4 normal = normalize(In.Normal);
    //光源計算（ハーフランバート版）
    float light = 0.5f - 0.5f * dot(normal.xyz, lv.xyz);
    light *= ofs; //明るさを減衰

    //テクスチャのピクセル色を取得
    outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord);
    outDiffuse.rgb *= In.Diffuse.rgb * light; //明るさを乗算
    outDiffuse.a   *= In.Diffuse.a;           //α値に明るさは関係ない

    //カメラからピクセルへ向かうベクトル
    float3 eyev = In.WorldPosition.xyz - CameraPosition.xyz;
    eyev = normalize(eyev);

    //ハーフベクトルを計算
    float3 halfv = eyev + lv.xyz; //視線ベクトル＋ライトベクトル
    halfv = normalize(halfv);

    //スペキュラーの計算
    float specular = -dot(halfv, normal.xyz); //ハーフベクトルと法線の内積
    specular = saturate(specular);
    specular = pow(specular, 30);
    outDiffuse.rgb += (specular * ofs);
    //ここまでは普通の光源計算

    //投影シャドウの処理
    float2 uv = In.LightPosition.xy / In.LightPosition.w; //デバイス座標(-1.0～1.0)へ変換
    uv *= float2(0.5f, -0.5f);                             //テクスチャ座標(0.0～1.0)へ変換
    uv += 0.5f;
    //シャドウマップの色を合成
    outDiffuse.rgb *= g_Texture1.Sample(g_SamplerState1, uv).rgb;
}
