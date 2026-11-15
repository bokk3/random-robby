# TO_GENERATE.md — 2.5D Dark Dungeoneer Asset Production Matrix

## Global Technical Specifications
- **Canvas & Grid**: 1024x1024 px (4x4 grid = 256x256 px per cell) or 2048x2048 px (512x512 px per cell).
- **Chroma Key Background**: Solid flat Magenta (`#FF00FF`), zero anti-aliasing on outer silhouette edges.
- **Auto-Extracted Shader Channels (via `slice_normal_rig.py`)**:
  - `_D` (BaseColor + Alpha): Chroma-key stripped, nearest-neighbor filtered.
  - `_N` (Tangent Normal): Generated via Sobel/bevel height-from-luminance pass + directional edge emboss.
  - `_ORM` (Occlusion, Roughness, Metallic): Packed from palette material tags (leather, stone, iron, glass).
  - `_E` (Emissive Mask): Extracted from high-saturation luminance keys (Lantern `#FFCC00`, Magma `#FF5500`, Alchemical Flask `#00FFFF`).
- **World Scale**: 64 texels = 100 Unreal Units (1 meter).

## Phase 0: Baseline Assets (Completed & Ready for Slicing)
- [x] `CH_Player_Cardinal_Walk_4x4.png`: Hooded dungeoneer with lantern (Row 1: S, Row 2: N, Row 3: E, Row 4: W).
- [x] `CH_Zombie_Exploder_Walk_4x4.png`: Shackled bloated zombie with glowing magma belly (Row 1: S, Row 2: N, Row 3: E, Row 4: W).
- [x] `CH_Zombie_FastThrower_Side_4x4.png`: Hooded alchemist zombie running and hurling glowing flasks (Rows 1 & 3: E throw, Rows 2 & 4: W throw).

## Phase 1: Character & Zombie Animation Completion
- [ ] `CH_Player_Diagonal_Walk_4x4.png`
  - **Row 1**: 4-frame walk SE (front-right three-quarter view, lantern visible).
  - **Row 2**: 4-frame walk SW (front-left three-quarter view).
  - **Row 3**: 4-frame walk NE (back-right three-quarter view, cloak dominant).
  - **Row 4**: 4-frame walk NW (back-left three-quarter view).
- [ ] `CH_Player_Action_Socket_4x4.png`
  - **Row 1**: Detached right/left arm + lantern/flashlight sub-sprites across 8 compass angles (for independent aim pivoting).
  - **Row 2**: 4-frame dodge-step / quick-sidestep blur frames (S, N, E, W).
  - **Row 3**: 4-frame hit-stagger & lantern-flicker recovery.
  - **Row 4**: 4-frame collapse / downed state (revivable in Co-Op).
- [ ] `CH_Zombie_Exploder_Detonate_4x4.png`
  - **Row 1**: 4-frame Front/Three-Quarter Fuse Swell (belly distorts, chains snap, magma shifts orange -> white).
  - **Row 2**: 4-frame Side Fuse Swell.
  - **Row 3**: 4-frame Violent Detonation Burst (shattered ribcage, magma droplets, black smoke ring).
  - **Row 4**: 4 static Ground Decals (charred crater, broken iron shackles, glowing ember pool, scorched stone).
- [ ] `CH_Zombie_FastThrower_Cardinal_4x4.png`
  - **Row 1**: 4-frame South (front) fast sprint cycle.
  - **Row 2**: 4-frame North (back) fast sprint cycle.
  - **Row 3**: 4-frame South (front) overhead flask windup and throw.
  - **Row 4**: 4-frame North (back) overhead flask windup and throw.
  - *Note*: Keep flask liquid pure Cyan (`#00FFFF`) so UE 5.8 dynamically tints it Orange (Fire), Green (Poison), or Violet-Blue (Slo-Mo Chrono Gas).

## Phase 2: Projectiles, Hazards & Sensory VFX Atlas
- [ ] `VFX_Projectiles_Hazards_4x4.png`
  - **Row 1**: 4-frame tumbling glass flask + 4-frame glass shatter splash (neutral greyscale/cyan for dynamic material tinting).
  - **Row 2**: 4-frame Ignited Ground Fire loop (crisp pixel flames + rising ash embers).
  - **Row 3**: 4-frame Bubbling Poison Puddle + rising toxic spore skulls/bubbles.
  - **Row 4**: 4-frame Slo-Mo Chrono-Gas distortion cloud (swirling temporal ripples & floating hourglass shards).
- [ ] `VFX_Sensory_Overlays_4x4.png`
  - **Row 1**: 4 concentric jagged Acoustic Ripple rings (for Acute Hearing footstep/shriek pings through fog).
  - **Row 2**: 4 glowing directional Footprint & Claw-mark decals (for Tracking/Scent sense).
  - **Row 3**: 4-frame Illusion Glitch shimmer strip (rendered over Mirror-Palace decoys when inspected with upgraded senses).
  - **Row 4**: 4-frame Chrono-Leech tether beam particles (connecting slowed players to the accelerating Fast Zombie).

## Phase 3: Modular 2.5D Dungeon & Mirror-Palace Architecture
- [ ] `ENV_Dungeon_Tileset_4x4.png`
  - **Row 1**: 4 seamless flagstone floor tiles (dry, mossy, cracked, flooded puddle mask).
  - **Row 2**: 4 modular 2.5D North/South wall segments with deep stone relief and iron sconces.
  - **Row 3**: 4 modular East/West wall caps, archway pillars, and heavy iron dungeon doors.
  - **Row 4**: 4 environmental clutter props (broken barrels, bone piles, rusted chains, cobweb corners).
- [ ] `ENV_Mirror_Palace_Props_4x4.png`
  - **Row 1**: 4 ornate Gothic standing mirror frames with pure Green (`#00FF00`) interior glass masks (for stencil portal replacement).
  - **Row 2**: 4 spiderweb-cracked mirror overlays (semi-transparent mask for damaged illusions).
  - **Row 3**: 4 shattered mirror frames with glass shards scattered at the base.
  - **Row 4**: 4 rotatable 45-degree crystal/brass reflector pedestals (bounces player flashlight beams).

## Phase 4: Progression Nodes (Vendor, Resting Ground, Vault)
- [ ] `NPC_Dungeon_Vendor_4x4.png`
  - **Row 1**: 4-frame idle breathing of hunched relic merchant with lantern-lit backpack.
  - **Row 2**: 4-frame coat-opening / presenting wares gesture.
  - **Row 3**: 4-frame trade completed / coin-toss animation.
  - **Row 4**: 4 merchant shop props (ornate carpet, potion rack, scale, hanging curiosities).
- [ ] `ENV_Resting_And_Loot_4x4.png`
  - **Row 1**: 4-frame Sanctuary Campfire Brazier (warm flame loop that clears local fog).
  - **Row 2**: 4-frame Sensory Shrine crystal pulsing (used to unlock Dark Vision / Hearing perks).
  - **Row 3**: 4-frame Co-Op Party Stash Chest & Minor Floor Chests (closed -> open).
  - **Row 4**: 4-frame Terminal Boss Loot Vault (massive chained vault door unlocking and spilling gold/relics).

## Phase 5: Audio & Procedural Voice Assets
- [ ] `AUD_Phoneme_Bank_24x.wav`: 24 short (60ms–110ms) dry vocal syllable grains (`ba, da, ka, mu, ri, to,esh, vu...`) for `MS_GibberishSynth`.
- [ ] `AUD_Hero_Vendor_Lines`: 6 generated key voice lines (Vendor greeting, rare item reaction, Resting Ground whisper).
- [ ] `AUD_Timed_Foley`: Stone footsteps (4 surfaces), Exploder belly-swell heartbeat ticks, glass shatter, gas hiss, time-stretch pitch sweep.