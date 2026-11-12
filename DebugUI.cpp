// DebugUI.cpp
// The "Debug (F1)" window: FPS, player, mining, floor and render sections, and a
// message log. Each new game system adds its own collapsing section here.

#include "main.h"
#include "renderer.h"
#include "manager.h"
#include "DebugUI.h"
#include "Player.h"
#include "TileMap.h"
#include "OreData.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

#include <cstdarg>
#include <random>

namespace
{
    bool g_Visible = true;
    bool g_ShowDemo = false;

    // Fixed-size ring buffer: no allocations while playing.
    const int kLogLines = 64;
    const int kLogWidth = 128;
    char g_Log[kLogLines][kLogWidth];
    int  g_LogHead = 0;     // next slot to write
    int  g_LogCount = 0;

    void NewFloor(TileMap* map, Player* player, uint32_t seed, int floorNo)
    {
        map->Generate(seed, floorNo);
        if (player)
            player->SetPosition(map->GetStartPosition());
    }
}

void DebugUI::Init()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(GetWindow());
    ImGui_ImplDX11_Init(Renderer::GetDevice(), Renderer::GetDeviceContext());
}

void DebugUI::Uninit()
{
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

bool DebugUI::WantsKeyboard()
{
    return ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureKeyboard;
}

void DebugUI::Log(const char* format, ...)
{
    char* line = g_Log[g_LogHead];

    va_list args;
    va_start(args, format);
    vsnprintf(line, kLogWidth, format, args);
    va_end(args);

    OutputDebugStringA(line);
    OutputDebugStringA("\n");

    g_LogHead = (g_LogHead + 1) % kLogLines;
    if (g_LogCount < kLogLines)
        g_LogCount++;
}

void DebugUI::Draw()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (ImGui::IsKeyPressed(ImGuiKey_F1, false))
        g_Visible = !g_Visible;

    if (g_ShowDemo)
        ImGui::ShowDemoWindow(&g_ShowDemo);

    if (g_Visible)
    {
        ImGui::SetNextWindowPos(ImVec2(SCREEN_WIDTH - 340.0f, 10.0f), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(330.0f, 560.0f), ImGuiCond_FirstUseEver);

        if (ImGui::Begin("Debug (F1)", &g_Visible))
        {
            const ImGuiIO& io = ImGui::GetIO();
            ImGui::Text("%.1f FPS  (%.2f ms/frame)", io.Framerate, 1000.0f / io.Framerate);

            Player* player = Manager::GetGameObj<Player>();
            TileMap* map = Manager::GetGameObj<TileMap>();

            // ---- Player
            if (player && ImGui::CollapsingHeader("Player", ImGuiTreeNodeFlags_DefaultOpen))
            {
                const Vector3 p = player->GetPosition();
                ImGui::Text("Position  %.2f, %.2f", p.x, p.z);
                ImGui::Text("Tile      %d, %d", (int)floorf(p.x), (int)floorf(p.z));
            }

            // ---- Mining
            if (player && ImGui::CollapsingHeader("Mining", ImGuiTreeNodeFlags_DefaultOpen))
            {
                const int level = player->GetMineLevel();
                const float xp = player->GetMineXp();
                const int need = MiningXpToNext(level);

                ImGui::Text("Mining Lv %d    power %.0f", level, player->GetMinePower());
                char overlay[32];
                snprintf(overlay, sizeof(overlay), "%.0f / %d XP", xp, need);
                ImGui::ProgressBar(xp / (float)need, ImVec2(-1.0f, 0.0f), overlay);

                if (ImGui::Button("Give 10 XP"))
                {
                    player->GainMiningXp(10.0f);
                    Log("[debug] gave 10 mining XP");
                }
                ImGui::SameLine();
                if (ImGui::Button("Level up"))
                {
                    player->GainMiningXp((float)need - xp);
                    Log("[debug] forced a mining level-up");
                }

                if (map)
                    ImGui::Text("Blocks left on this floor: %d", map->GetBlocksLeft());

                if (ImGui::BeginTable("ore", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                {
                    ImGui::TableSetupColumn("Ore");
                    ImGui::TableSetupColumn("Need Lv");
                    ImGui::TableSetupColumn("XP");
                    ImGui::TableSetupColumn("Base HP");
                    ImGui::TableHeadersRow();
                    for (int g = 0; g < kOreCount; g++)
                    {
                        const OreDef& ore = kOre[g];
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::TextColored(ImVec4(ore.r, ore.g, ore.b, 1.0f), "%s", ore.name);
                        ImGui::TableNextColumn();
                        ImGui::Text("%d", ore.requiredLevel);
                        ImGui::TableNextColumn();
                        ImGui::Text("%d", ore.xp);
                        ImGui::TableNextColumn();
                        ImGui::Text("%.0f", ore.baseHp);
                    }
                    ImGui::EndTable();
                }
            }

            // ---- Floor
            if (map && ImGui::CollapsingHeader("Floor", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::Text("Seed %u    Floor %d", map->GetSeed(), map->GetFloor());
                ImGui::Text("Walls drawn this frame: %d", map->GetDrawnWalls());

                if (ImGui::Button("Regenerate"))
                {
                    NewFloor(map, player, std::random_device{}(), map->GetFloor());
                    Log("[debug] regenerated floor %d (seed %u)", map->GetFloor(), map->GetSeed());
                }
                ImGui::SameLine();
                if (ImGui::Button("Next floor"))
                {
                    NewFloor(map, player, map->GetSeed(), map->GetFloor() + 1);
                    Log("[debug] went down to floor %d", map->GetFloor());
                }
            }

            // ---- Render
            if (ImGui::CollapsingHeader("Render"))
            {
                ImGui::ColorEdit3("Clear colour", Renderer::GetClearColor());
                ImGui::Checkbox("ImGui demo window", &g_ShowDemo);
            }

            // ---- Log (newest first)
            if (ImGui::CollapsingHeader("Log", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::BeginChild("log", ImVec2(0.0f, 160.0f), ImGuiChildFlags_Borders);
                for (int i = 0; i < g_LogCount; i++)
                {
                    const int index = (g_LogHead - 1 - i + kLogLines) % kLogLines;
                    ImGui::TextUnformatted(g_Log[index]);
                }
                ImGui::EndChild();
            }
        }
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

