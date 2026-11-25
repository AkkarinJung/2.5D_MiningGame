#pragma once
// GameState.h: run-wide flags shared by the game systems.

// A level-up card pick is open (set by Cards).
inline bool& CardPickOpen()
{
    static bool open = false;
    return open;
}

// The stairs, restart or death prompt is open (set by Prompts).
inline bool& PromptOpen()
{
    static bool open = false;
    return open;
}

// Player, Monsters and Projectiles skip their update while this is true; drawing continues.
// Hit-stop (juice): seconds left of a short freeze on big hits (set by Juice)
inline float& HitStopTimer()
{
    static float seconds = 0.0f;
    return seconds;
}

inline bool GamePaused()
{
    return CardPickOpen() || PromptOpen() || HitStopTimer() > 0.0f;
}

// Deepest floor reached this session (saved to disk in a later step).
inline int& BestFloor()
{
    static int best = 1;
    return best;
}
