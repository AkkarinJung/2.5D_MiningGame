#include "Game.h"
#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "Camera.h"
#include "field.h"
#include "Player.h"
#include "Enemy.h"
#include "Score.h"
#include "Fade.h"
#include "Result.h"
#include "GameObject.h"

void Game::Init()
{
    Manager::AddGameObj<Camera>();
    Manager::AddGameObj<field>();      // flat floor (replaces MeshField)
    Manager::AddGameObj<Player>();

    Manager::AddGameObj<Enemy>()->SetPosition({ -3.0f, 0.0f, 3.0f });
    Manager::AddGameObj<Enemy>()->SetPosition({ 0.0f, 0.0f, 4.0f });
    Manager::AddGameObj<Enemy>()->SetPosition({ 3.0f, 0.0f, 3.0f });

    Manager::AddGameObj<Score>()->SetPosition({ 100.0f, 100.0f, 0.0f });
    Manager::AddGameObj<Fade>()->Init(false, nullptr);
}

void Game::Uninit() {}

void Game::Update()
{
    if (Manager::GetGameObjs<Enemy>().empty())
        Manager::ChangeScene<Result>(3.0f);
}

void Game::Draw() {}