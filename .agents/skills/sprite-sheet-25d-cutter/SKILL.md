---
name: sprite-sheet-25d-cutter
description: Generates, modifies, and slices 2D semi-pixelated asset sheets into 4-directional and 8-compass 2.5D cutout meshes with normal maps in Unreal Engine 5.8. Use when creating character rigs, zombies, props, or modifying downloaded free-use assets.
---

# 2D-to-2.5D Asset Map Cutter & Rigging Skill

## Decision Tree
1. **Source Check**:
   - **Generated Asset**: Proceed directly to slicing and normal-map extraction.
   - **Downloaded Free-Use Asset**: Run the mandatory transformation pass first (palette quantization, edge normal synthesis, silhouette remix) via `scripts/slice_normal_rig.py --remix`.
2. **Directional Target**:
   - **4-Directional Asymmetric Rig**: Slice into N, S, E, W sheets (or N, S, E with mirrored W if symmetrical) and assemble into multi-plane 2.5D cutout sockets (Head, Torso, Limbs, Weapon/Flashlight).
   - **8-Compass Billboard**: Slice full 8-octant atlas (`N, NE, E, SE, S, SW, W, NW`).

## Execution Instructions
1. Execute the black-box slicer script:
   `python3 .agents/skills/sprite-sheet-25d-cutter/scripts/slice_normal_rig.py --input <raw_sheet.png> --directions 4 --generate-normals --output Content/Art/Rigs/<AssetName>/`
2. Import the generated `_D` (Diffuse), `_N` (Tangent Normal), and `_ORM` (Occlusion/Roughness/Metallic) atlases into UE 5.8 with `TextureGroup = TEXTUREGROUP_Pixels2D` (Nearest-Neighbor filtering).
3. Bind the sliced pivot sockets to `U25DDirectionalRigComponent` and verify texel density (64 px / 100 UU).