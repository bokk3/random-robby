# TO_GENERATE_DUNGEON.md — 2.5D Modular Dungeon & Encounter Assets

## Technical Standards for Environment Sheets
- **Resolution**: 2048x2048 px per 4x4 sheet (512x512 px per cell = 8x8 meters per floor tile or 4x4 meters at high 128px/m hero density).
- **Two Sheet Types**:
  1. **Seamless Atlases (`ATLAS_*`)**: Edge-to-edge tileable stone/brick cells (NO `#FF00FF` background; fills the full 512x512 cell).
  2. **Cutout Prop Sheets (`PROP_*`)**: Isolated 2.5D vertical standing structures on a solid `#FF00FF` magenta background with crisp dark pixel outlines.
- **Shader Auto-Extraction Channels**:
  - `_D`: BaseColor + Alpha cutout.
  - `_N`: Deep-relief Sobel + Bevel Tangent Normal Map (crucial so the player's narrow flashlight cone rakes across mortar grooves).
  - `_H`: 8-bit Heightmap for Parallax Occlusion Mapping (POM) on floors and walls.
  - `_R`: Roughness/Wetness mask (puddles = `0.05` roughness to reflect the player's warm lantern, magma zombies, and cyan flasks).

---

## Set 1: Core Dungeon Floors & 8-Compass Architecture
- [ ] `ATLAS_Dungeon_Floors_4x4.png` (Seamless 512x512 cells, no magenta)
  - **Row 1 (Dry Crypt Stone)**: 4 variations of worn dark-slate flagstones with deep mortar cracks.
  - **Row 2 (Wet & Flooded Stone)**: 4 flagstone tiles with shallow standing water puddles (high specular mask) and drainage grates.
  - **Row 3 (Diagonal & Octagonal Transitions)**: 45-degree chamfered border tiles and compass-rose floor inlays to guide 4-axis (8-compass) movement.
  - **Row 4 (Corrupted / Hazard Floors)**: Scorched magma-cracked stone, mossy poison-stained cobbles, and shattered glass tiles (for Mirror-Palace transitions).

- [ ] `PROP_Dungeon_Walls_And_Cutaways_4x4.png` (`#FF00FF` Magenta Background, 2.5D top-down 60° pitch)
  - **Row 1 (North/Back Wall Faces)**: 4 tall heavy dungeon wall segments (plain stone, chained wall with iron shackles, alcoved skull wall, cracked mossy wall).
  - **Row 2 (South Wall Cutaway Stubs)**: 4 low knee-high cross-section wall stubs (rendered when the camera occludes a South wall so the player stays visible).
  - **Row 3 (45-Degree Diagonal Wall Segments)**: 4 angled NE/NW/SE/SW wall wedges so corridors support true 8-compass diagonal movement without jagged stair-stepping.
  - **Row 4 (Pillars & Buttresses)**: 4 freestanding square and octagonal stone pillars (intact, cracked, chain-wrapped, torch-mounted).

- [ ] `PROP_Doors_And_Level_Transitions_4x4.png` (`#FF00FF` Magenta Background)
  - **Row 1 (North/South Archways & Portcullis)**: Stone archway frame, closed iron portcullis, raised portcullis, heavy timber-and-iron boss door.
  - **Row 2 (East/West Archways & Doors)**: Side-perspective archway frame, closed side gate, open side gate, barred prison divider.
  - **Row 3 (Descending Stairs — Next Dungeon Floor)**: North-facing stone staircase descending into pitch black, South-facing staircase, spiral stairwell, chains-and-pulley lift platform.
  - **Row 4 (Wall Light Fixtures — Animated 4-Frame)**: 4-frame flickering iron wall sconce torch (provides sparse landmarks in deep fog).

---

## Set 2: The Mirror-Palace Illusion Maze
- [ ] `PROP_Mirror_Palace_Maze_4x4.png` (`#FF00FF` Magenta Background + `#00FF00` Portal Glass Key)
  - **Row 1 (True Reflective Wall Panels)**: 4 ornate brass-and-black-iron standing mirrors with pure `#00FF00` interior panes (UE 5.8 replaces `#00FF00` with real-time stencil reflection + flashlight bounce).
  - **Row 2 (Illusory Pass-Through Phantasm Mirrors)**: 4 frameless or hairline-seam mirror panels with faint runic edge markings (allows players with upgraded Dark Vision to spot the fake wall and walk right through).
  - **Row 3 (Cracked & Shattered Mirror States)**: 4 progressive destruction states (hairline crack, spiderweb fracture, half-broken frame, empty jagged frame with floor glass shards).
  - **Row 4 (Rotatable Light-Bounce Prisms)**: 4 compass-aligned (N/E/S/W and NE/NW/SE/SW) pedestal mirrors that players can rotate to bounce their flashlight beam around corners.

- [ ] `ATLAS_Mirror_Maze_Floors_4x4.png` (Seamless 512x512 cells)
  - **Row 1**: 4 polished black obsidian checkerboard tiles (ultra-low roughness `0.08` for floor reflections).
  - **Row 2**: 4 cracked glass tiles over a bottomless dark void.
  - **Row 3**: 4 inlaid brass compass tracks (shows where rotatable mirror pedestals slide along the 4 axes).
  - **Row 4**: 4 illusory bridge tiles that fade in only when illuminated by a flashlight cone or Acute Hearing ping.

---

## Set 3: Mid-Run Encounters (Resting Ground & Vendor)
- [ ] `PROP_Resting_Ground_Sanctuary_4x4.png` (`#FF00FF` Magenta Background)
  - **Row 1 (Sanctuary Brazier — 4-Frame Loop)**: Central stone fire pit with warm roaring flames (pushes back Volumetric Fog and recharges flashlight/stamina).
  - **Row 2 (Sensory Upgrade Shrine — 4-Frame Loop)**: Ancient stone eye/ear monolith pulsing with ethereal pale-blue energy (where players unlock Dark Vision, Acute Hearing, and Scent).
  - **Row 3 (Co-Op Party Stash & Bench)**: Heavy iron-bound Party Stash chest (for trading/sharing gear), stone sitting bench, tattered canvas tent, hanging lantern post.
  - **Row 4 (Protective Ward Boundary)**: 4 glowing salt-and-rune floor circle segments that visually mark the safe zone perimeter where zombies cannot enter.

- [ ] `PROP_Vendor_Emporium_4x4.png` (`#FF00FF` Magenta Background)
  - **Row 1 (Vendor Stall Structure)**: Ornate tattered merchant canopy tent, wooden trade counter, heavy iron safe, hanging brass scales.
  - **Row 2 (Alchemical & Gear Displays)**: Rack of glowing potions (echoing the cyan/amber bottles), crate of spare lanterns/batteries, rack of medieval melee/ranged weapons, scroll cabinet.
  - **Row 3 (Merchant Rug & Floor Clutter)**: 4 sections of an intricate worn Persian-style dungeon rug that defines the trading interaction zone.
  - **Row 4 (Animated Shop Sign — 4-Frame Swing)**: Hanging iron lantern-and-coin shop sign swaying gently in the dungeon draft.

---

## Set 4: Terminal Loot Point & Breakable Clutter
- [ ] `PROP_Loot_Progression_And_Vault_4x4.png` (`#FF00FF` Magenta Background)
  - **Row 1 (Minor Loot Containers)**: Rusted iron lockbox (closed/open), rotting wooden supply crate (intact/smashed).
  - **Row 2 (Mid-Tier Dungeon Chests)**: Silver-trimmed stone sarcophagus chest (closed, unlocking, open with glow, empty).
  - **Row 3 (Major Floor Loot Vault — 4-Frame Unlock)**: Massive chained golden-and-obsidian Vault Chest at the end of the floor (chains snapping -> lid rising -> blinding gold/relic beam).
  - **Row 4 (Treasure Hoard Piles)**: Scattered gold coin mounds, glowing artifact pedestals, jewel goblets, ancient tome stacks.

- [ ] `PROP_Breakables_And_Traps_4x4.png` (`#FF00FF` Magenta Background)
  - **Row 1 (Breakable Barrels & Urns)**: 2 intact + 2 shattered frames of dungeon barrels and clay funerary urns (triggers acoustic noise ripples when smashed!).
  - **Row 2 (Floor Hazard Vents)**: Vent grate dormant, vent spewing fire, vent leaking poison spores, vent leaking Slo-Mo Chrono Gas.
  - **Row 3 (Hanging Foreground Occluders)**: 4 ceiling-hung iron chains and cobweb banners placed close to the camera plane with parallax scrolling for deep 2.5D layering.
  - **Row 4 (Acoustic Chime Traps)**: Bone chimes and floor glass patches that emit loud `FNoiseEvent` ripples when stepped on, alerting nearby Fast Throwers and Exploders.

---

## Ready-to-Use Prompts for Your First 4 Dungeon Sheets

Use these prompts in your image generator to match the exact dark pixel-art line weight and palette of your Hooded Player, Slow Exploder, and Fast Thrower:

1. **Modular Walls, Diagonal Wedges & Pillars** (`PROP_Dungeon_Walls_And_Cutaways_4x4.png`):
   ```text
   4x4 modular dungeon architecture sprite sheet on a solid flat magenta #FF00FF background, top-down 2.5D three-quarter perspective, dark fantasy semi-pixelated style with crisp black pixel outlines. Row 1: 4 tall heavy dark grey stone dungeon wall segments with iron shackles, mortar cracks, and recessed skull alcoves. Row 2: 4 low cross-section cutaway stone wall stubs showing rubble interior. Row 3: 4 angled 45-degree diagonal stone wall corner wedges for octagonal corridors. Row 4: 4 freestanding thick stone dungeon pillars wrapped in rusted chains. No anti-aliasing on magenta background.
   ```

2. **Seamless Dungeon Floor Atlas** (`ATLAS_Dungeon_Floors_4x4.png`):
   ```text
   4x4 grid of seamless tileable top-down dungeon floor textures filling the entire canvas, dark fantasy semi-pixelated pixel art style, high detail. Row 1: 4 dark slate flagstone floor tiles with deep cracked mortar lines. Row 2: 4 wet dungeon stone floor tiles with reflective dark water puddles and iron drain grates. Row 3: 4 octagonal stone tiles with faded brass compass-rose inlays. Row 4: 4 hazardous dungeon floor tiles with glowing orange magma cracks, toxic green moss, and shattered mirror glass. Strict grid alignment.
   ```

3. **Mirror-Palace Maze Set** (`PROP_Mirror_Palace_Maze_4x4.png`):
   ```text
   4x4 modular sprite sheet on a solid flat magenta #FF00FF background, top-down 2.5D perspective, semi-pixelated dark gothic dungeon style. Row 1: 4 tall ornate gothic black-iron and tarnished brass standing mirrors with solid flat bright green #00FF00 glass interiors. Row 2: 4 frameless tall dungeon mirror walls with glowing faint cyan runes etched on the edges. Row 3: 4 shattered gothic mirror frames with jagged broken glass shards on the base. Row 4: 4 rotatable 45-degree angled mirror pedestals on stone bases for bouncing light beams. Crisp pixel outlines.
   ```

4. **Resting Ground, Shrine & Loot Vault** (`PROP_Resting_Ground_Sanctuary_4x4.png`):
   ```text
   4x4 sprite sheet on a solid flat magenta #FF00FF background, top-down 2.5D perspective, semi-pixelated dark fantasy dungeon style. Row 1: 4-frame animation of a stone dungeon brazier campfire with warm glowing orange pixel flames. Row 2: 4-frame animation of an ancient stone monolith shrine carved with a glowing pale-blue eye symbol. Row 3: heavy iron co-op party stash chest, tattered camp tent, wooden bench, and standing lantern post. Row 4: 4-frame animation of a massive chained golden boss loot vault chest snapping its chains and opening with bright treasure glow.
   ```
