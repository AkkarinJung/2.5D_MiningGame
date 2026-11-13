#pragma once
// Monsters: every monster on the floor as plain structs in one vector.
// Sleep until they see the player, then chase along a shared distance map
// (breadth-first search from the player's tile) so they find their way around walls.
// Also owns the potions lying on the floor, since they share the sprite renderer.

#include "GameObject.h"
#include "renderer.h"
#include "MonsterData.h"
#include <vector>
#include <random>

class TileMap;

struct Monster
{
    MonsterType type = MonsterType::Slime;
    bool  awake = false;
    float x = 0.0f, z = 0.0f;
    float velX = 0.0f, velZ = 0.0f;     // hop / wander velocity
    float knockX = 0.0f, knockZ = 0.0f;
    float hp = 1.0f, maxHp = 1.0f;
    int   attack = 1;
    float flash = 0.0f;                 // > 0: drawn white (just got hit)
    float timer = 0.0f;                 // slime hop timer
    float phase = 0.0f;                 // bat zigzag phase
    uint8_t state = 0;                  // brute: BruteWalk / BruteWindUp / BruteCharge / BruteRest
    float stateTimer = 0.0f;
    float chargeAngle = 0.0f;
};

struct Potion
{
    float x = 0.0f, z = 0.0f;
};

class Monsters : public GameObject
{
private:
    std::vector<Monster> m_List;
    std::vector<Potion>  m_Potions;
    std::vector<int> m_Flow;            // steps to the player's tile (-1 = unreachable)
    std::mt19937 m_Rng;

    uint32_t m_Seed = 0;                // floor currently populated
    int m_FloorNo = 0;
    int m_FlowTile = -1;                // player tile the flow map was built from
    int m_FlowBlocks = -1;              // blocks left when it was built
    float m_SpawnTimer = 0.0f;
    float m_Time = 0.0f;
    bool  m_Frozen = false;

    static const int kSpriteCount = kMonsterTypeCount + 1;     // monsters + potion

    ID3D11Buffer* m_QuadVB[kSpriteCount] = {};
    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;
    ID3D11ShaderResourceView* m_Atlas = nullptr;
    ID3D11SamplerState* m_PointSampler = nullptr;

    void PopulateFloor(const TileMap& map);
    void BuildFlow(const TileMap& map, int playerTile);
    void UpdateOne(Monster& m, const TileMap& map, float px, float pz, float dt);
    void Steer(const Monster& m, const TileMap& map, float px, float pz, float& sx, float& sz) const;
    void UpdateBrute(Monster& m, const TileMap& map, float px, float pz, float sx, float sz,
        float dt, float& vx, float& vz);
    void RemoveDead();
    int  Flow(int x, int z) const;
    MonsterType RandomType();

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    void Spawn(MonsterType type, float x, float z);

    // Melee swing: damages every monster in the arc. Returns how many died.
    int HitInArc(float ox, float oz, float facing, float range, float arc, float damage);

    void DropPotion(float x, float z) { m_Potions.push_back({ x, z }); }

    void KillAll();
    int Count() const { return (int)m_List.size(); }
    int AwakeCount() const;
    int PotionsOnFloor() const { return (int)m_Potions.size(); }
    bool& Frozen() { return m_Frozen; }
};

