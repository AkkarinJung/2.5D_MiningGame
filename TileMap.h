#pragma once
// TileMap: one dungeon floor as a 48x34 grid of tiles.
// Generates the layout (walls, pillars, broken wall runs, stairs) from a seed,
// draws the floor and walls, and answers "is this tile solid?" for collision.
#include "GameObject.h"
#include <vector>
#include <cstdint>

enum class Tile : uint8_t { Floor, Wall, Block, Stairs };

class TileMap : public GameObject
{
public:
    static const int W = 48;
    static const int H = 34;

private:
    std::vector<Tile> m_Tiles;          // W * H, index = z * W + x
    std::vector<int>  m_Dist;           // steps from the start tile (-1 = unreachable)
    std::vector<int>  m_VisibleWalls;   // walls that touch open floor (the only ones drawn)

    uint32_t m_Seed = 0;
    int m_FloorNo = 1;
    int m_StartX = 0, m_StartZ = 0;
    int m_StairsX = 0, m_StairsZ = 0;
    int m_DrawnWalls = 0;

    ID3D11Buffer* m_CubeVB = nullptr;
    ID3D11Buffer* m_FloorVB = nullptr;
    ID3D11Buffer* m_QuadVB = nullptr;
    UINT m_CubeVertexCount = 0;

    ID3D11InputLayout* m_VertexLayout = nullptr;
    ID3D11VertexShader* m_VertexShader = nullptr;
    ID3D11PixelShader* m_PixelShader = nullptr;
    ID3D11ShaderResourceView* m_WallTexture = nullptr;
    ID3D11ShaderResourceView* m_FloorTexture = nullptr;

    void Set(int x, int z, Tile t);
    bool BoxHits(float x, float z, float half) const;

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;

    void Generate(uint32_t seed, int floorNo);

    Tile Get(int x, int z) const;       // outside the map counts as Wall
    bool Blocks(int x, int z) const;    // Wall or Block
    bool IsStairs(const Vector3& pos) const;

    // Move on the floor plane one axis at a time so bodies slide along walls.
    void MoveAndSlide(Vector3& pos, Vector3& vel, float dt, float half) const;

    Vector3 GetStartPosition() const { return Vector3(m_StartX + 0.5f, 0.0f, m_StartZ + 0.5f); }
    uint32_t GetSeed() const { return m_Seed; }
    int GetFloor() const { return m_FloorNo; }
    int GetDrawnWalls() const { return m_DrawnWalls; }
};

