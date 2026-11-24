#pragma once
// Hud: the in-game heads-up display - HP, floor, mining level and XP, stats, skill
// cooldown, potions, owned cards, a 4-line message log and the key help.
// Drawn with Dear ImGui for now (no mouse input, no focus); the pixel-font UI replaces it later.

#include "GameObject.h"

class Hud : public GameObject
{
public:
    void Init() override;
    void Update() override;
};
