# Art to generate

## Global Technical Specifications
- **Canvas & Grid**: 1024x1024 px (4x4 grid = 256x256 px per cell) or 2048x2048 px (512x512 px per cell).
- **Chroma Key Background**: Solid flat Magenta (`#FF00FF`), zero anti-aliasing on outer silhouette edges.
- **Auto-Extracted Shader Channels (via `slice_normal_rig.py`)**:
  - `_D` (BaseColor + Alpha): Chroma-key stripped, nearest-neighbor filtered.
  - `_N` (Tangent Normal): Generated via Sobel/bevel height-from-luminance pass + directional edge emboss.
  - `_ORM` (Occlusion, Roughness, Metallic): Packed from palette material tags (leather, stone, iron, glass).
  - `_E` (Emissive Mask): Extracted from high-saturation luminance keys (Lantern `#FFCC00`, Magma `#FF5500`, Alchemical Flask `#00FFFF`).
- **World Scale**: 64 texels = 100 Unreal Units (1 meter).

## 16-Cell (4x4) Walk-Cycle Kinematic Specification
All walking character, zombie, and humanoid NPC `4x4` sheets MUST strictly conform to this Row/Column layout (verified with `slice_normal_rig.py --validate-stride --repair-stride` and consumed directly by `U25DAnimOrchestratorComponent`):

- **Rows (4 Orientations)**:
  - **Row 0 (Top)**: South (Front view, walking toward camera) — or SE on diagonal sheets.
  - **Row 1**: North (Back view, walking away from camera) — or SW on diagonal sheets.
  - **Row 2**: East (Right side profile, walking right) — or NE on diagonal sheets.
  - **Row 3 (Bottom)**: West (Left side profile, walking left) — or NW on diagonal sheets.

- **Columns (4 Alternating Stride Phases per Row)**:
  - **Column 0 (Frame 0 — Contact / Idle Pose A)**: Both feet planted close together directly beneath hips as weight transfers; torso and head at peak height (`+1 to +2 px`). Default standing Idle frame when `Velocity.IsNearlyZero()`.
  - **Column 1 (Frame 1 — Full Left Step)**: Left leg extended forward, right leg pushing off behind; torso dips (`-1 to -2 px`). Fires `OnFootstepContact(EFoot::Left)`. South view: left foot lower and slightly larger; North view: right heel lifts; East/West side profiles: open scissor stance with left leg forward.
  - **Column 2 (Frame 2 — Passing Pose B)**: Mid-stride weight transfer with legs crossing directly under the center of gravity; silhouette narrows and torso rises back to peak height (`+1 to +2 px`).
  - **Column 3 (Frame 3 — Full Right Step)**: Exact opposite of Column 1. Right leg extended forward, left leg pushing off behind; torso dips (`-1 to -2 px`). Fires `OnFootstepContact(EFoot::Right)`. South view: right foot lower; East/West side profiles: opposite scissor stance with right leg forward.

### Reusable Character / Zombie Generation Prompt Template
```text
Strict 4x4 grid (16 equal cells, zero borders or cell outlines), semi-pixelated dark-gothic 2.5D character sprite sheet on a solid flat magenta (#FF00FF) background with zero anti-aliasing.
Rows: Row 0 = South (front), Row 1 = North (back), Row 2 = East (right side), Row 3 = West (left side). (Or Row 0 SE, Row 1 SW, Row 2 NE, Row 3 NW for diagonal sheets.)
Columns (alternating stride legs per row):
- Col 0: Contact / Idle Pose A — both feet planted together beneath hips, torso/head at peak (+1 to +2 px).
- Col 1: Full Left Step — left leg forward, right leg pushing off behind, torso dipped (-1 to -2 px), clear asymmetric leg separation.
- Col 2: Passing Pose B — legs crossing beneath center of gravity, narrower stance, torso back at peak height (+1 to +2 px).
- Col 3: Full Right Step — opposite of Col 1 (right leg forward, left leg behind, torso dipped -1 to -2 px).
Keep bottom-center foot baseline locked at consistent Y across all 16 cells and preserve crisp key emissives (#FFCC00 lantern, #FF5500 magma, #00FFFF flask).
```

## Next batch: copy-ready prompts (v1.6–v1.8)

Paste each prompt into the image generator as-is, one sheet per prompt. Every sheet uses the global format
above: 1024×1024, a 4×4 grid of 256×256 cells with no borders, flat magenta `#FF00FF` background, no
anti-aliasing on silhouettes, the same pixel density as the existing walk sheets, and the feet of every
figure on the same baseline about 20 px above the bottom of its cell. Keep emissive colours pure: lantern
flame `#FFCC00`, magma and kiln heat `#FF5500`, flask liquid `#00FFFF`. Characters must match
[STORY.md](STORY.md).

Save each result as `Content/Art/Raw/<File name>.png`, then slice it (this writes the `_D`, `_N`, `_ORM`,
`_E` maps and updates the asset manifest):

```powershell
python .claude/skills/sprite-sheet-25d-cutter/scripts/slice_normal_rig.py --mode rig --input "Content/Art/Raw/<File name>.png" --output "Content/Art/Rigs/<Folder>"
```

Walk sheets also take `--validate-stride --repair-stride`. Attack and NPC sheets don't (their columns aren't a
stride). The code picks a sheet up once it's in the sheet registry (plan step M6); until then the game uses
the walk frames plus code-driven poses.

"Hit frame" is the column where the blow lands. Gameplay times the damage to it, so draw the impact there.

### Priority 1: the hero's moves

**Done (2026-11-21):** sheets 1-5 came from Gemini (originals in `Content/Art/Raw/Gemini/`), were cleaned to exact
#FF00FF, halved to 1024 px and sliced into `Content/Art/Rigs/DungeoneerHeroMoves/<Set>/`. The game loads them as
`HeroCombo`, `HeroHeavy`, `HeroEvade`, `HeroSkills`, `HeroHurtIdle` cells and picks frames in
`ADungeoneerCharacter::ResolvePoseFrame`. Slice future move sheets into their own folder too: the slicer writes
`AtlasMetadata.json` and `rig_sockets.json` into its output folder and would overwrite the walk sheet's.

#### 1. `Rig_DungeoneerHero_Combo` (folder `DungeoneerHero`)

| Row | Facing | Col 0 | Col 1 (hit frame) | Col 2 | Col 3 |
| --- | --- | --- | --- | --- | --- |
| 0 | South | wind-up, lantern drawn back over right shoulder | swing mid, lantern at full arm in front, bright arc trail | follow-through low left, trail fading | recover to guard |
| 1 | North | same, seen from behind | same | same | same |
| 2 | East | same, right profile | same | same | same |
| 3 | West | same, left profile | same | same | same |

```text
Strict 4x4 grid sprite sheet, 16 equal 256x256 cells, no borders, flat solid magenta (#FF00FF) background, no anti-aliasing on silhouettes. Semi-pixelated dark gothic 2.5D pixel art, same style and scale as a top-down roguelite. Character: a hooded, cloaked lamplighter in rain-worn climbing leathers with steel bracers, no sword; their weapon is a brass chapel lantern with edged glass panes on a short chain, flame pure #FFCC00. Animation: a fast lantern melee swing, 4 frames per row. Col 0: wind-up, lantern drawn back over the right shoulder, body coiled. Col 1: swing at full extension in front of the body, the lantern leaving a thick curved #FFCC00 light trail (this is the impact frame). Col 2: follow-through low across the body, trail thinning. Col 3: recover to a ready stance. Rows: Row 0 facing the camera (south), Row 1 facing away (north), Row 2 right side profile (east), Row 3 left side profile (west). Feet on the same baseline in every cell, character fills about 70% of the cell height, identical proportions and palette across all cells.
```

#### 2. `Rig_DungeoneerHero_Heavy` (folder `DungeoneerHero`)

| Col 0 | Col 1 | Col 2 (hit frame) | Col 3 |
| --- | --- | --- | --- |
| crouched, lantern held low and glowing hot | lantern raised high overhead in both hands | slammed into the floor, shockwave of light and stone chips | straightening up, lantern smoking |

```text
Strict 4x4 grid sprite sheet, 16 equal 256x256 cells, no borders, flat solid magenta (#FF00FF) background, no anti-aliasing. Semi-pixelated dark gothic 2.5D pixel art. Same hooded, cloaked lamplighter with steel bracers and a brass chapel lantern on a short chain (flame #FFCC00). Animation: a charged overhead lantern smash, 4 frames per row. Col 0: crouched charge, lantern held low near the floor, flame swelling brighter. Col 1: lantern raised high overhead with both hands, cloak lifting. Col 2: lantern slammed into the ground in front, a flat ring of #FFCC00 light and flying stone chips at floor level (impact frame). Col 3: straightening up, thin smoke curling from the lantern. Rows: Row 0 south (facing camera), Row 1 north (back), Row 2 east (right profile), Row 3 west (left profile). Feet on a shared baseline, identical proportions and palette in every cell.
```

#### 3. `Rig_DungeoneerHero_Evade` (folder `DungeoneerHero`)

| Col 0 | Col 1 | Col 2 (hit frame) | Col 3 |
| --- | --- | --- | --- |
| roll start, diving low | tucked mid-roll, cloak wrapped, lantern hugged | dash lunge, lantern thrust forward like a spear | parry brace, lantern shutter snapped open toward the enemy |

```text
Strict 4x4 grid sprite sheet, 16 equal 256x256 cells, no borders, flat solid magenta (#FF00FF) background, no anti-aliasing. Semi-pixelated dark gothic 2.5D pixel art. Same hooded, cloaked lamplighter with steel bracers and a brass chapel lantern (flame #FFCC00). 4 frames per row. Col 0: start of a dodge roll, diving low and forward. Col 1: tucked mid-roll, cloak wrapped tight, lantern hugged to the chest, slight motion smear. Col 2: a long forward lunge, lantern thrust out at arm's length with a straight streak of #FFCC00 light behind it (impact frame). Col 3: parry brace, feet planted, lantern held up in front with its shutter snapped open, a flat burst of light toward the attacker. Rows: Row 0 south (facing camera), Row 1 north (back), Row 2 east (right profile), Row 3 west (left profile). Shared foot baseline, identical proportions and palette.
```

#### 4. `Rig_DungeoneerHero_Skills` (folder `DungeoneerHero`)

| Col 0 | Col 1 | Col 2 | Col 3 |
| --- | --- | --- | --- |
| overhand throw of a small fire flask (Bottled Kiln) | chain cast forward, hook flying (Lamplighter's Hook) | lantern spun in a full circle on its chain, motion blur (Censer's Wheel) | lantern held high, flaring white-gold (Borrowed Noon) |

```text
Strict 4x4 grid sprite sheet, 16 equal 256x256 cells, no borders, flat solid magenta (#FF00FF) background, no anti-aliasing. Semi-pixelated dark gothic 2.5D pixel art. Same hooded, cloaked lamplighter with steel bracers and brass chapel lantern (flame #FFCC00). Each column is a different spell-like action, one pose each. Col 0: overhand throw, a small glass flask of #FF5500 fire leaving the hand. Col 1: casting a chain forward from the lantern, an iron hook at the end flying away. Col 2: spinning the lantern on its chain in a full circle around the body, a ring-shaped #FFCC00 motion blur. Col 3: lantern held high overhead, flaring so brightly the flame turns near-white with #FFCC00 rays. Rows: Row 0 south (facing camera), Row 1 north (back), Row 2 east (right profile), Row 3 west (left profile). Shared foot baseline, identical proportions and palette.
```

#### 5. `Rig_DungeoneerHero_HurtIdle` (folder `DungeoneerHero`)

| Col 0 | Col 1 | Col 2 | Col 3 |
| --- | --- | --- | --- |
| idle breath in | idle breath out | hit flinch, recoiling back | downed, kneeling with lantern guttering |

```text
Strict 4x4 grid sprite sheet, 16 equal 256x256 cells, no borders, flat solid magenta (#FF00FF) background, no anti-aliasing. Semi-pixelated dark gothic 2.5D pixel art. Same hooded, cloaked lamplighter with steel bracers and brass chapel lantern (flame #FFCC00). Col 0: standing idle, chest slightly raised (breathing in). Col 1: standing idle, shoulders slightly lowered (breathing out), lantern swaying a little. Col 2: hit flinch, upper body recoiling backward, head turned away, lantern flame flickering. Col 3: downed on one knee, one hand on the floor, lantern on the ground beside them with a small weak flame. Rows: Row 0 south (facing camera), Row 1 north (back), Row 2 east (right profile), Row 3 west (left profile). Shared foot baseline, identical proportions and palette.
```

### Priority 2: enemies that telegraph their attacks (needed for M3)

**Done (2026-11-21):** sheets 6 (Exploder), 8 (Slip) and 9 (Colossus) are sliced into
`Content/Art/Rigs/<Enemy>Moves/Attack/`. Sheets 8 and 9 came back as new designs, so 8b and 9b below ask for
matching walk sheets; the game only uses those attack frames once the matching walk sheet exists.

#### 6. `Rig_ZombieExploder_Attack` (folder `ZombieExploder`), the Tapper

| Col 0 | Col 1 | Col 2 (hit frame) | Col 3 |
| --- | --- | --- | --- |
| wind-up: rears back, belly glowing brighter | shoulder lunge forward, shackle chains swinging | fuse lit: belly swollen, cracks glowing white | burst: body splits in a ring of magma and black smoke |

```text
Strict 4x4 grid sprite sheet, 16 equal 256x256 cells, no borders, flat solid magenta (#FF00FF) background, no anti-aliasing. Semi-pixelated dark gothic 2.5D pixel art. Character: a "Tapper", a dead miner swollen with firedamp gas, bloated grey-green body, broken iron shackles on wrists and ankles, a glowing cracked magma belly in #FF5500. Col 0: rearing back for a lunge, belly glowing brighter, mouth open. Col 1: heavy shoulder lunge forward, chains swinging (impact frame). Col 2: fuse lit, belly swollen to bursting, cracks across the skin glowing white-orange. Col 3: exploding, the body splitting into a ring of #FF5500 magma droplets and black smoke, broken shackles flying. Rows: Row 0 south (facing camera), Row 1 north (back), Row 2 east (right profile), Row 3 west (left profile). Shared foot baseline, same scale as a walking zombie that fills about 70% of the cell height.
```

#### 7. `Rig_ZombieFastThrower_Attack` (folder `ZombieFastThrower`), the Runner

| Col 0 | Col 1 (throw release) | Col 2 | Col 3 (hit frame) |
| --- | --- | --- | --- |
| flask wind-up, arm cocked back | flask thrown overhand | knife draw, crouched | knife slash across, arc trail |

```text
Strict 4x4 grid sprite sheet, 16 equal 256x256 cells, no borders, flat solid magenta (#FF00FF) background, no anti-aliasing. Semi-pixelated dark gothic 2.5D pixel art. Character: a "Runner", a lean, fast dead glassworks courier in a tattered hood and apron, belt of glass flasks, liquid in the flasks pure cyan #00FFFF (tinted in-game). Col 0: winding up a throw, arm cocked far back with a flask, body twisted. Col 1: overhand throw release, flask leaving the hand (release frame). Col 2: crouched, drawing a short glass-cutter's knife. Col 3: fast knife slash across the body with a thin pale arc trail (impact frame). Rows: Row 0 south (facing camera), Row 1 north (back), Row 2 east (right profile), Row 3 west (left profile). Shared foot baseline, same scale as the hero.
```

#### 8. `Rig_ZombieMirrorStalker_Attack` (folder `ZombieMirrorStalker`), the Slip

| Col 0 | Col 1 | Col 2 (hit frame) | Col 3 |
| --- | --- | --- | --- |
| crouched, reflection shimmering | lunge forward, arms reaching | bite/grab at full stretch | fading: body breaking into mirror shards |

```text
Strict 4x4 grid sprite sheet, 16 equal 256x256 cells, no borders, flat solid magenta (#FF00FF) background, no anti-aliasing. Semi-pixelated dark gothic 2.5D pixel art. Character: a "Slip", a person's reflection that stayed behind in a mirror: a pale silvered humanoid with a glassy, slightly see-through surface, cracked like a mirror, hollow bright eyes. Col 0: crouched low, surface shimmering. Col 1: lunging forward, long arms reaching out. Col 2: at full stretch, jaws and hands snapping shut (impact frame). Col 3: fading away, the body breaking into falling mirror shards. Rows: Row 0 south (facing camera), Row 1 north (back), Row 2 east (right profile), Row 3 west (left profile). Shared foot baseline, same scale as the hero.
```

#### 9. `Rig_BossGuardian_Attack` (folder `BossGuardian`), the Cinder Colossus

| Row | Col 0 | Col 1 | Col 2 (hit frame) | Col 3 |
| --- | --- | --- | --- | --- |
| 0 slam | arms raised | arms high, heart blazing | fists slammed down, ring of fire | recovery, slag dripping |
| 1 sweep | arm drawn back to one side | sweeping across | arm at full sweep, fire trail | recovery |
| 2 mortar | chest vents open | glob of molten glass launched upward | second glob | vents closing |
| 3 charge / death | head down, charging | mid-charge | collapsing into slag | heart bead left on the floor |

```text
Strict 4x4 grid sprite sheet, 16 equal 256x256 cells, no borders, flat solid magenta (#FF00FF) background, no anti-aliasing. Semi-pixelated dark gothic 2.5D pixel art, front three-quarter view (facing the camera) in every cell. Character: the Cinder Colossus, a huge walking glass kiln: firebrick and black slag body, slumped melted shapes of old workers and tools fused into it, a glowing heart in the chest in #FF5500. Row 0 (ground slam): arms raised; arms high with the heart blazing; fists slammed into the ground with a flat ring of fire (impact); recovering with slag dripping. Row 1 (sweep): one arm drawn back to the side; sweeping across; arm at the end of the sweep with a curved fire trail (impact); recovering. Row 2 (mortar): chest vents opening; a glob of molten glass launched upward; a second glob launched; vents closing. Row 3: charging head-down; mid-charge; collapsing into a heap of slag; only a small glowing glass bead left on the floor. Feet on a shared baseline, the Colossus fills about 90% of the cell height.
```

#### 8b. `Rig_ZombieMirrorStalker_Walk` (folder `ZombieMirrorStalkerMoves/Walk`), new Slip walk cycle

The Slip was redesigned by its attack sheet (sheet 8): a silvered, cracked mirror-man instead of the old
hooded blue wraith. This walk sheet replaces the old one so both match. Standard walk layout (rows S, N, E, W;
columns contact, left step, passing, right step). Slice with `--validate-stride --repair-stride`.

```text
Strict 4x4 grid sprite sheet, 16 equal 256x256 cells, no borders, no grid lines, flat solid magenta (#FF00FF) background, no anti-aliasing. Semi-pixelated dark gothic 2.5D pixel art. Character: a "Slip", the reflection of a dead person that stayed behind in a mirror: a lean, hairless humanoid made of polished silver mirror-glass, smooth chrome surface with dark crack lines across the body like a broken mirror, glowing pale white eyes, long fingers. Same design and proportions as a crouching, lunging mirror-man attack sheet. Animation: a prowling, hunched walk cycle, 4 frames per row. Col 0: contact, feet together, torso high. Col 1: left foot forward, torso dipped. Col 2: passing pose, legs crossing, torso high. Col 3: right foot forward, torso dipped. Rows: Row 0 facing the camera (south), Row 1 facing away (north), Row 2 right side profile walking right (east), Row 3 left side profile walking left (west). Feet on the same baseline in every cell, figure fills about 70% of the cell height, identical proportions and silver palette in all cells. No grid lines, no borders, no labels. Keep every figure fully inside its own 256x256 cell. 1:1 square image.
```

#### 9b. `Rig_BossGuardian_Walk` (folder `BossGuardianMoves/Walk`), new Cinder Colossus walk cycle

The Colossus was redesigned by its attack sheet (sheet 9): a firebrick-and-slag kiln giant instead of the
old blocky horned robot. This walk sheet replaces the old one. Standard walk layout. Slice with
`--validate-stride --repair-stride`.

```text
Strict 4x4 grid sprite sheet, 16 equal 256x256 cells, no borders, no grid lines, flat solid magenta (#FF00FF) background, no anti-aliasing. Semi-pixelated dark gothic 2.5D pixel art. Character: the Cinder Colossus, a huge walking glass kiln: a hulking giant with a torso of dark red firebrick held together by iron straps, black slag and charred skin on its arms and legs, a small bald slag head with glowing orange eyes, a glowing cracked heart of molten glass in the middle of its brick chest in #FF5500, a ragged leather apron, iron chimney stubs on its back. Animation: a slow, heavy stomping walk cycle, 4 frames per row. Col 0: contact, feet planted wide, body high. Col 1: left foot forward, body sinks, embers drop. Col 2: passing pose, body high. Col 3: right foot forward, body sinks. Rows: Row 0 facing the camera (south), Row 1 facing away (north, showing the chimney stubs), Row 2 right side profile walking right (east), Row 3 left side profile walking left (west). Feet on the same baseline in every cell, the Colossus fills about 90% of the cell height, identical proportions and palette in all cells. No grid lines, no borders, no labels. Keep every figure fully inside its own 256x256 cell. 1:1 square image.
```

### Priority 3: new enemy, NPCs, doors and effects

#### 10. `Rig_Glazier_Walk` and `Rig_Glazier_Attack` (folder `Glazier`), new Mirror Palace enemy

Walk sheet: the standard 4×4 walk layout. Attack sheet: Col 0 shield raised (block), Col 1 shield bash wind-up, Col 2 bash (hit frame), Col 3 shield shattered/death.

```text
Strict 4x4 grid sprite sheet, 16 equal 256x256 cells, no borders, flat solid magenta (#FF00FF) background, no anti-aliasing. Semi-pixelated dark gothic 2.5D pixel art. Character: a "Glazier", a dead Mirror Palace glass-setter in a heavy leather apron and soot mask, carrying a large rectangular pane of mirror glass in an iron frame as a tower shield. Standard 4-frame walk cycle per row: Col 0 contact/idle, Col 1 left step, Col 2 passing, Col 3 right step. Rows: Row 0 south (facing camera), Row 1 north (back), Row 2 east (right profile), Row 3 west (left profile). The mirror shield is always held in front. Shared foot baseline, same scale as the hero.
```

```text
Strict 4x4 grid sprite sheet, 16 equal 256x256 cells, no borders, flat solid magenta (#FF00FF) background, no anti-aliasing. Semi-pixelated dark gothic 2.5D pixel art. Same "Glazier": dead glass-setter in a leather apron and soot mask with a large iron-framed mirror pane used as a tower shield. Col 0: braced behind the raised mirror shield, blocking. Col 1: pulling the shield back to bash. Col 2: slamming the shield forward, a bright flash off the mirror (impact frame). Col 3: the mirror shield shattered into shards and the Glazier collapsing. Rows: Row 0 south (facing camera), Row 1 north (back), Row 2 east (right profile), Row 3 west (left profile). Shared foot baseline.
```

#### 11. `Rig_NPC_Tamsin` (folder `NPC_Tamsin`), Keeper of the Brazier

Rows: 0 idle in the coals (4 frames), 1 talking (4 frames), 2 handing something over (4 frames), 3 fading back into the fire (4 frames). She always faces the camera.

```text
Strict 4x4 grid sprite sheet, 16 equal 256x256 cells, no borders, flat solid magenta (#FF00FF) background, no anti-aliasing. Semi-pixelated dark gothic 2.5D pixel art, facing the camera in every cell. Character: Tamsin Vell, Keeper of the Brazier, an older woman made of ember and smoke sitting in the coals of a stone brazier, practical and dry, shawl and rolled sleeves, outlines glowing #FF5500 and #FFCC00. Row 0: idle, 4 frames of gentle flame flicker. Row 1: talking, 4 frames of small hand gestures and mouth movement. Row 2: leaning forward and handing over a small glowing object, 4 frames. Row 3: sinking back and fading into the fire, 4 frames. The brazier stays in the same place in every cell.
```

#### 12. `Rig_NPC_Nibbot` (folder `NPC_Nibbot`), the merchant in the mirror

Rows: 0 idle (4), 1 talking (4), 2 presenting wares (4), 3 coin flip after a sale (4). Facing the camera.

```text
Strict 4x4 grid sprite sheet, 16 equal 256x256 cells, no borders, flat solid magenta (#FF00FF) background, no anti-aliasing. Semi-pixelated dark gothic 2.5D pixel art, facing the camera in every cell. Character: Nibbot, a cheerful merchant who is only a reflection: he stands inside a tall ornate gilt mirror frame, his whole figure mirror-reversed and silvery, with a pack of curiosities and a lantern-lit coat lined with trinkets. Row 0: idle, 4 frames of shifting his weight. Row 1: talking, 4 frames of animated gestures. Row 2: opening his coat to show wares, 4 frames. Row 3: flipping a gold coin, 4 frames. The mirror frame stays fixed in every cell.
```

#### 13. `PROP_Shutter_Door_4x4` (dungeon sheet, `--mode dungeon`)

Rows: 0 closed (4 variants), 1 raising (4 frames), 2 open (4 variants), 3 cracked/broken (4 variants).

```text
Strict 4x4 grid prop sheet, 16 equal 256x256 cells, no borders, flat solid magenta (#FF00FF) background, no anti-aliasing. Semi-pixelated dark gothic 2.5D pixel art, three-quarter top-down view. Subject: a glass shutter gate that seals a dungeon doorway during a fight: a tall iron frame filled with thick smoky glass panes and an iron lattice, faint #FFCC00 light glinting on the glass. Row 0: closed gate, 4 slight variations. Row 1: 4 frames of the gate sliding up into the archway. Row 2: open archway with the gate tucked above, 4 variations. Row 3: cracked and shattered gate, 4 variations. Same doorway width and base line in every cell.
```

#### 14. `VFX_Combat_4x4` (eco sheet, `--mode eco`)

Rows: 0 flask tumble and shatter, 1 hit sparks (4), 2 gas puffs (4, neutral grey for in-game tint), 3 dust puffs and blast ring (4).

```text
Strict 4x4 grid effects sheet, 16 equal 256x256 cells, no borders, flat solid magenta (#FF00FF) background, no anti-aliasing. Semi-pixelated dark gothic pixel art effects, each centred in its cell. Row 0: a small glass flask tumbling through the air in 2 frames, then shattering into a splash in 2 frames (liquid pure cyan #00FFFF). Row 1: 4 frames of an impact spark burst, white core with #FFCC00 sparks flying outward. Row 2: 4 frames of a soft rolling gas puff, neutral light grey so it can be tinted. Row 3: 2 frames of a small dust puff at floor level, then 2 frames of a flat expanding blast ring in #FF5500 with black smoke.
```

### Older requests

The Phase 0–5 lists above are the original wishlist. The batch here replaces their character and VFX rows
(`CH_Player_Action_Socket_4x4`, `CH_Zombie_Exploder_Detonate_4x4`, `CH_Zombie_FastThrower_Cardinal_4x4`,
`VFX_Projectiles_Hazards_4x4`). `CH_Player_Diagonal_Walk_4x4` is still wanted.

---

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
---

## Hero combat sheets

Superseded by "Next batch: copy-ready prompts" at the top of this file (sheets 1–5).
