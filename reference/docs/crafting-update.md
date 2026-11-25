# Deepvein: Crafting Update for DirectX

Nov 26, 2026 · @Akkarin

This doc ports the **Deepvein Craft** browser prototype into the existing DirectX 11 game, one milestone at a time. It is an add-on to *Deepvein: Development Guide v2*: v2 still describes the engine, modules and rendering; this doc only lists what changes.

## How to use this doc

Give Claude Code one milestone (C0, C1, …) per session, not the whole doc. Each milestone lists the files it touches, the data to add and how to check it.

**Workflow rules stay exactly as before** (CLAUDE.md from the starter guide):

- Never run `git push`, and do not commit. You review and commit yourself. `.claude/settings.json` keeps denying `Bash(git push:*)`.
- After every task, Claude Code writes the report: what it added, what each part is for, which files changed, how to test it.
- Same base as v2: Visual Studio 2026, toolset v145, **C++14**, the GM31 template (`Manager` / `Scene` / `GameObject` / `Renderer`), fixed 60 Hz update, Dear ImGui.
- Same code rules: one `.h/.cpp` pair per module, data tables in `*Data.h` headers, ComPtr/RAII, no raw `new`/`delete` in game code, pools instead of per-frame allocation.
- English stays in code; Japanese goes in `asset\text\ja.txt` through `Tr()`.
- Every new value gets an F1 panel control before it gets art or UI polish.

**Prompt to start each session** (copy, then change the milestone id):

```
Read CLAUDE.md, Deepvein: Development Guide v2 and this doc's milestone C1.
Implement only C1. Do not commit or push. When done, write the report.
```

**Reference build:** the browser prototype *Deepvein Craft* (Version 11) is the gameplay reference. When this doc and the prototype disagree, this doc wins.

## What changes vs Guide v2

Mining stops giving levels: ore now drops materials for crafting, and levels with card picks come from killing monsters. Everything not in this table stays as v2 describes it.

| System | Guide v2 (now) | After this update |
| --- | --- | --- |
| Mining reward | Mining XP, mining level, card pick on level-up | Ore drops material pickups; no mining XP or mining level |
| Mining power | Mining level raises power; "too hard" hit at low level | Power = class mine + pickaxe tier bonus; ore needs a pickaxe of its tier, otherwise no damage |
| Levels and cards | From mining | From monster kills (XP per monster); same 3-card pick screen |
| Cards | 24 cards | 22 cards: Sharp Pick and Bright Lamp removed (crafted gear covers them) |
| Inventory | Potion counter only | Bag: 12 slots with stacks; materials, potions, bombs, torches |
| Gear | None | Craft menu (C): pickaxe, armor, lamp tiers + items + upgrades |
| Weapons | Class attack is fixed | 15 weapons, locked by class; each has attack style, element and weapon skill; swap with X |
| Skills | Class skill (K / Shift) | Class skill + weapon skill (Z / middle click) |
| Elements | Only Mage freeze, Rogue poison card | Fire, Frost, Shock, Venom, Earth from the weapon; monster weaknesses |
| Monsters | 4 types | 6 types: + Ember Slime, Frost Bat (floor 2+) |
| First map | Floor 1 dungeon | Floor 0 green field (no monsters, sun light, lamp off), gate to floor 1 |
| Settings | F1 panel only | Player options menu (Esc) saved to settings.json; F1 stays for tuning |
| Restart / language buttons | Restart: R key prompt; language: button in F1 Render (the HUD has no buttons) | Inside the options menu (R key still restarts) |

## Implementation order

Ten milestones, each playable and testable on its own. Do them in order: each one builds on the data from the one before.

**Order fix: do C4 before C2.** C2 removes mining XP, and today mining XP is the only source of level-ups (`Player::GainMiningXp` calls `Cards::QueueLevelUp`). In the listed order, C2 and C3 would have no card picks at all, so they are not playable on their own. With C4 first, kill XP is already working when mining XP goes. Order: C0, C1, C4, C2, C3, then C5 to C9. Each card removal moves to the milestone that replaces it: Sharp Pick goes in C2 (pickaxe tiers), Bright Lamp in C3 (lamp tiers).

| # | Milestone | New files | Changed files |
| --- | --- | --- | --- |
| C0 | Data headers and state, no behaviour change | `MaterialData.h`, `CraftData.h`, `WeaponData.h`, `ElementData.h` | `GameState.h` |
| C1 | Bag and floor pickups | `Inventory.h/.cpp`, `Pickups.h/.cpp` | `HUD`, `DebugUI`, `Player`, `Monsters` |
| C2 | Ore drops replace mining XP; pickaxe gate | – | `TileMap`, `Player`, `Monsters` (drops) |
| C3 | Craft menu: gear tiers, items, upgrades, consumables | `Crafting.h/.cpp` | `Player`, `HUD`, `Lighting` (torches) |
| C4 | Combat levels from kills; card pool edit | – | `Player`, `Monsters`, `Cards`, `CardData.h` |
| C5 | Weapons and class locks; X to swap | – | `Player`, `Projectiles`, `ClassData.h` |
| C6 | Elements, weaknesses, status effects, 2 new monsters | `Elements.h/.cpp` | `Monsters`, `MonsterData.h`, `Projectiles` |
| C7 | Weapon skills on Z | `WeaponSkills.h/.cpp` | `Player`, `HUD` |
| C8 | Floor 0 green field, sun shadows, lamp off | – | `TileMap`, `Lighting`, `ShadowMap` |
| C9 | Options menu and settings.json | `OptionsMenu.h/.cpp` | `SaveData`, `HUD`, `Renderer`, `Manager` |

UI in every milestone uses ImGui stand-ins, same as v2's HUD and card pick. The pixel-art UI milestone in v2 replaces them later.

## Still to do from Guide v2

Guide v2 left 14 items unfinished: 3 open decisions, 7 next steps and 4 known issues. They stay on the list; the table says where each one fits around C0–C9 so the two plans don't collide. "Mechanic changes" (open decision 2) is answered by this doc.

| Item (from v2) | Type | Do it | Why there / what changes because of this update |
| --- | --- | --- | --- |
| Release build: pointLitVS/PS have no Release shader settings | Known issue | **P0, before C0** | Five-minute fix; copy the Debug HLSL settings. New shaders in this update would hit the same problem. |
| Leftover template parts (Score, Result, BestFloor) | Known issue | P0 | Remove before adding new modules, so nothing new gets wired to them. Score is still called from `Player::GainMiningXp` and from `Enemy.cpp` (another template file that is still compiled), and `Result.h` is included by `Game.cpp` and `manager.cpp`. Remove those references along with the files. |
| G key makes a new floor in normal play | Known issue | P0 | Delete the G hotkey binding. F1 already has the "New random floor" button, so the action stays available there; C8 adds "go to floor 0/1" next to it. |
| Small juice: block crack stages | Next step | After C2 | Show 3 crack stages from block damage; on locked ore show no cracks, so the pickaxe gate reads clearly. |
| Small juice: card deal animation | Next step | After C3 | Cards now come from kills; the same animation also suits the "Crafted X!" burst. |
| Fog-of-war polish: edge fade, monster fade in/out, remembered-tile tint, clean wall edges | Next step | After C8 | Floor 0 is fully revealed, so polish only affects floor 1+. Pickups and torches must follow the same hide rules as monsters. |
| Minimap from fog-of-war data | Next step | After C8 | Add pickups (dots by material colour), torches and the gate/stairs; hide on floor 0 or show the whole field. |
| 1. Theme (deep mine / fungal / frozen / lava, or layered by depth) | Open decision | **Decide before real sprites** | The element system fits layering well: e.g. frozen floors spawn more Ember Slimes (weak to frost) and lava floors more Frost Bats. |
| 3. Keep or replace the 3D player model | Open decision | Decide before M9/M10 | With 15 weapons the 3D model would need 15 weapon models and attack animations; a billboard player only needs weapon sprites. |
| Real sprites and block textures (replace SpriteDef placeholders) | Next step | After the theme decision and C6 | New art from this update: 11 material icons, 15 weapon icons, Ember Slime, Frost Bat, element status effects, grass, hedge, tree, gate. |
| Pixel-art UI (replace ImGui stand-ins) | Next step | **After C9** | Wait until every new screen exists: bag strip, craft menu, weapon-skill button, Level/Pick/Weapon stats, options menu, plus v2's HUD, cards and prompts. |
| M9/M10: models and GPU skinning | Milestone | Only if the 3D player stays | Untouched by this update. CPU skinning stays as it is until then. |
| M11: audio and bloom | Milestone | Last | New sounds: pickup, bag full, craft, equip, each element hit, freeze, shatter, chain lightning, each weapon skill, gate. Bloom suits torches, fire and lightning. |
| v2 mechanic ideas not chosen: lamp fuel, gold and shop, floor objectives, light-fearing monsters, boss floor every 5 | Backlog | Later | Placeable torches is now done (C3). A shop would sit on floor 0; lamp fuel would be a craftable item. |

### Full order with both plans

1. **P0:** Release shader settings, remove template leftovers, delete the G hotkey (the F1 button stays).
2. **C0–C2:** (in the order C0, C1, C4, C2): data headers, bag and pickups, combat levels and the card pool, then ore drops and the pickaxe gate; then block crack stages.
3. **C3–C4:** crafting (C4 is already done above); then the card deal animation.
4. **C5–C7:** weapons, elements, weapon skills.
5. **C8:** floor 0; then fog-of-war polish and the minimap.
6. **C9:** options menu.
7. **Decide** the theme and the 3D player question.
8. Real sprites and textures, then the pixel-art UI.
9. M9/M10 only if the 3D player stays; M11 audio and bloom last.

## C0–C2: Materials, drops and the bag

Broken ore and dead monsters drop pickups; walking near them pulls them into a 12-slot bag. This replaces mining XP entirely.

### Materials (`MaterialData.h`, C0)

| Id | Name (en / ja) | Stack per slot | Use | Comes from |
| --- | --- | --- | --- | --- |
| Stone | Stone / 石 | 20 | material | every ore block (1–2) |
| Copper | Copper / 銅 | 20 | material | copper ore (2–3) |
| Silver | Silver / 銀 | 20 | material | silver ore (2–3) |
| Gold | Gold / 金 | 20 | material | gold ore (2) |
| Crystal | Crystal / クリスタル | 20 | material | crystal (1–2); Frost Bat 15% |
| Gel | Gel / ジェル | 20 | material | Cave Slime (1–2, 100%), Goblin (1, 30%), Ember Slime (1–2) |
| Fang | Fang / 牙 | 20 | material | Gloom Bat (1, 70%), Goblin (1–2, 80%), Frost Bat (1, 70%) |
| Core | Stone Core / 岩のコア | 10 | material | Stone Brute (1, 100%, + 2–3 stone); Ember Slime 15% |
| Potion | Potion / 回復薬 | 5 | Q: heal 40% max HP | floor (1–2), any kill 6%, craft |
| Bomb | Bomb / 爆弾 | 5 | E: throw | craft |
| Torch | Torch / たいまつ | 5 | F: place light | craft |

The Miner trait changes from "4 card choices, 1.5× mining XP" to **+1 ore from every block**.

```cpp
// MaterialData.h (C++14)
enum class MatId : uint8_t { Stone, Copper, Silver, Gold, Crystal, Gel, Fang, Core, Potion, Bomb, Torch, Count };
struct MatDef { const char* name; uint8_t stack; bool usable; uint32_t color; int spriteIndex; };
extern const MatDef kMats[(int)MatId::Count];
struct DropDef { MatId id; uint8_t min, max; float chance; };   // per monster type, in MonsterData.h
```

### Bag (`Inventory.h/.cpp`, C1)

- Fixed array of slots, no heap: `struct Slot { MatId id; uint8_t n; bool used; }; Slot slots[20]; int slotCount = 12;` (20 = max after two Bigger Bag upgrades).
- `int Count(MatId)`, `int Room(MatId)`, `int Add(MatId, int n)` returns how many fit (fills existing stacks, then empty slots), `void Take(MatId, int n)`, `bool CanPay(const Cost&)`, `void Pay(const Cost&)`.
- `int RoomAfterPay(const Cost&, MatId)` for crafting: copy the 20 slots to a local array, pay, measure, discard the copy.
- Drop a stack: removes the slot and spawns a pickup at the player that cannot be grabbed until the player has moved 1.1 tiles away.

### Pickups (`Pickups.h/.cpp`, C1)

- Potions already exist in two places and move into this system in C1: `Monsters` keeps floor potions (`m_Potions`, 1–2 per floor, 12% kill drop) with its own pickup code, and `Player` keeps a potion counter. C1 moves floor potions and kill drops into `Pickups` and the counter into the bag, so the game has one pickup system and Q reads the bag. The kill drop also goes down from 12% to 6%, as in the Materials table.
- Pool of 128 pickups (`std::array`, `active` flag). One pickup per material type per break, carrying a count.
- Spawn pops up: `vy = 2.6–4.0`, sideways speed 1.0–2.2 in a random direction, gravity 14, bounce off walls/blocks by flipping velocity × −0.4, stop on landing.
- On the ground: pulled toward the player within 1.7 tiles at speed `2.5 + 6×(1 − d/1.7)` tiles/s; collected within 0.45.
- Partly full bag: take what fits, leave the rest, wait 1.2 s before retrying, show "Bag full" at most every 1.5 s.
- Draw as 8×8 billboards from the same atlas as monsters (height 0.42; potion 0.55), with a small idle bob.

### Ore drops and the pickaxe gate (C2)

- Remove `GainMiningXP`, the mining level and its HUD bar. `BreakBlock` now spawns: metal of the ore's tier (copper 2–3, silver 2–3, gold 2, crystal 1–2, +1 for Miner) and 1–2 stone.
- A pickaxe of tier *t* can mine ore tier *t*: Stone Pick (0) → copper, Copper Pick (1) → silver, Silver Pick (2) → gold, Gold Pick (3) → crystal, Crystal Drill (4) only adds power.
- Hitting locked ore does **no damage**: small shake, grey debris, and a floating "Need a Copper Pick" at most every 0.9 s. Locked ore within 6 tiles shows the needed pick name above it in the ore tier's colour.
- Floor generation: the first 3 ore clusters of every floor are copper, so the first craft is always reachable.
- Mining power = class mine + pick bonus (0, 2, 4, 7, 11).

## C3: Crafting

The craft menu (C, or a HUD button) pauses the game and lists four groups: Gear, Weapons, Items, Upgrades (Weapons stays hidden until C5). A row is enabled only when the bag holds the cost (and, for items, has room for the result).

### Gear tiers (`CraftData.h`)

One slot each, crafted one tier at a time. The menu shows only the next tier of each slot, or "Maxed".

| Gear | Tier 1 | Tier 2 | Tier 3 | Tier 4 | Effect per tier |
| --- | --- | --- | --- | --- | --- |
| Pickaxe | Copper Pick: 4 copper, 3 stone | Silver Pick: 4 silver, 2 copper | Gold Pick: 4 gold, 2 silver | Crystal Drill: 3 crystal, 2 gold | mine bonus 0/2/4/7/11 and unlocks ore tier |
| Armor | Copper Mail: 6 copper, 2 gel | Silver Mail: 5 silver, 3 gel | Gold Mail: 5 gold, 1 core | Crystal Mail: 4 crystal, 2 core | armor 0/1/2/3/5, max HP +0/4/8/13/20 |
| Lamp | Copper Lantern: 2 copper, 2 gel | Gold Lantern: 2 gold, 3 gel | Crystal Lantern: 2 crystal, 1 gold | – | lamp radius +0/1/2/3 (replaces the Bright Lamp card) |

Armor from gear and armor from the Iron Skin card add together: `armor = gearArmor[t] + armorFromCards`. When armor tier rises, add the max-HP difference to both `maxHp` and `hp`.

### Items (go into the bag)

| Item | Cost | Use |
| --- | --- | --- |
| Potion | 3 gel | Q: heal 40% of max HP; refused at full HP |
| Bomb | 4 stone, 1 copper | E: thrown 3.4 tiles toward the aim, blast radius 1.5 × blast multiplier, damage attack × 2, mining × 2 |
| Torch | 2 stone, 1 gel | F: placed at your feet, lights the floor until you leave it; max 6 per floor; not allowed on floor 0 |

Miner's Dynamite and the crafted bomb share one projectile type with its own radius, damage and mining values.

### Upgrades

| Upgrade | Cost | Max | Effect |
| --- | --- | --- | --- |
| Bigger Bag | 4 gel, 2 fang | 2 | +4 bag slots |
| Heart Charm | 1 crystal, 4 gel | 3 | +8 max HP, heal 8 |
| Swift Boots | 3 silver, 3 fang | 2 | +10% move speed |

### Torches and lights

Torches use v2's light list: each is a point light (colour #ff9a40, radius 7.5) with flicker. Keep the 16-nearest rule; torches compete with crystals and flashes like any other light. Torch model: a thin stick plus a small emissive flame.

### Craft menu layout (ImGui stand-in)

- Left column: bag grid (4 per row) with an × button per slot to drop the stack, then the equipped list (weapon, pick, armor, lamp).
- Right column: groups, each row = icon, name, description, cost chips (have/need, red when short), and a button: Craft / Need more / Bag full / Maxed / Equip / Equipped.
- After a craft: keep the menu open, show "Crafted X!" at the top, play the level-up burst at the player, log it.
- C or Esc closes; the game resumes.

## C5 + C7: Weapons and weapon skills

The weapon now decides the attack shape, reach, speed, mining strength, element and second skill. The class still decides HP, move speed, class skill and trait, whatever weapon is held.

### Rules

- **Locked by class.** A class can craft and hold only its own class weapon plus weapons of its families. Other weapons are hidden from its craft menu.
  - Warrior: blade, heavy · Miner: heavy, pole · Mage: magic, shot · Rogue: knife, shot
- Each class starts owning and holding its class weapon. Crafted weapons stay owned for the run; **X** cycles owned weapons, or press Equip in the craft menu.
- Damage for attacks, skills and bombs: `AttackNow() = baseAttack + weapon.atk`, where `baseAttack` = class attack + Heavy Arm cards. No class bonus multiplier.
- Attack cooldown = `weapon.cd / attackSpeed`. Equipping resets the attack timer. The player's element = the held weapon's element (or none).
- Melee uses v2's arc attack with the weapon's range and arc; Broad Blade still multiplies both. Bolts use v2's `Projectiles` with the weapon's speed, pierce count and colour; Split Bolt still fans any bolt weapon into 3.
- HUD: a Weapon stat shows "Name · Element" in the element colour.

```cpp
// WeaponData.h (C++14)
enum class Elem : uint8_t { None, Fire, Frost, Shock, Venom, Earth };
enum class WFam : uint8_t { Class, Blade, Pole, Shot, Knife, Heavy, Magic };
struct WeaponDef {
  const char* id; const char* name; int classLock;   // classLock = class index for class weapons, -1 otherwise
  WFam fam; Elem elem; bool bolt;
  float cd, range, arc; int atk; float mine;           // melee: range/arc in tiles/radians
  float boltSpeed; int pierce; float burst, shock; bool wave; float crit;
  Cost cost; int skill;                                 // index into kWeaponSkills
};
```

### Weapon table

| Weapon | Class / family | Element | Type | Cooldown (s) | Reach, arc | Atk | Mining × | Extra | Cost |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Warrior's Sword | Warrior only | – | melee | 0.45 | 1.4, 1.9 | +0 | 1.0 | – | starter |
| Miner's Mattock | Miner only | – | melee | 0.40 | 1.15, 1.3 | +0 | 1.3 | – | starter |
| Mage's Staff | Mage only | – | bolt | 0.50 | speed 11 | +0 | 1.0 | – | starter |
| Rogue's Daggers | Rogue only | – | melee | 0.20 | 1.05, 1.4 | +0 | 0.9 | – | starter |
| Ember Sword | blade | Fire | melee | 0.42 | 1.4, 1.8 | +2 | 1.0 | – | 5 copper, 2 stone, 1 gel |
| Crystal Blade | blade | Frost | melee | 0.42 | 1.5, 2.0 | +9 | 1.2 | slash wave every swing | 4 crystal, 1 core |
| Quarry Axe | heavy | Earth | melee | 0.75 | 1.55, 2.3 | +6 | 1.6 | – | 5 silver, 4 stone |
| Thunder Hammer | heavy | Shock | melee | 0.80 | 1.3, 1.5 | +6 | 2.0 | shockwave r 1.2 at 1.1 tiles ahead, 50% damage | 4 gold, 5 stone |
| Barbed Spear | pole | Venom | melee | 0.50 | 2.4, 0.5 | +3 | 0.8 | – | 4 copper, 3 stone, 1 fang |
| Rockbreaker Maul | pole | Earth | melee | 0.70 | 2.0, 0.9 | +5 | 1.8 | – | 3 gold, 6 stone, 1 core |
| Fire Staff | magic | Fire | bolt | 0.60 | speed 10 | +4 | 1.0 | bursts r 1.1, 70% damage | 3 gold, 1 crystal, 3 gel |
| Frost Wand | magic | Frost | bolt | 0.38 | speed 13 | +2 | 0.6 | – | 3 silver, 1 crystal, 2 gel |
| Spark Crossbow | shot | Shock | bolt | 0.55 | speed 15 | +2 | 0.6 | pierces 1 | 3 copper, 2 silver, 2 fang |
| Venom Blowgun | shot | Venom | bolt | 0.42 | speed 16 | +1 | 0.4 | pierces 2 | 2 copper, 3 fang, 2 gel |
| Frostfang Daggers | knife | Frost | melee | 0.18 | 1.05, 1.4 | +1 | 0.7 | +15% crit | 4 silver, 3 fang |

Bolt range: lifetime = 12 / speed seconds (about 12 tiles). Shockwaves and bursts are soft explosions: flash 2, shake 0.1, fewer debris than a bomb.

### Weapon skills (C7, `WeaponSkills.h/.cpp`)

Z, middle click, or the HUD button (with its own cooldown fill). One shared timer for all weapons, so swapping right after a skill gives no free second cast. Cooldown is multiplied by the Focus card. Every skill hit goes through `HitMonster`, so it carries the weapon's element.

| Weapon | Skill | Cooldown (s) | Effect |
| --- | --- | --- | --- |
| Warrior's Sword | Lunge | 5 | Dash 0.10 s at 24 tiles/s, hits each monster passed for 150% damage |
| Miner's Mattock | Ground Slam | 6 | Explosion at the player: r 1.7, 120% damage, mining × 3 |
| Mage's Staff | Blink | 5 | Teleport up to 3.2 tiles toward the aim (first free spot in line of sight), 0.3 s invincible |
| Rogue's Daggers | Fan of Knives | 5 | 10 bolts in a circle, speed 12, life 0.35 s, 80% damage |
| Ember Sword | Flame Arc | 5 | Melee arc 4.2 rad, reach 1.9 × Broad Blade, 180% damage |
| Crystal Blade | Crystal Storm | 6 | 8 slash waves in all directions |
| Quarry Axe | Earthsplitter | 6 | 4 soft explosions in a line, 0.95 tiles apart, 0.08 s apart, r 0.85, mining × 1.5 |
| Thunder Hammer | Thunderclap | 7 | Explosion r 2.5, 160% damage; monsters within 2.8 stunned 1.2 s |
| Barbed Spear | Javelin | 4 | Bolt speed 16, pierces everything, life 0.7 s, 200% damage |
| Rockbreaker Maul | Quake | 8 | Breaks every ore within 2.8 tiles that the pick allows (staggered 0.07 s per tile of distance); monsters within 2.8 hit and stunned 1.4 s |
| Fire Staff | Meteor | 7 | Warning ring 4 tiles ahead, 0.45 s later explosion r 2, 250% damage, mining × 2 |
| Frost Wand | Ice Nova | 8 | Freezes all monsters within 3.2 for 2.2 s, 80% damage |
| Spark Crossbow | Volley | 5 | 5 bolts, 0.2 rad apart |
| Venom Blowgun | Toxic Cloud | 7 | Cloud r 1.7, 3 tiles ahead, 6 ticks every 0.5 s: poisons and deals 35% damage |
| Frostfang Daggers | Blizzard Step | 5 | Dash 0.15 s, freezes every monster passed for 1.6 s |

Rogue's class skill Shadow Dash and the dash skills share one dash routine; a `dashFreezes` flag picks the Blizzard Step behaviour.

## C6: Elements, weaknesses and new monsters

Every player hit carries the held weapon's element. Monsters take 1.5× from their weak element and 0.4× from a resisted one, so the player swaps weapons to match the monster.

### Element effects (`Elements.h/.cpp`)

| Element | Colour | On every hit | Special |
| --- | --- | --- | --- |
| Fire | #ff7a3a | Burn 3 s: tick every 0.5 s for `1 + floor/2 + ceil(attack × 0.25)` (× 0.4 if fire-resistant) | A burning monster that dies sets every monster within 2.2 tiles burning |
| Frost | #7fe0ff | +1 chill stack (resets after 2.5 s without a hit); chilled = 55% speed | 3 stacks = frozen 1.6 s: cannot move or deal contact damage. A frozen monster that dies shatters: 30% of its max HP to monsters within 1.9 |
| Shock | #fff07a | Lightning jumps to the 2 nearest monsters within 2.8 tiles for 60% damage (their own weakness applies) | Draw a jagged line (7 points) for 0.18 s + a short flash; chain hits do not chain again |
| Venom | #9fe05a | Poison 4 s (ticks add `ceil(attack × 0.2)`) | A poisoned monster drops its whole drop table twice |
| Earth | #d0a060 | Knockback × 2, stagger 0.35 s (no movement) | Mining × 1.5. Breaking ore also breaks every same-tier ore next to it 0.14 s later, so a whole vein falls (only ore the pick can mine) |

Status fields to add to the monster struct: `burn, burnTick, chill, chillTimer, frozen, stun, venomMarked`. Movement speed factor each update: `frozen or stun → 0`, Mage Frost Nova slow → 0.3, chilled → 0.55, else 1.

Visuals: the monster billboard tints (frozen #4ab8e0, burning flickers #9a3a00/#5a1a00, chilled #1f6a7a, poisoned #2a4a10) and burning spawns small orange debris. Damage numbers use the element colour, "!" after a weak hit, "…" after a resisted one (crit gold still wins).

### Weakness table (`MonsterData.h`)

| Monster | HP | Weak to | Resists | XP | Appears |
| --- | --- | --- | --- | --- | --- |
| Cave Slime | 8 | Shock | Venom | 3 | floor 1+ |
| Gloom Bat | 4 | Fire | – | 2 | floor 1+ |
| Goblin | 12 | Shock | – | 6 | floor 2+ |
| Stone Brute | 32 | Frost | Venom | 15 | floor 3+ |
| **Ember Slime** (new) | 10 | Frost | Fire | 4 | floor 2+ |
| **Frost Bat** (new) | 6 | Fire | Frost | 3 | floor 2+ |

New monsters reuse existing AI: Ember Slime = slime hop (atk 3, speed 1.1, sight 6), Frost Bat = bat flutter (atk 2, speed 3.5, sight 8). Sprites: recoloured slime (orange #e0603a, highlight #ffd08a) and bat (ice blue #6fc8e0).

**Weakness marker:** a small diamond in the weak element's colour above every monster within 8 tiles, drawn bigger when the held weapon's element matches. It can be turned off in Options.

## C4: Combat levels and cards

Kills give XP; each level-up gives +2 max HP and opens the same 3-card pick screen v2 already has. Only the trigger and the rarity source change.

- XP needed for the next level: `8 + (level − 1) × 7` (the old mining curve).
- XP per kill: `round(baseXP × (1 + 0.15 × (floor − 1)))` with baseXP from the weakness table above.
- Level-ups queue, as before (several picks in a row are fine). Every class picks from **3** cards; the Miner's 4th choice is gone with its old trait.
- Card rarity odds use the killing monster's tier instead of the ore tier: slime and bat = row 0 (72/25/3), goblin, Ember Slime and Frost Bat = row 1 (42/46/12), brute = row 2 (16/52/32). Prospector and `0.03 × (level − 1)` still shift odds toward rare.
- Pick screen text: "Level N! Choose a card — you levelled up by slaying a Goblin. Tougher monsters mean rarer cards."
- HUD: replace the Mine Lv stat with **Level** + XP bar; add a **Pick** stat showing the pickaxe name in its tier colour.

### Card pool changes (`CardData.h`)

| Card | Change |
| --- | --- |
| Sharp Pick | Removed in C2, together with the pickaxe tiers |
| Bright Lamp | Removed in C3, together with the lamp tiers |
| Potion Pouch | Adds 2 potions to the bag; any that don't fit drop at the player's feet |
| Iron Skin | Adds to `armorFromCards`, so it stacks with crafted armor |
| Big Bomb | Also enlarges crafted bombs (shared blast multiplier) |
| Prospector | Text: "Level-up cards lean rarer" |
| Sword Wave, Chain Blast, Split Bolt, Venom Blade | Unchanged; they work with any weapon the class can hold |
| All others | Unchanged |

The death and restart prompts show `class · floor · Level · kills · crafted count` instead of mining level and card count. The restart prompt adds "everything in your bag will be lost". The stairs prompt now says "N ore blocks still here. Those materials stay behind if you go."

## C8: Floor 0, the green field

Every run and every restart starts on floor 0: a safe daylight field with the dungeon gate at the north end. Walking onto the gate opens the stairs prompt; entering goes to floor 1.

### Layout (`TileMap`)

- Whole map = wall, then open tiles x 5–30, y 5–22 (26 × 18). Gate tile (17, 5) = stairs. Player starts at (17.5, 18.5).
- No ore, monsters, potions or respawns. Torches cannot be placed ("It's bright out here").
- Ground: grass texture (64 px, greens with a few flowers, tiled every 4 tiles). Border walls draw with a hedge texture at height 0.7–0.9 instead of stone.
- A ring of trees 2 tiles outside the field, every 3rd tile: cylinder trunk (0.8 high) + 7-sided cone leaves (1.5 high), scale 0.9–1.4.
- Gate: two stone pillars (0.32 × 1.7) and a lintel, two torches beside it, the label "Dungeon" / "ダンジョン" above it.
- Fog of war: fully revealed on floor 0 (fill the R8 texture with 255). Normal rules from floor 1.
- HUD floor shows "Town" / "町". Best-depth records count dungeon floors only.

### Lighting (`Lighting`, `ShadowMap`)

Reuse v2's existing moon directional light and its 2048 px shadow map as the **sun**. Do not write a second shadow system.

| Setting | Floor 0 | Floor 1+ (unchanged v2 values) |
| --- | --- | --- |
| Player lamp | Intensity 0, cube shadows skipped | On, cube shadows on |
| Sun (moon light) | On: colour #fff0d0, intensity 1.25, from (W/2 − 10, 24, H/2 + 12) toward the field centre, ortho box ±19 × ±15, near 1, far 70 | Off (or v2 moon settings if the F1 toggle is on) |
| Ambient | #fff0d8 × 0.42 | v2 near-black |
| Distance fog | 14 to 30, colour #9cc4d8; clear colour the same | 11 to 21, v2 colour |

Switch these in one place when a floor is generated (e.g. `Lighting::ApplyFloorPreset(int floor)`), so `Lighting::BeginFrame` and the lamp code don't need per-frame floor checks beyond the lamp intensity.

Prompt on the gate: title "Dungeon entrance", text "Ready? Monsters wait inside.", buttons "Stay in town" / "Enter". Entering floor 1 logs "You step into the dungeon."

## C9: Options menu

A player-facing options menu opens with Esc (when no prompt is open) or a HUD "Options (Esc)" button, pauses the game, and saves to settings.json. The F1 debug panel stays separate and unchanged.

### Settings (`OptionsMenu.h/.cpp`)

| Setting | Range / values | Default | Applies to |
| --- | --- | --- | --- |
| Language | English / 日本語 | English | `Tr()` (moves from the F1 Render button) |
| Brightness | 60–160%, step 5 | 100% | a `brightness` multiplier on the final colour in pointLitPS (add it to buffer b6) |
| Render quality | Low / Medium / High | High | lamp cube shadow size 256 / 512 / 1024 and sun shadow 1024 / 2048 / 2048 |
| Shadows | On / Off | On | skip both shadow passes when off |
| Camera distance | 75–135%, step 5 | 100% | multiplies the camera's height and back offset |
| Screen shake | 0–100%, step 10 | 100% | multiplies Juice shake strength |
| Damage numbers | On / Off | On | monster damage numbers only |
| Weakness markers | On / Off | On | the element diamonds |

Below the settings: a read-only controls list (WASD, attack, class skill, Z weapon skill, X swap, C craft, Q/E/F items, R restart, Esc options), then buttons **Back to game**, **Restart run** (only during a run; opens the existing restart prompt), **Reset settings**.

### Behaviour

- Esc priority: close the open prompt or menu first; open Options only when nothing is open. This keeps v2's "Esc closes a prompt".
- Options can open from the class select screen; closing returns to it.
- The language button moves from F1 Render into Options, and Options gets a Restart button. R still opens the restart prompt directly.
- Sliders apply live; toggles and segments apply on click.

### settings.json

Keep v2's file and safe `.tmp` write. Put the player options under their own key so they never overwrite F1 tuning values:

```json
{
  "options": { "language": "en", "brightness": 100, "quality": 2, "shadows": true,
               "cameraDistance": 100, "shake": 100, "damageNumbers": true, "weaknessMarkers": true },
  "...existing F1 keys stay as they are...": null
}
```

Missing keys fall back to the defaults above, so old settings files keep loading.

## F1 panel, text and save data

Each milestone adds its F1 controls in the same session, so every feature can be tested before its art exists.

### F1 panel additions (`DebugUI`)

| Section | New controls | Added in |
| --- | --- | --- |
| Mining | Remove mining level/XP controls; add pickaxe tier slider (0–4) | C2 |
| Bag (new) | Add material × 1 / × 10 per type, clear bag, slot count, pickup magnet radius, "Bag full" test | C1 |
| Crafting (new) | Armor and lamp tier sliders, "free crafting" toggle (ignores cost), give one of each item | C3 |
| Player | Level and XP, give 10 XP, force level-up pick (moved from Mining) | C4 |
| Weapons (new) | Equip any weapon of the current class, give all class weapons, reset weapon-skill cooldown, debug-only "ignore class lock" toggle (off by default, never saved) | C5, C7 |
| Elements (new) | Status durations and multipliers (weak 1.5, resist 0.4, chain 60%, shatter 30%), show weakness markers | C6 |
| Monsters | Spawn Ember Slime and Frost Bat; freeze / burn / poison all | C6 |
| Floor | Go to floor 0, go to floor 1, sun direction and intensity | C8 |
| Render | Language button moves to Options; keep frame stats | C9 |

### Text (`asset\text\ja.txt`)

Add one `English<TAB>Japanese` line per new string. Names and descriptions come straight from this doc's tables; the prototype's Japanese is the reference. Examples:

```
Craft (C)	クラフト (C)
Need a Copper Pick	銅のツルハシが必要
Bag full	カバンがいっぱい
Ember Sword	炎の剣
Frozen!	凍結!
Dungeon entrance	ダンジョンの入口
Options (Esc)	設定 (Esc)
```

Strings built with numbers ("+3 Copper", "Level 4!") use a format string per language, not English concatenation.

### Save data (`SaveData`)

- records.json: unchanged keys; best depth ignores floor 0. Optional new totals: items crafted, weapons crafted.
- settings.json: new `options` object (C9 above).
- Bag, gear, weapons and cards are **per run** and are not saved. A future town storage chest would add a new file rather than change these two.

## Acceptance checks and conflict notes

A milestone is done when its checks pass in a Debug build with the D3D debug layer showing no new warnings and no new leaks at exit.

### Checks per milestone

| # | Check |
| --- | --- |
| C0 | Game plays exactly as before; new headers compile; no new warnings |
| C1 | F1 "add 10 copper" fills the bag strip; pickups pop, get pulled in, stop at a full bag and are picked up after freeing a slot; a dropped stack is not re-grabbed until you step away |
| C2 | Breaking copper drops copper + stone; silver ore shows "Copper Pick" and takes no damage with the Stone Pick; no mining XP or mining level anywhere; first 3 clusters per floor are copper |
| C3 | Copper Pick craft unlocks silver; armor tier raises armor and max HP; Q/E/F use items from the bag; torch lights the floor and is limited to 6; Bag full blocks item crafts |
| C4 | Killing a goblin gives XP and a level-up pick; no level-up comes from mining once C2 lands; Sharp Pick (after C2) and Bright Lamp (after C3) never appear; Potion Pouch overflow drops at your feet |
| C5 | Each class sees only its weapons in the craft menu; X cycles owned weapons; Atk on the HUD changes with the weapon; Spark Crossbow pierces one monster |
| C6 | Fire spreads on a burning kill; 3 frost hits freeze; shock chains to 2; venom doubles drops; earth breaks a whole copper vein; weak hits show "!" and more damage |
| C7 | Every weapon's Z skill fires once per cooldown; swapping right after a cast does not allow a second cast |
| C8 | New run starts on the field with no lamp and sun shadows; gate prompt enters floor 1 with v2 lighting back; death restarts on the field |
| C9 | Esc opens Options only when nothing is open; every setting applies and survives a restart of the exe; F1 values in settings.json are untouched |

### Conflict notes for Claude Code

- **Keep v2's modules and names.** `Cards`, `CardData.h`, the card pick screen, `Prompts`, `Juice`, `Lighting`, `ShadowMap` and `SaveData` are extended, not replaced or renamed.
- **One sun, not two shadow systems.** Floor 0 reuses the moon light and its shadow map; the lamp cube shadow stays the dungeon's main shadow.
- **Light budget stays 16.** Torches, crystals, stairs light and flashes share the nearest-16 list; the lamp stays slot 0 (intensity 0 on floor 0).
- **No mining level left behind.** Remove its HUD bar, F1 controls, save fields and strings together in C2, so nothing reads a dead value.
- **Prompt order:** level-up pick first, then the stairs prompt (as in v2); the craft menu and Options cannot open over another prompt.
- **Do not touch** the 3D player model or CPU skinning inside C0–C9. The G hotkey removal, Release shader settings and template leftovers are done in P0 only, before C0 (see "Still to do from Guide v2").
- **Same workflow:** no commit, no push, report after each milestone. If a step needs a decision this doc does not cover, Claude Code asks instead of guessing.
