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

// The craft menu (C key, Crafting Update C3) is open. Nothing opens it yet.
inline bool& CraftMenuOpen()
{
    static bool open = false;
    return open;
}

inline bool GamePaused()
{
    return CardPickOpen() || PromptOpen() || CraftMenuOpen() || HitStopTimer() > 0.0f;
}

// Counts for the current run, shown on the death and restart prompts (Crafting Update C4).
// Reset when a run starts. Nothing fills them yet.
struct RunStats
{
    int kills = 0;
    int itemsCrafted = 0;
    int weaponsCrafted = 0;
};

inline RunStats& CurrentRun()
{
    static RunStats stats;
    return stats;
}
