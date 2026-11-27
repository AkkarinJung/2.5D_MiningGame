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
#include "Monsters.h"
#include "ClassData.h"
#include "Cards.h"
#include "Text.h"
#include "SaveData.h"
#include "Lighting.h"
#include "ShadowMap.h"
#include "Juice.h"
#include "Pickups.h"
#include <shellapi.h>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

#include <cstdarg>
#include <random>

namespace
{
    bool g_Visible = true;
    bool g_InFrame = false;     // BeginFrame ran and Draw has not rendered it yet
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

    // Fonts: ImGui's default font, plus a Japanese Windows font merged in for kana and kanji
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontDefault();
    const char* const japaneseFonts[] = { "C:\\Windows\\Fonts\\YuGothM.ttc",
                                          "C:\\Windows\\Fonts\\meiryo.ttc",
                                          "C:\\Windows\\Fonts\\msgothic.ttc" };
    for (const char* path : japaneseFonts)
    {
        if (GetFileAttributesA(path) == INVALID_FILE_ATTRIBUTES)
            continue;
        ImFontConfig config;
        config.MergeMode = true;
        io.Fonts->AddFontFromFileTTF(path, 13.0f, &config);     // same base size as the default font
        break;
    }

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
    vsnprintf(line, kLogWidth, Tr(format), args);    // lines keep the language they were written in
    va_end(args);

    OutputDebugStringA(line);
    OutputDebugStringA("\n");

    g_LogHead = (g_LogHead + 1) % kLogLines;
    if (g_LogCount < kLogLines)
        g_LogCount++;
}

int DebugUI::LogCount()
{
    return g_LogCount;
}

const char* DebugUI::LogLine(int index)
{
    if (index < 0 || index >= g_LogCount)
        return "";
    return g_Log[(g_LogHead - 1 - index + kLogLines) % kLogLines];
}

void DebugUI::BeginFrame()
{
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    g_InFrame = true;
}

void DebugUI::Draw()
{
    if (!g_InFrame)
        return;
    g_InFrame = false;

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
                ImGui::Text("Class     %s", kClass[(int)player->GetClass()].name);
                ImGui::Text("Position  %.2f, %.2f", p.x, p.z);
                ImGui::Text("Tile      %d, %d", (int)floorf(p.x), (int)floorf(p.z));

                const int hp = player->GetHp(), maxHp = player->GetMaxHp();
                char hpText[32];
                snprintf(hpText, sizeof(hpText), "HP %d / %d", hp, maxHp);
                ImGui::ProgressBar((float)std::max(0, hp) / (float)maxHp, ImVec2(-1.0f, 0.0f), hpText);
                if (ImGui::Checkbox("God mode", &player->GodMode()))
                    Log("[debug] god mode %s", player->GodMode() ? "on" : "off");
                ImGui::SameLine();
                if (ImGui::Button("Heal to full"))
                {
                    player->HealFull();
                    Log("[debug] healed to full");
                }
                const float skillLeft = std::max(0.0f, player->GetSkillTimer());
                char skillText[64];
                if (skillLeft > 0.0f)
                    snprintf(skillText, sizeof(skillText), "%s  %.1fs",
                        kClass[(int)player->GetClass()].skillName, skillLeft);
                else
                    snprintf(skillText, sizeof(skillText), "%s  ready (K / Shift)",
                        kClass[(int)player->GetClass()].skillName);
                ImGui::ProgressBar(1.0f - skillLeft / player->GetSkillCooldown(), ImVec2(-1.0f, 0.0f), skillText);

                ImGui::Text("Potions %d  (Q to drink)", player->GetPotions());
                ImGui::SameLine();
                if (ImGui::Button("+1 potion"))
                {
                    player->AddPotion();
                    Log("[debug] added a potion");
                }
            }

            // ---- Level (C4): combat level from kills
            if (player && ImGui::CollapsingHeader("Level", ImGuiTreeNodeFlags_DefaultOpen))
            {
                const int need = XpToNextLevel(player->GetLevel());
                ImGui::Text("Level %d    kills %d", player->GetLevel(), player->GetKills());
                char overlay[32];
                snprintf(overlay, sizeof(overlay), "%d / %d XP", player->GetXp(), need);
                ImGui::ProgressBar((float)player->GetXp() / need, ImVec2(-1.0f, 0.0f), overlay);
                if (ImGui::Button("Give 10 XP##combat"))
                {
                    player->GainXp(10, MonsterType::Slime);
                    Log("[debug] gave 10 combat XP");
                }
                ImGui::SameLine();
                if (ImGui::Button("Level up##combat"))
                {
                    player->GainXp(need - player->GetXp(), MonsterType::Goblin);
                    Log("[debug] forced a combat level-up (goblin odds)");
                }
                ImGui::TextDisabled("XP per kill on this floor:");
                for (int t = 0; t < kMonsterTypeCount; t++)
                {
                    if (t > 0)
                        ImGui::SameLine();
                    ImGui::TextDisabled("%s %d", kMonster[t].name, KillXp(kMonster[t].xp, map ? map->GetFloor() : 1));
                }
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
                // C8: the town (floor 0) and its sun
                if (ImGui::Button("Go to floor 0 (town)"))
                {
                    NewFloor(map, player, map->GetSeed(), 0);
                    Log("[debug] went to the town");
                }
                ImGui::SameLine();
                if (ImGui::Button("Go to floor 1"))
                {
                    NewFloor(map, player, map->GetSeed(), 1);
                    Log("[debug] went to floor 1");
                }
                if (map->IsTown())
                {
                    ShadowMap::Settings& sun = ShadowMap::GetSettings();
                    Lighting::Settings& town = Lighting::GetSettings();
                    ImGui::SliderFloat3("Sun direction", sun.sunDir, -30.0f, 30.0f, "%.1f");
                    ImGui::SliderFloat("Sun intensity", &sun.sunIntensity, 0.0f, 3.0f, "%.2f");
                    ImGui::ColorEdit3("Sun colour", sun.sunColor);
                    ImGui::SliderFloat("Sun shadow ambient", &sun.sunShadowedAmbient, 0.0f, 1.0f, "%.2f");
                    ImGui::ColorEdit3("Town ambient", town.townAmbient, ImGuiColorEditFlags_HDR);
                    ImGui::ColorEdit3("Town fog / sky", town.townFogColor);
                    ImGui::DragFloatRange2("Town fog", &town.townFogStart, &town.townFogEnd, 0.1f, 0.0f, 80.0f);
                }
            }

            // ---- Monsters
            Monsters* monsters = Manager::GetGameObj<Monsters>();
            if (monsters && ImGui::CollapsingHeader("Monsters", ImGuiTreeNodeFlags_DefaultOpen))
            {
                Pickups* loot = Manager::GetGameObj<Pickups>();

                ImGui::Text("Count %d    awake %d    potions on floor %d",
                    monsters->Count(), monsters->AwakeCount(), loot ? loot->CountOf(MatId::Potion) : 0);
                ImGui::Checkbox("Freeze AI", &monsters->Frozen());

                for (int t = 0; t < kMonsterTypeCount; t++)
                {
                    if (t % 2 == 1)
                        ImGui::SameLine();
                    char label[48];
                    snprintf(label, sizeof(label), "+ %s", kMonster[t].name);
                    if (ImGui::Button(label) && player && map)
                    {
                        // Spawn on the first free tile around the player, 3 tiles out if possible
                        const Vector3 p = player->GetPosition();
                        const int px = (int)floorf(p.x), pz = (int)floorf(p.z);
                        int sx = px, sz = pz;
                        const int offsets[8][2] = { { 3,0 },{ -3,0 },{ 0,3 },{ 0,-3 },{ 2,2 },{ -2,2 },{ 2,-2 },{ -2,-2 } };
                        for (const auto& o : offsets)
                            if (map->Get(px + o[0], pz + o[1]) == Tile::Floor) { sx = px + o[0]; sz = pz + o[1]; break; }
                        monsters->Spawn((MonsterType)t, sx + 0.5f, sz + 0.5f);
                        Log("[debug] spawned a %s", kMonster[t].name);
                    }
                }
                if (ImGui::Button("Kill all"))
                {
                    monsters->KillAll();
                    Log("[debug] killed all monsters");
                }
            }

            // ---- Bag (C1)
            Pickups* pickups = Manager::GetGameObj<Pickups>();
            if (player && ImGui::CollapsingHeader("Bag"))
            {
                Inventory& bag = player->GetBag();
                ImGui::Text("Slots %d / %d used    pickups on floor %d", bag.UsedSlots(), bag.SlotCount(),
                    pickups ? pickups->Count() : 0);

                // Add any material x1 / x10 (anything that does not fit drops at your feet)
                if (ImGui::BeginTable("##addmat", 3, ImGuiTableFlags_SizingFixedFit))
                {
                    for (int m = 0; m < kMatCount; m++)
                    {
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        ImGui::Text("%s (%d)", kMat[m].name, bag.Count((MatId)m));
                        ImGui::TableNextColumn();
                        ImGui::PushID(m);
                        if (ImGui::SmallButton("+1"))
                            player->AddToBagOrDrop((MatId)m, 1);
                        ImGui::TableNextColumn();
                        if (ImGui::SmallButton("+10"))
                            player->AddToBagOrDrop((MatId)m, 10);
                        ImGui::PopID();
                    }
                    ImGui::EndTable();
                }

                // Drop a slot (tests "not grabbed again until you step away")
                for (int i = 0; i < bag.SlotCount(); i++)
                {
                    const Inventory::Slot s = bag.GetSlot(i);      // a copy: dropping empties the slot
                    if (!s.used)
                        continue;
                    ImGui::PushID(1000 + i);
                    if (ImGui::SmallButton("x"))
                    {
                        Log("[debug] dropped %d %s", s.n, kMat[(int)s.id].name);
                        player->DropBagSlot(i);
                    }
                    ImGui::PopID();
                    ImGui::SameLine();
                    ImGui::Text("slot %d: %s x%d", i + 1, kMat[(int)s.id].name, s.n);
                }

                if (ImGui::Button("Clear bag"))
                {
                    bag.Clear();
                    Log("[debug] emptied the bag");
                }
                ImGui::SameLine();
                if (ImGui::Button("Fill bag (Bag full test)"))
                {
                    bag.Add(MatId::Stone, bag.Room(MatId::Stone));
                    Log("[debug] filled the bag with stone");
                }
                ImGui::SameLine();
                if (ImGui::Button("Spawn copper here") && pickups)
                {
                    const Vector3 p = player->GetPosition();
                    for (int k = 0; k < 3; k++)
                        pickups->Spawn(MatId::Copper, 2, p.x, p.z);
                }

                int slots = bag.SlotCount();
                if (ImGui::SliderInt("Slot count", &slots, kBagSlotsStart, Inventory::kMaxSlots))
                    bag.SetSlotCount(slots);
                Pickups::Settings& ps = Pickups::GetSettings();
                ImGui::SliderFloat("Magnet radius", &ps.magnetRadius, 0.5f, 5.0f, "%.2f");
                ImGui::SliderFloat("Retry delay (bag full)", &ps.retryDelay, 0.2f, 3.0f, "%.1f s");
            }


            // ---- Cards
            Cards* cards = Manager::GetGameObj<Cards>();
            if (cards && player && ImGui::CollapsingHeader("Cards"))
            {
                ImGui::Text("Armor %d   crit %.0f%%   luck %.0f%%", player->GetArmor(),
                    player->GetCrit() * 100.0f, player->GetLuck() * 100.0f);

                bool any = false;
                for (int i = 0; i < kCardCount; i++)
                {
                    const int owned = cards->Owned((CardId)i);
                    if (owned <= 0)
                        continue;
                    const float* rc = kRarityColor[(int)kCard[i].rarity];
                    ImGui::TextColored(ImVec4(rc[0], rc[1], rc[2], 1.0f), "%s x%d", kCard[i].name, owned);
                    any = true;
                }
                if (!any)
                    ImGui::TextDisabled("No cards yet. Each mining level-up lets you pick one.");

                static int addIndex = 0;
                ImGui::SetNextItemWidth(180.0f);
                if (ImGui::BeginCombo("##addcard", kCard[addIndex].name))
                {
                    for (int i = 0; i < kCardCount; i++)
                        if (ImGui::Selectable(kCard[i].name, i == addIndex))
                            addIndex = i;
                    ImGui::EndCombo();
                }
                ImGui::SameLine();
                if (ImGui::Button("Add card"))
                {
                    cards->Grant((CardId)addIndex);
                    Log("[debug] added card %s", kCard[addIndex].name);
                }
                if (ImGui::Button("Force a level-up pick"))
                {
                    player->GainMiningXp((float)MiningXpToNext(player->GetMineLevel()) - player->GetMineXp(), 3);
                    Log("[debug] forced a level-up (crystal odds)");
                }
            }

            // ---- Juice
            if (ImGui::CollapsingHeader("Juice"))
            {
                Juice::Settings& js = Juice::GetSettings();
                auto row = [](const char* name, bool* on, float* strength, float maxValue)
                {
                    ImGui::PushID(name);
                    ImGui::Checkbox(name, on);
                    if (strength)
                    {
                        ImGui::SameLine(150.0f);
                        ImGui::SetNextItemWidth(-1.0f);
                        ImGui::SliderFloat("##strength", strength, 0.0f, maxValue, "%.2f");
                    }
                    ImGui::PopID();
                };
                row("Hit-stop", &js.hitStop, &js.hitStopStrength, 2.0f);
                row("Screen shake", &js.shake, &js.shakeStrength, 3.0f);
                row("Damage numbers", &js.numbers, &js.numberSize, 2.0f);
                row("Debris", &js.debris, &js.debrisAmount, 3.0f);
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::SliderFloat("##debrisSize", &js.debrisSize, 0.02f, 0.2f, "Debris size %.2f");
                row("Block wobble", &js.wobble, &js.wobbleStrength, 3.0f);
                row("Dust ring", &js.dustRing, nullptr, 0.0f);
                row("Level-up burst", &js.levelUpBurst, nullptr, 0.0f);
                ImGui::Text("Particles: %d / 320", Juice::ParticleCount());
            }

            // ---- Lighting (M7)
            if (ImGui::CollapsingHeader("Lighting"))
            {
                Lighting::Settings& ls = Lighting::GetSettings();
                ImGui::Text("Lights: %d requested, %d used (max 16)", Lighting::LightsRequested(), Lighting::LightsUsed());
                ImGui::Checkbox("Lighting on", &ls.enabled);
                ImGui::ColorEdit3("Ambient", ls.ambient);
                ImGui::SliderFloat("Lamp radius", &ls.lampRadius, 2.0f, 14.0f, "%.1f");
                ImGui::SliderFloat("Lamp intensity", &ls.lampIntensity, 0.0f, 5.0f, "%.2f");
                ImGui::Checkbox("Flicker", &ls.flicker);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(120.0f);
                ImGui::SliderFloat("Strength##flicker", &ls.flickerStrength, 0.0f, 4.0f, "%.1f");
                ImGui::Checkbox("Fog", &ls.fog);
                ImGui::SliderFloat("Fog start", &ls.fogStart, 0.0f, 30.0f, "%.1f");
                ImGui::SliderFloat("Fog end", &ls.fogEnd, 1.0f, 40.0f, "%.1f");
                ImGui::Separator();
                ImGui::Checkbox("Fog of war", &ls.fogOfWar);
                ImGui::SliderFloat("View = lamp +", &ls.viewExtra, 0.0f, 6.0f, "%.1f tiles");
                ImGui::SliderFloat("Seen brightness", &ls.seenBrightness, 0.0f, 1.0f, "%.2f");
                if (ImGui::Button("Reveal map") && map)
                {
                    map->RevealAll();
                    Log("[debug] map revealed");
                }
                ImGui::SameLine();
                if (ImGui::Button("Test flash at player") && player)
                {
                    const Vector3 p = player->GetPosition();
                    Lighting::Flash(p.x, 1.2f, p.z, 6.0f, 0xffa050, 5.0f);
                }
            }

            // ---- Shadows: the lamp is the main shadow caster; the moon is optional and faint
            if (ImGui::CollapsingHeader("Shadows"))
            {
                ShadowMap::Settings& ss = ShadowMap::GetSettings();

                ImGui::Checkbox("Lamp shadows (cube map, 6 extra passes)", &ss.lampShadows);
                const int sizes[3] = { 256, 512, 1024 };
                const char* sizeNames[3] = { "256", "512", "1024" };
                int sizeIndex = ss.lampFaceSize == 256 ? 0 : (ss.lampFaceSize == 1024 ? 2 : 1);
                if (ImGui::Combo("Lamp face size", &sizeIndex, sizeNames, 3))
                    ss.lampFaceSize = sizes[sizeIndex];
                ImGui::SliderFloat("Lamp bias", &ss.lampBias, 0.0f, 0.15f, "%.3f");
                ImGui::SliderFloat("Slope bias (all shadows)", &ss.slopeBias, 0.0f, 6.0f, "%.2f");

                if (ImGui::TreeNode("Moon (faint light from above, off by default)"))
                {
                    ImGui::Checkbox("Moon shadows", &ss.enabled);
                    ImGui::SameLine();
                    ImGui::Checkbox("3x3 PCF", &ss.pcf);
                    ImGui::SliderFloat("Moon intensity", &ss.moonIntensity, 0.0f, 2.0f, "%.2f");
                    ImGui::SliderFloat3("Moon direction", ss.moonDir, -12.0f, 12.0f, "%.1f");
                    ImGui::ColorEdit3("Moon colour", ss.moonColor);
                    ImGui::SliderFloat("Ambient in shadow", &ss.shadowedAmbient, 0.0f, 1.0f, "%.2f");
                    ImGui::SliderFloat("Moon bias", &ss.bias, 0.0f, 0.01f, "%.4f");
                    ImGui::SliderFloat("Box half size", &ss.boxHalfSize, 6.0f, 40.0f, "%.1f");
                    ImGui::TextDisabled("Moon shadow map (depth, red = far):");
                    ImGui::Image((ImTextureID)(intptr_t)ShadowMap::GetSRV(), ImVec2(160.0f, 160.0f));
                    ImGui::TreePop();
                }
            }


            // ---- Save
            if (ImGui::CollapsingHeader("Save"))
            {
                const SaveData::RecordsData& r = SaveData::Records();
                ImGui::Text("Deepest floor %d    runs %d", r.deepestFloor, r.runs);
                ImGui::Text("By class: Warrior %d  Miner %d  Mage %d  Rogue %d",
                            r.deepestByClass[0], r.deepestByClass[1], r.deepestByClass[2], r.deepestByClass[3]);
                ImGui::TextWrapped("Last: %s", SaveData::LastStatus());
                if (ImGui::Button("Open save folder"))
                    ShellExecuteW(nullptr, L"open", SaveData::Folder(), nullptr, nullptr, SW_SHOWNORMAL);
                ImGui::SameLine();
                if (ImGui::Button("Reset records"))
                {
                    SaveData::ResetRecords();
                    Log("[debug] records reset");
                }
            }

            // ---- Render
            if (ImGui::CollapsingHeader("Render"))
            {
                if (ImGui::Button(LanguageButtonLabel()))
                    ToggleLanguage();
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

