#pragma once

#include "GameObject.h"
#include "animationModel.h"
#include "ClassData.h"

class Player : public GameObject
{
private:
    Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f }; //???x


    ID3D11InputLayout* m_VertexLayout;
    ID3D11VertexShader* m_VertexShader;
    ID3D11PixelShader* m_PixelShader;

    bool m_Ground = true;
    float m_MoveAnimation = 0.0f;
    class Audio* m_JumpSE;

    GameObject* m_Child;
    GameObject* m_Shadow;

    class AnimationModel* m_AnimationModel;
    int m_AnimationFrame = 0;
    std::string m_AnimationName;

    int m_NextAnimationFrame = 0;
    std::string m_NextAnimationName;

    float m_Blend = 0.0f;

    enum STATE
    {
        STATE_GROUND,
        STATE_ATTACK,

    };

    STATE m_State = STATE_GROUND;
    float m_StateTime = 0.0f;

    bool m_Hit = false;
    bool m_HitStop = false;

    // Mining (M3). Fixed Miner values until classes arrive in M5.
    int   m_MineLevel = 1;
    float m_MineXp = 0.0f;
    float m_MinePower = 5.0f;
    float m_AttackCooldown = 0.0f;
    Vector3 m_Facing{ 0.0f, 0.0f, 1.0f };

    // Health (M4). Miner values until classes arrive in M5.
    int   m_Hp = 24;
    int   m_MaxHp = 24;
    int   m_Armor = 0;
    float m_Attack = 3.0f;
    float m_Invincible = 0.0f;
    bool  m_GodMode = false;
    int   m_Potions = 1;        // start with 1

    // Class (M5): filled from kClass in Init
    PlayerClass m_Class = PlayerClass::Miner;
    float m_Speed = 4.0f;
    float m_AttackRate = 0.4f;      // seconds between swings
    float m_AttackRange = 1.15f;
    float m_AttackArc = 1.3f;
    float m_InvincibleTime = 0.6f;
    float m_Crit = 0.0f;
    float m_MiningXpScale = 1.0f;

    // Skills (M5 step 2)
    float m_SkillTimer = 0.0f;      // > 0: skill on cooldown
    float m_DashTime = 0.0f;        // > 0: Rogue Shadow Dash in progress
    float m_DashAngle = 0.0f;
    int   m_DashId = 0;
    float m_DashDamage = 0.0f;

    // Card modifiers (M5 step 3), changed only by ApplyCard
    float m_AttackSpeed = 1.0f;     // Quick Hands
    float m_Area = 1.0f;            // Broad Blade: reach x area
    float m_CooldownScale = 1.0f;   // Focus
    float m_Luck = 0.0f;            // Prospector
    float m_Twin = 0.0f;            // Twin Strike chance
    float m_TwinTimer = 0.0f;       // > 0: second attack fires when it runs out
    int   m_Leech = 0;              // HP per kill
    int   m_Thorns = 0;             // damage back on contact
    float m_Aftershock = 0.0f;      // damage around broken blocks
    float m_Blast = 1.0f;           // Big Bomb radius scale
    bool  m_Executioner = false;
    bool  m_Motherlode = false;
    bool  m_SwordWave = false;
    bool  m_ChainBlast = false;
    bool  m_SplitBolt = false;
    bool  m_Venom = false;

    void Attack();                  // DoAttack + Twin Strike roll
    void DoAttack();                // melee swing or Mage bolt(s)
    void UseSkill();
    float AimAngle(float reach);    // nearest visible monster in reach, else facing
    float Reach() const { return m_AttackRange * m_Area; }
    float Arc() const;
    float RollDamage(float base);   // crit: double damage


public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    void UpdateGround();
    void UpdateAttack();

    void SetAnimation(const char* AnimationName);
    void GainMiningXp(float xp, int oreGrade = 0);  // level-ups queue a card pick for this grade
    int GetMineLevel() const { return m_MineLevel; }
    float GetMineXp() const { return m_MineXp; }
    float GetMinePower() const { return m_MinePower; }

    bool TakeDamage(int amount, float fromX, float fromZ, const char* source);// true if it hurt
    void HealFull() { m_Hp = m_MaxHp; }
    int  GetHp() const { return m_Hp; }
    int  GetMaxHp() const { return m_MaxHp; }
    bool IsDead() const { return m_Hp <= 0; }
    bool& GodMode() { return m_GodMode; }
    void AddPotion() { m_Potions++; }
    int  GetPotions() const { return m_Potions; }
    void DrinkPotion();

    PlayerClass GetClass() const { return m_Class; }

    // Mine the block on (x, z) with this power; handles "too hard", logging and XP.
    void MineTile(int x, int z, float power);
    float GetSkillTimer() const { return m_SkillTimer; }
    float GetSkillCooldown() const { return kClass[(int)m_Class].skillCooldown; }

    // Cards
    void ApplyCard(CardId id);
    void OnKill();                  // Leech
    float GetLuck() const { return m_Luck; }
    int  GetThorns() const { return m_Thorns; }
    int  GetArmor() const { return m_Armor; }
    float GetCrit() const { return m_Crit; }
    bool IsExecutioner() const { return m_Executioner; }
    bool HasVenom() const { return m_Venom; }
};

