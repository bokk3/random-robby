---
name: sprite-sheet-25d-cutter
description: Generates, modifies, and slices 2D semi-pixelated asset sheets into 4-directional and 8-compass 2.5D cutout meshes with normal maps in Unreal Engine 5.8. Use when creating character rigs, zombies, props, or modifying downloaded free-use assets.
---

# 2D-to-2.5D Asset Map Cutter & Rigging Skill

## Decision Tree
1. **Source Check**:
   - **Generated Asset**: Run stride validation (`--validate-stride` / `--repair-stride`) before slicing and normal-map extraction.
   - **Downloaded Free-Use Asset**: Run the mandatory transformation pass first (palette quantization, edge normal synthesis, silhouette remix) via `scripts/slice_normal_rig.py --remix --validate-stride`.
2. **Directional Target**:
   - **4-Directional Asymmetric Rig**: Slice into Row 0 (South/SE), Row 1 (North/SW), Row 2 (East/NE), Row 3 (West/NW) sheets and assemble into multi-plane 2.5D cutout sockets or `U25DAnimOrchestratorComponent` / `U25DDirectionalRigComponent`.
   - **8-Compass Billboard**: Slice full 8-octant atlas (`N, NE, E, SE, S, SW, W, NW`).

## 16-Cell (4x4) Walk-Cycle Kinematic Specification
All walking character and zombie sprite sheets MUST strictly follow this 4-Row x 4-Column matrix so distance-locked playback transitions cleanly and feet alternate without hopping or sliding:

- **Rows (4 Orientations)**:
  - **Row 0 (Top)**: South (Front view, walking toward camera) — or SE on diagonal sheets.
  - **Row 1**: North (Back view, walking away from camera) — or SW on diagonal sheets.
  - **Row 2**: East (Right side profile, walking right) — or NE on diagonal sheets.
  - **Row 3 (Bottom)**: West (Left side profile, walking left) — or NW on diagonal sheets.

- **Columns (4 Alternating Stride Phases per Row)**:
  - **Column 0 (Frame 0 — Contact / Idle Pose A)**: Both feet planted close together directly beneath the hips as weight transfers; torso and head at peak height (`+1 to +2 px`). Used as the default standing Idle frame when `Velocity.IsNearlyZero()`.
  - **Column 1 (Frame 1 — Full Left Step)**: Left leg extended forward, right leg pushing off behind; torso dips (`-1 to -2 px`). Triggers `OnFootstepContact(EFoot::Left)`. South view: left foot lower and slightly larger; North view: right heel lifts; East/West side profiles: open scissor stance with left leg forward.
  - **Column 2 (Frame 2 — Passing Pose B)**: Mid-stride weight transfer with legs crossing directly under the center of gravity; silhouette narrows and torso rises back to peak height (`+1 to +2 px`).
  - **Column 3 (Frame 3 — Full Right Step)**: Exact opposite of Column 1. Right leg extended forward, left leg pushing off behind; torso dips (`-1 to -2 px`). Triggers `OnFootstepContact(EFoot::Right)`. South view: right foot lower; East/West side profiles: opposite scissor stance with right leg forward.

### Reusable AI / Procedural Prompt Template (4x4 Walk Cycle)
```text
Strict 4x4 grid (16 equal cells, zero borders or labels) semi-pixelated dark-gothic 2.5D sprite sheet on a solid flat magenta (#FF00FF) chroma-key background with zero anti-aliasing on outer silhouette edges.
Rows (top to bottom): Row 0 = South (front view), Row 1 = North (back view), Row 2 = East (right side profile), Row 3 = West (left side profile).
Columns (left to right, alternating walk stride):
- Col 0: Contact / Idle Pose A — both feet planted close together directly beneath hips, torso & head at peak height (+1 to +2 px).
- Col 1: Full Left Step — left leg extended forward, right leg pushing back, torso dipped (-1 to -2 px), open opposite scissor stance in side view / left foot lower in South view.
- Col 2: Passing Pose B — legs crossing beneath center of gravity, narrower silhouette, torso back at peak height (+1 to +2 px).
- Col 3: Full Right Step — exact opposite stride of Col 1 (right leg forward, left leg back, torso dipped -1 to -2 px).
Consistent proportions, bottom-center foot baseline aligned across every cell, emissive accents kept crisp (#FFCC00 lantern / #FF5500 magma / #00FFFF flask).
```

## Execution Instructions
1. Validate, repair duplicated strides if needed, and slice:
   `python3 .agents/skills/sprite-sheet-25d-cutter/scripts/slice_normal_rig.py --input <raw_sheet.png> --directions 4 --generate-normals --validate-stride --repair-stride --output Content/Art/Rigs/<AssetName>/`
2. Import the generated `_D` (Diffuse), `_N` (Tangent Normal), `_E` (Emissive), and `_ORM` (Occlusion/Roughness/Metallic) atlases into UE 5.8 with `TextureGroup = TEXTUREGROUP_Pixels2D` (Nearest-Neighbor filtering).
3. Bind the sliced UV cells and pivots to `U25DAnimOrchestratorComponent` / `U25DDirectionalRigComponent` and verify texel density (64 px / 100 UU).