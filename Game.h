#pragma once
#include "Scene.h"
class Game : public Scene
{
private:
    int m_RecordedFloor = 0;    // last floor sent to the save records

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};

