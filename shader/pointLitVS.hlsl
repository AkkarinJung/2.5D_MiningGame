// pointLitVS: like unlitTextureVS, but also passes the world position and normal
// to the pixel shader so it can light each pixel with point lights (M7).
#include "common.hlsl"

void main(in VS_IN In, out PS_IN Out)
{
    matrix wvp = mul(World, View);
    wvp = mul(wvp, Projection);

    Out.Position = mul(In.Position, wvp);
    Out.WorldPosition = mul(In.Position, World);

    float4 normal = float4(In.Normal.xyz, 0.0f);
    Out.Normal = normalize(mul(normal, World));

    Out.TexCoord = In.TexCoord;
    Out.Diffuse = In.Diffuse * Material.Diffuse;
}
