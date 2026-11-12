#pragma once
// TileMap: one dungeon floor as a 48x34 grid of tiles.
// Generates the layout (walls, pillars, broken wall runs, stairs) from a seed,
// draws the floor and walls, and answers "is this tile solid?" for collision.

#include "GameObject.h"
#include "renderer.h"   // VERTEX_3D
#include <vector>
#include <cstdint>

enum class Tile : uint8_t { Floor, Wall, Block, Stairs };

// What one mining hit did to a block.
struct MineResult
{
    bool hit = false;       // there was a block on that tile
    bool tooHard = false;   // mining level below the ore's requirement (25% damage)
    bool broken = false;    // the block broke this hit
    int  grade = 0;         // ore grade index into kOre
};

class TileMap : public GameObject
{
public:
    static const int W = 48;
    static const int H = 34;

private:
    std::vector<Tile> m_Tiles;          // W * H, index = z * W + x
    std::vector<int>  m_Dist;           // steps from the start tile (-1 = unreachable)
    std::vector<int>  m_VisibleWalls;   // walls that touch open floor (the only ones drawn)
    std::vector<uint8_t> m_OreGrade;    // valid where the tile is Block
    std::vector<float>   m_BlockHp;
    std::vector<float>   m_BlockDamage;
    int m_BlocksLeft = 0;

    uint32_t m_Seed = 0;
    int m_FloorNo = 1;
    int m_StartX = 0, m_StartZ = 0;
    int m_StairsX = 0, m_StairsZ = 0;
    int m_DrawnWalls = 0;

    ID3D11Buffer* m_CubeVB = nullptr;
    ID3D11Buffer* m_WallVB = nullptr;           // every visible wall baked into one mesh
    UINT m_WallVertexCount = 0;
    std::vector<VERTEX_3D> m_CubeVertices;      // CPU copy of the unit cube, used for baking
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
    void PlaceOre(uint32_t seed, int floorNo);
    void BuildWallMesh();

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

    // Tiles hit by a melee swing: rays fanned across the arc, each stopping at the
    // first wall or block. Writes up to maxOut tile indices (z * W + x), returns the count.
    int BlocksInArc(float ox, float oz, float facing, float range, float arc, int* out, int maxOut) const;

    // Apply one mining hit to the block on tile (x, z).
    MineResult MineBlock(int x, int z, float power, int mineLevel);

    Vector3 GetStartPosition() const { return Vector3(m_StartX + 0.5f, 0.0f, m_StartZ + 0.5f); }
    uint32_t GetSeed() const { return m_Seed; }
    int GetFloor() const { return m_FloorNo; }
    int GetDrawnWalls() const { return m_DrawnWalls; }
    int GetBlocksLeft() const { return m_BlocksLeft; }
};
