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
inline bool GamePaused()
{
    return CardPickOpen() || PromptOpen();
}
