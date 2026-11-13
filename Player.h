#pragma once

#include "GameObject.h"
#include "animationModel.h"

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

    void Swing();


public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    void UpdateGround();
    void UpdateAttack();

    void SetAnimation(const char* AnimationName);
    void GainMiningXp(float xp);
    int GetMineLevel() const { return m_MineLevel; }
    float GetMineXp() const { return m_MineXp; }
    float GetMinePower() const { return m_MinePower; }

    void TakeDamage(int amount, float fromX, float fromZ, const char* source);
    void HealFull() { m_Hp = m_MaxHp; }
    int  GetHp() const { return m_Hp; }
    int  GetMaxHp() const { return m_MaxHp; }
    bool IsDead() const { return m_Hp <= 0; }
    bool& GodMode() { return m_GodMode; }
};

