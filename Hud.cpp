// Hud.cpp
// Top-left panel: class, HP bar, potions / floor, mining level + XP bar, attack, mining,
// armor / skill cooldown bar / owned cards as coloured chips.
// Bottom-left: the last 4 log lines, newest on top, older ones fading.
// Bottom-right: key help.

#include "main.h"
#include "renderer.h"
#include "manager.h"
#include "Hud.h"
#include "Player.h"
#include "TileMap.h"
#include "Cards.h"
#include "OreData.h"
#include "DebugUI.h"
#include <cstring>

#include "imgui/imgui.h"
namespace
{
    const ImVec4 kLamp(0.88f, 0.66f, 0.23f, 1.0f);
    const ImVec4 kBlood(0.82f, 0.33f, 0.31f, 1.0f);
    const ImVec4 kXp(0.73f, 0.88f, 0.48f, 1.0f);
    const ImVec4 kGem(0.44f, 0.86f, 0.92f, 1.0f);
    const ImVec4 kDim(0.61f, 0.55f, 0.48f, 1.0f);

    const ImGuiWindowFlags kOverlay = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBringToFrontOnFocus;

    void Bar(float fraction, const ImVec4& color, float width, const char* overlay)
    {
        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
        ImGui::ProgressBar(std::max(0.0f, std::min(1.0f, fraction)), ImVec2(width, 0.0f), overlay);
        ImGui::PopStyleColor();
    }
}

void Hud::Init()
{
    m_Layer = 4;
}

void Hud::Update()
{
    Player* player = Manager::GetGameObj<Player>();
    TileMap* map = Manager::GetGameObj<TileMap>();
    Cards* cards = Manager::GetGameObj<Cards>();
    if (!player || !map)
        return;

    const ClassDef& cls = kClass[(int)player->GetClass()];

    // ---- top-left status panel
    const float panelW = 470.0f;
    ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f));
    ImGui::SetNextWindowSize(ImVec2(panelW, 0.0f));         // 0 height = fit the contents
    ImGui::SetNextWindowBgAlpha(0.6f);
    ImGui::Begin("##hud_status", nullptr, kOverlay);
    ImGui::PushFont(NULL, 18.0f);

    // Row 1: class, HP, potions
    ImGui::TextColored(kLamp, "%s", cls.name);
    ImGui::SameLine(90.0f);
    char text[64];
    snprintf(text, sizeof(text), "HP %d / %d", std::max(0, player->GetHp()), player->GetMaxHp());
    Bar((float)player->GetHp() / player->GetMaxHp(), kBlood, 230.0f, text);
    ImGui::SameLine();
    ImGui::Text("Potion x%d", player->GetPotions());

    // Row 2: floor, mining level + XP, stats
    ImGui::TextColored(kDim, "Floor");
    ImGui::SameLine();
    ImGui::Text("%d", map->GetFloor());
    ImGui::SameLine(90.0f);
    ImGui::TextColored(kDim, "Mine Lv");
    ImGui::SameLine();
    ImGui::TextColored(kXp, "%d", player->GetMineLevel());
    ImGui::SameLine();
    const int need = MiningXpToNext(player->GetMineLevel());
    Bar(player->GetMineXp() / need, kXp, 70.0f, "");
    ImGui::SameLine();
    ImGui::TextColored(kDim, "Atk");
    ImGui::SameLine();
    ImGui::Text("%.0f", player->GetAttack());
    ImGui::SameLine();
    ImGui::TextColored(kDim, "Mine");
    ImGui::SameLine();
    ImGui::Text("%.0f", player->GetMinePower());
    ImGui::SameLine();
    ImGui::TextColored(kDim, "Armor");
    ImGui::SameLine();
    ImGui::Text("%d", player->GetArmor());

    // Row 3: skill cooldown
    const float left = std::max(0.0f, player->GetSkillTimer());
    if (left > 0.0f)
        snprintf(text, sizeof(text), "%s  %.1fs", cls.skillName, left);
    else
        snprintf(text, sizeof(text), "%s  ready", cls.skillName);
    Bar(1.0f - left / player->GetSkillCooldown(), left > 0.0f ? kDim : kGem, -1.0f, text);

    // Row 4+: owned cards as chips, wrapped to the panel width
    if (cards && cards->TotalOwned() > 0)
    {
        ImGui::PushFont(NULL, 15.0f);
        bool first = true;
        for (int i = 0; i < kCardCount; i++)
        {
            const int owned = cards->Owned((CardId)i);
            if (owned <= 0)
                continue;
            char chip[48];
            if (owned > 1)
                snprintf(chip, sizeof(chip), "%s x%d", kCard[i].name, owned);
            else
                snprintf(chip, sizeof(chip), "%s", kCard[i].name);

            const float w = ImGui::CalcTextSize(chip).x;
            if (!first)
            {
                ImGui::SameLine(0.0f, 12.0f);
                if (ImGui::GetContentRegionAvail().x < w)
                    ImGui::NewLine();           // wrap to the next row
            }
            const float* rc = kRarityColor[(int)kCard[i].rarity];
            ImGui::TextColored(ImVec4(rc[0], rc[1], rc[2], 1.0f), "%s", chip);
            first = false;
        }
        ImGui::PopFont();
    }
    else
        ImGui::TextColored(kDim, "No cards yet. Each mining level-up lets you pick one.");

    ImGui::PopFont();
    ImGui::End();

    // ---- bottom-left message log: 4 lines, newest on top, older ones fade
    ImGui::SetNextWindowPos(ImVec2(10.0f, SCREEN_HEIGHT - 10.0f), 0, ImVec2(0.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::Begin("##hud_log", nullptr, kOverlay | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PushFont(NULL, 17.0f);
    int shown = 0;
    for (int i = 0; i < DebugUI::LogCount() && shown < 4; i++)
    {
        const char* line = DebugUI::LogLine(i);
        if (strncmp(line, "[debug]", 7) == 0)
            continue;                           // debug-panel actions stay out of the game log
        const float alpha = std::max(0.25f, 1.0f - 0.22f * shown);
        ImGui::TextColored(ImVec4(0.92f, 0.87f, 0.81f, alpha), "%s", line);
        shown++;
    }
    if (shown == 0)
        ImGui::TextColored(kDim, "Mine ore to raise your mining level.");
    ImGui::PopFont();
    ImGui::End();

    // ---- bottom-right key help
    ImGui::SetNextWindowPos(ImVec2(SCREEN_WIDTH - 10.0f, SCREEN_HEIGHT - 10.0f), 0, ImVec2(1.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::Begin("##hud_keys", nullptr, kOverlay | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PushFont(NULL, 15.0f);
    ImGui::TextColored(kDim, "WASD move   J / Space attack & mine (hold)");
    ImGui::TextColored(kDim, "K / Shift skill   Q potion   R restart   F1 debug");
    ImGui::PopFont();
    ImGui::End();
}
