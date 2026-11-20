#pragma once
// GameState.h: run-wide flags shared by the game systems.

// True while a card pick (later also the stairs / restart prompts) is open.
// Player, Monsters and Projectiles skip their update while it is set; drawing continues.
inline bool& GamePaused()
{
    static bool paused = false;
    return paused;
}