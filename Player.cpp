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

#include "Monsters.h"
#include "Projectiles.h"
#include "Pickups.h"
#include "Cards.h"
#include "GameState.h"
#include "Text.h"
#include "Lighting.h"
#include "ShadowMap.h"
#include "Juice.h"
#include "Tree.h"
#include "Box.h"
#include "Shadow.h"

#include "TileMap.h"
#include "OreData.h"

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

    m_Bag.Clear();
    m_Bag.Add(MatId::Potion, 1);        // every run starts with one potion

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
        "shader\\pointLitVS.cso");

    Renderer::CreatePixelShader(&m_PixelShader,
        "shader\\pointLitPS.cso");

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
    // Lamp (M7): the light the player carries, with a slight flicker. Added before the
    // pause check so the dungeon stays lit while a card pick or prompt is open.
    {
        const Lighting::Settings& ls = Lighting::GetSettings();
        Lighting::SetFocus(m_Position.x, m_Position.z);
        if (!Lighting::IsTown())        // daylight in the town (floor 0): lamp off, no lamp shadows
            Lighting::SetLamp(m_Position.x, 1.7f, m_Position.z, ls.lampRadius + m_LampBonus, 0xffb866,
                ls.lampIntensity * Lighting::Flicker(0.0f));   // light 0, casts shadows

        // Fog of war: you see a little further than your lamp reaches (lamp radius + 2)
        TileMap* map = Manager::GetGameObj<TileMap>();
        if (map)
            map->UpdateVisibility(m_Position.x, m_Position.z, ls.lampRadius + m_LampBonus + ls.viewExtra);
    }

    if (GamePaused())
        return;     // card pick open: freeze in place

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

    if (m_TwinTimer > 0.0f)
    {
        m_TwinTimer -= dt;
        if (m_TwinTimer <= 0.0f && !IsDead())
            DoAttack();             // Twin Strike: the second attack, 0.1 s later
    }

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

    // Hold J or Space to attack; the cooldown sets the pace
    if ((Input::GetKeyPress('J') || Input::GetKeyPress(VK_SPACE)) && m_AttackCooldown <= 0.0f && m_DashTime <= 0.0f)
    {
        m_State = STATE_ATTACK;
        m_StateTime = 0.0f;
        m_AttackCooldown = m_AttackRate / m_AttackSpeed;   // Quick Hands
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
    if (m_StateTime >= m_AttackRate / m_AttackSpeed)
        m_State = STATE_GROUND;
}

void Player::Draw()
{
    if (m_Invincible > 0.0f && m_DashTime <= 0.0f && !IsDead() && ((int)(m_Invincible * 14.0f) % 2) == 1)
        return;

    // 入力レイアウト設定
    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

    // シェーダ設定
    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    m_AnimationModel->Update(m_AnimationName.c_str(), m_AnimationFrame, m_NextAnimationName.c_str(), m_NextAnimationFrame, m_Blend);
    GameObject::Draw();
}

// Shadow pass (M8): the character model, depth only. Uses last frame's animated pose.
void Player::DrawShadow()
{
    if (ShadowMap::IsLampPass())
        return;     // the lamp is inside the player: the player must not shadow its own lamp

    if (m_Invincible > 0.0f && m_DashTime <= 0.0f && !IsDead() && ((int)(m_Invincible * 14.0f) % 2) == 1)
        return;     // blinking: no shadow on the hidden frames either

    ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
    ctx->IASetInputLayout(m_VertexLayout);
    ctx->VSSetShader(m_VertexShader, NULL, 0);
    ctx->PSSetShader(nullptr, NULL, 0);
    Renderer::SetWorldMatrix(GetMatrx());
    m_AnimationModel->Draw();
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
    m_LastCrit = std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) < m_Crit;
    if (m_LastCrit)
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

float Player::Arc() const
{
    // Broad Blade widens the swing a little too, up to 1.4 pi
    return std::min(XM_PI * 1.4f, m_AttackArc * (1.0f + (m_Area - 1.0f) * 0.6f));
}

// One attack, plus a Twin Strike roll for a second one 0.1 s later.
void Player::Attack()
{
    DoAttack();
    static std::mt19937 rng(std::random_device{}());
    if (m_Twin > 0.0f && std::uniform_real_distribution<float>(0.0f, 1.0f)(rng) < m_Twin)
        m_TwinTimer = 0.1f;
}

// The Mage fires a bolt (three with Split Bolt); everyone else swings at monsters and
// blocks in the arc (and launches a slash wave with Sword Wave).
void Player::DoAttack()
{
    const ClassDef& cls = kClass[(int)m_Class];
    Projectiles* projectiles = Manager::GetGameObj<Projectiles>();

    if (cls.ranged)
    {
        const float angle = AimAngle(7.0f);
        if (projectiles)
        {
            projectiles->FireBolt(m_Position.x, m_Position.z, angle, RollDamage(m_Attack), m_MinePower);
            if (m_SplitBolt)
            {
                projectiles->FireBolt(m_Position.x, m_Position.z, angle - 0.28f, RollDamage(m_Attack), m_MinePower);
                projectiles->FireBolt(m_Position.x, m_Position.z, angle + 0.28f, RollDamage(m_Attack), m_MinePower);
            }
        }
        return;
    }

    const float facing = AimAngle(Reach() + 1.3f);

    Monsters* monsters = Manager::GetGameObj<Monsters>();
    if (monsters)
        monsters->HitInArc(m_Position.x, m_Position.z, facing, Reach(), Arc(), RollDamage(m_Attack));

    if (m_SwordWave && projectiles)
        projectiles->FireWave(m_Position.x, m_Position.z, facing,
            RollDamage(ceilf(m_Attack * 0.7f)), m_MinePower * 0.5f);

    TileMap* map = Manager::GetGameObj<TileMap>();
    if (!map)
        return;
    int hits[16];
    const int count = map->BlocksInArc(m_Position.x, m_Position.z, facing, Reach(), Arc(), hits, 16);
    for (int k = 0; k < count; k++)
        MineTile(hits[k] % TileMap::W, hits[k] / TileMap::W, m_MinePower);
}

// K / Shift: the class skill, then its cooldown.
void Player::UseSkill()
{
    if (IsDead())
        return;

    m_SkillTimer = GetSkillCooldown();      // Focus shortens it

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
            monsters->HitInArc(x, z, 0.0f, 1.9f * m_Area, XM_2PI, RollDamage(m_Attack * 2.0f));
        if (map)
        {
            int hits[32];
            const int count = map->BlocksInArc(x, z, 0.0f, 1.9f * m_Area, XM_2PI, hits, 32);
            for (int k = 0; k < count; k++)
                MineTile(hits[k] % TileMap::W, hits[k] / TileMap::W, m_MinePower * 2.0f);
        }
        if (projectiles)
            projectiles->AddRing(x, z, 1.9f * m_Area, 0xfff3b0, 0.3f);
        if (camera)
            Juice::Shake(0.15f);
        DebugUI::Log("Whirlwind!");
        break;
    }
    case PlayerClass::Miner:        // Dynamite: thrown 3.4 tiles, radius 1.9, 3x attack and mining
    {
        const float angle = AimAngle(5.0f);
        if (projectiles)
            projectiles->ThrowBomb(x, z, x + cosf(angle) * 3.4f, z + sinf(angle) * 3.4f,
                1.9f * m_Blast, RollDamage(m_Attack * 3.0f), m_MinePower * 3.0f);   // Big Bomb: x1.4 radius
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
        Lighting::Flash(x, 1.2f, z, 6.0f, 0x6fdcea, 4.0f);
        DebugUI::Log("Frost Nova!");
        break;
    }
    default:                        // Rogue - Shadow Dash: 24 tiles/s for 0.17 s, invincible
    {
        m_DashAngle = AimAngle(4.0f);
        m_DashTime = 0.17f;
        m_DashId = monsters ? monsters->NewHitId() : m_DashId + 1;   // ids shared with Sword Wave
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

    const OreDef& ore = kOre[result.grade];
    const uint32_t oreRgb = ((uint32_t)(ore.r * 255.0f) << 16) | ((uint32_t)(ore.g * 255.0f) << 8) | (uint32_t)(ore.b * 255.0f);

    if (result.tooHard)
    {
        DebugUI::Log("Too hard! %s needs mining Lv%d",
            Tr(kOre[result.grade].name), kOre[result.grade].requiredLevel);
        Juice::TooHard(x, z, ore.requiredLevel);
    }
    if (!result.broken)
        Juice::BlockHit(x, z, oreRgb);
    if (result.broken)
    {
        Juice::BlockBreak(x, z, oreRgb, result.grade, (int)roundf(ore.xp * m_MiningXpScale));
        DebugUI::Log("You dug out %s. (%d blocks left)",
            Tr(kOre[result.grade].name), map->GetBlocksLeft());
        m_BlocksBroken++;
        GainMiningXp(kOre[result.grade].xp * m_MiningXpScale, result.grade);  // Miner: 1.5x

        const float bx = x + 0.5f, bz = z + 0.5f;
        Projectiles* projectiles = Manager::GetGameObj<Projectiles>();
        if (m_Aftershock > 0.0f)            // Aftershock: hurt monsters around the broken block
        {
            Monsters* monsters = Manager::GetGameObj<Monsters>();
            if (monsters)
                monsters->HitInRadius(bx, bz, 2.0f, m_Aftershock, 4.0f, 0.0f, false);
            if (projectiles)
                projectiles->AddRing(bx, bz, 2.2f, 0xffb35a);
        }
        if (m_ChainBlast && projectiles)    // Chain Blast: the block explodes 0.18 s later
            projectiles->QueueExplosion(bx, bz, 0.18f, 1.7f, m_Attack * 2.0f, m_MinePower * 1.5f);
    }
}

// Mining XP: each level needs 8 + 7 per level and gives +1 mining power.
void Player::GainMiningXp(float xp, int oreGrade)
{
    Cards* cards = Manager::GetGameObj<Cards>();
    m_MineXp += xp;
    while (m_MineXp >= (float)MiningXpToNext(m_MineLevel))
    {
        m_MineXp -= (float)MiningXpToNext(m_MineLevel);
        m_MineLevel++;
        m_MinePower += 1.0f;

        DebugUI::Log("Mining Lv%d! (power %.0f)", m_MineLevel, m_MinePower);
        Juice::LevelUp(m_Position.x, m_Position.z, m_MineLevel);
        if (cards)
            cards->QueueLevelUp(oreGrade, m_MineLevel);     // card pick, odds from this ore
    }

}

// Contact damage from a monster: armor, minimum 1, 0.6 s invincibility, knockback away.
bool Player::TakeDamage(int amount, float fromX, float fromZ, const char* source)
{
    if (m_Invincible > 0.0f || IsDead() || m_GodMode)
        return false;

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
        Juice::PlayerHurt(m_Position.x, m_Position.z, damage);   // red number, debris, shake

    DebugUI::Log("The %s hits you for %d. (HP %d/%d)", Tr(source), damage, std::max(0, m_Hp), m_MaxHp);
    if (IsDead())
    {
        m_KilledBy = source;
        DebugUI::Log("Slain by the %s...", Tr(source));
    }
    return true;
}

// Leech: heal on every kill.
void Player::OnKill()
{
    m_Kills++;
    if (m_Leech > 0 && !IsDead() && m_Hp < m_MaxHp)
    {
        const int heal = std::min(m_Leech, m_MaxHp - m_Hp);
        m_Hp += heal;
        DebugUI::Log("Leech +%d HP", heal);
        Juice::Heal(m_Position.x, m_Position.z, heal);
    }
}

// One card's effect. Cards.cpp handles the deck; this only changes the player's numbers.
void Player::ApplyCard(CardId id)
{
    switch (id)
    {
    case CardId::HeavyArm:    m_Attack += 1.0f; break;
    case CardId::SharpPick:   m_MinePower += 2.0f; break;
    case CardId::ToughHide:   m_MaxHp += 6; m_Hp = std::min(m_MaxHp, m_Hp + 6); break;
    case CardId::FleetFoot:   m_Speed *= 1.12f; break;
    case CardId::QuickHands:  m_AttackSpeed += 0.15f; break;
    case CardId::BrightLamp:  m_LampBonus += 1.8f; break;     // the prototype's +1.8 lamp reach per copy
    case CardId::PotionPouch: AddToBagOrDrop(MatId::Potion, 2); break;     // overflow drops at your feet
    case CardId::SecondWind:  m_Hp = m_MaxHp; break;
    case CardId::IronSkin:    m_Armor += 1; break;
    case CardId::Leech:       m_Leech += 2; break;
    case CardId::KeenEdge:    m_Crit += 0.15f; break;
    case CardId::Thorns:      m_Thorns += 4; break;
    case CardId::BroadBlade:  m_Area += 0.2f; break;
    case CardId::Focus:       m_CooldownScale *= 0.8f; break;
    case CardId::Prospector:  m_Luck += 0.12f; break;
    case CardId::BigBomb:     m_Blast *= 1.4f; break;
    case CardId::TwinStrike:  m_Twin += 0.3f; break;
    case CardId::Aftershock:  m_Aftershock += 8.0f; break;
    case CardId::Executioner: m_Executioner = true; break;
    case CardId::Motherlode:
    {
        m_Motherlode = true;
        TileMap* map = Manager::GetGameObj<TileMap>();
        if (map)
            map->SetMotherlode(true);   // takes effect when the next floor is generated
        break;
    }
    case CardId::SwordWave:   m_SwordWave = true; break;
    case CardId::ChainBlast:  m_ChainBlast = true; break;
    case CardId::SplitBolt:   m_SplitBolt = true; break;
    case CardId::VenomBlade:  m_Venom = true; break;
    default: break;
    }
}

// Q: drink a potion, healing 40% of max HP (rounded up, never above max).
void Player::DrinkPotion()
{
    if (IsDead() || GetPotions() <= 0 || m_Hp >= m_MaxHp)
        return;

    const int heal = std::min(m_MaxHp - m_Hp, (int)ceilf(m_MaxHp * 0.4f));
    m_Hp += heal;
    m_Bag.Take(MatId::Potion, 1);
    DebugUI::Log("You drink a potion. +%d HP (%d left)", heal, GetPotions());
    Juice::Heal(m_Position.x, m_Position.z, heal);
}

// C1: whatever does not fit in the bag lands at the player's feet.
void Player::AddToBagOrDrop(MatId id, int n)
{
    const int left = n - m_Bag.Add(id, n);
    if (left <= 0)
        return;
    Pickups* pickups = Manager::GetGameObj<Pickups>();
    if (pickups)
        pickups->DropFromPlayer(id, left, m_Position.x, m_Position.z);
}

void Player::DropBagSlot(int slot)
{
    MatId id;
    int n = 0;
    if (!m_Bag.RemoveSlot(slot, id, n))
        return;
    Pickups* pickups = Manager::GetGameObj<Pickups>();
    if (pickups)
        pickups->DropFromPlayer(id, n, m_Position.x, m_Position.z);
}
