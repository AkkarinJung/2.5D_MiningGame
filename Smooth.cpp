#include "main.h"
#include "renderer.h"
#include "Smooth.h"
#include "Camera.h"
#include "manager.h"
#include "modelRenderer.h"


void Smooth::Init()
{
    m_Layer = 3;


    ModelRenderer* m_ModelRenderer = AddGameComponent<ModelRenderer>(this);
    m_ModelRenderer->Load("asset\\model\\sphere_smooth.obj");

    // シェーダー読込
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout,
        "shader\\glassVS.cso");

    Renderer::CreatePixelShader(&m_PixelShader,
        "shader\\glassPS.cso");

}
void Smooth::Uninit()
{
    m_VertexLayout->Release();
    m_VertexShader->Release();
    m_PixelShader->Release();

    GameObject::Uninit();
}
void  Smooth::Update()
{

    GameObject::Update();
}
void  Smooth::Draw()
{
    // 入力レイアウト設定
    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

    // シェーダ設定
    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);


    GameObject::Draw();
}
