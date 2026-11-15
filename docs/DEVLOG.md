# Dark Dungeoneer — Engineering Devlog

Technical notes covering architecture decisions, constraint trade-offs, Unreal Engine 5.8 compiler migrations, and UI direction for **Dark Dungeoneer** (`UE 5.8` / `C++20`).

---

## Devlog #01 (`v0.1.0` – `v0.3.0`) — 4-Axis Compass Locomotion, Viewport Isolation & 2.5D Sprite Shading

### Why 4 Discrete Axes + `FDungeoneerSensoryViewExtension`
- Top-down 2.5D sprites suffer visually when rotated arbitrarily in screen space or allowed uneven diagonal speed (`sqrt(2)`). `UDungeoneerMovementComponent` quantizes raw stick/WASD vectors across 8 compass octants (`N, NE, E, SE, S, SW, W, NW`), pulling normalized vectors from `DungeoneerMath::GCompassUnitVectors` (`INV_SQRT_2 = 0.70710678f`) so cardinal and diagonal velocities match within `< 1e-5f`.
- Aiming the narrow-beam `USpotLightComponent` flashlight is intentionally decoupled from movement octants so players can kite backward through dark corridors while sweeping corridors ahead.
- For co-op parity between Local Split-Screen (`UDungeoneerGameViewportClient`) and Online Multiplayer, applying Dark Vision or Acute Hearing globally via a single `PostProcessVolume` would leak Player 1's night-vision reveal into Player 2's screen. Instead, each `ULocalPlayer` owns a `UDungeoneerLocalPlayerSubsystem` hosting an isolated `FDungeoneerSensoryViewExtension` and dedicated `MID_SensoryOverlay`, injected only when `InView.PlayerIndex == LocalPlayer->GetControllerId()`.

### 2.5D Texel Snapping & Pro Asset Pipeline
- To eliminate sub-pixel shimmering on drifting ortho-feel cameras, `Shaders/Private/PixelLit25D.ush` snaps UV coordinates to texel centers (`64 px / 100 UU` calibration) and uses 4x4 Bayer dithering for shadows without translucent sorting artifacts.
- Character sheets (`DungeoneerHero`, `ZombieExploder`, `ZombieFastThrower`) were generated using `Model: "pro"`, then passed through `.agents/skills/sprite-sheet-25d-cutter/scripts/slice_normal_rig.py` to strip chroma halos, derive Scharr tangent-space normal maps (`_N.png`), pack `_ORM.png` channels, and record every file hash into `Content/AssetManifest.json`.
- *Honest art note*: Visual review caught that `Rig_ZombieFastThrower_Raw_D.png` uses East/West sprint/throw frames across its 4 rows rather than distinct South/North silhouettes, and a follow-up AI regeneration attempt hit quota exhaustion (`429`). We preserved the existing East/West sheet cleanly until fresh art quota is available.

---

## Devlog #02 (`v0.4.0` – `v0.6.0`) — Zero-Allocation Hot Paths, Chrono-Leech Math & Deterministic Audio

### Keeping `Tick` Heap-Free
- Instead of runtime `GetAllActorsOfClass` scans or `SpawnActor` projectile bursts, `UZombieSpatialSubsystem` maintains fixed-size `TrackedPlayers` / `TrackedZombies` arrays, pre-allocates 16 hazard zones per element (`Fire`, `Poison`, `ChronoGas`) and 32 scorch decals during startup, and bins active entities into a `32×32` (`1024`-cell) spatial grid.
- The **Chrono-Leech Conservation Law** binds `AHazardZone_ChronoGas` directly to its owning `AZombieFastThrower`: as a player's movement speed scales down by `V_player = V_base * (1 - S)`, the linked thrower gains proportional velocity (`V_zombie = V_zombie_base * (1 + S * SpeedStealScalar)`) and shortened throw intervals.

### Mirror-Palace Stencils & Step-Cycle Audio
- `UDungeonGeneratorSubsystem` constructs deterministic multi-floor room DAGs from a single replicated `FRandomStream` seed (`Entry -> Combat -> MirrorPalaceMaze -> Vendor/RestingGround -> DeepFogGauntlet -> MajorLootVault`).
- Phantasm illusion walls (`AMirrorWall_Phantasm`, `CustomDepthStencil = 104`) diverge slightly in normal roughness (`0.48` vs `0.25`) and trigger phase-inverted acoustic rings so perceptive players can spot fake walls by light grazing or footsteps alone.
- All gameplay footsteps, Exploder fuse ticks, and Gibberish syllables (`CharCode % 24` mapping to pre-synthesized micro-grains in `Content/Audio/Voices/`) execute deterministically on distance accumulators and `UAnimNotify_DungeoneerTimedCue`—zero `FTimerHandle` loops anywhere in C++.

---

## Devlog #03 (`v0.7.0` – `v0.7.1`) — Compiling Against UE 5.8.2 & Designing the Main Menu Mockup

### Real-Engine Compilation Fixes (`D:\epuc\UE_5.8`)
Running `UnrealBuildTool` and headless tests against the installed UE `5.8.2` (`Build.bat`) caught five concrete UE 5.8 API/UHT rules:
1. **Shared Editor Build Environment (`V7`)**: UE 5.8 enforces `BuildSettingsVersion.V7` on Editor targets; `BuildSettingsVersion.V5` mismatched UE 5.8's shared warning configuration until upgraded in both `.Target.cs` files.
2. **UHT Reflection Offset Cap (`uint16`)**: Embedding a `336 KB` `TStaticArray<FZombieSpatialCell, 1024>` directly ahead of `UPROPERTY()` hazard pools in `UZombieSpatialSubsystem` pushed `FirePool`'s member offset past `65535`, failing MSVC `STRUCT_OFFSET` codegen (`C4309`). Wrapping the grid in `TUniquePtr<FSpatialGridBuffer>` allocated once inside `Initialize()` shrunk the `UObject` footprint while preserving zero `Tick` allocations.
3. **Blueprint Type Constraints & Non-`constexpr` Math**: `FDungeoneerStepEvent::StepSequenceIndex` switched from `uint32` to `int32`, and `FVector2f` / `FVector` compass lookup helpers moved from `constexpr` to `inline const` / `inline`.
4. **Engine Header Updates**: Switched `FDungeoneerSensoryViewExtension` to inherit `FSceneViewExtensionBase` with tracked `TWeakObjectPtr<ULocalPlayer>`, fixed `OnlineSubsystem` headers (`Interfaces/VoiceInterface.h`, `Interfaces/OnlinePresenceInterface.h`), and updated automation flags to `EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter`.

With those fixes, `DarkDungeoneerEditor (Win64 Development)` compiled cleanly into `UnrealEditor-DarkDungeoneer.dll` and all three automation specs (`DiagonalEqualsCardinalSpeed`, `ChronoLeechConservationLaw`, `GibberishPhonemeDeterminism`) passed in `UnrealEditor-Cmd.exe` (`EXIT CODE: 0`).

### Main Menu Visual Direction (`v0.7.1` -> Next: `v0.8.0`)
- To establish a concrete UI target before building CommonUI/UMG blueprints, we designed [`MENU_MOCKUP.md`](MENU_MOCKUP.md) (`docs/assets/dark_dungeoneer_menu_concept.png`) using our actual Pro-generated sprites under directional flashlight + Chrono-Gas lighting, paired with an interactive multi-tab prototype (`docs/assets/dark_dungeoneer_menu_mockup.html`).
- Next step (`v0.8.0`): implement `TASK-801` through `TASK-804` (`UDungeoneerMainMenuWidget`, `AMenuDioramaStageActor`, `UDungeoneerCoopLobbyWidget`, `UDungeoneerMenuFooterWidget`) and extend automated tests to cover `TradeTwoPhaseCommitTest`.
