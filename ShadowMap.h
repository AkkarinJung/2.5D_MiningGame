#pragma once
// ShadowMap (M8): one directional "moon" shadow map, 2048x2048, with 3x3 PCF.
// Each frame, before the normal drawing, Manager runs a shadow pass: every object's
// DrawShadow() renders its depth as seen from the moon into this texture. In the main
// pass shader\pointLitPS compares each pixel against it (texture t1, sampler s1, buffer b7).
// The light's box follows the player and is snapped to whole shadow-map texels, so
// shadows do not shimmer while moving. Needs main.h included first.

namespace ShadowMap
{
    struct Settings
    {
        bool  enabled = true;
        bool  pcf = true;                   // 3x3 soft edges
        float bias = 0.0015f;               // compare bias in the shader (raise if you see acne)
        float slopeBias = 1.5f;             // rasterizer slope-scaled bias in the shadow pass
        float boxHalfSize = 18.0f;          // tiles around the player covered by the map
        float moonDir[3] = { 4.0f, -10.0f, 6.0f };      // direction the moonlight travels
        float moonColor[3] = { 0.44f, 0.53f, 0.69f };
        float moonIntensity = 0.45f;
        float shadowedAmbient = 0.7f;       // ambient light kept inside shadows (0..1)
    };

    void Init();                            // after Renderer::Init
    void Uninit();

    void BeginPass(float focusX, float focusZ); // bind the depth map, set the moon's view/projection
    void EndPass();                         // restore the screen target; bind the map for the main pass

    Settings& GetSettings();
    ID3D11ShaderResourceView* GetSRV();     // for the debug-panel preview
}