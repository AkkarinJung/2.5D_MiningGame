// ShadowMap.cpp
// R32_TYPELESS texture bound as a D32_FLOAT depth target in the shadow pass and as an
// R32_FLOAT shader resource in the main pass; a comparison sampler for PCF; a rasterizer
// state with slope-scaled depth bias and no culling (sprites are double-sided quads).

#include "main.h"
#include "renderer.h"
#include "ShadowMap.h"
#include "Lighting.h"
#include <cmath>

namespace
{
    const int kSize = 2048;

    // Matches cbuffer ShadowBuffer in pointLitPS.hlsl
    struct ShadowBuffer
    {
        XMFLOAT4X4 lightViewProj;           // transposed, like the renderer's other matrices
        XMFLOAT4 moonDirIntensity;          // xyz = direction the light travels, w = intensity
        XMFLOAT4 moonColor;                 // rgb, w = ambient kept in shadow
        XMFLOAT4 params;                    // x = texel size, y = bias, z = PCF on, w = shadows on
        XMFLOAT4 lampPosFar;                // xyz = lamp position, w = far plane (lamp radius)
        XMFLOAT4 lampParams;                // x = lamp shadows on, y = bias, z = 2 / face size, w = near plane
    };

    const float kLampNear = 0.05f;

    ShadowMap::Settings g_Settings;

    ID3D11Texture2D* g_Texture = nullptr;
    ID3D11DepthStencilView* g_DSV = nullptr;
    ID3D11ShaderResourceView* g_SRV = nullptr;
    ID3D11SamplerState* g_Sampler = nullptr;
    ID3D11RasterizerState* g_Raster = nullptr;
    ID3D11Buffer* g_Buffer = nullptr;
    float g_RasterSlope = -1.0f;            // slope bias the current rasterizer state was made with

    // Saved screen state, restored by EndPass
    ID3D11RenderTargetView* g_SavedRTV = nullptr;
    ID3D11DepthStencilView* g_SavedDSV = nullptr;
    ID3D11RasterizerState* g_SavedRaster = nullptr;
    D3D11_VIEWPORT g_SavedViewport{};

    // Lamp cube map
    ID3D11Texture2D*          g_LampTexture = nullptr;
    ID3D11DepthStencilView*   g_LampDSV[6] = {};
    ID3D11ShaderResourceView* g_LampSRV = nullptr;
    int   g_LampSize = 0;                   // face size the cube was created with
    bool  g_LampActive = false;             // lamp shadows are being rendered this frame
    bool  g_InLampPass = false;
    float g_Lamp[4] = {};                   // x, y, z, radius of this frame's lamp

    void Release(IUnknown* p);

    void CreateLampCube(int size)
    {
        for (ID3D11DepthStencilView*& dsv : g_LampDSV)
            if (dsv) { dsv->Release(); dsv = nullptr; }
        if (g_LampSRV)     { g_LampSRV->Release();     g_LampSRV = nullptr; }
        if (g_LampTexture) { g_LampTexture->Release(); g_LampTexture = nullptr; }

        ID3D11Device* device = Renderer::GetDevice();
        D3D11_TEXTURE2D_DESC td{};
        td.Width = td.Height = size;
        td.MipLevels = 1;
        td.ArraySize = 6;                               // one slice per cube face
        td.Format = DXGI_FORMAT_R32_TYPELESS;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        td.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
        device->CreateTexture2D(&td, nullptr, &g_LampTexture);
        assert(g_LampTexture);

        for (int face = 0; face < 6; face++)
        {
            D3D11_DEPTH_STENCIL_VIEW_DESC dd{};
            dd.Format = DXGI_FORMAT_D32_FLOAT;
            dd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
            dd.Texture2DArray.FirstArraySlice = face;
            dd.Texture2DArray.ArraySize = 1;
            device->CreateDepthStencilView(g_LampTexture, &dd, &g_LampDSV[face]);
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = DXGI_FORMAT_R32_FLOAT;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
        sd.TextureCube.MipLevels = 1;
        device->CreateShaderResourceView(g_LampTexture, &sd, &g_LampSRV);
        g_LampSize = size;
    }

    void CreateRasterizer()
    {
        if (g_Raster) { g_Raster->Release(); g_Raster = nullptr; }
        D3D11_RASTERIZER_DESC rd{};
        rd.FillMode = D3D11_FILL_SOLID;
        rd.CullMode = D3D11_CULL_NONE;
        rd.DepthClipEnable = TRUE;
        rd.DepthBias = 0;
        rd.SlopeScaledDepthBias = g_Settings.slopeBias;
        Renderer::GetDevice()->CreateRasterizerState(&rd, &g_Raster);
        g_RasterSlope = g_Settings.slopeBias;
    }

    void Release(IUnknown* p) { if (p) p->Release(); }
}

namespace ShadowMap
{
    void Init()
    {
        ID3D11Device* device = Renderer::GetDevice();

        D3D11_TEXTURE2D_DESC td{};
        td.Width = td.Height = kSize;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R32_TYPELESS;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
        device->CreateTexture2D(&td, nullptr, &g_Texture);
        assert(g_Texture);

        D3D11_DEPTH_STENCIL_VIEW_DESC dd{};
        dd.Format = DXGI_FORMAT_D32_FLOAT;
        dd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        device->CreateDepthStencilView(g_Texture, &dd, &g_DSV);

        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = DXGI_FORMAT_R32_FLOAT;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        sd.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(g_Texture, &sd, &g_SRV);

        // Comparison sampler: returns how much of the 2x2 footprint is lit; outside the map = lit
        D3D11_SAMPLER_DESC smp{};
        smp.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
        smp.AddressU = smp.AddressV = smp.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
        smp.BorderColor[0] = smp.BorderColor[1] = smp.BorderColor[2] = smp.BorderColor[3] = 1.0f;
        smp.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
        smp.MaxLOD = D3D11_FLOAT32_MAX;
        device->CreateSamplerState(&smp, &g_Sampler);

        D3D11_BUFFER_DESC bd{};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(ShadowBuffer);
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        device->CreateBuffer(&bd, nullptr, &g_Buffer);

        CreateRasterizer();
        CreateLampCube(g_Settings.lampFaceSize);

        // Start with "shadows off" in the buffer so the first frame is valid
        ShadowBuffer cb{};
        XMStoreFloat4x4(&cb.lightViewProj, XMMatrixIdentity());
        Renderer::GetDeviceContext()->UpdateSubresource(g_Buffer, 0, nullptr, &cb, 0, 0);

        ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
        ctx->PSSetConstantBuffers(7, 1, &g_Buffer);
        ctx->PSSetSamplers(1, 1, &g_Sampler);
    }

    void Uninit()
    {
        Release(g_Texture); g_Texture = nullptr;
        Release(g_DSV);     g_DSV = nullptr;
        Release(g_SRV);     g_SRV = nullptr;
        Release(g_Sampler); g_Sampler = nullptr;
        Release(g_Raster);  g_Raster = nullptr;
        Release(g_Buffer);  g_Buffer = nullptr;
        for (ID3D11DepthStencilView*& dsv : g_LampDSV)
        {
            Release(dsv);
            dsv = nullptr;
        }
        Release(g_LampSRV);     g_LampSRV = nullptr;
        Release(g_LampTexture); g_LampTexture = nullptr;
    }

    void BeginPass(float focusX, float focusZ)
    {
        ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
        if (g_Settings.slopeBias != g_RasterSlope)
            CreateRasterizer();                         // slider changed in the debug panel

        if (g_Settings.lampFaceSize != g_LampSize)
            CreateLampCube(g_Settings.lampFaceSize);    // size changed in the debug panel

        // The maps cannot be read and written at once: unbind them from the pixel shader first
        ID3D11ShaderResourceView* none[2] = { nullptr, nullptr };
        ctx->PSSetShaderResources(1, 2, none);          // t1 (moon) and t2 (lamp)

        // This frame's lamp (set by the player during Update)
        g_InLampPass = false;
        g_LampActive = g_Settings.lampShadows &&
                       Lighting::GetLamp(g_Lamp[0], g_Lamp[1], g_Lamp[2], g_Lamp[3]);

        // Remember the screen's targets, viewport and rasterizer
        ctx->OMGetRenderTargets(1, &g_SavedRTV, &g_SavedDSV);
        UINT count = 1;
        ctx->RSGetViewports(&count, &g_SavedViewport);
        ctx->RSGetState(&g_SavedRaster);

        // Depth only: no colour target
        ctx->OMSetRenderTargets(0, nullptr, g_DSV);
        ctx->ClearDepthStencilView(g_DSV, D3D11_CLEAR_DEPTH, 1.0f, 0);
        D3D11_VIEWPORT vp{};
        vp.Width = vp.Height = (float)kSize;
        vp.MaxDepth = 1.0f;
        ctx->RSSetViewports(1, &vp);
        ctx->RSSetState(g_Raster);

        // Moon camera: orthographic box around the player, looking along the light direction
        XMVECTOR dir = XMVector3Normalize(XMVectorSet(g_Settings.moonDir[0], g_Settings.moonDir[1], g_Settings.moonDir[2], 0.0f));
        XMVECTOR center = XMVectorSet(focusX, 0.0f, focusZ, 1.0f);
        XMVECTOR eye = XMVectorSubtract(center, XMVectorScale(dir, 40.0f));
        XMMATRIX view = XMMatrixLookAtLH(eye, center, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
        const float size = g_Settings.boxHalfSize * 2.0f;
        XMMATRIX proj = XMMatrixOrthographicLH(size, size, 1.0f, 80.0f);

        // Snap to whole texels: move the box by the sub-texel offset of the world origin
        XMVECTOR origin = XMVector3TransformCoord(XMVectorZero(), view * proj);
        const float half = kSize * 0.5f;
        const float sx = XMVectorGetX(origin) * half, sy = XMVectorGetY(origin) * half;
        proj = proj * XMMatrixTranslation((roundf(sx) - sx) / half, (roundf(sy) - sy) / half, 0.0f);

        Renderer::SetViewMatrix(view);              // the camera sets these back in the main pass
        Renderer::SetProjectionMatrix(proj);

        // Constants for the main pass
        ShadowBuffer cb{};
        XMStoreFloat4x4(&cb.lightViewProj, XMMatrixTranspose(view * proj));
        XMFLOAT3 d;
        XMStoreFloat3(&d, dir);
        cb.moonDirIntensity = XMFLOAT4(d.x, d.y, d.z, g_Settings.moonIntensity);
        cb.moonColor = XMFLOAT4(g_Settings.moonColor[0], g_Settings.moonColor[1], g_Settings.moonColor[2], g_Settings.shadowedAmbient);
        cb.params = XMFLOAT4(1.0f / kSize, g_Settings.bias, g_Settings.pcf ? 1.0f : 0.0f, g_Settings.enabled ? 1.0f : 0.0f);
        cb.lampPosFar = XMFLOAT4(g_Lamp[0], g_Lamp[1], g_Lamp[2], g_Lamp[3]);
        cb.lampParams = XMFLOAT4(g_LampActive ? 1.0f : 0.0f, g_Settings.lampBias, 2.0f / g_LampSize, kLampNear);
        ctx->UpdateSubresource(g_Buffer, 0, nullptr, &cb, 0, 0);
    }

    bool MoonShadowsActive()
    {
        return g_Settings.enabled && g_Settings.moonIntensity > 0.0f;
    }


    bool LampShadowsActive()
    {
        return g_LampActive;
    }

    // Cube face cameras, in Direct3D's face order (+X, -X, +Y, -Y, +Z, -Z):
    // each looks along one axis with a 90-degree field of view, so together they see everything.
    void BeginLampFace(int face)
    {
        static const float kLook[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
        static const float kUp[6][3]   = { { 0, 1, 0 }, {  0, 1, 0 }, { 0, 0, -1 }, { 0, 0, 1 }, { 0, 1, 0 }, { 0, 1,  0 } };

        ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
        g_InLampPass = true;

        ctx->OMSetRenderTargets(0, nullptr, g_LampDSV[face]);
        ctx->ClearDepthStencilView(g_LampDSV[face], D3D11_CLEAR_DEPTH, 1.0f, 0);
        D3D11_VIEWPORT vp{};
        vp.Width = vp.Height = (float)g_LampSize;
        vp.MaxDepth = 1.0f;
        ctx->RSSetViewports(1, &vp);

        const XMVECTOR eye = XMVectorSet(g_Lamp[0], g_Lamp[1], g_Lamp[2], 1.0f);
        const XMVECTOR look = XMVectorSet(kLook[face][0], kLook[face][1], kLook[face][2], 0.0f);
        const XMVECTOR up = XMVectorSet(kUp[face][0], kUp[face][1], kUp[face][2], 0.0f);
        Renderer::SetViewMatrix(XMMatrixLookAtLH(eye, XMVectorAdd(eye, look), up));
        Renderer::SetProjectionMatrix(XMMatrixPerspectiveFovLH(XM_PIDIV2, 1.0f, kLampNear, g_Lamp[3]));
    }

    bool IsLampPass()
    {
        return g_InLampPass;
    }

    void EndPass()
    {
        ID3D11DeviceContext* ctx = Renderer::GetDeviceContext();
        ctx->OMSetRenderTargets(1, &g_SavedRTV, g_SavedDSV);
        ctx->RSSetViewports(1, &g_SavedViewport);
        ctx->RSSetState(g_SavedRaster);
        // The Get calls above added references: release them
        Release(g_SavedRTV);    g_SavedRTV = nullptr;
        Release(g_SavedDSV);    g_SavedDSV = nullptr;
        Release(g_SavedRaster); g_SavedRaster = nullptr;

        ctx->PSSetShaderResources(1, 1, &g_SRV);        // t1: moon map for pointLitPS
        ctx->PSSetShaderResources(2, 1, &g_LampSRV);    // t2: lamp cube map
        ctx->PSSetSamplers(1, 1, &g_Sampler);           // s1
        g_InLampPass = false;
    }

    Settings& GetSettings() { return g_Settings; }
    ID3D11ShaderResourceView* GetSRV() { return g_SRV; }
}
