# Gemini browser batch: remaining sheets

Prompts are in [TO_GENERATE.md](TO_GENERATE.md), section "Next batch: copy-ready prompts". Paste each one into Gemini as-is, then add this line at the end of every prompt:

> No grid lines, no borders, no labels. Keep every figure and effect fully inside its own 256x256 cell. 1:1 square image.

Gemini tends to draw black cell dividers and to let light trails cross into the next cell. That line cuts both down.

Save each result as a PNG in `Content/Art/Raw/` under the exact file name below. Done already: Combo, Evade.

| # | Prompt in TO_GENERATE.md | Save as | Slice mode |
| --- | --- | --- | --- |
| 2 | `Rig_DungeoneerHero_Heavy` | `Rig_DungeoneerHero_Heavy.png` | rig |
| 4 | `Rig_DungeoneerHero_Skills` | `Rig_DungeoneerHero_Skills.png` | rig |
| 5 | `Rig_DungeoneerHero_HurtIdle` | `Rig_DungeoneerHero_HurtIdle.png` | rig |
| 6 | `Rig_ZombieExploder_Attack` | `Rig_ZombieExploder_Attack.png` | rig |
| 7 | `Rig_ZombieFastThrower_Attack` | `Rig_ZombieFastThrower_Attack.png` | rig |
| 8 | `Rig_ZombieMirrorStalker_Attack` | `Rig_ZombieMirrorStalker_Attack.png` | rig |
| 9 | `Rig_BossGuardian_Attack` | `Rig_BossGuardian_Attack.png` | rig |
| 10a | `Rig_Glazier_Walk` (first prompt) | `Rig_Glazier_Walk.png` | rig, with stride check |
| 10b | `Rig_Glazier_Attack` (second prompt) | `Rig_Glazier_Attack.png` | rig |
| 11 | `Rig_NPC_Tamsin` | `Rig_NPC_Tamsin.png` | rig |
| 12 | `Rig_NPC_Nibbot` | `Rig_NPC_Nibbot.png` | rig |
| 13 | `PROP_Shutter_Door_4x4` | `PROP_Shutter_Door_4x4.png` | dungeon |
| 14 | `VFX_Combat_4x4` | `VFX_Combat_4x4.png` | eco |

`CH_Player_Diagonal_Walk_4x4` is also still wanted (see the Phase 1 list in TO_GENERATE.md). Save it as `Rig_DungeoneerHero_DiagonalWalk.png`.

## Slice commands

Run from the repo root. Each command writes the `_D`, `_N`, `_ORM`, `_E` maps and updates `Content/AssetManifest.json`.

```powershell
$S = ".claude/skills/sprite-sheet-25d-cutter/scripts/slice_normal_rig.py"

# Hero sheets
foreach ($n in "Heavy","Skills","HurtIdle","DiagonalWalk") {
  python $S --mode rig --input "Content/Art/Raw/Rig_DungeoneerHero_$n.png" --output "Content/Art/Rigs/DungeoneerHero"
}

# Enemies and boss
python $S --mode rig --input "Content/Art/Raw/Rig_ZombieExploder_Attack.png"      --output "Content/Art/Rigs/ZombieExploder"
python $S --mode rig --input "Content/Art/Raw/Rig_ZombieFastThrower_Attack.png"   --output "Content/Art/Rigs/ZombieFastThrower"
python $S --mode rig --input "Content/Art/Raw/Rig_ZombieMirrorStalker_Attack.png" --output "Content/Art/Rigs/ZombieMirrorStalker"
python $S --mode rig --input "Content/Art/Raw/Rig_BossGuardian_Attack.png"        --output "Content/Art/Rigs/BossGuardian"

# Glazier: walk sheet gets the stride check, attack sheet does not
python $S --mode rig --input "Content/Art/Raw/Rig_Glazier_Walk.png" --output "Content/Art/Rigs/Glazier" --validate-stride --repair-stride
python $S --mode rig --input "Content/Art/Raw/Rig_Glazier_Attack.png" --output "Content/Art/Rigs/Glazier"

# NPCs
python $S --mode rig --input "Content/Art/Raw/Rig_NPC_Tamsin.png" --output "Content/Art/Rigs/NPC_Tamsin"
python $S --mode rig --input "Content/Art/Raw/Rig_NPC_Nibbot.png" --output "Content/Art/Rigs/NPC_Nibbot"

# Door and effects
python $S --mode dungeon --input "Content/Art/Raw/PROP_Shutter_Door_4x4.png" --output "Content/Art/Dungeon/PROP_Shutter_Door_4x4"
python $S --mode eco --input "Content/Art/Raw/VFX_Combat_4x4.png" --output "Content/Art/Ecosystem/VFX_Combat_4x4" --cell-size 256

python scripts/verify_architecture_invariants.py
```

## Before slicing

- Check the corners are plain magenta. Black divider lines at the cell edges become opaque lines in the `_D` map. Tell me and I'll strip them, as I did for Combo and Evade.
- Check each row faces the right way (row 0 south, 1 north, 2 east, 3 west) and the feet sit on the same line in every cell.
- If Gemini returns a JPG, convert it to PNG first, or just hand it to me.

Drop the files in `Content/Art/Raw/` and tell me. I'll clean the edges, slice everything, check the results and run the verifier.
