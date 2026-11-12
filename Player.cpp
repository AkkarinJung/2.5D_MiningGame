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

#include "Enemy.h"
#include "Tree.h"
#include "Box.h"
#include "Shadow.h"

#include "TileMap.h"
#include "OreData.h"
#include "Score.h"

void Player::Init()
{
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
    if (map)
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
        m_Velocity += input * (20.0f * dt);
        m_Facing = input;
    }

    if (m_State == STATE_GROUND)
        SetAnimation(move ? "Run" : "Idle");

    // Hold J or Space to swing; the cooldown sets the pace
    if ((Input::GetKeyPress('J') || Input::GetKeyPress(VK_SPACE)) && m_AttackCooldown <= 0.0f)
    {
        m_State = STATE_ATTACK;
        m_StateTime = 0.0f;
        m_AttackCooldown = 0.4f;
        Swing();
    }
}

void Player::UpdateAttack()
{
    const float dt = 1.0f / 60.0f;

    UpdateGround();             // you can still walk while swinging
    SetAnimation("Attack");

    m_StateTime += dt;
    if (m_StateTime >= 0.4f)
        m_State = STATE_GROUND;
}

void Player::Draw()
{
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

// One pickaxe swing: find the blocks in the arc in front of the player and mine them.
void Player::Swing()
{
    TileMap* map = Manager::GetGameObj<TileMap>();
    if (!map)
        return;

    const float facing = atan2f(m_Facing.z, m_Facing.x);
    int hits[16];
    const int count = map->BlocksInArc(m_Position.x, m_Position.z, facing,
        1.15f, 1.3f,      // Miner reach and arc (radians)
        hits, 16);

    for (int k = 0; k < count; k++)
    {
        const int x = hits[k] % TileMap::W;
        const int z = hits[k] / TileMap::W;
        const MineResult result = map->MineBlock(x, z, m_MinePower, m_MineLevel);

        if (result.tooHard)
        {
            char text[96];
            sprintf_s(text, "Too hard! %s needs mining Lv%d\n",
                kOre[result.grade].name, kOre[result.grade].requiredLevel);
            OutputDebugStringA(text);
        }
        if (result.broken)
        {
            char text[96];
            sprintf_s(text, "You dug out %s. (%d blocks left)\n",
                kOre[result.grade].name, map->GetBlocksLeft());
            OutputDebugStringA(text);
            GainMiningXp((float)kOre[result.grade].xp);
        }
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

        char text[64];
        sprintf_s(text, "Mining Lv%d! (power %.0f)\n", m_MineLevel, m_MinePower);
        OutputDebugStringA(text);
        // M5: queue a card pick here, tagged with this ore grade
    }

    Score* level = Manager::GetGameObj<Score>();
    if (level)
        level->SetScore(m_MineLevel);
}



