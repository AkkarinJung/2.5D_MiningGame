#pragma once
// Juice: game-feel effects (the guide's sub-objectives). Every effect has an on/off toggle
// and a strength slider in the F1 panel.
//   hit-stop        a 40-60 ms freeze on big hits (kills, crits, explosions)
//   screen shake    scaled by ore grade, explosions and getting hurt
//   damage numbers  pop up and float away; crits yellow, heals pink, XP green
//   debris          chunks in the ore's colour that burst out, bounce and fade (they cast shadows)
//   dust ring       spreads where a block broke
//   level-up burst  a ring of light and particles around the player
// (Block wobble lives in TileMap, which owns the blocks; its setting is here too.)
// Game code calls the static functions; one Juice object in the Game scene does the work.

#include "GameObject.h"
#include "renderer.h"
#include <cstdint>

class Juice : public GameObject
{
public:
    struct Settings
    {
        bool  hitStop = true;        float hitStopStrength = 1.0f;
        bool  shake = true;          float shakeStrength = 1.0f;
        bool  numbers = true;        float numberSize = 1.0f;
        bool  debris = true;         float debrisAmount = 1.0f;
        float debrisSize = 0.06f;    // edge length of a chunk in tiles
        bool  wobble = true;         float wobbleStrength = 1.0f;
        bool  dustRing = true;
        bool  levelUpBurst = true;
    };

    // ---- called from game code (safe to call when there is no Juice object)
    static void HitStop(float seconds);                         // clamped to 60 ms x strength
    static void Shake(float amount);
    static void Number(float x, float y, float z, const char* text, uint32_t rgb, bool big = false);
    static void Debris(float x, float y, float z, uint32_t rgb, int count, float speed = 1.0f);

    static void BlockHit(int tileX, int tileZ, uint32_t oreRgb);           // chips
    static void TooHard(int tileX, int tileZ, int requiredLevel);          // red popup, at most every 0.9 s
    static void BlockBreak(int tileX, int tileZ, uint32_t oreRgb, int grade, int xp);
    static void MonsterHit(float x, float height, float z, int damage, bool crit, bool executed, bool direct);
    static void MonsterKilled(float x, float z);
    static void PlayerHurt(float x, float z, int damage);
    static void Heal(float x, float z, int amount);
    static void Explosion(float x, float z);
    static void LevelUp(float x, float z, int level);

    static Settings& GetSettings();
    static int ParticleCount();

    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;
    void DrawShadow() override;
};
