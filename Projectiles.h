#pragma once
// Projectiles: things the player's attacks and skills put into the world -
// Mage bolts, Miner dynamite, and the expanding rings that show explosions,
// Frost Nova and Whirlwind. Plain structs in vectors, removed with swap-and-pop.

#include "GameObject.h"
#include "renderer.h"
#include <vector>
#include <cstdint>

struct Bolt
{
    float x, z, vx, vz;
    float life;         // seconds left
    float damage, mine;
};

struct Bomb
{
    float startX, startZ, targetX, targetZ;
    float x, z, y;
    float t, duration;
    float radius, damage, mine;
};

struct Ring
{
    float x, z, radius;
    float t, life;
    float r, g, b;
};

class Projectiles : public GameObject
{
private:
    std::vector<Bolt> m_Bolts;
    std::vector<Bomb> m_Bombs;
    std::vector<Ring> m_Rings;
    float m_Time = 0.0f;

    ID3D11Buffer* m_BoltVB = nullptr;
    ID3D11Buffer* m_BombVB = nullptr;
    ID3D11Buffer* m_RingVB = nullptr;           // flat quad on the floor, centred
    ID3D11ShaderResourceView* m_Atlas = nullptr;
    ID3D11ShaderResourceView* m_RingTexture = nullptr;
    ID3D11SamplerState* m_PointSampler = nullptr;
    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;

    void Explode(float x, float z, float radius, float damage, float mine);
public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    // Mage bolt: 11 tiles/s for 1.1 s, mines the first block, hurts the first monster.
    void FireBolt(float x, float z, float angle, float damage, float mine);
    // Miner dynamite: lands on the target after 0.65 s (stops short at walls) and explodes.
    void ThrowBomb(float x, float z, float targetX, float targetZ, float radius, float damage, float mine);
    // Visual only: a ring that grows to radius and fades out.
    void AddRing(float x, float z, float radius, uint32_t rgb, float life = 0.4f);

    int Count() const { return (int)(m_Bolts.size() + m_Bombs.size()); }
};

