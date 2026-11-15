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

---

## Devlog #06 (`v0.9.1`) — 2.5D Modular Dungeon HISM Assembly, South-Wall Cutaways & Mirror Light-Bounce

### Dungeon Atlas Slicing & `#00FF00` Mirror Portal Extraction (`slice_normal_rig.py --mode dungeon`)
- Extending `.agents/skills/sprite-sheet-25d-cutter/scripts/slice_normal_rig.py` cleanly required splitting environment assets into two distinct material families under `--mode dungeon` (`TO_GENERATE_DUNGEON.md`):
  1. **`ATLAS_*` Seamless Tile Arrays**: Edge-to-edge 4x4 floor tiles skip `#FF00FF` chroma keying completely (`alpha = 255`), synthesizing deep-relief mortar normal maps (`_N.png`), 8-bit POM heightmaps (`_H.png`), and wetness/roughness masks (`_R.png`) where standing water puddles encode exact `0.05` specular roughness (`byte 13`) to mirror dynamic player lanterns and zombie embers.
  2. **`PROP_*` Cutouts & Mirror Portal Keys**: Strips `#FF00FF` magenta into clean `0` alpha, detects `#00FF00` pure green interior mirror panes, packs `MirrorPortalMask = 255` directly into the Red channel of `_ORM.png` (`R = MirrorPortalMask`, `G = Roughness`, `B = Metallic`), and swaps the diffuse glass to dark slate `(18, 24, 32, 255)` so unmasked fallback renders never flash neon green.

### Zero-Draw-Call HISM Assembly, Per-Viewport Cutaways & Law-of-Reflection Bounce
- **`ADungeoneerTileMapInstancer`**: Instantiates `UDungeonGeneratorSubsystem` floor graphs into 4 `UHierarchicalInstancedStaticMeshComponent` buckets (`FloorHISM`, `WallHISM`, `MirrorHISM`, `BreakableHISM`) supporting both cardinal (`N/S/E/W`) and 45° diagonal chamfer walls (`NE/NW/SE/SW`) so 8-compass traversal flows around octagon corners without jagged stair-stepping.
  - Note on UE `5.8.2` HISM instances in headless `-NullRHI` tests: `AddInstance()` skips creation if `GetStaticMesh()` is null, so `EnsureMeshAssetsInitialized()` binds `/Engine/BasicShapes/Plane.Plane` and `Cube.Cube` fallbacks safely outside the CDO constructor (`OnConstruction` / `BeginPlay`), guaranteeing `PerInstanceSMCustomData` reads and sweeps work identically in headless CI and live gameplay.
  - **Split-Screen Cutaway Zero-Allocations**: `UpdateSouthWallCutawaysForViewports()` iterates every `ULocalPlayer` viewport (`GetGameInstance()->GetLocalPlayers()`), runs a pre-allocated `World->SweepMultiByChannel` plus deterministic segment-vs-box intersection against `South` / `SouthEast` / `SouthWest` wall entries using a fixed `PendingCutawayFlags` byte buffer, and swaps occluded foreground walls via `WallHISM->SetCustomDataValue(..., 0, 1.0f, ...)` while leaving `WallHISM->bCastHiddenShadow = true`.
- **`UMirrorLightBounceComponent`**: Attached on `ADungeoneerCharacter` and pre-allocating its secondary `USpotLightComponent` during `BeginPlay()` (never in `TickComponent`), computes $\mathbf{R} = \mathbf{I} - 2(\mathbf{I} \cdot \mathbf{N})\mathbf{N}$ whenever the flashlight ray strikes a `MirrorReflective` actor or HISM pane, steers the bounce beam down the reflected corridor, and reveals any downstream `Phantasm` illusion seams (`CustomData[0] = 1.0f`).
- **Breakable Noise Emitters**: Stepping onto intact `PROP_Breakables_And_Traps` instances (`EvaluateEntityStepOnBreakables`) transitions their HISM UV cell (`0/1` -> `2/3`) and pushes an acoustic `FDungeoneerNoiseEvent` into `UDungeoneerSensoryWorldSubsystem` so nearby enemies and the **Acute Hearing** sensory stencil ring react immediately.

### Verification (`v0.9.1`)
- Added `DarkDungeoneer.Dungeon.TileMapInstancerCutawayAndMirrorBounce` to `DungeoneerVerificationSpecs.cpp`, checking 4-axis HISM assembly, `bCastHiddenShadow` retention across cutaway toggles, specular $\mathbf{R}$ vector accuracy + Phantasm seam reveal, and breakable UV swap + `UDungeoneerSensoryWorldSubsystem` ripple emission.
- Updated `scripts/verify_architecture_invariants.py` (`85` tracked files + dungeon atlas/prop disk checks) and compiled both `DarkDungeoneerEditor Win64 Development` and standalone `DarkDungeoneer Win64 Development`, passing all `8` headless UE `5.8.2` automation tests (`EXIT CODE: 0`).

---

## Devlog #07 (`v0.9.2`) — Eliminating the Launch Black Screen & Enabling In-Place Catacombs Play

### Root-Cause Diagnosis from `Saved/Logs/DarkDungeoneer.log`
When launching `Launch-DarkDungeoneer.bat` (`-game -Windowed -ResX=1600 -ResY=900`), the window opened black due to five compounding issues:
1. **`OpenWorld` Default Map Streaming Stall**: `Config/DefaultEngine.ini` pointed `GameDefaultMap` and `EditorStartupMap` to `/Engine/Maps/Templates/OpenWorld`. On the development machine's `Intel(R) HD Graphics 520` (`i5-6300U`), UE 5.8 spent over 4 minutes initializing WorldPartition, waiting on `528` landscape/HLOD textures, and compiling `50` OpenWorld PSOs before bringing the viewport online. Switching both settings to `/Engine/Maps/Entry` eliminates WorldPartition streaming overhead completely.
2. **Null `StaticMesh` Subobjects on `AMenuDioramaStageActor`**: `ShowcaseFloorMesh`, `ExploderShowcaseProxy`, `FastThrowerShowcaseProxy`, `PhantasmMirrorShowcaseProxy`, and `DecoyProjectionProxy` were allocated without calling `SetStaticMesh(...)`, leaving zero geometry in front of `DioramaCamera` to catch `HeroFlashlightSpot`.
3. **Unclamped View Target & Unbuilt Floor 1 HISM**: `ADungeoneerGameMode::StartPlay()` neither spawned `ADungeoneerTileMapInstancer` nor disabled `bAutoManageActiveCameraTarget`, allowing pawn possession to override the diorama view target into empty dark space.
4. **Missing `.umap` Level Targets**: Clicking `"ENTER THE CATACOMBS"` in `UDungeoneerMainMenuWidget` invoked `ServerTravel` to `/Game/Maps/L_DungeonRun` (not yet authored on disk), failing/reloading into emptiness instead of starting in the procedural dungeon.
5. **No Default Character Visual Mesh or Direct Input Fallback**: `ADungeoneerCharacter` relied solely on unassigned Blueprint `UInputMappingContext` assets and had no default visual geometry or local lantern fill light.

### Fixes & Verification (`v0.9.2`)
- **Visible Diorama & HISM Dungeon Materials**: `AMenuDioramaStageActor::EnsureShowcaseMeshesInitialized()`, `ADungeoneerTileMapInstancer::EnsureMeshAssetsInitialized()`, and `ADungeoneerCharacter::EnsureDefaultVisualsInitialized()` assign `/Engine/BasicShapes/{Plane,Cylinder,Cube}` meshes plus distinct `UMaterialInstanceDynamic` instances derived from `/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial` (`Color` + `Roughness`), paired with `DioramaAmbientFillLight`, `HeroLanternFillLight`, and `[0.25f, 2.0f]` camera auto-exposure bounds so scenes render clearly across integrated GPUs.
- **In-Place Dungeon Transition & Zero-Allocation Controls**: `ADungeoneerGameMode::EnterLiveDungeonFromMenu()` collapses menu/lobby/footer widgets cleanly, hides the diorama stage, warps players into the selected procedural `ADungeoneerTileMapInstancer` room node, blends the camera smoothly onto `ADungeoneerCharacter`, and enables direct `WASD`/Arrow + Mouse flashlight aiming (`PollDirectKeyboardAndMouseFallback()`) when Enhanced Input DataAssets are absent.
- Verified with `scripts/verify_architecture_invariants.py` (`85` tracked files, zero `Tick` allocations, zero banned symbols), rebuilt both `DarkDungeoneerEditor` and standalone `DarkDungeoneer.exe`, and passed all `8` headless UE `5.8.2` automation tests (`EXIT CODE: 0`).

---

## Devlog #08 (`v0.9.3`) — Single-Cell Texture Bootstrapping, Doorway/Corridor Carving & Live Room Encounters

### Upgrading Flat Colored Shapes to Real 2.5D Pixel Art Without Shader-Compile Stalls
- On hardware like `Intel(R) HD Graphics 520`, compiling bespoke runtime HLSL material graphs at startup introduces multi-minute PSO hitches, whereas `/Engine/BasicShapes/BasicShapeMaterial` only exposes solid `Color`/`Roughness`.
- **Two-part C++20 solution in `UDungeoneerAssetBootstrapSubsystem`**:
  1. **Zero-Dependency Single-Cell Cropping (`ExtractAtlasCellTexture`)**: Using UE 5.8's built-in `IImageWrapperModule`, `UDungeoneerAssetBootstrapSubsystem::Initialize()` decodes all `16` dungeon sheets in `Content/Art/Dungeon/` and `18` rig sheets in `Content/Art/Rigs/`, then crops representative single `4x4` cells (`Floor_Tile0`, `Wall_Full0`, `Wall_Cutaway`, `Mirror_Intact`, `Breakable_Intact`, `Hero_Cell0`, `Exploder_Cell0`, `Thrower_Cell0`) into transient `PF_B8G8R8A8` textures configured to `TF_Nearest` + `TEXTUREGROUP_Pixels2D` (`TMGS_NoMipmaps`) into a dedicated `ExtractedCellTextures` cache (leaving the 18-rig sheet invariant intact).
  2. **Pre-Compiled Engine Shaders (`CreateDungeonOrRigMaterial`)**: HISM components (`FloorHISM`, `WallHISM`, `MirrorHISM`, `BreakableHISM`) bind `/Engine/EngineDebugMaterials/VertexPaint_2Tex_Color` (which sets `bUsedWithInstancedStaticMeshes = true` and exposes `Diffuse`, `Diffuse2`, `Normal`, `Normal2`, `DiffuseColor`, `Roughness`), while actor meshes (`ADungeoneerCharacter`, `AZombieBase`, `AMenuDioramaStageActor`) bind `/Engine/Engine_MI_Shaders/M_Shader_Simple`, falling back cleanly to `BasicShapeMaterial` if run in a bare test world without a `UGameInstance`. Because `OnConstruction` runs before `BeginPlay` (before `GameInstance` subsystems are reachable), actors track `bTexturedVisualsApplied` / `bTexturedHISMApplied` so `BeginPlay` upgrades fallback MIDs automatically.

### Topological Doorways, T-Junction-Safe Corridors & Lit Room Encounters
- **`ADungeoneerTileMapInstancer::BuildDungeonFromFloorGraph` World-Building Upgrade**:
  - Checking actual room bounds in `UDungeonGeneratorSubsystem` revealed that while `Entry -> CombatA/B` and `Vendor/RestingGround -> DeepFogGauntlet -> MajorLootVault` leave a `400 UU` open gap between rooms (`RoomPitchX = 2800`, `RoomExtents.X = 1200`), the wider `MirrorPalaceMaze` (`RoomExtents.X = 1600`) **flush-abuts** `CombatA/B` at `X = 4000` and `Vendor/RestingGround` at `X = 7200` (`1200 + 1600 = 2800`).
  - Pre-computing every room's `FBox2D` layout handles **both** cases uniformly: open-gap edges route a 3-segment stone corridor across `MidX` (`TSet<FIntPoint> CorridorGridCells` merges overlapping branches at T-junctions without duplicate tiles or internal blocker walls, and South corridor flanks register `EDungeoneerWallAxis::South` for automatic camera cutaway), while abutting edges align shared West/East doorways within the overlapping Y wall span (`DoorCarveRadius` suppresses perimeter walls and diagonal corner chamfers cleanly).
- **Live Room Encounters (`SpawnEncountersForFloorGraph`)**:
  - `AZombieBase` (`AZombieExploder`, `AZombieFastThrower`), `ARestingGroundActor`, `AVendorActor`, and `AMajorLootVaultActor` now instantiate visible 2.5D meshes and atmospheric `UPointLightComponent` beacons (magma orange, chrono cyan, sanctuary amber, merchant cyan, vault gold) and spawn deterministically per room node, tracked in `SpawnedEncounterActors` and cleaned up on rebuilds.
- Extended `DungeoneerVerificationSpecs.cpp` with assertions for all `16` dungeon atlases, `16` extracted single-cell tiles, `CreateDungeonOrRigMaterial`, non-zero connecting corridor tiles, Entry East doorway aperture clearance, and spawned room encounters (`>= 5`); all `8` headless UE `5.8.2` specs passed (`EXIT CODE: 0`).

---

## Devlog #09 — Camera-Relative 2.5D Sprite Facing & Integrated-GPU Dynamic Lighting Optimization (`v0.9.4`)

### 1. Fixing Character & Zombie Directional Facing
- **Root Cause Analysis**:
  1. `ExtractDefaultSingleCellTiles` previously cropped only `Cell 0` (Front view `-X`), leaving all actors stuck displaying the camera-facing sprite regardless of heading.
  2. `HeroVisualMesh` was a cylinder overwritten with `(-1, 1, 1)` scale by `DirectionalRigComponent`, and both hero movement/aim defaults started at `ECompassOctant::S` rather than `ECompassOctant::E` (`+X`, facing deeper into the dungeon from the `-60°` overhead `CameraBoom`).
  3. `ADungeoneerCharacter::PollDirectKeyboardAndMouseFallback` continuously aimed at `GetMousePosition`, so clicking Play on the left-hand menu or resting the laptop trackpad forced the hero to face backward (`-X` / `W`) even while running East with `WASD`.
- **Implementation**:
  - Added `DungeoneerMath::OctantToScreenDirectionalRow` (`W -> 0`, `E -> 1`, `N/NE/NW -> 2`, `S/SE/SW -> 3`) while preserving `OctantToCardinalRow` for the distance-driven animation contract.
  - Extended `ExtractDefaultSingleCellTiles` to crop all 4 directional rows (`Row0..Row3`, cells `0, 4, 8, 12`) for `Hero`, `Exploder`, and `Thrower` (`40` entries in `ExtractedCellTextures`), solved the `/Engine/BasicShapes/Plane.Plane` tangent/UV orientation (`ComputeSpritePlaneOrientation`, `GetCameraFacingSpritePlaneRotation`, `ShouldMirrorSpritePlaneU`, horizontally mirroring albedo pixels and tangent-X red normals when `bFlipHorizontalForSpritePlane` applies), and bound masked 2-sided materials (`M_ES_Phong_Masked_Master_2sided` / `M_Shader_SimpleMaskedTwoSided`).
  - Pre-allocated `4` directional `MID` instances on `ADungeoneerCharacter` and `AZombieBase` (`UpdateHeroDirectionalVisual`, `UpdateZombieDirectionalVisual`) with zero per-tick allocations, defaulted the hero to `ECompassOctant::E` (`CurrentFlashlightYawDeg = 0.0f`) and zombies to `ECompassOctant::W`, and gated mouse-cursor aiming behind a `> 4.0 px²` motion check with a `0.45s` active-mouse window so `WASD` movement cleanly drives facing when the cursor is idle.

### 2. Fixing ~3 FPS on Integrated Graphics (`Intel(R) HD Graphics 520`) While Keeping Dynamic Lighting
- **Profiling Diagnosis (`Saved/Logs/DarkDungeoneer.log`)**:
  - UE 5.8 defaulted to `Scalability@3` (High), enabling Lumen GI/reflections, Virtual Shadow Maps, DistanceField AO/Shadowing, Capsule Shadows, TSR 200% upscaling, full-res volumetric fog froxels, shadow-casting secondary point lights, and unculled HISM dungeon instances at `1600x900` on an `i5-6300U` / `Intel(R) HD Graphics 520`.
- **Rendering Budget Rebalance**:
  - Updated `Config/DefaultEngine.ini` and added runtime `ECVF_SetByCode` console variable enforcement in `ADungeoneerGameMode::ApplyLowSpecDynamicLightingCVars()` (active in game worlds outside commandlets) so cached scalability settings cannot override the low-spec budget.
  - Turned off Lumen GI/reflections, VSM (`r.Shadow.Virtual.Enable=0`, standard cascade/projection shadows at `r.ShadowQuality=2`, `r.Shadow.MaxResolution=1024`), DistanceField AO/Shadowing, Capsule Shadows, and TSR; switched anti-aliasing to FXAA (`r.AntiAliasingMethod=1`), scaled internal resolution (`r.ScreenPercentage=75`, `t.MaxFPS=60`), and lowered `Launch-DarkDungeoneer.bat` to `1280x720`.
  - Preserved the core atmospheric dynamic flashlight shadows and Volumetric Fog at 8x lighter froxel resolution (`GridPixelSize=16`, `GridSizeZ=64`, `HistoryMissSupersampleCount=1`), disabled shadows on non-hero secondary lights (`ARestingGroundActor::CampfireLight`, `AMenuDioramaStageActor::HeroFlashlightSpot`), and set `SetCullDistances(0, 3800)` on all `ADungeoneerTileMapInstancer` HISM components (`FloorHISM`, `WallHISM`, `MirrorHISM`, `BreakableHISM`).

### 3. Build & Verification (`v0.9.4`)
- Compiled `DarkDungeoneerEditor Win64 Development` (`15` actions, exit code `0`) and standalone `DarkDungeoneer Win64 Development` (`Binaries/Win64/DarkDungeoneer.exe`, `14` actions, exit code `0`) cleanly.
- Verified `python scripts/verify_architecture_invariants.py` (`ALL CHECKS PASSED: 85 core/subsystem/tool files verified`).
- Verified all `8` headless UE `5.8.2` automation tests passed (`EXIT CODE: 0`), including updated `40`-cell bootstrap checks, `OctantToScreenDirectionalRow`, all 12 `*_Row{0..3}_D` textures, plane normal orientation, and masked 2-sided Hero `MID` verification.

---

## Devlog #10 — Solving Invisible Masked Cutout Sprites via `OpacityMask` Synthesis (`v0.9.5`)

### Root Cause Diagnosis
- After `v0.9.4`, characters and zombies disappeared entirely because switching `CreateDungeonOrRigMaterial` to UE 5.8's built-in masked two-sided shaders (`M_ES_Phong_Masked_Master_2sided` and `M_Shader_SimpleMaskedTwoSided`) left their `OpacityMask` parameter unbound.
- Inspecting the `.uasset` expression wiring showed both shaders read `OpacityMask` from a dedicated `TextureSampleParameter2D` (`ParameterName = OpacityMask`, `Opacity Mask Channel` defaulting to Red) whose default asset is black `/Engine/Engine_MI_Shaders/T_Emissive` rather than `Diffuse.Alpha`, discarding every fragment against `OpacityMaskClipValue`.

### Implementation & Verification
- **Silhouette Mask Generation (`DungeoneerAssetBootstrapSubsystem`)**: Added `SpriteMaskTextures` (`UPROPERTY(Transient)`) and `GetSpriteMaskTexture(DiffuseKey)` so every extracted character diffuse cell (`bIsCharacterSprite && !bIsNormalMap`, honoring `bMirrorHorizontal` and `*_Cell0_D` aliases) produces a crisp nearest-filtered `TC_Masks` silhouette texture (`255` where `Alpha >= 64`, `0` otherwise).
- **Masked Shader Binding & Billboard Lighting**: Updated `CreateDungeonOrRigMaterial` to prefer `M_Shader_SimpleMaskedTwoSided` and bind `OpacityMask` alongside `Diffuse`/`Normal`, tilted `ComputeSpritePlaneOrientation` to a `45°` billboard (`WorldUp = (0.7071, 0, 0.7071)`, `WorldNormal = (-0.7071, 0, 0.7071)`), set `SetCastShadow(false)` on `HeroVisualMesh` and `ZombieVisualMesh` to eliminate self-shadowing against local lights, and shifted `HeroLanternFillLight` (`(-65, 0, 110)`) and `ZombieEmissiveLight` (`(-45, 0, 65)`) in front of the billboards.
- **Verification**: Expanded `DungeoneerVerificationSpecs.cpp` (`HUDAndTradeModalAndAssetBootstrap`) to verify cached `Hero_Cell0_D`/`Hero_Row1_D` masks and exact `OpacityMask` binding (`K2_GetTextureParameterValue`); rebuilt `DarkDungeoneerEditor` (`10` actions) and `DarkDungeoneer` (`9` actions) with exit code `0`, passed `verify_architecture_invariants.py` (`85` files), and passed all `8` headless UE `5.8.2` specs (`EXIT CODE: 0`).

---

## Devlog #11 — True Alternating 4-Frame Walk Cycles & 16-Cell Distance-Locked Flipbooks (`v0.9.6`)

### Diagnosis: Why 4x4 Rows Looked Static in Playback
- Visual inspection of all three `_Raw_D.png` sprite sheets confirmed two compounding issues:
  1. **Static Sprite Sheet Poses**: On `Rig_DungeoneerHero_Raw_D.png` and `Rig_ZombieExploder_Raw_D.png`, columns `0..3` of side rows repeated near-identical scissored poses without upright passing frames or swapped front/back leg dominance (`16–34%` per-cell pixel diff). Meanwhile `Rig_ZombieFastThrower_Raw_D.png` lacked South/North rows completely (repeating East/West across all 4 rows) and baked a purple circular halo around the raised flask in Column 2.
  2. **Single-Column Runtime Extraction**: `UDungeoneerAssetBootstrapSubsystem::ExtractDefaultSingleCellTiles()` only sliced Column `0` (`Row0..Row3`) into `4` MIDs per actor, so runtime actors never stepped across walk columns regardless of distance traveled.

### Sprite Sheet Rebuild & Runtime 16-Cell Flipbook Pipeline
- **Articulated 4-Frame Gaits (`slice_normal_rig.py` Re-Slice)**:
  - Rebuilt `Rig_DungeoneerHero_Raw_D.png`, `Rig_ZombieExploder_Raw_D.png`, and `Rig_ZombieFastThrower_Raw_D.png` (`4x4` `1024x1024` grids) with distinct `Contact-L -> Upright Passing A -> Contact-R -> Upright Passing B` strides, overlapping limb seams behind torsos/bracers, counter-phase lantern/arm/shackle swing, hip bob, and authentic South/North hunched sprint frames on `ZombieFastThrower` without flask halos (raising per-column foreground diffs to `38–84%`), then regenerated `_N.png`, `_E.png`, `_ORM.png`, `AtlasMetadata.json`, `rig_sockets.json`, and `Content/AssetManifest.json` SHA-256 hashes.
- **Zero-Allocation 16-MID Runtime Flipbook**:
  - Expanded `UDungeoneerAssetBootstrapSubsystem::ExtractDefaultSingleCellTiles` to cache all `16` walk cells (`{Prefix}_R{0..3}_C{0..3}_{D,N}`) plus corresponding `SpriteMaskTextures` and backward-compatible `*_Row*`/`*_Cell0*` aliases (`136` entries total).
  - Updated `ADungeoneerCharacter` and `AZombieBase` to pre-allocate all `16` directional walk MIDs (`Row * 4 + Col`) at initialization and switch cleanly via `AdvanceWalkFlipbookByDistance` (`WalkCycleStrideUU = 140 UU`, snapping to neutral upright `Col 1` when idle and keeping zombie `OnZombieStepTriggered` footsteps synced with contact columns `0` and `2` under Chrono-Leech speed changes).
- **Verification**: Added `136`-cell and distance-step walk-column checks in `DungeoneerVerificationSpecs.cpp`; `python scripts/verify_architecture_invariants.py` passed (`85` files), `DarkDungeoneerEditor` (`8` actions) and `DarkDungeoneer` (`7` actions) compiled cleanly, and all `8` headless UE `5.8.2` specs passed (`UE_TEST_EXIT=0`).

---

## Devlog #12 — Standardized 16-Cell Kinematic Spec, Stride Validator & `U25DAnimOrchestratorComponent` Footstep Sync (`v0.9.7`)

### Standardizing the 4x4 Column Contract Across Pipeline & Runtime
- **Problem**: Having an implicit column order made newly generated AI sprite sheets prone to leg-duplication artifacts (sliding on one leg) and created drift between `U25DAnimOrchestratorComponent`, character actors, and slicing tools.
- **Specification (`SKILL.md`, `TO_GENERATE.md`)**: Locked every `4x4` walking sheet to `Row 0: South/SE`, `Row 1: North/SW`, `Row 2: East/NE`, `Row 3: West/NW` and `Col 0: Contact / Idle Pose A (peak height +1..+2 px)`, `Col 1: Full Left Step (dip -1..-2 px)`, `Col 2: Passing Pose B (peak +1..+2 px)`, `Col 3: Full Right Step (dip -1..-2 px)`.
- **Automated QA & Repair (`slice_normal_rig.py`)**: Implemented `--validate-stride` and `--repair-stride` comparing the lower-third foreground leg region (`IoU`, `MSE`, and combined pixel variance) between `Col 1` and `Col 3`. Rows below `15%` variance emit `[WARN]` and, with `--repair-stride`, synthesize `[Col 0, Col 1, Col 2, Col 3]` cleanly via lower-body depth shear and mirrored leg-split warping while keeping asymmetrical upper-body gear intact. Re-sliced sheets verified `22.4%–84.5%` lower-body variance across all rows.
- **C++20 Runtime (`U25DAnimOrchestratorComponent`, `ADungeoneerCharacter`, `AZombieBase`)**: Bound `Velocity.IsNearlyZero()` to lock UV selection to `Column 0`, computed walk columns via `FMath::FloorToInt((AccumulatedDistance / StrideLengthUU) * 4.0f) & 3`, and added `OnFootstepContact(EFoot::Left / EFoot::Right)` + `F25DStepContactEvent::Foot` firing on **Column 1** (`Left`) and **Column 3** (`Right`) alongside `UDungeoneerSensoryWorldSubsystem::BroadcastNoiseEvent` for fog ripples.
- **Verification**: `slice_normal_rig.py --self-test` passed all Rig/Stride-Repair/Dungeon checks, `verify_architecture_invariants.py` exited `0`, `DarkDungeoneerEditor` (`8` actions) + `DarkDungeoneer.exe` (`7` actions) compiled cleanly, and all `8` UE `5.8.2` automation tests passed (`UE_TEST_EXIT=0`).
