#pragma once
// MaterialData.h (C0): everything that can sit in the bag (Crafting Update).
// Materials drop from ore and monsters; potions, bombs and torches are crafted or found.
// Bag, pickups and crafting (C1-C3) read these rows; nothing uses them yet.

#include <cstdint>

enum class MatId : uint8_t
{
    Stone, Copper, Silver, Gold, Crystal, Gel, Fang, Core,  // materials
    Potion, Bomb, Torch,                                    // usable items (Q / E / F)
    Count
};

struct MatDef
{
    const char* name;       // English; Japanese comes from asset\text\ja.txt through Tr()
    uint8_t  stack;         // how many fit in one bag slot
    bool     usable;        // has a key (Q potion, E bomb, F torch)
    uint32_t color;         // 0xRRGGBB: pickup tint, HUD chip, minimap dot
    int      spriteIndex;   // frame in the item atlas; -1 = placeholder until the art exists
};

static const int kMatCount = (int)MatId::Count;

static const MatDef kMat[kMatCount] =
{
    { "Stone",      20, false, 0x8a8a96, -1 },
    { "Copper",     20, false, 0xe8915a, -1 },
    { "Silver",     20, false, 0xcfd6e0, -1 },
    { "Gold",       20, false, 0xffc94a, -1 },
    { "Crystal",    20, false, 0x70dbeb, -1 },
    { "Gel",        20, false, 0x9b6bd6, -1 },
    { "Fang",       20, false, 0xe8e0c8, -1 },
    { "Stone Core", 10, false, 0x7a6a5a, -1 },
    { "Potion",      5, true,  0xe04848, -1 },
    { "Bomb",        5, true,  0x505058, -1 },
    { "Torch",       5, true,  0xff9a40, -1 },
};

// Bag size: 12 slots, +4 per Bigger Bag upgrade (max 2) = 20
static const int kBagSlotsStart = 12;
static const int kBagSlotsMax = 20;

// A recipe cost: up to 3 different materials. count = how many entries are used.
struct CostItem
{
    MatId   id;
    uint8_t n;
};

struct Cost
{
    CostItem item[3];
    int      count;
};

// One line of a drop table (monsters in C2/C6, ore in C2): min..max pieces at this chance.
struct DropDef
{
    MatId   id;
    uint8_t min, max;
    float   chance;         // 0..1
};

// Ore grade (OreData.h order: copper, silver, gold, crystal) -> the metal it drops (C2)
static const MatId kOreMaterial[4] = { MatId::Copper, MatId::Silver, MatId::Gold, MatId::Crystal };
