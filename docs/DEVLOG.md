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

---

## Devlog #04 (`v0.8.0`) — Main Menu, Live 2.5D Diorama, Prediction & Seed-Only Replication

### Turning `MENU_MOCKUP.md` into C++20 UI Modules (`TASK-801` – `TASK-804`)
- Implemented all four `v0.8.0` UI classes purely in C++ (`Source/DarkDungeoneer/{Public,Private}/UI/`) using `BindWidgetOptional` fields so future data-only UMG Blueprint shells can style buttons and typography without placing gameplay logic in Blueprint graphs.
- `AMenuDioramaStageActor` reuses the `-60°` pitched dungeon camera, Hero narrow-cone `USpotLightComponent` interpolation across compass octants, Exploder CPD ignition pulses, Chrono-Gas mist toggles, `Stencil 104` Phantasm / `Stencil 106` Decoy cutouts, and deterministic phase-accumulated `Stencil 105` sonar bursts without `FTimerHandle` or `GetAllActorsOfClass`.
- `UDungeoneerCoopLobbyWidget` strictly reads `UGameInstance::GetLocalPlayers()`, `UDungeoneerLocalPlayerSubsystem`, `UDungeoneerGameViewportClient`, and `UDungeoneerSocialSubsystem`, ensuring multi-viewport sensory badges (`Dark Vision`, `Acute Hearing`, `Scent / Thermal`) stay isolated per split-screen pane.

### Multiplayer Prediction, Portal Budgeting & Fog Net Culling (Rules `05` & `06`)
- Added `FSavedMove_Dungeoneer` + `FNetworkPredictionData_Client_Dungeoneer` packing a quantized `uint8` `ChronoSlowRatio` (`0–255` over `[0, MaxChronoSlowRatio]`) into `FSavedMove_Character` moves so clients stepping across `AHazardZone_ChronoGas` boundaries replay movement deterministically without rubber-banding.
- Added `U25DDirectionalRigComponent::ConfigureVolumetricShadowProxy` to standardize hidden shadow-casting primitives (`bRenderInMainPass = false`, `bCastHiddenShadow = true`, `CastShadow = true`) under flashlight lighting.
- Added `UMirrorCaptureBudgetSubsystem` (`UTickableWorldSubsystem`) managing `AMirrorWall_Phantasm` reflection captures round-robin via `CaptureSceneDeferred()`, capped at `<= 2` portals per frame and inversely scaled when `2P–4P` local split-screen viewports are active.
- Replicated dungeon seeds cleanly through `ADungeoneerGameState` (`FDungeonFloorSeed` + LCG `[R]` rerolls) rather than replicating individual dungeon geometry, and overrode `AZombieBase::IsNetRelevantFor` + dynamic `SetNetUpdateFrequency(15.0f` vs `2.0f)` so zombies shrouded in darkness outside flashlight cones, `DarkVisionRadius`, and active acoustic pulses are culled from unnecessary replication while `AMirrorDecoyNode` clones remain strictly client-side.

### Verification (`v0.8.0`)
- Added `DarkDungeoneer.Social.TradeTwoPhaseCommitRollbackAndCommit` verifying insufficient-gold rollback (unlocking both parties' items with zero duplication) alongside atomic swap commit, plus `DarkDungeoneer.UI.MenuNavigationAndFloorSeedDeterminism` covering `FSavedMove_Dungeoneer` byte quantization, menu navigation wrap-around, LCG seed determinism, portal capture budgeting, and diorama spotlight interpolation.
- Rebuilt `DarkDungeoneerEditor (Win64 Development)` on UE `5.8.2` (`BuildSettingsVersion.V7`), passed `scripts/verify_architecture_invariants.py` (`73` files verified), and passed all 5 headless automation specs in `UnrealEditor-Cmd.exe` (`EXIT CODE: 0`).

---

## Devlog #05 (`v0.9.0`) — Per-Viewport HUD, 2PC Trade Modal, `.uasset` Bootstrap & 2.5D Hybrid Animation Pipeline

### In-Game HUD, Vendor 2PC Modal & Idempotent Asset Bootstrap (`TASK-901` – `TASK-903`)
- Added `UDungeoneerPlayerHUDWidget` (`Source/DarkDungeoneer/{Public,Private}/UI/DungeoneerPlayerHUDWidget.*`) attached once per-`ULocalPlayer` viewport (`AddToPlayerScreen(5)`) in `ADungeoneerCharacter::BeginPlay`. Reading local movement/aim octants directly on locally controlled pawns keeps octant feedback crisp at zero network latency while preserving replicated octant readouts on simulated proxies.
- Added `UDungeoneerTradeModalWidget` (`DungeoneerTradeModalWidget.*`) binding `AVendorActor`, `UGibberishSpeechComponent`, `UTradeManagerComponent`, and `UDungeoneerInventoryComponent`. Selecting a seeded catalog relic triggers procedural merchant gibberish (`SpeakProceduralGibberish`) and purchasing commits through `Server_ExecuteVendorPurchase` + `OnTradeAckReceived`.
- Added `UDungeoneerAssetBootstrapSubsystem` (`UGameInstanceSubsystem`) to import all `18` 2.5D rig PNGs (`TF_Nearest`, `TEXTUREGROUP_Pixels2D`), load `50` WAV voice stems/phoneme grains, register all sensory scalar/vector parameters on `MPC_SensoryState`, and serialize `MPC_SensoryState.uasset`, `DA_25DMaterialConfig.uasset`, and `DA_MovementConfig.uasset` idempotently (loading existing packages cleanly before save fallback).

### 2.5D Hybrid Sprite Sheet Slicer & Runtime Orchestrator (`slice_normal_rig.py` & `U25DAnimOrchestratorComponent`)
- Upgraded `.agents/skills/sprite-sheet-25d-cutter/scripts/slice_normal_rig.py` with vectorized `#FF00FF` hard-edge keying + zero-halo neighbor cleanup (`_D.png`), pure-NumPy Chamfer Euclidean distance-transform dome bevel + 3x3 Sobel luminance gradient normal maps (`_N.png`), high-luminance emissive extraction (`_E.png` for `#FFCC00` lantern gold, `#FF5500` magma belly, and `#00FFFF` cyan flasks), and 16-cell `AtlasMetadata.json` UV sub-rectangles + bottom-center foot pivots. Regenerated all 6 rig sets and synced SHA-256 hashes in `Content/AssetManifest.json`.
- Implemented `U25DAnimOrchestratorComponent` (`Source/DarkDungeoneer/{Public,Private}/Rendering/25DAnimOrchestratorComponent.*`) driving distance-locked 4-frame walk cycles (`AccumulatedDistance / StrideLength`), deterministic `OnStepContact` floor `UPhysicalMaterial` (`PhysicsCore`) + `UDungeoneerSensoryWorldSubsystem::BroadcastNoiseEvent` triggers on frames `0` and `2`, 120Hz procedural sub-frame springs (stride hop, velocity lean, belly-swell), hidden volumetric shadow capsule setup, and zero-MID Custom Primitive Data slots `[0..4]`.

### Verification (`v0.9.0`)
- Added `DarkDungeoneer.UI.HUDAndTradeModalAndAssetBootstrap` and `DarkDungeoneer.Rendering.25DAnimOrchestratorAndAtlasMetadata` to `DungeoneerVerificationSpecs.cpp`.
- Passed `scripts/verify_architecture_invariants.py` (`81` files verified), rebuilt both `DarkDungeoneerEditor Win64 Development` and standalone `DarkDungeoneer Win64 Development` (`Binaries/Win64/DarkDungeoneer.exe`) with zero project warnings, and passed all `7` headless UE `5.8.2` automation tests (`EXIT CODE: 0`).
