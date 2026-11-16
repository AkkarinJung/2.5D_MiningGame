#pragma once
// SpriteAtlas.h: small helpers for 8x8 pixel-art billboards. Builds one texture from
// SpriteDef rows, one quad per sprite (feet at y = 0, facing -Z), and a point sampler.
// Needs main.h and renderer.h included first.

#include "MonsterData.h"    // SpriteDef, PaletteEntry
#include <vector>

namespace SpriteAtlas
{
    // All sprites side by side in one RGBA texture (8 * count wide, 8 high).
    inline ID3D11ShaderResourceView* Build(const SpriteDef* const* sprites, int count)
    {
        const int w = 8 * count, h = 8;
        std::vector<uint32_t> pixels(w * h, 0);
        for (int k = 0; k < count; k++)
            for (int y = 0; y < 8; y++)
                for (int x = 0; x < 8; x++)
                {
                    const char key = sprites[k]->rows[y][x];
                    for (const PaletteEntry& p : sprites[k]->palette)
                        if (p.key != 0 && p.key == key)
                        {
                            const uint32_t r = (p.rgb >> 16) & 0xFF, g = (p.rgb >> 8) & 0xFF, b = p.rgb & 0xFF;
                            pixels[y * w + k * 8 + x] = 0xFF000000u | (b << 16) | (g << 8) | r;
                        }
                }

        D3D11_TEXTURE2D_DESC td{};
        td.Width = w;
        td.Height = h;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1;
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA init{};
        init.pSysMem = pixels.data();
        init.SysMemPitch = w * 4;

        ID3D11Texture2D* texture = nullptr;
        Renderer::GetDevice()->CreateTexture2D(&td, &init, &texture);
        assert(texture);
        ID3D11ShaderResourceView* srv = nullptr;
        Renderer::GetDevice()->CreateShaderResourceView(texture, nullptr, &srv);
        texture->Release();
        assert(srv);
        return srv;
    }

    // Quad for sprite k of count: x -0.5..0.5, y 0..1 (feet at 0), facing -Z.
    inline ID3D11Buffer* CreateQuad(int k, int count)
    {
        const float atlasW = 8.0f * count;
        const float u0 = (k * 8 + 0.01f) / atlasW, u1 = ((k + 1) * 8 - 0.01f) / atlasW;
        const XMFLOAT3 n(0, 0, -1);
        const XMFLOAT4 white(1, 1, 1, 1);
        VERTEX_3D v[4];
        v[0] = { XMFLOAT3(-0.5f, 1, 0), n, white, XMFLOAT2(u0, 0) };
        v[1] = { XMFLOAT3(0.5f, 1, 0), n, white, XMFLOAT2(u1, 0) };
        v[2] = { XMFLOAT3(-0.5f, 0, 0), n, white, XMFLOAT2(u0, 1) };
        v[3] = { XMFLOAT3(0.5f, 0, 0), n, white, XMFLOAT2(u1, 1) };

        D3D11_BUFFER_DESC bd{};
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(v);
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        D3D11_SUBRESOURCE_DATA vd{};
        vd.pSysMem = v;
        ID3D11Buffer* vb = nullptr;
        Renderer::GetDevice()->CreateBuffer(&bd, &vd, &vb);
        assert(vb);
        return vb;
    }

    // Point sampling keeps pixel art sharp.
    inline ID3D11SamplerState* CreatePointSampler()
    {
        D3D11_SAMPLER_DESC sd{};
        sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sd.MaxLOD = D3D11_FLOAT32_MAX;
        ID3D11SamplerState* sampler = nullptr;
        Renderer::GetDevice()->CreateSamplerState(&sd, &sampler);
        return sampler;
    }
}

