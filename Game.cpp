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
#include "Monsters.h"
#include "Projectiles.h"
#include "Result.h"
#include "Cards.h"
#include "Prompts.h"
#include "GameState.h"

void Game::Init()
{
    Manager::AddGameObj<Camera>();
    TileMap* map = Manager::AddGameObj<TileMap>();
    Player* player = Manager::AddGameObj<Player>();
    player->SetPosition(map->GetStartPosition());

    Manager::AddGameObj<Monsters>();
    Manager::AddGameObj<Projectiles>();
    Manager::AddGameObj<Cards>();      // after Player, so a level-up opens its pick the same frame
    Manager::AddGameObj<Prompts>();    // stairs, restart (R) and death screen

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
    if (player->IsDead() || GamePaused())
        return;                                 // death screen, card pick or prompt open

    // G: brand-new random floor (for testing until the F1 debug panel exists)
    if (Input::GetKeyTrigger('G'))
    {
        map->Generate(std::random_device{}(), 1);
        player->SetPosition(map->GetStartPosition());
    }
    // The stairs are handled by Prompts (stay or descend)
}

void Game::Draw() {}