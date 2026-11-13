// ClassSelect.cpp
// Four class cards side by side: description, stat bars, skill and trait.
// Click a card or press 1-4; the choice is stored in SelectedClass() and the game starts.

#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "input.h"
#include "ClassSelect.h"
#include "ClassData.h"
#include "Game.h"
#include "Fade.h"
#include "GameObject.h"

#include "imgui/imgui.h"

void ClassSelect::Init()
{
    m_Chosen = false;
    Manager::AddGameObj<Fade>()->Init(false, nullptr);
}

void ClassSelect::Uninit()
{
}

void ClassSelect::Choose(int index)
{
    if (m_Chosen || index < 0 || index >= kClassCount)
        return;
    m_Chosen = true;
    SelectedClass() = (PlayerClass)index;
    Manager::AddGameObj<Fade>()->Init(true, new Game());
}

void ClassSelect::Update()
{
    for (int i = 0; i < kClassCount; i++)
        if (Input::GetKeyTrigger((BYTE)('1' + i)))
            Choose(i);

    // Largest value of each stat, so the bars compare the classes
    const float maxHp = 30.0f, maxAttack = 4.0f, maxMining = 5.0f, maxSpeed = 5.6f;

    const ImVec2 size(1080.0f, 470.0f);
    ImGui::SetNextWindowPos(ImVec2((SCREEN_WIDTH - size.x) * 0.5f, (SCREEN_HEIGHT - size.y) * 0.5f));
    ImGui::SetNextWindowSize(size);
    ImGui::Begin("Choose your class", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    ImGui::PushFont(NULL, 20.0f);
    ImGui::TextWrapped("Pick a class and head down. Mine ore to level up, collect cards, "
        "and see how deep you can go.");
    ImGui::Spacing();

    if (ImGui::BeginTable("classes", kClassCount, ImGuiTableFlags_BordersInnerV))
    {
        for (int i = 0; i < kClassCount; i++)
        {
            const ClassDef& c = kClass[i];
            ImGui::TableNextColumn();
            ImGui::PushID(i);

            ImGui::PushFont(NULL, 28.0f);
            ImGui::TextColored(ImVec4(0.88f, 0.66f, 0.23f, 1.0f), "%s", c.name);
            ImGui::PopFont();
            ImGui::TextDisabled("press %d", i + 1);
            ImGui::TextWrapped("%s", c.description);
            ImGui::Spacing();

            const struct { const char* label; float value, max; } stats[] =
            {
                { "HP",     (float)c.hp, maxHp },
                { "Attack", c.attack,    maxAttack },
                { "Mining", c.mining,    maxMining },
                { "Speed",  c.speed,     maxSpeed },
            };
            for (const auto& s : stats)
            {
                char overlay[16];
                snprintf(overlay, sizeof(overlay), "%s %g", s.label, s.value);
                ImGui::ProgressBar(s.value / s.max, ImVec2(-1.0f, 0.0f), overlay);
            }

            ImGui::Spacing();
            ImGui::TextColored(ImVec4(0.44f, 0.86f, 0.92f, 1.0f), "Skill: %s", c.skillName);
            ImGui::TextWrapped("%s", c.skillDescription);
            ImGui::TextColored(ImVec4(0.44f, 0.86f, 0.92f, 1.0f), "Trait");
            ImGui::TextWrapped("%s", c.trait);

            ImGui::Spacing();
            if (ImGui::Button("Choose", ImVec2(-1.0f, 40.0f)))
                Choose(i);

            ImGui::PopID();
        }
        ImGui::EndTable();
    }

    ImGui::PopFont();
    ImGui::End();
}

void ClassSelect::Draw()
{
}