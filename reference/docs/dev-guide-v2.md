# Deepvein: Development Guide v2

Nov 25, 2026 · @Akkarin

## Status

The full prototype game (M0-M6) and the lighting and shadow milestones (M7-M8) run natively in DirectX 11, plus lamp shadows, fog of war and game juice. M9-M11 and the pixel-art UI remain. This guide replaces the original starter guide as the description of the game as built.

What changed from the original starter guide:

| Topic | Starter guide said | Built instead |
| --- | --- | --- |
| Project base | New CMake project, single main.cpp | The existing GM31 school template (Manager / GameObject / Scene / Renderer), Visual Studio 2026 solution, toolset v145 |
| Language | C++20 | C++14 (template default) |
| Characters | Pixel-art billboards | Monsters and items are 8x8 billboards; the player is still the 3D Mixamo model (CPU skinning) |
| Game UI | Sprite batch + bitmap font | Dear ImGui stand-ins for HUD, class select, cards, prompts |
| Text | text\_ja.json / text\_en.json | Code is English; Japanese lines in asset\\text\\ja.txt (English, tab, Japanese) |
| Save | nlohmann/json | Small hand-written JSON reader/writer, safe .tmp writes |
| Map size, darkness | 48x34, LOS darkness | 48x34; real point lights, lamp radius 3, near-black ambient, fog of war |
| Shadows | Moon shadow map only | Lamp cube shadows (main); moon shadows kept but off by default |
| Extras not in the guide | none | Bright Lamp card added; fog of war; game juice done |

## Milestones

&#91;embedded content: Deepvein roadmap · 4 phases, 1 gate\]

Art and UI work waits on the theme and mechanics decision, so nothing gets drawn twice. M9-M10 only matter if the 3D player model stays.

## How it's built

**Toolchain:** Visual Studio 2026 (platform toolset v145), C++14, Direct3D 11, DirectXTex, Assimp, Dear ImGui 1.92.7. The game runs on the GM31 template: a static `Manager` owns the current `Scene`, a scene holds `GameObject`s in draw layers, and `Renderer` is a static wrapper around the device and context. Updates run at a fixed 60 Hz.

**Modules (one .h/.cpp pair each):**

| Module | Job |
| --- | --- |
| TileMap | Floor generation, ore, mining, baked wall meshes, fog-of-war visibility and fog texture, block wobble |
| Player | Movement, class stats, skills, cards, lamp, damage, the 3D model |
| Monsters / MonsterData.h | Monster types, AI, billboard drawing, HP bars |
| Projectiles | Bolts, waves, bombs, rings |
| Cards / CardData.h | The 24 level-up cards, deals and the pick screen |
| ClassData.h | The four classes and their skills |
| Prompts | Stairs, restart and death prompts |
| HUD | ImGui HUD, class select, card pick |
| Lighting | Point lights (16 nearest, lamp in slot 0), flicker, flashes, fog |
| ShadowMap | Lamp cube shadows (6 faces) and the optional moon shadow map |
| Juice | Hit-stop, screen shake, damage numbers, debris, dust, level-up burst |
| SaveData | records.json and settings.json in %LOCALAPPDATA%\\Deepvein |
| Text | `Tr()` lookup into asset\\text\\ja.txt |
| DebugUI | The F1 tuning panel |
| GameState.h | Shared flags: paused, hit-stop timer, floor, game over |

**Frame order (Manager::Draw):**

1. `Renderer::Begin`
2. `Lighting::Upload` picks lights and fills buffer b6
3. `ShadowMap::BeginPass`
4. Moon shadow pass, only if moon shadows are on
5. Six lamp cube faces: every object's `DrawShadow`
6. `ShadowMap::EndPass` binds the shadow textures
7. Scene layers draw with the point-lit shader
8. `DebugUI::Draw` (HUD and F1 panel)

Update order matters for lighting: `Lighting::BeginFrame` runs at the start of `Manager::Update`, then objects add their lights, and the player sets the lamp and refreshes fog of war.

## Game systems

The numbers below are the current defaults. Most can be changed live in the F1 panel. If the mechanics change, this section is the one to rewrite.

**Floors.** Each floor is a 48x34 tile grid with rock, ore, crystals and a staircase down. Rock hardness and ore value rise with depth. Standing on the stairs opens a prompt: stay and mine, or descend (monsters below hit harder). Each new floor is recorded in the save file.

**Mining.** Hitting a block lowers its HP. A block that is too hard for the current mining power gives a "too hard" hit. Broken blocks give mining XP; mining levels raise mining power and deal a card pick. The Motherlode card adds extra ore weight to the next floors.

**Classes.** Four classes in ClassData.h, picked at the start of a run (keys 1-4):

| Class | Attack | Skill (K / Shift) | Perk |
| --- | --- | --- | --- |
| Warrior | Wide sword arc | Whirlwind, 4 s | Longer invincibility after a hit |
| Miner | Pickaxe | Dynamite, 5 s | 4 card choices, 1.5x mining XP |
| Mage | Magic bolts | Frost Nova, 6 s | Ranged attacks |
| Rogue | Fast daggers | Shadow Dash, 2.5 s | +15% crit chance |

**Cards.** Level-ups pause the game and deal cards (3, or 4 for the Miner) from the 24 in CardData.h: stats, mining power, skill upgrades, potions, Motherlode, and Bright Lamp (+2 lamp radius). `Player::ApplyCard` turns each card id into its effect.

**Monsters and potions.** Monster types live in MonsterData.h and draw as 8x8 billboards from one atlas. One or two potions lie on each floor and monsters can drop more. Q drinks one and heals 40% of max HP. Monsters on tiles the player can't see aren't drawn.

**Run end.** At 0 HP the death prompt shows the run; R opens a restart prompt any time (same class or new class). Best depth and totals go to records.json.

**Language.** English is in the code. Pressing the language button in F1 Render switches to Japanese from asset\\text\\ja.txt (UTF-8, one `English<TAB>Japanese` line per string). Missing lines fall back to English.

## Rendering

The cave should feel dark: the player's lamp is the main light, and anything outside it is black or dimly remembered.

**Lights.** `Lighting` collects point lights every frame: the lamp, crystals, stairs, and short flashes from breaks and explosions. The 16 lights nearest the player go into constant buffer b6, and the lamp is always slot 0. Shaders pointLitVS/PS add the lights, emission and distance fog.

| Setting | Default | Note |
| --- | --- | --- |
| Ambient | 0.025, 0.02, 0.035 | Near black |
| Lamp radius | 3.0 tiles | Bright Lamp card adds +2 |
| Lamp intensity | 2.6 | With flicker |
| Distance fog | 11 to 21 | From the camera |
| Fog of war | On | View radius = lamp radius + viewExtra (0) |
| Seen brightness | 0.2 | Remembered tiles out of sight |
| Moon | Off | Intensity 0.08 when on |

**Shadows.** The lamp casts omnidirectional shadows. Each frame the scene draws depth into the six faces of a 512 px cube map (bias 0.03), and the pixel shader compares against it using the largest axis of the light-to-pixel vector. The old moon directional shadow map (2048 px, PCF) still exists but is off by default, and its pass is skipped while off.

**Fog of war.** TileMap casts line of sight from the player within the view radius and writes a 48x34 R8 texture: black for never seen, `seenBrightness` for remembered, full for visible. Monsters, HP bars and projectiles on hidden tiles are not drawn, so nothing leaks through the dark.

**Shader slots:**

| Slot | Contents |
| --- | --- |
| b6 | Lights (16) and fog settings |
| b7 | Moon shadow matrix and lamp shadow params |
| t1 / s1 | Moon shadow map, comparison sampler |
| t2 | Lamp cube shadow map |
| t3 / s2 | Fog-of-war texture, linear clamp sampler |

**Juice.** Hit-stop pauses updates for a few frames on big hits. Screen shake, floating damage numbers, a pool of 320 debris cubes that cast shadows (size 0.06), block wobble, a dust ring on breaks and a level-up burst. Each effect has a switch and a strength in F1 Juice.

## Controls and F1 panel

| Key | Action |
| --- | --- |
| W A S D | Move |
| J / Space (hold) | Attack and mine |
| K / Shift | Class skill |
| Q | Drink a potion |
| R | Open the restart prompt |
| 1-4 | Pick a class or card; 1 / 2 at the stairs prompt |
| Esc | Close a prompt (keep playing) |
| F1 | Debug panel |
| G | New random floor (old test key, now also in F1) |

The F1 panel is for tuning while playing. Its sections:

- **Player:** class, position, HP, god mode, heal, skill cooldown, +1 potion
- **Mining:** mining level and power, give 10 XP, level up
- **Floor:** regenerate, next floor
- **Monsters:** spawn by type, kill all, freeze AI
- **Cards:** add any card, force a level-up pick
- **Juice:** every effect's switch and strength
- **Lighting:** ambient, lamp, flicker, fog, fog of war, reveal map, test flash
- **Shadows:** lamp shadows first; moon in a sub-section (off by default)
- **Save:** status, open save folder, reset records
- **Render:** language button, frame stats
- **Log:** recent game events

## Open decisions

The core loop works, so mechanics and theme can still change. Decide these before building more content, because they change the art list, the cards and the monster table.

**1. Theme.** The Art & Sprite Guide lists four options: Classic deep mine, Fungal caverns, Frozen depths, Lava foundry. A theme changes the palette, block textures, light colors (lamp, crystals), monster set and card names. Choosing one first lets the art and the remaining milestones follow it. The themes could also be layered by depth (for example mine, then fungal, then lava).

**2. Mechanic changes.** Write each idea as a short note here before coding it. Questions to answer for each:

- What does the player do differently each minute?
- Which existing systems does it touch (mining XP, cards, classes, light radius, fog of war)?
- Does it need new art or UI?
- Can it be tried with the F1 panel first?

Ideas that fit the systems already built:

| Idea | Builds on | Size |
| --- | --- | --- |
| Lamp fuel: the light shrinks over time, ore or pickups refill it | Lamp radius, fog of war | Small |
| Gold and a shop between floors | Ore, stairs prompt, cards | Medium |
| Floor objectives (break N blocks, find a key) to open the stairs | Stairs prompt, blocks left count | Small |
| Light-fearing monsters that flee the lamp or hunt in the dark | Lighting, monster AI | Medium |
| Placeable torches that leave permanent light | Lighting (16 lights), fog of war | Small |
| Boss floor every 5 floors | Monsters, floor generation | Large |

**3. Keep or replace the 3D player model.** Monsters are billboards but the player is a skinned 3D model. Either keep it and do M9/M10 (models, GPU skinning), or switch the player to a billboard so all characters match the pixel style.

## Next steps and known issues

**Next steps, roughly in order:**

1. Pick the theme and write down the mechanic changes (Open decisions above).
2. Pixel-art UI to replace the ImGui stand-ins (HUD, cards, prompts), using the Art & Sprite Guide.
3. Real sprites and block textures in place of the SpriteDef placeholders.
4. Fog-of-war polish: smooth fade at the edge, monster fade in and out, a tint for remembered tiles, clean wall edges.
5. Minimap built from the fog-of-war data.
6. M11: audio and bloom.
7. M9/M10: models and GPU skinning, if the 3D player stays.
8. Small juice: block crack stages, card deal animation.

**Known issues:**

- **Release build:** pointLitVS and pointLitPS have no Release shader settings in the project (only shadowAlphaPS does), so Release builds may miss those shaders. Copy the Debug HLSL settings to Release.
- **CPU skinning:** the player model is skinned on the CPU every frame. Fine for one model, slow for more.
- **Leftover template parts:** Score, Result and BestFloor from the template aren't used by the game.
- **G key:** still makes a new random floor in normal play; remove it or limit it to debug builds before release.
- **Not built yet:** block crack stages, card deal animation, pixel UI, audio.
