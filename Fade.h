#pragma once
#include "GameObject.h"
class Fade :public GameObject
{
private:
	ID3D11Buffer* m_VertexBuffer;
	ID3D11InputLayout* m_VertexLayout;
	ID3D11VertexShader* m_VertexShader;
	ID3D11PixelShader* m_PixelShader;

	bool m_FadeOut;
	float m_FadeTime;
	class Scene* m_NextScence;
public:
	void Init() override {}
	void Init(bool FadeOut, Scene* NextScence);
	void Uninit();
	void Update();
	void Draw();
};

