#pragma once
// MonsterData.h: monster types as a data table (stats and 8x8 pixel-art sprites
// from the Deepvein prototype). Behaviour lives in Monsters.cpp; numbers live here.

#include <cstdint>

enum class MonsterType : uint8_t { Slime, Bat, Goblin, Count };

struct PaletteEntry
{
    char     key;
    uint32_t rgb;       // 0xRRGGBB
};

struct MonsterDef
{
    const char* name;
    float hp;           // at floor 1; x (1 + 0.22 per floor)
    int   attack;       // at floor 1; +1 every 2 floors
    float speed;        // tiles per second
    float sight;        // wakes when the player is this close and in line of sight
    float radius;       // collision half-width / circle radius
    float height;       // sprite height in tiles
    const char* rows[8];        // '.' = transparent, other letters look up the palette
    PaletteEntry palette[4];
};

static const int kMonsterTypeCount = (int)MonsterType::Count;

static const MonsterDef kMonster[kMonsterTypeCount] =
{
    { "Cave Slime", 8.0f, 2, 1.0f, 6.0f, 0.32f, 0.80f,
      { "........", "........", "..pppp..", ".pwpppp.", ".peppep.", "pppppppp", "pppppppp", ".pppppp." },
      { { 'p', 0x9a6be0 },{ 'w', 0xd6c2ff },{ 'e', 0x1a1024 },{ 0, 0 } } },

    { "Gloom Bat", 4.0f, 1, 3.3f, 8.0f, 0.25f, 0.75f,
      { "........", "........", "r.r..r.r", "rrrrrrrr", "rryrryrr", ".rrrrrr.", "r..rr..r", "........" },
      { { 'r', 0xc25a5a },{ 'y', 0xffd166 },{ 0, 0 },{ 0, 0 } } },

    { "Goblin", 12.0f, 3, 2.7f, 9.0f, 0.30f, 1.00f,
      { "..g..g..", "..gggg..", ".grggrg.", "..gggg..", ".tttttt.", "g.tttt.g", "..tttt..", "..t..t.." },
      { { 'g', 0x7d8c3a },{ 'r', 0xff5a4a },{ 't', 0x6a4a2e },{ 0, 0 } } },
};