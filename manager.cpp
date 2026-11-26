#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "Camera.h"
#include "input.h"
#include "Title.h"
#include "Game.h"
#include "audio.h"
#include "DebugUI.h"
#include "SaveData.h"
#include "Text.h"
#include "Lighting.h"
#include "ShadowMap.h"

#include "GameObject.h"

std::list<GameObject*> Manager::m_GameObjects;
Scene* Manager::m_Scene = nullptr;
Scene* Manager::m_NextScene = nullptr;
float Manager::m_ChangeTime = 0.0f;


void Manager::Init()
{
	Renderer::Init();
	Lighting::Init();           // point-light constant buffer (b6)
	ShadowMap::Init();          // moon shadow map (t1, s1, b7)
	DebugUI::Init();
	SaveData::Load();          // settings + records from %LOCALAPPDATA%\Deepvein
	LoadTranslations();        // asset\text\ja.txt
	Input::Init();
	Audio::InitMaster();

	ChangeScene<Title>(0.0f);
}


void Manager::Uninit()
{
	if (m_Scene != nullptr)
		m_Scene->Uninit();

	for (GameObject* obj : m_GameObjects) {
		if (obj != nullptr) {
			obj->Uninit();
			delete obj;
			obj = nullptr;
		}
	}
	m_GameObjects.clear();

	DebugUI::Uninit();
	Audio::UninitMaster();
	ShadowMap::Uninit();
	Lighting::Uninit();
	Renderer::Uninit();
	Input::Uninit();
}

void Manager::Update()
{
	float dt = 1.0f / 60.0f;
	Input::Update();
	DebugUI::BeginFrame();
	Lighting::BeginFrame();     // game objects add this frame's lights in Update

	if (m_Scene != nullptr)
		m_Scene->Update();

	for (GameObject* obj : m_GameObjects)
	{
		if (obj != nullptr) {
			obj->Update();
		}
	}

	m_GameObjects.remove_if([](GameObject* object)
		{
			return object->Destory();
		});

	if (m_NextScene != nullptr)
	{
		m_ChangeTime -= dt;
		if (m_ChangeTime < 0.0f)
		{
			if (m_Scene != nullptr)
			{
				m_Scene->Uninit();
				delete m_Scene;
			}

			for (GameObject* obj : m_GameObjects)
			{
				if (obj != nullptr) {
					obj->Uninit();
					delete obj;
					obj = nullptr;
				}
			}

			m_GameObjects.clear();

			m_Scene = m_NextScene;
			m_Scene->Init();


			m_NextScene = nullptr;
		}
	}

}

void Manager::Draw()
{
	Renderer::Begin();
	Lighting::Upload();         // nearest 16 lights -> shader

	ShadowMap::BeginPass(Lighting::FocusX(), Lighting::FocusZ());
	if (ShadowMap::MoonShadowsActive())         // skipped when moon shadows are off (saves a pass)
		for (GameObject* obj : m_GameObjects)
			obj->DrawShadow();
	// Lamp shadows: the same casters again, once for each of the 6 cube faces around the lamp
	if (ShadowMap::LampShadowsActive())
		for (int face = 0; face < 6; face++)
		{
			ShadowMap::BeginLampFace(face);
			for (GameObject* obj : m_GameObjects)
				obj->DrawShadow();
		}
	ShadowMap::EndPass();

	Camera* camera = GetGameObj<Camera>();
	if (camera)
	{
		Vector3 forward = camera->GetFoward();
		Vector3 position = camera->GetPosition();

		for (GameObject* obj : m_GameObjects)
		{
			obj->CalcCameraZ(position, forward);
		}
		m_GameObjects.sort([](GameObject* a, GameObject* b)
			{
				return a->GetCameraZ() > b->GetCameraZ();
			});
	}

	for (int layer = 0; layer < 5; layer++)
	{
		for (GameObject* obj : m_GameObjects) {
			if (obj->GetLayer() == layer)
			{
				if (obj != nullptr) {
					obj->Draw();
				}
			}
		}
	}
	DebugUI::Draw();
	Renderer::End();
}
