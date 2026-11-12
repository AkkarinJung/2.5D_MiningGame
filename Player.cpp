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

void Player::Init()
{
    m_Layer = 1;
    m_Position = { 0.0f, 0.0f, 0.0f };
    m_Scale = { 0.01f, 0.01f, 0.01f };


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
    m_Shadow->SetScale({ 1.5f ,1.5f ,1.5f });
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

    Vector3 rot = m_Child->GetRotation();
    rot.y += 1.0f * dt;
    m_Child->SetRotation(rot);

    bool oldGround = m_Ground;
    m_Ground = false;

    m_Rotation.y = atan2f(m_Velocity.x, m_Velocity.z);

    //return scale to original
    //m_Scale.x += (1.0f - m_Scale.x) * 0.1f;
    //m_Scale.y += (1.0f - m_Scale.y) * 0.1f;
    //m_Scale.z += (1.0f - m_Scale.z) * 0.1f;

    m_Velocity.y += -98.0f * dt;

    m_Velocity.x += -m_Velocity.x * 5.0f * dt;
    m_Velocity.z += -m_Velocity.z * 5.0f * dt;


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
    float dt = 1.0 / 60.0f;

    Camera* camera = Manager::GetGameObj<Camera>();
    Vector3 forward = camera->GetFoward();
    Vector3 right = camera->GetRight();

    forward.y = 0.0f;
    forward.normalize();

    right.y = 0.0f;
    right.normalize();

    bool move = false;

    if (Input::GetKeyPress('D'))
    {
        m_Velocity += right * (50.0f * dt);
        move = true;
    }
    if (Input::GetKeyPress('A'))
    {
        m_Velocity -= right * (50.0f * dt);
        move = true;
    }
    if (Input::GetKeyPress('W'))
    {
        m_Velocity += forward * (50.0f * dt);
        move = true;
    }
    if (Input::GetKeyPress('S'))
    {
        m_Velocity -= forward * (50.0f * dt);
        move = true;
    }

    if (move)
    {
        SetAnimation("Run");
    }
    else
    {
        SetAnimation("Idle");
    }

    if (Input::GetKeyTrigger(VK_SPACE))
    {
        m_Velocity.y += 20.0f;

        //m_Scale.y = 2.0f;
        //m_Scale.x = 0.5f;
        //m_Scale.z = 0.5f;

        m_JumpSE->Play();
    }

    if (Input::GetKeyTrigger('J'))
    {
        m_Hit = false;
        m_State = STATE_ATTACK;
        m_StateTime = 0.0f;
    }
}
void Player::UpdateAttack()
{
    float dt = 1.0 / 60.0f;

    if (!m_Hit && m_StateTime > 0.5f)
    {
        auto enemies = Manager::GetGameObjs<Enemy>();

        for (auto enemy : enemies)
        {
            Vector3 dir = enemy->GetPosition() - m_Position;
            float lenght = VectorMag(dir);
            if (lenght < 2.0f)
            {
                dir.normalize();

                enemy->AddDamage(1);
                enemy->Shake(dir * 1.0f);

                m_Hit = true;
                m_HitStop = true;

                break;
            }
        }
    }

    if (m_StateTime > 0.8f)
        m_HitStop = false;

    SetAnimation("Attack");
    m_StateTime += dt;
    if (m_StateTime > 1.0f)
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



