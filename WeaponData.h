#pragma once
// WeaponData.h (C0): the 15 weapons and their weapon skills (Crafting Update, C5 + C7).
// The weapon decides attack shape, reach, speed, mining strength, element and the Z skill.
// The class still decides HP, speed, class skill and trait. Each class can only craft and
// hold its own class weapon plus weapons of its two families. Nothing uses this yet.

#include "MaterialData.h"
#include "ElementData.h"
#include <cstdint>

enum class WFam : uint8_t { Class, Blade, Pole, Shot, Knife, Heavy, Magic, Count };

enum class WeaponId : uint8_t
{
    WarriorSword, MinerMattock, MageStaff, RogueDaggers,       // class starters
    EmberSword, CrystalBlade, QuarryAxe, ThunderHammer, BarbedSpear,
    RockbreakerMaul, FireStaff, FrostWand, SparkCrossbow, VenomBlowgun, FrostfangDaggers,
    Count
};

struct WeaponDef
{
    const char* name;
    int   classLock;        // class index (ClassData.h order) for class weapons, -1 otherwise
    WFam  fam;
    Elem  elem;
    bool  bolt;             // true = fires bolts, false = melee arc
    float cooldown;         // seconds between attacks (divided by attack speed)
    float range;            // melee reach in tiles (0 for bolt weapons)
    float arc;              // melee swing width in radians (0 for bolt weapons)
    int   atk;              // added to the base attack
    float mine;             // mining multiplier
    float boltSpeed;        // tiles per second (0 for melee); range = 12 / speed seconds
    int   pierce;           // monsters a bolt passes through
    float burst;            // bolt bursts on hit with this radius, 70% damage (0 = none)
    float shock;            // melee shockwave radius 1.1 tiles ahead, 50% damage (0 = none)
    bool  wave;             // a slash wave on every swing
    float crit;             // extra crit chance
    Cost  cost;             // count 0 = starter (owned from the start, not crafted)
};

static const int kWeaponCount = (int)WeaponId::Count;

static const WeaponDef kWeapon[kWeaponCount] =
{
    // name                 lock fam          elem         bolt   cd     range arc  atk mine  speed pierce burst shock wave   crit   cost
    { "Warrior's Sword",    0, WFam::Class, Elem::None,  false, 0.45f, 1.40f, 1.9f, 0, 1.0f,  0.0f, 0, 0.0f, 0.0f, false, 0.0f,  { {}, 0 } },
    { "Miner's Mattock",    1, WFam::Class, Elem::None,  false, 0.40f, 1.15f, 1.3f, 0, 1.3f,  0.0f, 0, 0.0f, 0.0f, false, 0.0f,  { {}, 0 } },
    { "Mage's Staff",       2, WFam::Class, Elem::None,  true,  0.50f, 0.0f,  0.0f, 0, 1.0f, 11.0f, 0, 0.0f, 0.0f, false, 0.0f,  { {}, 0 } },
    { "Rogue's Daggers",    3, WFam::Class, Elem::None,  false, 0.20f, 1.05f, 1.4f, 0, 0.9f,  0.0f, 0, 0.0f, 0.0f, false, 0.0f,  { {}, 0 } },
    { "Ember Sword",       -1, WFam::Blade, Elem::Fire,  false, 0.42f, 1.40f, 1.8f, 2, 1.0f,  0.0f, 0, 0.0f, 0.0f, false, 0.0f,
      { { { MatId::Copper, 5 }, { MatId::Stone, 2 }, { MatId::Gel, 1 } }, 3 } },
    { "Crystal Blade",     -1, WFam::Blade, Elem::Frost, false, 0.42f, 1.50f, 2.0f, 9, 1.2f,  0.0f, 0, 0.0f, 0.0f, true,  0.0f,
      { { { MatId::Crystal, 4 }, { MatId::Core, 1 } }, 2 } },
    { "Quarry Axe",        -1, WFam::Heavy, Elem::Earth, false, 0.75f, 1.55f, 2.3f, 6, 1.6f,  0.0f, 0, 0.0f, 0.0f, false, 0.0f,
      { { { MatId::Silver, 5 }, { MatId::Stone, 4 } }, 2 } },
    { "Thunder Hammer",    -1, WFam::Heavy, Elem::Shock, false, 0.80f, 1.30f, 1.5f, 6, 2.0f,  0.0f, 0, 0.0f, 1.2f, false, 0.0f,
      { { { MatId::Gold, 4 }, { MatId::Stone, 5 } }, 2 } },
    { "Barbed Spear",      -1, WFam::Pole,  Elem::Venom, false, 0.50f, 2.40f, 0.5f, 3, 0.8f,  0.0f, 0, 0.0f, 0.0f, false, 0.0f,
      { { { MatId::Copper, 4 }, { MatId::Stone, 3 }, { MatId::Fang, 1 } }, 3 } },
    { "Rockbreaker Maul",  -1, WFam::Pole,  Elem::Earth, false, 0.70f, 2.00f, 0.9f, 5, 1.8f,  0.0f, 0, 0.0f, 0.0f, false, 0.0f,
      { { { MatId::Gold, 3 }, { MatId::Stone, 6 }, { MatId::Core, 1 } }, 3 } },
    { "Fire Staff",        -1, WFam::Magic, Elem::Fire,  true,  0.60f, 0.0f,  0.0f, 4, 1.0f, 10.0f, 0, 1.1f, 0.0f, false, 0.0f,
      { { { MatId::Gold, 3 }, { MatId::Crystal, 1 }, { MatId::Gel, 3 } }, 3 } },
    { "Frost Wand",        -1, WFam::Magic, Elem::Frost, true,  0.38f, 0.0f,  0.0f, 2, 0.6f, 13.0f, 0, 0.0f, 0.0f, false, 0.0f,
      { { { MatId::Silver, 3 }, { MatId::Crystal, 1 }, { MatId::Gel, 2 } }, 3 } },
    { "Spark Crossbow",    -1, WFam::Shot,  Elem::Shock, true,  0.55f, 0.0f,  0.0f, 2, 0.6f, 15.0f, 1, 0.0f, 0.0f, false, 0.0f,
      { { { MatId::Copper, 3 }, { MatId::Silver, 2 }, { MatId::Fang, 2 } }, 3 } },
    { "Venom Blowgun",     -1, WFam::Shot,  Elem::Venom, true,  0.42f, 0.0f,  0.0f, 1, 0.4f, 16.0f, 2, 0.0f, 0.0f, false, 0.0f,
      { { { MatId::Copper, 2 }, { MatId::Fang, 3 }, { MatId::Gel, 2 } }, 3 } },
    { "Frostfang Daggers", -1, WFam::Knife, Elem::Frost, false, 0.18f, 1.05f, 1.4f, 1, 0.7f,  0.0f, 0, 0.0f, 0.0f, false, 0.15f,
      { { { MatId::Silver, 4 }, { MatId::Fang, 3 } }, 2 } },
};

// The two weapon families each class may use (ClassData.h order: Warrior, Miner, Mage, Rogue)
static const WFam kClassFamilies[4][2] =
{
    { WFam::Blade, WFam::Heavy },
    { WFam::Heavy, WFam::Pole },
    { WFam::Magic, WFam::Shot },
    { WFam::Knife, WFam::Shot },
};

// True if a class (ClassData.h index) may craft and hold this weapon.
inline bool ClassCanUse(int classIndex, WeaponId id)
{
    const WeaponDef& w = kWeapon[(int)id];
    if (w.fam == WFam::Class)
        return w.classLock == classIndex;
    return classIndex >= 0 && classIndex < 4 &&
           (kClassFamilies[classIndex][0] == w.fam || kClassFamilies[classIndex][1] == w.fam);
}

// Weapon skills (C7): one per weapon, same order as WeaponId. Z / middle click.
// One shared cooldown timer for all weapons, multiplied by the Focus card.
struct WeaponSkillDef
{
    const char* name;
    float cooldown;         // seconds
    const char* description;
};

static const WeaponSkillDef kWeaponSkill[kWeaponCount] =
{
    { "Lunge",         5.0f, "Dash forward, hitting each monster you pass for 150% damage." },
    { "Ground Slam",   6.0f, "Blast around you for 120% damage that also smashes blocks." },
    { "Blink",         5.0f, "Teleport a short way toward your aim, briefly invincible." },
    { "Fan of Knives", 5.0f, "Throw 10 knives in a circle for 80% damage." },
    { "Flame Arc",     5.0f, "A huge burning swing for 180% damage." },
    { "Crystal Storm", 6.0f, "Send slash waves out in all 8 directions." },
    { "Earthsplitter", 6.0f, "A line of four blasts that rips through rock." },
    { "Thunderclap",   7.0f, "A thunder blast for 160% damage that stuns nearby monsters." },
    { "Javelin",       4.0f, "Throw a spear that pierces everything for 200% damage." },
    { "Quake",         8.0f, "Break every ore around you and stun nearby monsters." },
    { "Meteor",        7.0f, "Call a meteor ahead for 250% damage." },
    { "Ice Nova",      8.0f, "Freeze every monster around you." },
    { "Volley",        5.0f, "Fire 5 bolts in a spread." },
    { "Toxic Cloud",   7.0f, "Leave a poison cloud ahead of you." },
    { "Blizzard Step", 5.0f, "Dash and freeze every monster you pass." },
};
