#pragma once
// Pickups (C1): items lying on the floor. A fixed pool of 128 (no allocation per frame).
// A new pickup pops up and bounces, lands, and is pulled toward the player once close;
// touching it puts it in the bag. With a full bag it stays put and retries a little later.
// Drawn as 8x8 pixel-art billboards like the monsters, hidden by fog of war.

#include "GameObject.h"
#include "renderer.h"
#include "MaterialData.h"
#include <array>
#include <random>

class Pickups : public GameObject
{
public:
    struct Settings
    {
        float magnetRadius = 1.7f;          // pulled toward the player within this distance
        float collectRadius = 0.45f;        // picked up within this distance
        float retryDelay = 1.2f;            // full bag: wait this long before trying again
        float bagFullMessageGap = 1.5f;     // "Bag full" at most this often
    };

private:
    struct Item
    {
        bool  active = false;
        MatId id = MatId::Stone;
        int   count = 0;
        float x = 0.0f, y = 0.0f, z = 0.0f;
        float vx = 0.0f, vy = 0.0f, vz = 0.0f;
        bool  onGround = false;
        float retry = 0.0f;                 // seconds until it may be picked up again
        bool  waitAway = false;             // dropped by the player: not until they step away
        float dropX = 0.0f, dropZ = 0.0f;
    };

    static const int kPoolSize = 128;
    std::array<Item, kPoolSize> m_Items;
    std::mt19937 m_Rng;
    float m_Time = 0.0f;
    float m_BagFullTimer = 0.0f;

    static const int kSpriteCount = 6;      // chunk, potion, gel, fang, bomb, torch
    ID3D11Buffer* m_QuadVB[kSpriteCount] = {};
    ID3D11InputLayout*  m_Layout = nullptr;
    ID3D11VertexShader* m_VS = nullptr;
    ID3D11PixelShader*  m_PS = nullptr;
    ID3D11PixelShader*  m_ShadowPS = nullptr;
    ID3D11ShaderResourceView* m_Atlas = nullptr;
    ID3D11SamplerState* m_PointSampler = nullptr;

    Item* NewItem();                        // a free pool slot, or nullptr when full
    void  Collect(Item& item);

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;
    void DrawShadow() override;

    // Pops up from (x, z) in a random direction and bounces (ore breaks, monster drops)
    void Spawn(MatId id, int count, float x, float z);
    // Lies still on the floor (floor loot)
    void Place(MatId id, int count, float x, float z);
    // Dropped by the player: not picked up again until they move 1.1 tiles away
    void DropFromPlayer(MatId id, int count, float x, float z);

    void Clear();                           // new floor
    int  Count() const;                     // active pickups
    int  CountOf(MatId id) const;           // pieces of one material lying around

    static Settings& GetSettings();
};
