#pragma once
#include "GameObject.h"
class Camera :public GameObject
{
private:
	//Vector3 m_Position{ 0.0f,0.0f,0.0f };
	Vector3 m_Target{ 0.0f,0.0f,0.0f };
	XMMATRIX m_ViewMatrix;
	XMMATRIX m_projection;
	Vector3 m_Shake;
	float m_ShakeTime;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw()override;

	XMMATRIX GetViewMatrix() { return m_ViewMatrix; }

	Vector3 GetFoward()override
	{
		Vector3 forward = m_Target - m_Position;
		forward.normalize();

		return forward;
	}
	Vector3 GetRight()override
	{
		Vector3 forward = m_Target - m_Position;
		Vector3 up = Vector3(0.0f, 1.0f, 0.0f);
		Vector3 right = Vector3::cross(up, forward);
		right.normalize();

		return right;

	}

	void Shake(Vector3 Shake)
	{
		m_Shake = Shake;
		m_ShakeTime = 0;
	}

	bool CheckView(Vector3 Position, float Size);

};

