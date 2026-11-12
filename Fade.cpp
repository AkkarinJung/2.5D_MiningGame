#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "Fade.h"
#include "Scene.h"

void Fade::Init(bool FadeOut, Scene* NextScence)
{
    m_Layer = 4;

    m_FadeOut = FadeOut;
    m_FadeTime = 0.0f;
    m_NextScence = NextScence;

    float x = 0.0f;
    float y = 0.0f;
    float Width = SCREEN_WIDTH;
    float Height = SCREEN_HEIGHT;

    VERTEX_3D vertex[4];

    vertex[0].Position = XMFLOAT3(x, y, 0.0f);
    vertex[0].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
    vertex[0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);

    vertex[1].Position = XMFLOAT3(x + Width, y, 0.0f);
    vertex[1].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
    vertex[1].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);

    vertex[2].Position = XMFLOAT3(x, y + Height, 0.0f);
    vertex[2].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
    vertex[2].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[2].TexCoord = XMFLOAT2(0.0f, 1.0f);

    vertex[3].Position = XMFLOAT3(x + Width, y + Height, 0.0f);
    vertex[3].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
    vertex[3].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    vertex[3].TexCoord = XMFLOAT2(1.0f, 1.0f);

    // 頂点バッファ生成
    D3D11_BUFFER_DESC bd{};
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = sizeof(VERTEX_3D) * 4;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = vertex;

    Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer);

    // シェーダー読込
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout,
        "shader\\unlitTextureVS.cso");

    Renderer::CreatePixelShader(&m_PixelShader,
        "shader\\unlitTexturePS.cso");

}

void Fade::Uninit()
{
    m_VertexBuffer->Release();

    m_VertexLayout->Release();
    m_VertexShader->Release();
    m_PixelShader->Release();
}

void Fade::Update()
{
    float dt = 1.0f / 60.0f;
    m_FadeTime += dt;
    if (m_FadeTime > 0.5f)
    {
        if (m_FadeOut)
        {
            Manager::ChangeScene(m_NextScence);
        }
        SetDestory();
    }
}

void Fade::Draw()
{
    // 入力レイアウト設定
    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

    // シェーダ設定
    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    // マトリクス設定
    Renderer::SetWorldViewProjection2D();

    // マトリクス設定
    XMMATRIX world, scale, rot, trans;
    scale = XMMatrixScaling(1.0f, 1.0f, 1.0f);
    rot = XMMatrixRotationRollPitchYaw(0.0f, 0.0f, 0.0f);
    trans = XMMatrixTranslation(0.0f, 0.0f, 0.0f);
    world = scale * rot * trans;

    Renderer::SetWorldMatrix(world);

    //fade alpha
    float alpha;
    if (m_FadeOut)
    {
        alpha = m_FadeTime / 0.5f;
    }
    else
    {
        alpha = 1.0f - m_FadeTime / 0.5f;
    }

    // マテリアル設定
    MATERIAL material{};
    material.Diffuse = { 0.0f, 0.0f, 0.0f, alpha };
    material.TextureEnable = false;
    Renderer::SetMaterial(material);


    // 頂点バッファ設定
    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);

    // プリミティブトポロジ設定
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    // ポリゴン描画
    Renderer::GetDeviceContext()->Draw(4, 0);
}

