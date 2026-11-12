// ProjectionShadow0VS.hlsl  影を描く用（ライトカメラからシャドウマップへ描画）
#include "common.hlsl"

void main(in VS_IN In, out PS_IN Out)
{
    Out = (PS_IN)0; // 未使用メンバーを0で初期化（未初期化の警告を防ぐ）

    matrix wvp;
    wvp = mul(World, View);
    wvp = mul(wvp, Projection);
    Out.Position = mul(In.Position, wvp); //頂点変換
}
