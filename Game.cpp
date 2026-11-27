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
#include "Monsters.h"
#include "Projectiles.h"
#include "Pickups.h"
#include "Cards.h"
#include "Prompts.h"
#include "Hud.h"
#include "Juice.h"
#include "GameState.h"
#include "SaveData.h"
#include "Lighting.h"

void Game::Init()
{
    CurrentRun() = RunStats();          // fresh kill / craft counts for the new run
    Manager::AddGameObj<Camera>();
    TileMap* map = Manager::AddGameObj<TileMap>();
    Player* player = Manager::AddGameObj<Player>();
    player->SetPosition(map->GetStartPosition());

    Manager::AddGameObj<Monsters>();
    Manager::AddGameObj<Projectiles>();
    Manager::AddGameObj<Pickups>();    // C1: loot on the floor, pulled into the bag
    Manager::AddGameObj<Juice>();      // debris, damage numbers, hit-stop, shake
    Manager::AddGameObj<Cards>();      // after Player, so a level-up opens its pick the same frame
    Manager::AddGameObj<Prompts>();    // stairs, restart (R) and death screen

    Manager::AddGameObj<Hud>();        // HP, floor, mining level, skill, potions, cards, log

    Manager::AddGameObj<Fade>()->Init(false, nullptr);
}

void Game::Uninit()
{
    Lighting::ApplyFloorPreset(1);      // leaving the game from the town: back to the dark clear colour
}

void Game::Update()
{
    TileMap* map = Manager::GetGameObj<TileMap>();
    Player* player = Manager::GetGameObj<Player>();
    if (!map || !player)
        return;
    // Any new floor (stairs, G key, debug "Next floor") updates the deepest-floor records
    if (map->GetFloor() != m_RecordedFloor)
    {
        m_RecordedFloor = map->GetFloor();
        SaveData::RecordFloor(player->GetClass(), m_RecordedFloor);
    }
    if (player->IsDead() || GamePaused())
        return;                                 // death screen, card pick or prompt open

}

void Game::Draw() {}