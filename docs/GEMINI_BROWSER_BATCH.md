# Gemini browser batch: remaining sheets

Prompts are in [TO_GENERATE.md](TO_GENERATE.md), section "Next batch: copy-ready prompts". Paste each one into
Gemini as-is, then add this line at the end of every prompt:

> No grid lines, no borders, no labels. Keep every figure and effect fully inside its own 256x256 cell. 1:1 square image.

Gemini tends to draw black cell dividers and to let light trails cross into the next cell. That line cuts both down.

## Status

Done and in the game (2026-11-21): hero Combo, Heavy, Evade, Skills, HurtIdle; Exploder attack; Slip attack;
Colossus attack; Glazier walk and attack; Tamsin; Nibbot rows 0-1; combat VFX sheet (gas puffs, flask). The Slip and Colossus attack sheets introduced new designs (silver mirror-man; firebrick kiln
giant), so their walk sheets must be redone to match. Until then the game keeps showing their old walk sheets
and does not use their attack frames.

## Still to generate

| # | Prompt in TO_GENERATE.md | Save as | Priority |
| --- | --- | --- | --- |
| 8b | `Rig_ZombieMirrorStalker_Walk` | `Rig_ZombieMirrorStalker_Walk.png` | High: unlocks the Slip attack frames |
| 9b | `Rig_BossGuardian_Walk` | `Rig_BossGuardian_Walk.png` | High: unlocks the Colossus attack frames |
| 7 | `Rig_ZombieFastThrower_Attack` | `Rig_ZombieFastThrower_Attack.png` | High |
| 12 | `Rig_NPC_Nibbot` rows 2-3 only (presenting wares, coin flip): keep him the silver reflection in the gilt frame from rows 0-1; the first try drew him as a brown-coated man there | `Rig_NPC_Nibbot.png` | Medium |
| 13 | `PROP_Shutter_Door_4x4` | `PROP_Shutter_Door_4x4.png` | Medium |
| – | `CH_Player_Diagonal_Walk_4x4` (Phase 1 list) | `Rig_DungeoneerHero_DiagonalWalk.png` | Low |

## Where files go

Drop the PNG or JPG anywhere under `Content/Art/` and tell me; I'll clean the background, halve 2048 px images to
1024, rename and slice them. If you slice yourself, give **every sheet its own output folder**: the slicer writes
`AtlasMetadata.json` and `rig_sockets.json` into the output folder, and sharing a folder overwrites the walk
sheet's copies.

```powershell
$S = ".claude/skills/sprite-sheet-25d-cutter/scripts/slice_normal_rig.py"
python $S --mode rig --input "Content/Art/Raw/Rig_ZombieMirrorStalker_Walk.png" --output "Content/Art/Rigs/ZombieMirrorStalkerMoves/Walk" --validate-stride --repair-stride
python $S --mode rig --input "Content/Art/Raw/Rig_BossGuardian_Walk.png"        --output "Content/Art/Rigs/BossGuardianMoves/Walk" --validate-stride --repair-stride
python $S --mode rig --input "Content/Art/Raw/Rig_ZombieFastThrower_Attack.png"  --output "Content/Art/Rigs/ZombieFastThrowerMoves/Attack"
python scripts/verify_architecture_invariants.py
```

## Before slicing

- Check the corners are plain magenta. Black divider lines at cell edges become opaque lines in the `_D` map.
- Check each row faces the right way (row 0 south, 1 north, 2 east, 3 west) and the feet sit on one line.
- Check every cell shows the right character (one Colossus frame came back with an Exploder in it).
