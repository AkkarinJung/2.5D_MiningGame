// Prompts.cpp
// Stairs: "Stay and mine" or "Descend" (heals 15%, next floor, records the deepest floor).
// Restart (R): same class, new class, or keep playing (Esc). Death: run stats and retry.

#include "main.h"
#include "renderer.h"
#include "manager.h"
#include "input.h"
#include "Prompts.h"
#include "Player.h"
#include "TileMap.h"
#include "Cards.h"
#include "Fade.h"
#include "Game.h"
#include "ClassSelect.h"
#include "GameState.h"
#include "DebugUI.h"
#include <cmath>

#include "imgui/imgui.h"

namespace
{
    // A centred, fixed window for one prompt.
    bool BeginPrompt(const char* id, float width, float height)
    {
        ImGui::SetNextWindowPos(ImVec2((SCREEN_WIDTH - width) * 0.5f, (SCREEN_HEIGHT - height) * 0.5f));
        ImGui::SetNextWindowSize(ImVec2(width, height));
        const bool open = ImGui::Begin(id, nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);
        ImGui::PushFont(NULL, 20.0f);
        return open;
    }

    void EndPrompt()
    {
        ImGui::PopFont();
        ImGui::End();
    }

    void Title(const char* text, ImVec4 color = ImVec4(0.88f, 0.66f, 0.23f, 1.0f))
    {
        ImGui::PushFont(NULL, 30.0f);
        ImGui::TextColored(color, "%s", text);
        ImGui::PopFont();
        ImGui::Spacing();
    }

    // Two buttons side by side; returns 1 or 2 when clicked, 0 otherwise.
    int TwoButtons(const char* a, const char* b)
    {
        const float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        int pressed = 0;
        if (ImGui::Button(a, ImVec2(w, 44.0f))) pressed = 1;
        ImGui::SameLine();
        if (ImGui::Button(b, ImVec2(w, 44.0f))) pressed = 2;
        return pressed;
    }
}

void Prompts::Init()
{
    m_Layer = 4;
    m_State = State::None;
    PromptOpen() = false;
}

void Prompts::Update()
{
    Player* player = Manager::GetGameObj<Player>();
    TileMap* map = Manager::GetGameObj<TileMap>();
    Cards* cards = Manager::GetGameObj<Cards>();
    if (!player || !map)
        return;

    const float dt = 1.0f / 60.0f;

    if (m_Leaving)
    {
        PromptOpen() = true;        // keep everything frozen until the scene changes
        return;
    }

    if (m_State == State::None)
    {
        if (player->IsDead())
        {
            m_DeathTimer += dt;
            if (m_DeathTimer >= 0.9f)
            {
                BestFloor() = std::max(BestFloor(), map->GetFloor());
                m_State = State::Death;
            }
        }
        else if (!cards || !cards->IsPicking())     // queued card picks come first
        {
            if (map->IsStairs(player->GetPosition()))
            {
                if (!m_StairLock)
                    m_State = State::Stairs;
            }
            else
                m_StairLock = false;

            if (m_State == State::None && Input::GetKeyTrigger('R'))
                m_State = State::Restart;
        }
    }

    PromptOpen() = m_State != State::None;

    switch (m_State)
    {
    case State::Stairs:  DrawStairs();  break;
    case State::Restart: DrawRestart(); break;
    case State::Death:   DrawDeath();   break;
    default: break;
    }
}

// ---------------------------------------------------------------- actions

void Prompts::Descend()
{
    Player* player = Manager::GetGameObj<Player>();
    TileMap* map = Manager::GetGameObj<TileMap>();
    if (!player || !map)
        return;

    player->Heal((int)ceilf(player->GetMaxHp() * 0.15f));
    map->Generate(map->GetSeed(), map->GetFloor() + 1);
    player->SetPosition(map->GetStartPosition());
    BestFloor() = std::max(BestFloor(), map->GetFloor());
    Manager::AddGameObj<Fade>()->Init(false, nullptr);      // fade in on the new floor

    DebugUI::Log("Floor %d. The air feels more hostile.", map->GetFloor());
    m_State = State::None;
    m_StairLock = false;
}

void Prompts::RestartRun(bool sameClass)
{
    m_State = State::None;
    m_Leaving = true;
    if (sameClass)
        Manager::AddGameObj<Fade>()->Init(true, new Game());         // SelectedClass() is unchanged
    else
        Manager::AddGameObj<Fade>()->Init(true, new ClassSelect());
}

// ---------------------------------------------------------------- windows

void Prompts::DrawStairs()
{
    TileMap* map = Manager::GetGameObj<TileMap>();
    if (BeginPrompt("stairs", 460.0f, 230.0f))
    {
        char title[48];
        snprintf(title, sizeof(title), "Stairs to floor %d", map->GetFloor() + 1);
        Title(title);

        const int left = map->GetBlocksLeft();
        if (left > 0)
            ImGui::TextWrapped("%d block%s still unbroken here. That's mining XP you'd leave behind. "
                "Monsters below hit harder.", left, left > 1 ? "s" : "");
        else
            ImGui::TextWrapped("This floor is mined out. Monsters below hit harder.");
        ImGui::Spacing();

        const int choice = TwoButtons("Stay and mine  [1]", "Descend  [2]");
        if (choice == 1 || Input::GetKeyTrigger('1'))
        {
            m_State = State::None;
            m_StairLock = true;
        }
        else if (choice == 2 || Input::GetKeyTrigger('2'))
            Descend();
    }
    EndPrompt();
}

void Prompts::DrawRestart()
{
    Player* player = Manager::GetGameObj<Player>();
    TileMap* map = Manager::GetGameObj<TileMap>();
    Cards* cards = Manager::GetGameObj<Cards>();

    if (BeginPrompt("restart", 480.0f, 260.0f))
    {
        Title("Restart this run?");
        const int cardCount = cards ? cards->TotalOwned() : 0;
        ImGui::TextWrapped("Your run (%s, floor %d, Mining Lv%d, %d card%s) will be lost.",
            kClass[(int)player->GetClass()].name, map->GetFloor(),
            player->GetMineLevel(), cardCount, cardCount == 1 ? "" : "s");
        ImGui::Spacing();

        const int choice = TwoButtons("Same class", "New class");
        if (choice != 0)
        {
            RestartRun(choice == 1);
            EndPrompt();
            return;
        }
        if (ImGui::Button("Keep playing  [Esc]", ImVec2(-1.0f, 40.0f)) || Input::GetKeyTrigger(VK_ESCAPE))
            m_State = State::None;
    }
    EndPrompt();
}

void Prompts::DrawDeath()
{
    Player* player = Manager::GetGameObj<Player>();
    TileMap* map = Manager::GetGameObj<TileMap>();
    Cards* cards = Manager::GetGameObj<Cards>();

    if (BeginPrompt("death", 520.0f, 300.0f))
    {
        char title[64];
        snprintf(title, sizeof(title), "Slain by the %s", player->GetKilledBy());
        Title(title, ImVec4(0.82f, 0.33f, 0.31f, 1.0f));

        ImGui::Text("%s  -  floor %d  -  Mining Lv%d", kClass[(int)player->GetClass()].name,
            map->GetFloor(), player->GetMineLevel());
        ImGui::Text("%d kills   %d blocks   %d cards", player->GetKills(), player->GetBlocksBroken(),
            cards ? cards->TotalOwned() : 0);
        ImGui::TextColored(ImVec4(0.44f, 0.86f, 0.92f, 1.0f), "Deepest run: floor %d", BestFloor());
        ImGui::Spacing();
        ImGui::Spacing();

        const int choice = TwoButtons("Retry same class", "Choose a new class");
        if (choice != 0)
        {
            RestartRun(choice == 1);
            EndPrompt();
            return;
        }
    }
    EndPrompt();
}