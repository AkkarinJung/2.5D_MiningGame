// Cards.cpp
// Odds, dealing (shared + class cards, stack limits, no duplicates in one hand, rarity
// fallback, class-only epics favoured 40% of the time) and the pick window.
// The window uses Dear ImGui for now; the pixel-font game UI replaces it in M6.

#include "main.h"
#include "renderer.h"
#include "manager.h"
#include "input.h"
#include "Cards.h"
#include "Player.h"
#include "OreData.h"
#include "GameState.h"
#include "DebugUI.h"
#include "Text.h"
#include <algorithm>

#include "imgui/imgui.h"

void Cards::Init()
{
    m_Layer = 4;
    m_Rng.seed(std::random_device{}());
    m_Pending.reserve(8);
    CardPickOpen() = false;
}

void Cards::OddsFor(int grade, int level, float out[3]) const
{
    const Player* player = Manager::GetGameObj<Player>();
    const float luck = player ? player->GetLuck() : 0.0f;
    grade = std::min(std::max(grade, 0), kOreCount - 1);

    // Monster card tiers 0-2 use the same rows as copper, silver and gold ore
    float w[3] = { (float)kOre[grade].cardWeights[0], (float)kOre[grade].cardWeights[1],
                   (float)kOre[grade].cardWeights[2] };
    const float shift = std::min(0.85f, luck + 0.03f * (level - 1));
    const float moved = w[0] * shift;
    w[0] -= moved;
    w[1] += moved * 0.6f;
    w[2] += moved * 0.4f;
    const float total = w[0] + w[1] + w[2];
    for (int i = 0; i < 3; i++)
        out[i] = w[i] / total;
}

void Cards::DealHand(const PendingPick& pick)
{
    const Player* player = Manager::GetGameObj<Player>();
    const int cls = player ? (int)player->GetClass() : -1;

    OddsFor(pick.grade, pick.level, m_Odds);

    // Pool: shared cards plus this class's cards, minus any at their stack limit
    int pool[kCardCount];
    int poolSize = 0;
    for (int i = 0; i < kCardCount; i++)
    {
        const CardDef& c = kCard[i];
        if (c.onlyFor != -1 && c.onlyFor != cls)
            continue;
        if (c.maxStacks > 0 && m_Deck[i] >= c.maxStacks)
            continue;
        pool[poolSize++] = i;
    }

    const int wanted = (player && cls >= 0) ? kClass[cls].cardChoices : 3;
    m_HandSize = 0;
    for (int k = 0; k < wanted && poolSize > 0; k++)
    {
        const float roll = std::uniform_real_distribution<float>(0.0f, 1.0f)(m_Rng);
        const int rarity = roll < m_Odds[0] ? 0 : (roll < m_Odds[0] + m_Odds[1] ? 1 : 2);

        // Options of the rolled rarity; fall back to the next lower rarity if none are left
        int options[kCardCount];
        int count = 0;
        for (int r = rarity; r >= 0 && count == 0; r--)
            for (int p = 0; p < poolSize; p++)
                if ((int)kCard[pool[p]].rarity == r)
                    options[count++] = pool[p];
        if (count == 0)
            for (int p = 0; p < poolSize; p++)
                options[count++] = pool[p];

        // On an epic roll, offer only class-only epics 40% of the time
        if (rarity == 2 && std::uniform_real_distribution<float>(0.0f, 1.0f)(m_Rng) < 0.4f)
        {
            int own[kCardCount];
            int ownCount = 0;
            for (int o = 0; o < count; o++)
                if (kCard[options[o]].onlyFor != -1)
                    own[ownCount++] = options[o];
            if (ownCount > 0)
            {
                std::copy(own, own + ownCount, options);
                count = ownCount;
            }
        }

        const int card = options[std::uniform_int_distribution<int>(0, count - 1)(m_Rng)];
        m_Hand[m_HandSize++] = card;

        // Never the same card twice in one hand
        for (int p = 0; p < poolSize; p++)
            if (pool[p] == card) { pool[p] = pool[--poolSize]; break; }
    }
    m_HandOpen = true;
}

void Cards::Grant(CardId id)
{
    Player* player = Manager::GetGameObj<Player>();
    if (!player)
        return;
    player->ApplyCard(id);
    if (!kCard[(int)id].once)
        m_Deck[(int)id]++;
}

void Cards::Choose(int slot)
{
    if (!m_HandOpen || slot < 0 || slot >= m_HandSize)
        return;
    const CardDef& c = kCard[m_Hand[slot]];
    Grant(c.id);
    DebugUI::Log("Took %s.", Tr(c.name));

    m_Pending.erase(m_Pending.begin());     // tiny queue; erase at the front is fine
    m_HandOpen = false;
}

void Cards::Update()
{
    CardPickOpen() = !m_Pending.empty();
    if (m_Pending.empty())
        return;

    if (!m_HandOpen)
        DealHand(m_Pending.front());

    for (int i = 0; i < m_HandSize; i++)
        if (Input::GetKeyTrigger((BYTE)('1' + i)))
        {
            Choose(i);
            return;
        }

    // ---- the pick window
    const PendingPick& pick = m_Pending.front();
    const OreDef& ore = kOre[std::min(std::max(pick.grade, 0), kOreCount - 1)];
    const ImVec2 size(240.0f * m_HandSize + 40.0f, 360.0f);
    ImGui::SetNextWindowPos(ImVec2((SCREEN_WIDTH - size.x) * 0.5f, (SCREEN_HEIGHT - size.y) * 0.5f));
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Level up", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

    ImGui::PushFont(NULL, 26.0f);
    if (pick.fromKill)
        ImGui::TextColored(ImVec4(0.73f, 0.88f, 0.48f, 1.0f), Tr("Level %d! Choose a card"), pick.level);
    else
        ImGui::TextColored(ImVec4(0.73f, 0.88f, 0.48f, 1.0f), Tr("Mining Lv%d! Choose a card"), pick.level);
    ImGui::PopFont();
    ImGui::PushFont(NULL, 18.0f);
    if (pick.fromKill)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.92f, 0.80f, 0.62f, 1.0f));
        ImGui::TextWrapped(Tr("You levelled up by slaying a %s. Tougher monsters mean rarer cards."),
            Tr(kMonster[(int)pick.killer].name));
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ore.r, ore.g, ore.b, 1.0f));
        ImGui::TextWrapped(Tr("You levelled up on %s. Better ore means rarer cards. Keep one."), Tr(ore.name));
    }
    ImGui::PopStyleColor();
    for (int r = 0; r < 3; r++)
    {
        if (r > 0)
            ImGui::SameLine();
        ImGui::TextColored(ImVec4(kRarityColor[r][0], kRarityColor[r][1], kRarityColor[r][2], 1.0f),
            "%s %d%%   ", Tr(kRarityName[r]), (int)(m_Odds[r] * 100.0f + 0.5f));
    }
    ImGui::Spacing();

    if (ImGui::BeginTable("hand", m_HandSize, ImGuiTableFlags_BordersInnerV))
    {
        for (int i = 0; i < m_HandSize; i++)
        {
            const CardDef& c = kCard[m_Hand[i]];
            const float* rc = kRarityColor[(int)c.rarity];
            ImGui::TableNextColumn();
            ImGui::PushID(i);

            if (c.onlyFor >= 0)
                ImGui::TextColored(ImVec4(rc[0], rc[1], rc[2], 1.0f), Tr("%s - %s only   [%d]"),
                    Tr(kRarityName[(int)c.rarity]), Tr(kClass[c.onlyFor].name), i + 1);
            else
                ImGui::TextColored(ImVec4(rc[0], rc[1], rc[2], 1.0f), "%s   [%d]", Tr(kRarityName[(int)c.rarity]), i + 1);

            ImGui::PushFont(NULL, 24.0f);
            ImGui::TextWrapped("%s", Tr(c.name));
            ImGui::PopFont();
            ImGui::TextWrapped("%s", Tr(c.description));
            if (m_Deck[m_Hand[i]] > 0)
                ImGui::TextDisabled(Tr("Owned x%d"), m_Deck[m_Hand[i]]);

            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(rc[0] * 0.5f, rc[1] * 0.5f, rc[2] * 0.5f, 1.0f));
            if (ImGui::Button(Tr("Take"), ImVec2(-1.0f, 40.0f)))
                Choose(i);
            ImGui::PopStyleColor();

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::PopFont();
    ImGui::End();
}