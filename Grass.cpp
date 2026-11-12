#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "Grass.h"
#include "Camera.h"


void Grass::Init()
{
    m_Layer = 3;

    {
        VERTEX_3D vertex[4];

        vertex[0].Position = XMFLOAT3(-1.0f, 2.0f, 0.0f);
        vertex[0].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
        vertex[0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);

        vertex[1].Position = XMFLOAT3(1.0f, 2.0f, 0.0f);
        vertex[1].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
        vertex[1].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        vertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);

        vertex[2].Position = XMFLOAT3(-1.0f, 0.0f, 0.0f);
        vertex[2].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
        vertex[2].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
        vertex[2].TexCoord = XMFLOAT2(0.0f, 1.0f);

        vertex[3].Position = XMFLOAT3(1.0f, 0.0f, 0.0f);
        vertex[3].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
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
    }
    

    {
        m_InstanceNum = 3000;
        Vector3* pos = new Vector3[m_InstanceNum];
        for (unsigned int i = 0; i < m_InstanceNum; i++)
        {
            pos[i].x = (float)rand() / RAND_MAX * 100.0f - 50.0f;
            pos[i].z = (float)rand() / RAND_MAX * 100.0f - 50.0f;
            pos[i].y = 0.0f;
        }

        D3D11_BUFFER_DESC bd{};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(Vector3) * m_InstanceNum;
        bd.StructureByteStride = sizeof(Vector3);
        bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        bd.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA sd{};
        sd.pSysMem = pos;
        Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_PositionBuffer);

        delete[] pos;

        D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
        srvd.Format = DXGI_FORMAT_UNKNOWN;
        srvd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srvd.Buffer.FirstElement = 0;
        srvd.Buffer.NumElements = m_InstanceNum;

        Renderer::GetDevice()->CreateShaderResourceView(m_PositionBuffer, &srvd, &m_PositionSRV);
    }
    // シェーダー読込
    Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout,
        "shader\\GrassVS.cso");

    Renderer::CreatePixelShader(&m_PixelShader,
        "shader\\GrassPS.cso");

    TexMetadata metadata;
    ScratchImage image;
    LoadFromWICFile(L"asset\\bill_board\\grass.png", WIC_FLAGS_NONE, &metadata, image);
    CreateShaderResourceView(Renderer::GetDevice(), image.GetImages(),
        image.GetImageCount(), metadata, &m_Texture);
    assert(m_Texture);
}
void Grass::Uninit()
{
    m_VertexBuffer->Release();

    m_VertexLayout->Release();
    m_VertexShader->Release();
    m_PixelShader->Release();
}
void  Grass::Update()
{

}
void  Grass::Draw()
{
    Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

    Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
    Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

    Camera* camera = Manager::GetGameObj<Camera>();
    XMMATRIX view = camera->GetViewMatrix();
    XMMATRIX invView = XMMatrixInverse(NULL, view);
    invView.r[3].m128_f32[0] = 0.0f;
    invView.r[3].m128_f32[1] = 0.0f;
    invView.r[3].m128_f32[2] = 0.0f;

    XMMATRIX world, scale, trans;
    scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
    //rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
    trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
    world = scale * invView *trans;

    Renderer::SetWorldMatrix(world);

    MATERIAL material{};
    material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
    material.Ambient = { 1.0f, 1.0f, 1.0f, 1.0f };
    material.TextureEnable = true;
    Renderer::SetMaterial(material);

    Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_Texture);

    UINT stride = sizeof(VERTEX_3D);
    UINT offset = 0;
    Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);
    Renderer::GetDeviceContext()->VSSetShaderResources(2, 1, &m_PositionSRV);
    Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

    //Renderer::GetDeviceContext()->Draw(4, 0);
    Renderer::GetDeviceContext()->DrawInstanced(4, m_InstanceNum, 0, 0);

    GameObject::Draw();
}
