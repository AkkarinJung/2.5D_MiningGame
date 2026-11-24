// shadowAlphaPS (M8): used only in the shadow pass for sprites. Writes no colour; it just
// throws away transparent pixels so a sprite's silhouette casts a shadow, not its whole quad.
#include "common.hlsl"

Texture2D    g_Texture      : register(t0);
SamplerState g_SamplerState : register(s0);

void main(in PS_IN In)
{
    clip(g_Texture.Sample(g_SamplerState, In.TexCoord).a - 0.5f);
}
