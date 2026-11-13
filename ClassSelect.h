#pragma once
// ClassSelect: the scene between Title and Game where the player picks a class.
// Drawn with Dear ImGui for now; the pixel-font game UI replaces it in M6.
#include "Scene.h"

class ClassSelect : public Scene
{
private:
    bool m_Chosen = false;

    void Choose(int index);

public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;
};

