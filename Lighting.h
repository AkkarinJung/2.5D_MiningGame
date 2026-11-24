#pragma once
// Lighting (M7): point lights for the dungeon.
// Each frame game code adds lights during Update (the player's lamp, crystals, stairs)
// and short flashes (block breaks, explosions). At the start of drawing, the 16 lights
// nearest the player go into one constant buffer (b6) that shader\pointLitPS reads.
// Needs main.h included first.

#include <cstdint>

namespace Lighting
{
    struct Settings
    {
        bool  enabled = true;
        float ambient[3] = { 0.17f, 0.15f, 0.21f };    // cool purple dark, from the prototype
        float lampRadius = 7.0f;
        float lampIntensity = 2.2f;
        bool  flicker = true;                          // juice: on/off + strength
        float flickerStrength = 1.0f;
        bool  fog = true;
        float fogStart = 11.0f;                        // distance from the camera
        float fogEnd = 21.0f;
    };

    void Init();                    // after Renderer::Init
    void Uninit();

    void BeginFrame();              // start of Manager::Update: forget last frame's lights
    void SetFocus(float x, float z);// the player: nearest lights win when there are more than 16
    void AddLight(float x, float y, float z, float radius, float r, float g, float b, float intensity);
    void AddLight(float x, float y, float z, float radius, uint32_t rgb, float intensity);
    // A light that fades out on its own over about half a second.
    void Flash(float x, float y, float z, float radius, uint32_t rgb, float intensity);

    void Upload();                  // start of Manager::Draw: pick the 16 nearest, fill the buffer

    float Flicker(float seed);      // ~1.0 with a slight wobble (1.0 when flicker is off)
    Settings& GetSettings();
    int LightsRequested();          // lights added this frame (debug panel)
    int LightsUsed();               // lights sent to the shader
}
