#include "Game.h"
#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "input.h"
#include "Camera.h"
#include "TileMap.h"
#include "Player.h"
#include "Fade.h"
#include "GameObject.h"
#include "Score.h"
#include <random>

void Game::Init()
{
    Manager::AddGameObj<Camera>();
    TileMap* map = Manager::AddGameObj<TileMap>();
    Player* player = Manager::AddGameObj<Player>();
    player->SetPosition(map->GetStartPosition());

    Score* level = Manager::AddGameObj<Score>();   // top-left number = mining level (until the HUD exists)
    level->SetPosition({ 20.0f, 20.0f, 0.0f });
    level->SetScore(1);

    Manager::AddGameObj<Fade>()->Init(false, nullptr);
}

void Game::Uninit() {}

void Game::Update()
{
    TileMap* map = Manager::GetGameObj<TileMap>();
    Player* player = Manager::GetGameObj<Player>();
    if (!map || !player)
        return;

    // G: brand-new random floor (for testing until the F1 debug panel exists)
    if (Input::GetKeyTrigger('G'))
    {
        map->Generate(std::random_device{}(), 1);
        player->SetPosition(map->GetStartPosition());
    }

    // Stepping on the stairs builds the next floor (the stay/descend prompt comes in M6)
    if (map->IsStairs(player->GetPosition()))
    {
        map->Generate(map->GetSeed(), map->GetFloor() + 1);
        player->SetPosition(map->GetStartPosition());
    }
}

void Game::Draw() {}