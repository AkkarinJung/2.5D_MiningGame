#pragma once
// CraftData.h (C0): recipes for the craft menu (Crafting Update, C3).
// Gear: pickaxe, armor and lamp, crafted one tier at a time (the menu shows only the next tier).
// Items go into the bag; upgrades are permanent for the run. Nothing uses this yet.

#include "MaterialData.h"

enum class GearSlot : uint8_t { Pickaxe, Armor, Lamp, Count };

struct GearTier
{
    const char* name;
    Cost  cost;             // count 0 = the tier you start with
    float value;            // pickaxe: mining bonus / armor: armor points / lamp: radius bonus
    int   maxHp;            // armor only: extra max HP
};

// Pickaxe tier t mines ore grade t (copper 0, silver 1, gold 2, crystal 3); the drill only adds power.
static const int kPickTierCount = 5;
static const GearTier kPickTier[kPickTierCount] =
{
    { "Stone Pick",    { {}, 0 },                                           0.0f, 0 },
    { "Copper Pick",   { { { MatId::Copper, 4 },  { MatId::Stone, 3 } }, 2 },  2.0f, 0 },
    { "Silver Pick",   { { { MatId::Silver, 4 },  { MatId::Copper, 2 } }, 2 }, 4.0f, 0 },
    { "Gold Pick",     { { { MatId::Gold, 4 },    { MatId::Silver, 2 } }, 2 }, 7.0f, 0 },
    { "Crystal Drill", { { { MatId::Crystal, 3 }, { MatId::Gold, 2 } }, 2 },  11.0f, 0 },
};

static const int kArmorTierCount = 5;
static const GearTier kArmorTier[kArmorTierCount] =
{
    { "No Armor",      { {}, 0 },                                           0.0f, 0 },
    { "Copper Mail",   { { { MatId::Copper, 6 },  { MatId::Gel, 2 } }, 2 },    1.0f, 4 },
    { "Silver Mail",   { { { MatId::Silver, 5 },  { MatId::Gel, 3 } }, 2 },    2.0f, 8 },
    { "Gold Mail",     { { { MatId::Gold, 5 },    { MatId::Core, 1 } }, 2 },   3.0f, 13 },
    { "Crystal Mail",  { { { MatId::Crystal, 4 }, { MatId::Core, 2 } }, 2 },   5.0f, 20 },
};

// Lamp tiers replace the Bright Lamp card
static const int kLampTierCount = 4;
static const GearTier kLampTier[kLampTierCount] =
{
    { "Old Lantern",     { {}, 0 },                                         0.0f, 0 },
    { "Copper Lantern",  { { { MatId::Copper, 2 },  { MatId::Gel, 2 } }, 2 },  1.0f, 0 },
    { "Gold Lantern",    { { { MatId::Gold, 2 },    { MatId::Gel, 3 } }, 2 },  2.0f, 0 },
    { "Crystal Lantern", { { { MatId::Crystal, 2 }, { MatId::Gold, 1 } }, 2 }, 3.0f, 0 },
};

// True if a pickaxe of this tier can damage ore of this grade.
inline bool PickCanMine(int pickTier, int oreGrade) { return pickTier >= oreGrade; }

// Items: crafted into the bag (the craft is refused when the bag has no room for the result)
struct ItemRecipe
{
    MatId result;
    Cost  cost;
    const char* description;
};

static const int kItemRecipeCount = 3;
static const ItemRecipe kItemRecipe[kItemRecipeCount] =
{
    { MatId::Potion, { { { MatId::Gel, 3 } }, 1 },                        "Q: heal 40% of max HP." },
    { MatId::Bomb,   { { { MatId::Stone, 4 }, { MatId::Copper, 1 } }, 2 }, "E: throw a bomb that blasts monsters and blocks." },
    { MatId::Torch,  { { { MatId::Stone, 2 }, { MatId::Gel, 1 } }, 2 },    "F: place a torch that lights the floor." },
};

// Upgrades: permanent for the run, each with a maximum count
enum class UpgradeId : uint8_t { BiggerBag, HeartCharm, SwiftBoots, Count };

struct UpgradeDef
{
    const char* name;
    const char* description;
    Cost  cost;
    int   max;
};

static const int kUpgradeCount = (int)UpgradeId::Count;
static const UpgradeDef kUpgrade[kUpgradeCount] =
{
    { "Bigger Bag",  "+4 bag slots.",         { { { MatId::Gel, 4 },     { MatId::Fang, 2 } }, 2 }, 2 },
    { "Heart Charm", "+8 max HP and heal 8.", { { { MatId::Crystal, 1 }, { MatId::Gel, 4 } }, 2 },  3 },
    { "Swift Boots", "+10% move speed.",      { { { MatId::Silver, 3 },  { MatId::Fang, 3 } }, 2 }, 2 },
};

// Item numbers used in C3
static const float kBombThrowRange = 3.4f;      // tiles toward the aim
static const float kBombRadius = 1.5f;          // x the Big Bomb blast multiplier
static const float kBombDamageScale = 2.0f;     // x attack
static const float kBombMiningScale = 2.0f;
static const int   kTorchMaxPerFloor = 6;
static const uint32_t kTorchColor = 0xff9a40;
static const float kTorchRadius = 7.5f;
