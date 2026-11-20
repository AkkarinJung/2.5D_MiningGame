#pragma once
// CardData.h: every level-up card from the Deepvein prototype as a data table.
// The effect of each card is one case in Player::ApplyCard; the numbers and text live here.
// (Bright Lamp is left out until lighting exists in M7.)

#include "ClassData.h"
#include <cstdint>

enum class CardId : uint8_t
{
    HeavyArm, SharpPick, ToughHide, FleetFoot, QuickHands, PotionPouch, SecondWind,
    IronSkin, Leech, KeenEdge, Thorns, BroadBlade, Focus, Prospector, BigBomb,
    TwinStrike, Aftershock, Executioner, Motherlode, SwordWave, ChainBlast, SplitBolt, VenomBlade,
    Count
};

enum class Rarity : uint8_t { Common, Rare, Epic };

struct CardDef
{
    CardId id;
    const char* name;
    const char* description;
    Rarity rarity;
    int  maxStacks;         // 0 = unlimited
    bool once;              // applied but not kept in the deck (Second Wind)
    int  onlyFor;           // -1 = every class, otherwise (int)PlayerClass
};

static const int kCardCount = (int)CardId::Count;

static const CardDef kCard[kCardCount] =
{
    { CardId::HeavyArm,    "Heavy Arm",     "+1 attack.",                                             Rarity::Common, 0, false, -1 },
    { CardId::SharpPick,   "Sharp Pick",    "+2 mining. Blocks break faster.",                        Rarity::Common, 0, false, -1 },
    { CardId::ToughHide,   "Tough Hide",    "+6 max HP and heal 6.",                                  Rarity::Common, 0, false, -1 },
    { CardId::FleetFoot,   "Fleet Foot",    "+12% move speed.",                                       Rarity::Common, 3, false, -1 },
    { CardId::QuickHands,  "Quick Hands",   "+15% attack speed.",                                     Rarity::Common, 3, false, -1 },
    { CardId::PotionPouch, "Potion Pouch",  "+2 potions. Each heals 40% of max HP.",                  Rarity::Common, 0, false, -1 },
    { CardId::SecondWind,  "Second Wind",   "Heal to full right now.",                                Rarity::Common, 0, true,  -1 },

    { CardId::IronSkin,    "Iron Skin",     "+1 armor. Every hit you take is 1 lower.",               Rarity::Rare,   4, false, -1 },
    { CardId::Leech,       "Leech",         "Heal 2 HP whenever you kill a monster.",                 Rarity::Rare,   0, false, -1 },
    { CardId::KeenEdge,    "Keen Edge",     "+15% crit chance. Crits deal double damage.",            Rarity::Rare,   3, false, -1 },
    { CardId::Thorns,      "Thorns",        "Monsters that touch you take 4 damage.",                 Rarity::Rare,   0, false, -1 },
    { CardId::BroadBlade,  "Broad Blade",   "+20% attack reach.",                                     Rarity::Rare,   3, false, -1 },
    { CardId::Focus,       "Focus",         "-20% skill cooldown.",                                   Rarity::Rare,   3, false, -1 },
    { CardId::Prospector,  "Prospector",    "Level-up cards lean rarer.",                             Rarity::Rare,   3, false, -1 },
    { CardId::BigBomb,     "Big Bomb",      "+40% dynamite blast radius.",                            Rarity::Rare,   2, false, (int)PlayerClass::Miner },

    { CardId::TwinStrike,  "Twin Strike",   "30% chance for each attack to fire twice.",              Rarity::Epic,   2, false, -1 },
    { CardId::Aftershock,  "Aftershock",    "Breaking a block deals 8 damage to monsters within 2 tiles.", Rarity::Epic, 0, false, -1 },
    { CardId::Executioner, "Executioner",   "Instantly finish monsters below 25% HP.",                Rarity::Epic,   1, false, -1 },
    { CardId::Motherlode,  "Motherlode",    "From the next floor on, gold ore and crystal appear far more often.", Rarity::Epic, 1, false, -1 },
    { CardId::SwordWave,   "Sword Wave",    "Every swing launches a piercing slash wave.",            Rarity::Epic,   1, false, (int)PlayerClass::Warrior },
    { CardId::ChainBlast,  "Chain Blast",   "Broken blocks explode and set off the blocks around them.", Rarity::Epic, 1, false, (int)PlayerClass::Miner },
    { CardId::SplitBolt,   "Split Bolt",    "Magic bolts fire in 3 directions.",                      Rarity::Epic,   1, false, (int)PlayerClass::Mage },
    { CardId::VenomBlade,  "Venom Blade",   "Attacks poison, dealing damage over 3 seconds.",         Rarity::Epic,   1, false, (int)PlayerClass::Rogue },
};

static const char* const kRarityName[3] = { "Common", "Rare", "Epic" };
static const float kRarityColor[3][3] = { { 0.61f, 0.55f, 0.48f },{ 0.44f, 0.66f, 0.88f },{ 0.84f, 0.55f, 0.94f } };