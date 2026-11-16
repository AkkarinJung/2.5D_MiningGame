#include "main.h"
#include "renderer.h"
#include "Player.h"
//#include "modelRenderer.h"
#include "animationModel.h"
#include "input.h"
#include "Collision.h"

#include "Manager.h"
#include "Camera.h"
#include "audio.h"
#include "DebugUI.h"

#include "Enemy.h"
#include "Monsters.h"
#include "Projectiles.h"
#include "Tree.h"
#include "Box.h"
#include "Shadow.h"

#include "TileMap.h"
#include "OreData.h"
#include "Score.h"

#include <random>

void Player::Init()
{
    // Class stats (chosen on the class select screen)
    m_Class = SelectedClass();
    const ClassDef& cls = kClass[(int)m_Class];
    m_MaxHp = m_Hp = cls.hp;
    m_Speed = cls.speed;
    m_Attack = cls.attack;
    m_MinePower = cls.mining;
    m_AttackRate = cls.cooldown;
    m_AttackRange = cls.range;
    m_AttackArc = cls.arc;
    m_InvincibleTime = cls.invincibility;
    m_Crit = cls.crit;
    m_MiningXpScale = cls.miningXpScale;

    m_Layer = 1;
    m_Position = { 0.0f, 0.0f, 0.0f };
    m_Scale = { 0.005f, 0.005f, 0.005f };


    //ModelRenderer* m_ModelRenderer = AddGameComponent<ModelRenderer>(this);
    //m_ModelRenderer->Load("asset\\model\\player.obj");
    m_AnimationModel = AddGameComponent<AnimationModel>(this);
    m_AnimationModel->Load("asset\\model\\Idle.fbx");
    m_AnimationModel->LoadAnimation("asset\\model\\Idle.fbx", "Idle");
    m_AnimationModel->LoadAnimation("asset\\model\\Run.fbx", "Run");
    m_AnimationModel->LoadAnimation("asset\\model\\Punch.fbx", "Attack");

    m_AnimationName = "Idle";
    m_NextAnimationName = "Idle";

    // シェーダー読込
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout,
        "shader\\litTextureVS.cso");

    Renderer::CreatePixelShader(&m_PixelShader,
        "shader\\litTexturePS.cso");

    //BGM
    m_JumpSE = AddGameComponent<Audio>(this);
    m_JumpSE->Load("asset\\Audio\\wan.wav");

    m_Child = Manager::AddGameObj<GameObject>();
    m_Child->SetParent(this);
    m_Child->SetPosition({ 0.0f,2.0f,0.0f });

    //ModelRenderer* childModel = m_Child->AddGameComponent<ModelRenderer>(m_Child);
    //childModel->Load("asset\\model\\Rabbit\\rabbit_1.obj");
    m_Shadow = Manager::AddGameObj<Shadow>();
    m_Shadow->SetScale({ 0.4f, 0.4f, 0.4f });
}

void Player::Uninit()
{
    m_VertexLayout->Release();
    m_VertexShader->Release();
    m_PixelShader->Release();

    GameObject::Uninit();
}

void Player::Update()
{

    Vector3 oldPosition = m_Position;
    float dt = 1.0 / 60.0f;

    switch (m_State)
    {
    case Player::STATE_GROUND:
        UpdateGround();
        break;
    case Player::STATE_ATTACK:
        UpdateAttack();
        break;
    default:
        break;
    }

    m_AttackCooldown -= dt;
    m_Invincible -= dt;
    m_SkillTimer -= dt;

    Vector3 rot = m_Child->GetRotation();
    rot.y += 1.0f * dt;
    m_Child->SetRotation(rot);

    bool oldGround = m_Ground;
    m_Ground = false;

    m_Rotation.y = atan2f(m_Facing.x, m_Facing.z);   // face the last move direction

    //return scale to original
    //m_Scale.x += (1.0f - m_Scale.x) * 0.1f;
    //m_Scale.y += (1.0f - m_Scale.y) * 0.1f;
    //m_Scale.z += (1.0f - m_Scale.z) * 0.1f;

    m_Velocity.y += -98.0f * dt;

    m_Velocity.x += -m_Velocity.x * 5.0f * dt;
    m_Velocity.z += -m_Velocity.z * 5.0f * dt;


    TileMap* map = Manager::GetGameObj<TileMap>();
    if (m_DashTime > 0.0f && map)
    {
        // Shadow Dash: 24 tiles/s in 4 substeps, cutting every monster passed once
        m_DashTime -= dt;
        Monsters* monsters = Manager::GetGameObj<Monsters>();
        const float step = 24.0f * dt / 4.0f;
        for (int s = 0; s < 4; s++)
        {
            Vector3 move(cosf(m_DashAngle) * step, 0.0f, sinf(m_DashAngle) * step);
            map->MoveAndSlide(m_Position, move, 1.0f, 0.3f);
            if (monsters)
                monsters->HitAlongDash(m_Position.x, m_Position.z, m_DashDamage, m_DashId);
        }
        m_Velocity.x = m_Velocity.z = 0.0f;
    }
    else if (map)
        map->MoveAndSlide(m_Position, m_Velocity, dt, 0.3f);   // 0.3 = player half-width in tiles
    else
        m_Position += m_Velocity * dt;

    const float height = 0.0f;   // flat dungeon floor

    if (m_Position.y < height)
    {
        m_Position.y = height;
        m_Velocity.y = 0.0f;
        m_Ground = true;
    }

    auto trees = Manager::GetGameObjs<Tree>();
    for (auto tree : trees)
    {
        Vector3 treePos = tree->GetPosition();
        Vector3 playerPos = m_Position;

        treePos.y = 0.0f;
        playerPos.y = 0.0f;

        Vector3 dir = playerPos - treePos;
        float length = VectorMag(dir);
        if (length < 1.4f)
        {
            dir /= length;
            dir *= 1.4f - length;

            m_Position += dir;
        }
    }

    auto boxes = Manager::GetGameObjs<Box>();
    for (auto box : boxes)
    {
        Vector3 boxPosition = box->GetPosition();
        Vector3 boxScale = box->GetScale();

        if (boxPosition.x - boxScale.x < m_Position.x &&
            m_Position.x < boxPosition.x + boxScale.x &&
            boxPosition.z - boxScale.z < m_Position.z &&
            m_Position.z < boxPosition.z + boxScale.z)
        {
            if (boxPosition.y + boxScale.y < m_Position.y &&
                m_Position.y < boxPosition.y + boxScale.y * 2.0f)
            {
                //上面に衝突 top of the box
                m_Position.y = boxPosition.y + boxScale.y * 2.0f;
                m_Velocity.y = 0.0f;
                m_Ground = true;
            }
            else if (boxPosition.y - boxScale.y < m_Position.y &&
                m_Position.y < boxPosition.y + boxScale.y)
            {
                //側面に衝突
                m_Position.x = oldPosition.x;
                m_Position.z = oldPosition.z;
                m_Velocity.x = 0.0f;
                m_Velocity.z = 0.0f;
            }
        }
    }

    auto enemies = Manager::GetGameObjs<Enemy>();

    for (auto enemy : enemies)
    {
        const float playerRadius = 0.7f;
        const float enemyRadius = 0.7f;

        Vector3 dir = m_Position - enemy->GetPosition();
        float length = dir.lenght();

        if (Collision::SphereVsSphere(m_Position, playerRadius, enemy->GetPosition(), enemyRadius))
        {
            if (length > 0.0f)
            {
                float overlap = (playerRadius + enemyRadius) - length;
                dir /= length;
                m_Position += dir * overlap;
            }

            break;
        }
    }

    if (m_Ground)
    {
        //m_MoveAnimation += VectorMag(m_Velocity) * dt;
        //m_Scale.y += sinf(m_MoveAnimation * 3.0f) * 0.03f;
    }

    Vector3 shadowPos = m_Position;
    shadowPos.y = 0.01f;
    m_Shadow->SetPosition(shadowPos);

    if (!m_HitStop)
    {
        m_AnimationFrame++;
        m_NextAnimationFrame++;
        m_Blend += 0.1f;
        if (m_Blend > 1.0f)
            m_Blend = 1.0f;
    }

    GameObject::Update();
}

void Player::UpdateGround()
{
    const float dt = 1.0f / 60.0f;

    if (IsDead())
        return;     // no moving or swinging once HP hits 0

    Camera* camera = Manager::GetGameObj<Camera>();
    Vector3 forward = camera->GetFoward();
    Vector3 right = camera->GetRight();
    forward.y = 0.0f;
    forward.normalize();
    right.y = 0.0f;
    right.normalize();

    Vector3 input(0.0f, 0.0f, 0.0f);
    if (Input::GetKeyPress('D')) input += right;
    if (Input::GetKeyPress('A')) input -= right;
    if (Input::GetKeyPress('W')) input += forward;
    if (Input::GetKeyPress('S')) input -= forward;

    const float length = VectorMag(input);
    const bool move = length > 0.0f;
    if (move)
    {
        input /= length;                        // diagonals are not faster
        m_Velocity += input * (m_Speed * 5.0f * dt);   // friction is 5/s, so top speed = m_Speed
        m_Facing = input;
    }

    if (m_State == STATE_GROUND)
        SetAnimation(move ? "Run" : "Idle");

    if (Input::GetKeyTrigger('Q'))
        DrinkPotion();

    // Hold J or Space to swing; the cooldown sets the pace
    if ((Input::GetKeyPress('J') || Input::GetKeyPress(VK_SPACE)) && m_AttackCooldown <= 0.0f)
    {
        m_State = STATE_ATTACK;
        m_StateTime = 0.0f;
        m_AttackCooldown = m_AttackRate;
        Attack();
    }

    // K or Shift: class skill
    if ((Input::GetKeyTrigger('K') || Input::GetKeyTrigger(VK_SHIFT)) && m_SkillTimer <= 0.0f && m_DashTime <= 0.0f)
        UseSkill();
}

void Player::UpdateAttack()
{
    const float dt = 1.0f / 60.0f;

    UpdateGround();             // you can still walk while swinging
    SetAnimation("Attack");

    m_StateTime += dt;
    if (m_StateTime >= m_AttackRate)
        m_State = STATE_GROUND;
}

void Player::Draw()
{
    if (m_Invincible > 0.0f && !IsDead() && ((int)(m_Invincible * 14.0f) % 2) == 1)
        return;

    // 入力レイアウト設定
    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

    // シェーダ設定
    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    m_AnimationModel->Update(m_AnimationName.c_str(), m_AnimationFrame, m_NextAnimationName.c_str(), m_NextAnimationFrame, m_Blend);
    GameObject::Draw();
}

void Player::SetAnimation(const char* AnimationName)
{
    if (m_NextAnimationName != AnimationName)
    {
        m_AnimationName = m_NextAnimationName;
        m_AnimationFrame = m_NextAnimationFrame;

        m_NextAnimationName = AnimationName;
        m_NextAnimationFrame = 0;

        m_Blend = 0.0f;
    }
}

float Player::RollDamage(float base)
{
    static std::mt19937 rng(std::random_device{}());
    if (std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) < m_Crit)
    {
        DebugUI::Log("Critical!");
        return base * 2.0f;
    }
    return base;
}

// Keyboard auto-aim: the nearest monster the player can see within reach, else facing.
float Player::AimAngle(float reach)
{
    float angle = atan2f(m_Facing.z, m_Facing.x);
    Monsters* monsters = Manager::GetGameObj<Monsters>();
    if (monsters && monsters->FindTarget(m_Position.x, m_Position.z, reach, angle))
        m_Facing = Vector3(cosf(angle), 0.0f, sinf(angle));     // turn to face the target
    return angle;
}

// One attack: the Mage fires a bolt; everyone else swings at monsters and blocks in the arc.
void Player::Attack()
{
    const ClassDef& cls = kClass[(int)m_Class];

    if (cls.ranged)
    {
        const float angle = AimAngle(7.0f);
        Projectiles* projectiles = Manager::GetGameObj<Projectiles>();
        if (projectiles)
            projectiles->FireBolt(m_Position.x, m_Position.z, angle, RollDamage(m_Attack), m_MinePower);
        return;
    }

    const float facing = AimAngle(m_AttackRange + 1.3f);

    Monsters* monsters = Manager::GetGameObj<Monsters>();
    if (monsters)
        monsters->HitInArc(m_Position.x, m_Position.z, facing, m_AttackRange, m_AttackArc, RollDamage(m_Attack));

    TileMap* map = Manager::GetGameObj<TileMap>();
    if (!map)
        return;
    int hits[16];
    const int count = map->BlocksInArc(m_Position.x, m_Position.z, facing, m_AttackRange, m_AttackArc, hits, 16);
    for (int k = 0; k < count; k++)
        MineTile(hits[k] % TileMap::W, hits[k] / TileMap::W, m_MinePower);
}

// K / Shift: the class skill, then its cooldown.
void Player::UseSkill()
{
    if (IsDead())
        return;

    const ClassDef& cls = kClass[(int)m_Class];
    m_SkillTimer = cls.skillCooldown;

    Monsters* monsters = Manager::GetGameObj<Monsters>();
    Projectiles* projectiles = Manager::GetGameObj<Projectiles>();
    TileMap* map = Manager::GetGameObj<TileMap>();
    Camera* camera = Manager::GetGameObj<Camera>();
    const float x = m_Position.x, z = m_Position.z;

    switch (m_Class)
    {
    case PlayerClass::Warrior:      // Whirlwind: full circle, radius 1.9, 2x attack and mining
    {
        if (monsters)
            monsters->HitInArc(x, z, 0.0f, 1.9f, XM_2PI, RollDamage(m_Attack * 2.0f));
        if (map)
        {
            int hits[32];
            const int count = map->BlocksInArc(x, z, 0.0f, 1.9f, XM_2PI, hits, 32);
            for (int k = 0; k < count; k++)
                MineTile(hits[k] % TileMap::W, hits[k] / TileMap::W, m_MinePower * 2.0f);
        }
        if (projectiles)
            projectiles->AddRing(x, z, 1.9f, 0xfff3b0, 0.3f);
        if (camera)
            camera->Shake(Vector3(0.15f, 0.0f, 0.15f));
        DebugUI::Log("Whirlwind!");
        break;
    }
    case PlayerClass::Miner:        // Dynamite: thrown 3.4 tiles, radius 1.9, 3x attack and mining
    {
        const float angle = AimAngle(5.0f);
        if (projectiles)
            projectiles->ThrowBomb(x, z, x + cosf(angle) * 3.4f, z + sinf(angle) * 3.4f,
                1.9f, RollDamage(m_Attack * 3.0f), m_MinePower * 3.0f);
        DebugUI::Log("Dynamite!");
        break;
    }
    case PlayerClass::Mage:         // Frost Nova: radius 3.5, 1x attack, freeze 2.5 s, mines blocks within 3
    {
        if (monsters)
            monsters->HitInRadius(x, z, 3.5f, RollDamage(m_Attack), 5.0f, 2.5f);
        for (int tz = (int)floorf(z - 3.0f); tz <= (int)floorf(z + 3.0f); tz++)
            for (int tx = (int)floorf(x - 3.0f); tx <= (int)floorf(x + 3.0f); tx++)
            {
                const float dx = tx + 0.5f - x, dz = tz + 0.5f - z;
                if (dx * dx + dz * dz <= 9.0f)
                    MineTile(tx, tz, m_MinePower);
            }
        if (projectiles)
            projectiles->AddRing(x, z, 3.5f, 0x6fdcea, 0.45f);
        DebugUI::Log("Frost Nova!");
        break;
    }
    default:                        // Rogue - Shadow Dash: 24 tiles/s for 0.17 s, invincible
    {
        m_DashAngle = AimAngle(4.0f);
        m_DashTime = 0.17f;
        m_DashId++;
        m_DashDamage = RollDamage(m_Attack * 1.5f);     // one crit roll per dash
        m_Invincible = std::max(m_Invincible, 0.4f);
        DebugUI::Log("Shadow Dash!");
        break;
    }
    }
}

// Mine one tile (does nothing if it is not a block). Used by swings, bolts, bombs and skills.
void Player::MineTile(int x, int z, float power)
{
    TileMap* map = Manager::GetGameObj<TileMap>();
    if (!map)
        return;
    const MineResult result = map->MineBlock(x, z, power, m_MineLevel);
    if (!result.hit)
        return;

    if (result.tooHard)
        DebugUI::Log("Too hard! %s needs mining Lv%d",
            kOre[result.grade].name, kOre[result.grade].requiredLevel);
    if (result.broken)
    {
        DebugUI::Log("You dug out %s. (%d blocks left)",
            kOre[result.grade].name, map->GetBlocksLeft());
        GainMiningXp(kOre[result.grade].xp * m_MiningXpScale);  // Miner: 1.5x
    }
}

// Mining XP: each level needs 8 + 7 per level and gives +1 mining power.
void Player::GainMiningXp(float xp)
{
    m_MineXp += xp;
    while (m_MineXp >= (float)MiningXpToNext(m_MineLevel))
    {
        m_MineXp -= (float)MiningXpToNext(m_MineLevel);
        m_MineLevel++;
        m_MinePower += 1.0f;

        DebugUI::Log("Mining Lv%d! (power %.0f)", m_MineLevel, m_MinePower);
        // M5: queue a card pick here, tagged with this ore grade
    }

    Score* level = Manager::GetGameObj<Score>();
    if (level)
        level->SetScore(m_MineLevel);
}

// Contact damage from a monster: armor, minimum 1, 0.6 s invincibility, knockback away.
void Player::TakeDamage(int amount, float fromX, float fromZ, const char* source)
{
    if (m_Invincible > 0.0f || IsDead() || m_GodMode)
        return;

    const int damage = std::max(1, amount - m_Armor);
    m_Hp -= damage;
    m_Invincible = m_InvincibleTime;   // Warrior: 1 s

    const float dx = m_Position.x - fromX, dz = m_Position.z - fromZ;
    const float len = sqrtf(dx * dx + dz * dz);
    if (len > 0.0001f)
    {
        m_Velocity.x += dx / len * 6.0f;
        m_Velocity.z += dz / len * 6.0f;
    }

    Camera* camera = Manager::GetGameObj<Camera>();
    if (camera)
        camera->Shake(Vector3(0.12f, 0.0f, 0.12f));

    DebugUI::Log("The %s hits you for %d. (HP %d/%d)", source, damage, std::max(0, m_Hp), m_MaxHp);
    if (IsDead())
        DebugUI::Log("Slain by the %s...", source);
}

// Q: drink a potion, healing 40% of max HP (rounded up, never above max).
void Player::DrinkPotion()
{
    if (IsDead() || m_Potions <= 0 || m_Hp >= m_MaxHp)
        return;

    const int heal = std::min(m_MaxHp - m_Hp, (int)ceilf(m_MaxHp * 0.4f));
    m_Hp += heal;
    m_Potions--;
    DebugUI::Log("You drink a potion. +%d HP (%d left)", heal, m_Potions);
}
