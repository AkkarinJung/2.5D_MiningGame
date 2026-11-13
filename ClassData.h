#pragma once
// ClassData.h: the four playable classes as a data table (values from the Deepvein prototype) plus the class chosen for the current run.

#include <cstdint>

enum class PlayerClass : uint8_t { Warrior, Miner, Mage, Rogue, Count };

struct ClassDef
{
    const char* name;
    const char* description;
    int   hp;
    float speed;            // tiles per second
    float attack;
    float mining;           // starting mining power
    float cooldown;         // seconds between attacks
    float range;            // melee reach in tiles (Mage: bolts, added in the next step)
    float arc;              // melee swing width in radians
    bool  ranged;
    const char* skillName;
    const char* skillDescription;
    float skillCooldown;    // seconds (skills arrive in the next step)
    const char* trait;
    float invincibility;    // seconds after being hit
    float crit;             // starting crit chance (0..1)
    int   cardChoices;      // cards offered per level-up (cards arrive in step 3)
    float miningXpScale;
};

static const int kClassCount = (int)PlayerClass::Count;

static const ClassDef kClass[kClassCount] =
{
    { "Warrior", "Sweeps a wide arc with a sword. Tough enough to fight head-on.",
      30, 4.2f, 4.0f, 2.0f, 0.45f, 1.40f, 1.9f, false,
      "Whirlwind", "Spin and slash everything around you, blocks included.", 4.0f,
      "Longer invincibility after a hit", 1.0f, 0.0f, 3, 1.0f },

    { "Miner", "A pickaxe pro. Blocks crumble in a couple of swings.",
      24, 4.0f, 3.0f, 5.0f, 0.40f, 1.15f, 1.3f, false,
      "Dynamite", "Throw a bomb that blasts monsters and blocks alike.", 5.0f,
      "4 card choices, 1.5x mining XP", 0.6f, 0.0f, 4, 1.5f },

    { "Mage", "Fires magic bolts from range. Fragile, but rarely in reach.",
      16, 4.2f, 4.0f, 2.0f, 0.50f, 1.15f, 1.3f, true,
      "Frost Nova", "Damage nearby monsters and freeze them to a crawl.", 6.0f,
      "Ranged attacks", 0.6f, 0.0f, 3, 1.0f },

    { "Rogue", "Rapid dagger strikes. Fast on foot and crits often.",
      18, 5.6f, 2.0f, 2.0f, 0.20f, 1.05f, 1.4f, false,
      "Shadow Dash", "Dash while invincible, cutting everything in your path.", 2.5f,
      "+15% crit chance", 0.6f, 0.15f, 3, 1.0f },
};

// The class picked on the class select screen; read by Player::Init.
inline PlayerClass& SelectedClass()
{
    static PlayerClass selected = PlayerClass::Miner;
    return selected;
}