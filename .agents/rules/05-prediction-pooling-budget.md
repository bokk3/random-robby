---
trigger: glob
globs: "*.h, *.cpp"
description: "Enforces CMC network prediction for Chrono-Leech speed scaling, pre-allocated object pools for projectiles/hazards/decoys, and 2.5D shadow proxy budgets."
---

# Network Prediction, Entity Pooling & 2.5D Shadow Rules

1. **Chrono-Leech Network Prediction (`FSavedMove_Character`)**:
   - Never mutate `UCharacterMovementComponent::MaxWalkSpeed` on an arbitrary client/server `Tick` without prediction sync.
   - Replicate the active `ChronoSlowRatio` (quantized to `uint8` in `0..255` steps) inside a custom `FSavedMove_Dungeoneer` and `FNetworkPredictionData_Client_Dungeoneer` so client prediction and server authority compute identical step deltas without rollbacks.

2. **Pre-Allocated Subsystem Object Pools (Zero Runtime Spawn/Destroy)**:
   - Calling `SpawnActor` or `Destroy` during active dungeon combat for projectiles, hazard zones (Fire, Poison, Slo-Mo Gas), audio ripples, or Mirror-Maze zombie decoys is strictly forbidden.
   - Manage all transient combat entities through `UDungeoneerPoolSubsystem` (`UWorldSubsystem`) using pre-allocated contiguous ring buffers initialized during level load.

3. **2.5D Flashlight Shadow Proxies**:
   - Flat 2.5D cutout planes cast paper-thin shadows when lit from orthogonal compass angles (`E` or `W` relative to the camera).
   - Every character and zombie rig must include an invisible low-poly 3D cylindrical/capsule shadow proxy mesh (`bRenderInMainPass = false`, `bCastHiddenShadow = true`) so dynamic flashlight beams cast volumetric shadows from all 8 compass directions.

4. **Split-Screen & Mirror Capture Frame Budgeting**:
   - Never update more than 2 `USceneCaptureComponent2D` mirror portals on the same frame. Use round-robin time-slicing (`CaptureEveryFrame = false`, manual `CaptureSceneDeferred()`) scaled inversely by the active `ULocalPlayer` split-screen count.