---
trigger: glob
globs: "*.h, *.cpp, *.Build.cs, *.Target.cs"
description: "C++20 coding standards, 4-axis compass quantization, and zero-allocation hot-path constraints for UE 5.8."
---

# UE 5.8 C++20 & Locomotion Constraints

1. **No Blueprint Logic**: Implement all movement quantization, sensory buffs, AI state transitions, and replication in C++. Expose tuning scalars via `UDataAsset` or `UPROPERTY(EditDefaultsOnly, Category="Config")`.
2. **4-Axis (8-Compass) Quantization**:
   - Quantize 2D input vectors `(X, Y)` onto the 4 movement axes using `atan2` snapped to `PI / 4.0f` (45-degree octants: N, NE, E, SE, S, SW, W, NW).
   - Normalize diagonal step vectors (`INV_SQRT_2 = 0.70710678f`) so diagonal speed along NE/NW/SE/SW strictly equals cardinal speed along N/E/S/W.
   - Decouple facing octant (driven by aim/flashlight vector) from movement octant (driven by step vector).
3. **Chrono-Leech Conservation Law**:
   - When a player is inside a Slo-Mo Gas volume with debuff ratio `S` in `[0.0, MaxSlow]`, compute player speed as `V_player = V_base * (1.0f - S)`.
   - Apply the stolen velocity delta directly to the owning Fast Zombie's movement and projectile cadence: `V_zombie = V_zombie_base * (1.0f + (S * LeechMultiplier))`.
4. **Hot-Path Discipline**:
   - Never call `GetAllActorsOfClass`, `SpawnActor`, or dynamic `TArray` reallocations inside `Tick`. Use pre-allocated ring buffers and `UWorldSubsystem` spatial hashes.