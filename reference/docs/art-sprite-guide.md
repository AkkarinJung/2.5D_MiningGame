# Deepvein: Art & Sprite Guide

Nov 25, 2026 · @Akkarin

## Art direction

Deepvein is a lamp-lit pixel-art cave seen from above at about 58 degrees: a 3D dungeon of blocks and walls, with flat pixel-art characters standing in it. Most of the screen is near-black, so every sprite must read inside a small pool of warm lamp light.

- **Look:** chunky pixel art, sharp edges (point filtering), a small limited palette per object.
- **World:** walls, floor and ore are textured 3D cubes, lit per pixel by the lamp, crystals and flashes, with shadows and fog of war.
- **Characters, monsters, items:** camera-facing billboards (flat sprites that always face the camera), anchored at their feet.
- **Mood:** dark, quiet, a little dangerous; colour comes mainly from ore, crystals, potions and effects.
- **Engine today:** sprites are 8x8 pixel grids written as text rows in code (SpriteDef); textures are PNG/JPG files loaded with DirectXTex. The plan below moves sprites to PNG atlases drawn by artists.

## Sprite type: what to use for each object

Recommendation: **pixel-art billboards for everything that moves or is picked up, textured 3D cubes for the world, 2D pixel UI for the HUD.** This keeps the 2.5D prototype look, works with the lighting and shadows already built, and is the cheapest to draw for one person.

| Object | Sprite type | Size (px) | Frames | Why |
| --- | --- | --- | --- | --- |
| Player classes (4) | Billboard pixel sprite | 16x16 | idle 2, walk 4, attack 3, hurt 1 | Readable at the 58-degree camera; replaces the 3D model and its slow CPU skinning |
| Monsters (slime, bat, goblin, brute) | Billboard pixel sprite | 16x16 (brute 24x24) | move 2-4, wind-up 2 (brute) | Same pipeline as the player; flat sprites cast silhouette shadows already |
| Items (potion, bomb) | Billboard pixel sprite | 8x8 or 16x16 | 1-2 (bob in code) | Small and glowing, found in the dark |
| Projectiles (bolt, sword wave) | Billboard sprite, drawn unlit | 8x8 / 16x8 | 2-3 | Bright, should glow over darkness |
| Ore blocks (copper, silver, gold) | Textured cube | 16x16 per face | 1 + 3 crack stages | 3D blocks catch the lamp and cast shadows |
| Crystal | 3D crystal mesh or textured cube with glow | 16x16 | 1 | Emissive; a light source and landmark |
| Walls and floor | Tiling cube / floor textures | 16x16 (or 32x32) | 1 + 2-3 variants | Variation hides repetition in large rooms |
| Effects (rings, debris, flashes) | Procedural in code | n/a | n/a | Already done in Juice / Projectiles |
| HUD, cards, menus | 2D pixel UI + bitmap font | 16x16 icons, 9-slice panels | 1 | Replaces the ImGui stand-ins |

**Not recommended:** full 3D models for characters (slow CPU skinning today, a different look from the world), or hand-painted high-resolution sprites (do not match the 16x16 world textures and are expensive to animate).

Open question: keep the current 3D player model as a temporary stand-in until the 16x16 class sprites exist, or switch now to the placeholder 8x8 sprites already in code?

## Technical sprite spec

Every sprite follows these rules so it lines up with the 1-unit tile grid, stays sharp, and lights correctly.

- **Grid:** 16 px per tile (one world unit). Characters 16x16, large monsters 24x24 or 32x32, items 8x8 or 16x16.
- **Anchor:** bottom-centre = the feet. The quad is drawn from y = 0 upward, so empty rows under the feet make a sprite float.
- **Transparency:** fully transparent or fully opaque pixels only (the shader cuts alpha below 0.5). No soft edges or semi-transparent shadows inside the sprite; the engine draws real shadows.
- **Filtering:** point sampling, no mipmaps. Never scale sprites in the editor; scale by whole numbers only.
- **Palette:** 4-8 colours per sprite, one darker outline colour (not pure black) so sprites separate from the dark floor.
- **Facing:** draw facing right; the engine flips for left. Avoid text or asymmetric details that look wrong when flipped.
- **Atlas:** 256x256 PNG per group (characters, monsters, items, tiles), 1 px transparent padding around each frame, frames in rows (one row per animation).
- **Animation timing:** 8-10 fps for walk and idle, 12-15 fps for attacks; timings live in a data table, not in code.
- **Format:** 32-bit PNG (RGBA), sRGB, no colour profile tricks.
- **Emission:** parts that should glow in the dark (eyes, crystals, potion liquid, bolts) go on a separate emission mask frame of the same size (white = glow), so the shader can make only those pixels bright.

## Asset list

Everything the current game draws, and what art each needs. Placeholder = the 8x8 text sprites or textures in the repo today.

| Asset | Size (px) | Frames | Today |
| --- | --- | --- | --- |
| Warrior | 16x16 | idle 2, walk 4, attack 3, hurt 1 | 3D model (all classes) |
| Miner | 16x16 | idle 2, walk 4, attack 3, hurt 1 | 3D model |
| Mage | 16x16 | idle 2, walk 4, cast 3, hurt 1 | 3D model |
| Rogue | 16x16 | idle 2, walk 4, attack 3, dash 2, hurt 1 | 3D model |
| Cave Slime | 16x16 | hop 4 | 8x8 placeholder |
| Gloom Bat | 16x16 | flap 3 | 8x8 placeholder |
| Goblin | 16x16 | run 4 | 8x8 placeholder |
| Stone Brute | 24x24 | walk 4, wind-up 2, charge 2 | 8x8 placeholder |
| Red potion | 8x8 | 1 + emission | 8x8 placeholder |
| Dynamite bomb | 8x8 | 2 (fuse spark) | 8x8 placeholder |
| Magic bolt | 8x8 | 2 | 8x8 placeholder |
| Sword wave | 16x8 | 2 | bolt sprite tinted |
| Copper / silver / gold ore | 16x16 per face | 1 + 3 cracks | tinted brick texture |
| Crystal | 16x16 or mesh | 1 + emission | tinted cube |
| Wall | 16x16 | 3 variants | Brick.jpg |
| Floor | 16x16 | 3 variants | block\_field.png |
| Stairs | 16x16 | 1 + emission | gold square |
| HUD icons (HP, potion, skill x4, ore x4) | 16x16 | 1 | ImGui text |
| Card frames (common, rare, epic) | 64x96, 9-slice | 1 | ImGui window |
| Bitmap font (EN + JP) | 8x8 / 12x12 glyphs | n/a | ImGui + Windows font |

## Lighting and readability

The game is lit almost only by the player's lamp (radius 3 tiles, +1.8 per Bright Lamp), so sprites must be readable at low light and from 6-12 tiles of camera distance.

- **Value contrast first:** test every sprite in greyscale; the silhouette must read with no colour.
- **Mid-tone base colours:** the lamp multiplies colours, so very dark sprites vanish. Keep the darkest body colour above about 25% brightness.
- **Glow only where it means something:** crystal, potion liquid, eyes of awake monsters, magic. These go in the emission mask and stay visible in shadow (fog of war still hides them when out of sight).
- **Warm lamp, cool world:** the lamp is orange (#ffb866) and ambient is dark purple-blue, so cool-coloured materials (stone, slime purple) look right in shadow, and warm accents pop under the lamp.
- **Outlines:** one-pixel dark outline in a hue, not black, so sprites separate from the black of unexplored tiles.
- **Ore must be readable by shape, not only colour:** copper, silver, gold and crystal differ in vein pattern too, for colour-blind players.
- **Monsters vs floor:** monster colours must not match floor colours; the floor stays low-saturation.

## Theme options

The theme is not decided yet, and may change with the planned mechanic changes. All four options below keep the same engine and sprite rules; only palette, names and props change.

| Theme | Setting and mood | Palette | Ore / resource | Monsters |
| --- | --- | --- | --- | --- |
| Classic deep mine (current) | Abandoned dwarven mine, lantern light, quiet danger | Warm orange lamp, purple-grey stone | Copper, silver, gold, crystal | Slime, bat, goblin, stone brute |
| Fungal caverns | Glowing mushroom forest underground, eerie and alive | Teal and violet bioluminescence, dark green | Spores, amber, glowcaps, star-crystal | Spore slugs, moths, myconid guards |
| Frozen depths | Ice caves under a mountain, cold and still | Pale blue ice, white frost, warm lamp contrast | Frost iron, sapphire, aurora shards | Ice wisps, yetis, frozen miners |
| Lava foundry | Collapsed forge city near magma, hot and loud | Red-orange glow, black basalt, brass | Obsidian, cinnabar, molten gold, fire opal | Ember imps, slag golems, salamanders |

Decide the theme together with the mechanic changes, then update the asset list names and palettes. Placeholder art can stay until then.

## Pipeline

Art is drawn in a pixel editor, exported as PNG atlases into `asset\sprite\`, and listed in a data table that maps each sprite to its atlas frame.

1. **Draw** in Aseprite (recommended) or LibreSprite (free): one file per character or group, one tag per animation.
2. **Name** files and tags in lower case: `class_miner.aseprite`, tags `idle`, `walk`, `attack`, `hurt`.
3. **Export** a sprite sheet PNG (packed by rows, 1 px padding) plus the JSON frame data Aseprite writes.
4. **Engine (to build):** a texture-atlas loader reads the PNG with DirectXTex and the JSON for frame rectangles; `SpriteAtlas` switches from text rows to these frames; an `Animator` plays tags at the table's fps.
5. **Today:** sprites are 8x8 text rows in `MonsterData.h` / `Projectiles.cpp` (`SpriteDef`). They stay as placeholders until the PNG path exists.
6. **Check in game:** F1 panel shows the lamp, fog and shadows; check each sprite in the dark, under the lamp, frozen (cyan tint), poisoned (green tint) and flashing white.

Textures (walls, floor, ore faces) keep the current path: PNG/JPG in `asset\texture\`, loaded once at start.
