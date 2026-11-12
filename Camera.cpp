#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "Camera.h"
#include "Player.h"
#include "input.h"

void Camera::Init()
{
    m_Layer = 0;
	m_Position = { 0.0f, 1.0f, -5.0f };
}
void Camera::Uninit()
{

}
void  Camera::Update()
{
    Player* player = Manager::GetGameObj<Player>();
    Vector3 playerPos = player->GetPosition();

    float dt = 1.0 / 60.0f;

    if (Input::GetKeyPress(VK_RIGHT))
        m_Rotation.y += 3.0f * dt;
    if (Input::GetKeyPress(VK_LEFT))
        m_Rotation.y -= 3.0f * dt;

    float t = 0.1f;
    m_Target = m_Target * (1.0f - t) + (playerPos + Vector3(0.0f, 2.0f, 0.0f)) * t;

    m_Target += m_Shake * cosf(m_ShakeTime * 100.0f);
    m_ShakeTime += dt;
    m_Shake *= 0.9;

    m_Position = m_Target + Vector3(-sinf(m_Rotation.y) * 5.0f,
        2.0f,
        -cosf(m_Rotation.y) * 5.0f);
}
void  Camera::Draw()
{
    // プロジェクションマトリクス
    m_projection = XMMatrixPerspectiveFovLH(1.0f,
        (float)SCREEN_WIDTH / SCREEN_HEIGHT, 1.0f, 1000.0f);

    Renderer::SetProjectionMatrix(m_projection);

    // ビューマトリクス
    XMFLOAT3 up = XMFLOAT3(0.0f, 1.0f, 0.0f);
    m_ViewMatrix = XMMatrixLookAtLH(XMLoadFloat3((XMFLOAT3*)&m_Position),
        XMLoadFloat3((XMFLOAT3*)&m_Target),
        XMLoadFloat3(&up));

    Renderer::SetViewMatrix(m_ViewMatrix);
    Renderer::SetCameraPosition(m_Position);

}

bool Camera::CheckView(Vector3 Position, float Size)
{
    XMMATRIX vp;
    vp = m_ViewMatrix * m_projection;

    XMMATRIX invVp;
    invVp = XMMatrixInverse(nullptr, vp);

    XMFLOAT3 vpos[4];
    vpos[0] = XMFLOAT3(-1.0f, 1.0f, 1.0f);
    vpos[1] = XMFLOAT3(1.0f, 1.0f, 1.0f);
    vpos[2] = XMFLOAT3(-1.0f, -1.0f, 1.0f);
    vpos[3] = XMFLOAT3(1.0f, -1.0f, 1.0f);

    XMVECTOR vposv[4];
    vposv[0] = XMLoadFloat3(&vpos[0]);
    vposv[1] = XMLoadFloat3(&vpos[1]);
    vposv[2] = XMLoadFloat3(&vpos[2]);
    vposv[3] = XMLoadFloat3(&vpos[3]);

    XMVECTOR wposv[4];
    wposv[0] = XMVector3TransformCoord(vposv[0], invVp);
    wposv[1] = XMVector3TransformCoord(vposv[1], invVp);
    wposv[2] = XMVector3TransformCoord(vposv[2], invVp);
    wposv[3] = XMVector3TransformCoord(vposv[3], invVp);

    Vector3 wp[4];
    XMStoreFloat3((XMFLOAT3*)&wp[0], wposv[0]);
    XMStoreFloat3((XMFLOAT3*)&wp[1], wposv[1]);
    XMStoreFloat3((XMFLOAT3*)&wp[2], wposv[2]);
    XMStoreFloat3((XMFLOAT3*)&wp[3], wposv[3]);

    Vector3 v;
    v = Position - m_Position;
    //front
    {
        Vector3 v1, v2;
        v1 = wp[0] - m_Position;
        v2 = wp[2] - m_Position;
        Vector3 n;
        n = Vector3::cross(v1, v2);
        n.normalize();

        float d;
        d = Vector3::dot(n, v);

        if (d < -Size)
            return false;
    }
    //Right
    {
        Vector3 v1, v2;
        v1 = wp[3] - m_Position;
        v2 = wp[1] - m_Position;
        Vector3 n;
        n = Vector3::cross(v1, v2);
        n.normalize();

        float d;
        d = Vector3::dot(n, v);

        if (d < -Size)
            return false;
    }
    //Up
    {
        Vector3 v1, v2;
        v1 = wp[1] - m_Position;
        v2 = wp[0] - m_Position;
        Vector3 n;
        n = Vector3::cross(v1, v2);
        n.normalize();

        float d;
        d = Vector3::dot(n, v);

        if (d < -Size)
            return false;
    }
    //Down
    {
        Vector3 v1, v2;
        v1 = wp[2] - m_Position;
        v2 = wp[3] - m_Position;
        Vector3 n;
        n = Vector3::cross(v1, v2);
        n.normalize();

        float d;
        d = Vector3::dot(n, v);

        if (d < -Size)
            return false;
    }
    return true;
}