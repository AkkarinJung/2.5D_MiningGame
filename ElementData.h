#pragma once
// ElementData.h (C0): the five weapon elements and their default numbers (Crafting Update, C6).
// Every player hit carries the held weapon's element. Monsters take 1.5x from their weak
// element and 0.4x from a resisted one. C6 copies these defaults into F1-tunable settings;
// nothing uses them yet.

#include <cstdint>

enum class Elem : uint8_t { None, Fire, Frost, Shock, Venom, Earth, Count };

struct ElemDef
{
    const char* name;
    uint32_t color;         // 0xRRGGBB: damage numbers, weakness marker, HUD weapon text
};

static const int kElemCount = (int)Elem::Count;

static const ElemDef kElem[kElemCount] =
{
    { "None",  0xe8e0d0 },
    { "Fire",  0xff7a3a },
    { "Frost", 0x7fe0ff },
    { "Shock", 0xfff07a },
    { "Venom", 0x9fe05a },
    { "Earth", 0xd0a060 },
};

// Default numbers for C6 (all times in seconds, distances in tiles)
struct ElementTuning
{
    float weakMultiplier = 1.5f;
    float resistMultiplier = 0.4f;

    // Fire: burn ticks every 0.5 s for 1 + floor/2 + ceil(attack x 0.25); burning deaths spread
    float burnTime = 3.0f;
    float burnTick = 0.5f;
    float burnAttackScale = 0.25f;
    float burnSpreadRadius = 2.2f;

    // Frost: chill stacks (reset after 2.5 s without a hit), 3 stacks = frozen; frozen deaths shatter
    float chillReset = 2.5f;
    float chillSpeed = 0.55f;
    int   freezeStacks = 3;
    float freezeTime = 1.6f;
    float shatterFraction = 0.3f;       // of the frozen monster's max HP
    float shatterRadius = 1.9f;

    // Shock: chains to the nearest monsters for part of the damage (chains never chain again)
    int   chainCount = 2;
    float chainRadius = 2.8f;
    float chainFraction = 0.6f;
    float chainDrawTime = 0.18f;

    // Venom: poison ticks add ceil(attack x 0.2); a poisoned monster drops its table twice
    float poisonTime = 4.0f;
    float poisonAttackScale = 0.2f;

    // Earth: stronger knockback and a short stagger; breaks whole veins of same-tier ore
    float knockbackMultiplier = 2.0f;
    float staggerTime = 0.35f;
    float miningMultiplier = 1.5f;
    float veinDelay = 0.14f;
};

static const ElementTuning kElementDefaults{};
