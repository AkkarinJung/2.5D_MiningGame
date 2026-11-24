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
        float ambient[3] = { 0.025f, 0.02f, 0.035f };  // almost black: only lights reveal the cave
        float lampRadius = 3.0f;
        float lampIntensity = 2.6f;
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
    // The player's lamp: always light 0 in the shader, and the one light that casts shadows.
    void SetLamp(float x, float y, float z, float radius, uint32_t rgb, float intensity);
    bool GetLamp(float& x, float& y, float& z, float& radius);  // false if no lamp this frame
    // A light that fades out on its own over about half a second.
    void Flash(float x, float y, float z, float radius, uint32_t rgb, float intensity);

    void Upload();                  // start of Manager::Draw: pick the 16 nearest, fill the buffer
    float FocusX();                 // where SetFocus put the player (the shadow box follows it)
    float FocusZ();

    float Flicker(float seed);      // ~1.0 with a slight wobble (1.0 when flicker is off)
    Settings& GetSettings();
    int LightsRequested();          // lights added this frame (debug panel)
    int LightsUsed();               // lights sent to the shader
}
