#pragma once
// DebugUI: the F1 Dear ImGui debug panel plus an on-screen message log.
// This is the only GUI code allowed to change game state; game code reports
// events through DebugUI::Log instead of OutputDebugString.

class DebugUI
{
public:
    static void Init();             // after Renderer::Init (needs the window and device)
    static void Uninit();           // before Renderer::Uninit
    static void BeginFrame();       // start of Manager::Update: game UI may use ImGui from here on
    static void Draw();             // after the scene is drawn, before Present

    static void Log(const char* format, ...);   // printf-style, also goes to VS Output
    static bool WantsKeyboard();    // true while typing in / dragging a debug widget
};

