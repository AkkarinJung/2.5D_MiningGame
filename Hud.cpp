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
#include "Text.h"
#include "Camera.h"
#include <cfloat>
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
    ImGui::TextColored(kLamp, "%s", Tr(cls.name));
    ImGui::SameLine(90.0f);
    char text[64];
    snprintf(text, sizeof(text), Tr("HP %d / %d"), std::max(0, player->GetHp()), player->GetMaxHp());
    Bar((float)player->GetHp() / player->GetMaxHp(), kBlood, 230.0f, text);
    ImGui::SameLine();
    ImGui::Text(Tr("Potion x%d"), player->GetPotions());

    // Bag strip (C1): one box per slot, material colour + count
    {
        const Inventory& bag = player->GetBag();
        const float box = 26.0f, gap = 4.0f;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImGui::PushFont(NULL, 13.0f);
        for (int i = 0; i < bag.SlotCount(); i++)
        {
            if (i > 0)
            {
                ImGui::SameLine(0.0f, gap);
                if (ImGui::GetContentRegionAvail().x < box)
                    ImGui::NewLine();
            }
            const ImVec2 p0 = ImGui::GetCursorScreenPos();
            const ImVec2 p1(p0.x + box, p0.y + box);
            ImGui::Dummy(ImVec2(box, box));
            const Inventory::Slot& s = bag.GetSlot(i);
            draw->AddRectFilled(p0, p1, IM_COL32(20, 16, 14, 200), 3.0f);
            if (s.used)
            {
                const uint32_t c = kMat[(int)s.id].color;
                draw->AddRectFilled(ImVec2(p0.x + 4, p0.y + 4), ImVec2(p1.x - 4, p1.y - 9),
                    IM_COL32((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, 255), 2.0f);
                char n[8];
                snprintf(n, sizeof(n), "%d", s.n);
                const ImVec2 ts = ImGui::CalcTextSize(n);
                draw->AddText(ImVec2(p1.x - ts.x - 2, p1.y - ts.y), IM_COL32(240, 232, 220, 255), n);
            }
            draw->AddRect(p0, p1, IM_COL32(110, 96, 80, 200), 3.0f);
        }
        ImGui::PopFont();
    }

    // Row 2: floor, mining level + XP, stats
    if (map->IsTown())
        ImGui::TextUnformatted(Tr("Town"));
    else
        ImGui::Text(Tr("Floor %d"), map->GetFloor());      // Japanese: B%dF
    ImGui::SameLine(90.0f);
    ImGui::TextColored(kDim, "%s", Tr("Mine Lv"));
    ImGui::SameLine();
    ImGui::TextColored(kXp, "%d", player->GetMineLevel());
    ImGui::SameLine();
    const int need = MiningXpToNext(player->GetMineLevel());
    Bar(player->GetMineXp() / need, kXp, 70.0f, "");
    ImGui::SameLine();
    ImGui::TextColored(kDim, "%s", Tr("Atk"));
    ImGui::SameLine();
    ImGui::Text("%.0f", player->GetAttack());
    ImGui::SameLine();
    ImGui::TextColored(kDim, "%s", Tr("Mine"));
    ImGui::SameLine();
    ImGui::Text("%.0f", player->GetMinePower());
    ImGui::SameLine();
    ImGui::TextColored(kDim, "%s", Tr("Armor"));
    ImGui::SameLine();
    ImGui::Text("%d", player->GetArmor());

    // Row 3: skill cooldown
    const float left = std::max(0.0f, player->GetSkillTimer());
    if (left > 0.0f)
        snprintf(text, sizeof(text), "%s  %.1fs", Tr(cls.skillName), left);
    else
        snprintf(text, sizeof(text), Tr("%s  ready"), Tr(cls.skillName));
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
                snprintf(chip, sizeof(chip), "%s x%d", Tr(kCard[i].name), owned);
            else
                snprintf(chip, sizeof(chip), "%s", Tr(kCard[i].name));

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
        ImGui::TextColored(kDim, "%s", Tr("No cards yet. Each mining level-up lets you pick one."));

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
        ImGui::TextColored(kDim, "%s", Tr("Mine ore to raise your mining level."));
    ImGui::PopFont();
    ImGui::End();

    // ---- town (C8): "Dungeon" label floating above the gate
    Camera* camera = Manager::GetGameObj<Camera>();
    if (map->IsTown() && camera)
    {
        const Vector3 gate = map->GetStairsPosition();
        const XMMATRIX viewProj = camera->GetViewMatrix() * camera->GetProjectionMatrix();
        const XMVECTOR clip = XMVector3TransformCoord(XMVectorSet(gate.x, 2.45f, gate.z, 1.0f), viewProj);
        const float cz = XMVectorGetZ(clip);
        if (cz >= 0.0f && cz <= 1.0f)
        {
            const float sx = (XMVectorGetX(clip) + 1.0f) * 0.5f * SCREEN_WIDTH;
            const float sy = (1.0f - XMVectorGetY(clip)) * 0.5f * SCREEN_HEIGHT;
            const char* label = Tr("Dungeon");
            const float size = 22.0f;
            const ImVec2 ts = ImGui::GetFont()->CalcTextSizeA(size, FLT_MAX, 0.0f, label);
            const ImVec2 pos(sx - ts.x * 0.5f, sy - ts.y * 0.5f);
            ImDrawList* draw = ImGui::GetBackgroundDrawList();
            draw->AddText(ImGui::GetFont(), size, ImVec2(pos.x + 1.5f, pos.y + 1.5f), IM_COL32(0, 0, 0, 180), label);
            draw->AddText(ImGui::GetFont(), size, pos, IM_COL32(240, 200, 120, 255), label);
        }
    }

    // ---- bottom-right key help
    ImGui::SetNextWindowPos(ImVec2(SCREEN_WIDTH - 10.0f, SCREEN_HEIGHT - 10.0f), 0, ImVec2(1.0f, 1.0f));
    ImGui::SetNextWindowBgAlpha(0.35f);
    ImGui::Begin("##hud_keys", nullptr, kOverlay | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PushFont(NULL, 15.0f);
    ImGui::TextColored(kDim, "%s", Tr("WASD move   J / Space attack & mine (hold)"));
    ImGui::TextColored(kDim, "%s", Tr("K / Shift skill   Q potion   R restart   F1 debug"));
    ImGui::PopFont();
    ImGui::End();
}
