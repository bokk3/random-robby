---
name: compass-movement-sensory
description: Implements 4-axis (8-compass) player movement, directional flashlight aiming, and sensory upgrade abilities (Dark Vision, Acute Hearing, Scent Trails) in Unreal Engine 5.8 C++. Use when building or modifying player locomotion, input mapping, flashlight cones, or fog-piercing sensory perks.
---

# 4-Axis Compass Movement & Sensory Progression Skill

## Decision Tree
1. **Is the task modifying player movement or input?**
   - Go to **Protocol A: 4-Axis Compass Quantizer**.
2. **Is the task adding or tuning sensory abilities against darkness/fog?**
   - Go to **Protocol B: Sensory Ability Pipeline**.

## Protocol A: 4-Axis Compass Quantizer
1. Read Enhanced Input 2D `FVector2DMove` from `UInputAction`.
2. If `Move.SquaredLength() < DeadZoneSq`, zero velocity and maintain current `ECompassOctant`.
3. Compute angle `Theta = FMath::Atan2(Move.Y, Move.X)` and snap to the nearest multiple of `PI / 4.0f`:
   - `OctantIndex = FMath::RoundToInt(Theta / (PI / 4.0f)) & 7`
   - Map `0..7` to `(E, NE, N, NW, W, SW, S, SE)`.
4. Drive the 4-directional / 8-compass 2.5D mesh flipbook from the quantized octant and step phase accumulator.

## Protocol B: Sensory Ability Pipeline
1. Register sensory perks as `UGameplayAbility` or modular `USensoryPerkComponent` nodes.
2. Update the local player's camera post-process dynamic material instance (`MID_SensoryOverlay`) and `MPC_SensoryState` per-viewport:
   - **Dark Vision**: Increase local point-light ambient fill radius and reduce exponential fog extinction coefficient within `VisionRange`.
   - **Acute Hearing**: Subscribe to `FNoiseEvent` broadcasts from zombies and illusions; spawn a stencil-masked ring wave at the sound origin visible through occluded fog.

## Verification Checklist
- [ ] Diagonal speed (`NE`, `NW`, `SE`, `SW`) matches cardinal speed (`N`, `S`, `E`, `W`) to within `1e-5`.
- [ ] Split-screen Player 1 enabling Dark Vision does not alter Player 2's viewport darkness.