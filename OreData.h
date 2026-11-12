#pragma once
// Gameplay code reads these rows; adding or tuning an ore never means touching if-statements.

struct OreDef
{
    const char* name;
    float r, g, b;          // block tint
    int   requiredLevel;    // mining level needed for full damage (below it: 25%)
    int   xp;               // mining XP when broken
    float baseHp;           // block HP = baseHp + floor * (2 + grade)
    int   cardWeights[3];   // common / rare / epic odds for the level-up card pick (M5)
};

static const int kOreCount = 4;

static const OreDef kOre[kOreCount] =
{
    { "Copper Ore",  0.91f, 0.57f, 0.35f, 1,  4,  8.0f,{ 72, 25,  3 } },
    { "Silver Ore",  0.81f, 0.84f, 0.88f, 2,  8, 12.0f,{ 42, 46, 12 } },
    { "Gold Ore",    1.00f, 0.79f, 0.29f, 4, 14, 17.0f,{ 16, 52, 32 } },
    { "Crystal",     0.44f, 0.86f, 0.92f, 6, 24, 22.0f,{ 3, 40, 57 } },
};

// Mining XP needed to go from this level to the next.
inline int MiningXpToNext(int level) { return 8 + 7 * (level - 1); }
