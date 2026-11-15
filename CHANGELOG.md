# Changelog

All notable changes to **Dark Dungeoneer (UE 5.8)** are documented in this file.
The project adheres to [Semantic Versioning 2.0.0](https://semver.org/) and [Conventional Commits](https://www.conventionalcommits.org/).

---

## [Unreleased]

- **v1.0.0 (Planned)**: Full multi-floor vertical slice (`Entry -> Combat -> Mirror-Palace -> Resting Ground -> Deep Fog -> Loot Vault`).

---

## [0.9.3] - 2026-11-15

### Added
- **Dungeon & Rig Single-Cell Texture Bootstrapper (`TASK-907` — `UDungeoneerAssetBootstrapSubsystem`)**:
  - Extended `UDungeoneerAssetBootstrapSubsystem` with `ImportAllDungeonTexturesFromDisk()` loading all `16` generated dungeon sheets (`ATLAS_Dungeon_Floors_4x4`, `PROP_Dungeon_Walls_And_Cutaways_4x4`, `PROP_Mirror_Palace_Maze_4x4`, `PROP_Breakables_And_Traps_4x4`) configured with `TF_Nearest` + `TEXTUREGROUP_Pixels2D` (`TMGS_NoMipmaps`).
  - Added `IImageWrapper`-backed `ExtractAtlasCellTexture()` and `ExtractDefaultSingleCellTiles()` cropping `16` clean single-cell `UTexture2D` tiles (`Floor_Tile0`, `Wall_Full0`, `Wall_Cutaway`, `Mirror_Intact`, `Breakable_Intact`, `Hero_Cell0`, `Exploder_Cell0`, `Thrower_Cell0`) into transient `PF_B8G8R8A8` pixel-art textures without modifying the `18`-rig cache invariant.
  - Added `CreateDungeonOrRigMaterial()` binding pre-compiled Engine shaders (`/Engine/EngineDebugMaterials/VertexPaint_2Tex_Color` for HISM batches and `/Engine/Engine_MI_Shaders/M_Shader_Simple` for actor/diorama meshes) with fallback to `/Engine/BasicShapes/BasicShapeMaterial`, and wired auto-upgrade across `ADungeoneerTileMapInstancer`, `ADungeoneerCharacter`, `AMenuDioramaStageActor`, and `AZombieBase`.
- **Doorway Carving, 4-Axis Connecting Corridors & Live Room Encounters (`TASK-908` — `ADungeoneerTileMapInstancer`, `AZombieBase`, `DungeonEncounterActors`)**:
  - Updated `ADungeoneerTileMapInstancer::BuildDungeonFromFloorGraph()` to pre-compute room bounding rectangles, carve walkable East/West doorway apertures (plus corner-chamfer clearance) across connected DAG edges (handling both open void gaps and flush-abutting rooms such as `CombatA/B <-> MirrorPalaceMaze <-> Vendor/RestingGround`), and generate deduplicated `TSet<FIntPoint>` 3-tile-wide stone corridors (`FloorHISM`) flanked by cutaway-aware `WallHISM` corridor walls.
  - Added visible `UStaticMeshComponent` visuals and emissive/beacon `UPointLightComponent`s to `AZombieBase` (`AZombieExploder` magma orange + `CustomData[2]` belly pulse; `AZombieFastThrower` chrono cyan), `ARestingGroundActor`, `AVendorActor`, and `AMajorLootVaultActor`.
  - Added `ADungeoneerTileMapInstancer::SpawnEncountersForFloorGraph()` (`bSpawnRoomEncounters = true`) spawning and tracking per-room encounter actors (`SpawnedEncounterActors`, cleaned on rebuild in `ClearAllInstances()`).

### Verified
- Extended `DarkDungeoneer.UI.HUDAndTradeModalAndAssetBootstrap` and `DarkDungeoneer.Dungeon.TileMapInstancerCutawayAndMirrorBounce` in `DungeoneerVerificationSpecs.cpp` to assert all `16` dungeon textures, `16` extracted single-cell tiles, `CreateDungeonOrRigMaterial` HISM output, non-zero connecting corridor tiles, carved Entry East doorway clearance, and spawned room encounters (`>= 5`).
- Passed `scripts/verify_architecture_invariants.py` (`85` tracked files, zero `Tick` allocations, zero banned symbols).
- Compiled `DarkDungeoneerEditor Win64 Development` and `DarkDungeoneer Win64 Development` (`Binaries/Win64/DarkDungeoneer.exe`) and passed all `8` headless UE `5.8.2` automation tests (`EXIT CODE: 0`).

---

## [0.9.2] - 2026-11-15

### Fixed
- **Startup Black Screen on Launch (`DefaultEngine.ini`, `AMenuDioramaStageActor`, `ADungeoneerTileMapInstancer`)**:
  - Switched `GameDefaultMap` and `EditorStartupMap` in `Config/DefaultEngine.ini` from `/Engine/Maps/Templates/OpenWorld` to `/Engine/Maps/Entry`, eliminating the 4+ minute WorldPartition / 528-texture streaming stall and 50 PSO hitches observed on integrated GPUs (`Intel HD Graphics 520`).
  - Populated `AMenuDioramaStageActor::EnsureShowcaseMeshesInitialized()` (invoked in `OnConstruction` and `BeginPlay`) with `/Engine/BasicShapes` meshes (`Plane`, `Cylinder`, `Cube`) and `BasicShapeMaterial` colored MIDs (`Color` + `Roughness`) across `ShowcaseFloorMesh`, `ExploderShowcaseProxy`, `FastThrowerShowcaseProxy`, `PhantasmMirrorShowcaseProxy`, and `DecoyProjectionProxy`, paired with `DioramaAmbientFillLight` (`4500.0f`), boosted `HeroFlashlightSpot` (`24000.0f`), and auto-exposure bounds (`[0.25f, 2.0f]`).
  - Added default colored `BasicShapeMaterial` MIDs for `FloorHISM`, `WallHISM`, `MirrorHISM`, and `BreakableHISM` inside `ADungeoneerTileMapInstancer::EnsureMeshAssetsInitialized()`.
  - Disabled `OwningPC->bAutoManageActiveCameraTarget` inside `ADungeoneerGameMode::StartPlay()` and spawned `ADungeoneerTileMapInstancer` + `AMenuDioramaStageActor` inside the Floor 1 Entry room so possession never clobbers the Main Menu diorama camera into unlit void.
- **In-Place Dungeon Transition & Playable C++ Input Fallback (`ADungeoneerGameMode`, `UDungeoneerMainMenuWidget`, `ADungeoneerCharacter`)**:
  - Added `ADungeoneerGameMode::EnterLiveDungeonFromMenu(EDungeoneerMenuAction)` and updated `UDungeoneerMainMenuWidget::ActivateSpecificAction` to transition players seamlessly into the procedural `ADungeoneerTileMapInstancer` rooms (`Entry`, `Combat`, `MirrorPalaceMaze`, `RestingGround`/`Vendor`) when `.umap` level assets do not yet exist on disk.
  - Added default `HeroVisualMesh` (`Cylinder`), `HeroLanternFillLight` (`4800.0f`), boosted `FlashlightComponent` (`26000.0f`), camera exposure bounds on `ADungeoneerCharacter`, and a zero-allocation `PollDirectKeyboardAndMouseFallback()` loop (WASD/Arrow movement + mouse viewport 8-compass flashlight aiming when `DefaultMappingContext` is unassigned).

### Verified
- Extended `DarkDungeoneer.UI.MenuNavigationAndFloorSeedDeterminism` in `DungeoneerVerificationSpecs.cpp` to assert non-null `AMenuDioramaStageActor` showcase meshes and `ADungeoneerGameMode::EnterLiveDungeonFromMenu` HISM room construction.
- Passed `scripts/verify_architecture_invariants.py` (`85` tracked files verified, zero `Tick` allocations, zero banned symbols).
- Compiled both `DarkDungeoneerEditor Win64 Development` and `DarkDungeoneer Win64 Development` (`Binaries/Win64/DarkDungeoneer.exe`) with zero project warnings/errors, passing all `8` headless UE `5.8.2` automation tests (`EXIT CODE: 0`).

---

## [0.9.1] - 2026-11-15

### Added
- **Dungeon Mode Atlas & Mirror Portal Mask Extractor (`slice_normal_rig.py --mode dungeon`)**:
  - Extended `.agents/skills/sprite-sheet-25d-cutter/scripts/slice_normal_rig.py` with `--mode dungeon`, `--dungeon-sheet-type`, and `--synthesize-dungeon {atlas_floors,prop_walls,prop_mirrors,prop_breakables,all}`.
  - Seamless `ATLAS_*` exports (`_D.png` opaque RGBA, deep-relief `_N.png`, 8-bit POM heightmap `_H.png`, and wetness/puddle roughness mask `_R.png` encoding `0.05` specular puddles at byte `13`) plus 16-cell `AtlasMetadata.json`.
  - Cutout `PROP_*` exports stripping `#FF00FF` magenta to alpha `0` (`_D.png`) while extracting `#00FF00` interior mirror panes into the Red channel of `_ORM.png` (`MirrorPortalMask = 255`) and replacing raw green glass on Diffuse with dark mirror slate `(18, 24, 32, 255)`.
  - Generated all 4 dungeon sprite sheets (`ATLAS_Dungeon_Floors_4x4`, `PROP_Dungeon_Walls_And_Cutaways_4x4`, `PROP_Mirror_Palace_Maze_4x4`, `PROP_Breakables_And_Traps_4x4`) under `Content/Art/Dungeon/` and synced SHA-256 checksums in `Content/AssetManifest.json`.
- **Zero-Draw-Call Modular HISM Builder & Per-Viewport Cutaways (`ADungeoneerTileMapInstancer`)**:
  - Instantiates procedural `UDungeonGeneratorSubsystem` floor graphs into 4 batched `UHierarchicalInstancedStaticMeshComponent`s (`FloorHISM`, `WallHISM`, `MirrorHISM` tagged `MirrorReflective`, `BreakableHISM`) across all 4 axes (orthogonal N/S/E/W + 45-degree diagonal NE/NW/SE/SW corner chamfers).
  - Performs zero-allocation per-viewport sphere sweeps (`ULocalPlayer` viewports) and deterministic segment-vs-box checks in `Tick` to toggle occluded South/SE/SW foreground wall instances to Row 2 knee-high cutaway stubs (`CustomData[0] = 1.0f`) while preserving `bCastHiddenShadow = true`.
  - Evaluates steps onto `PROP_Breakables_And_Traps` instances (`EvaluateEntityStepOnBreakables`), swapping `CustomData[0..1]` to shattered UV cells and broadcasting `FDungeoneerNoiseEvent` ripples into `UDungeoneerSensoryWorldSubsystem` for the Acute Hearing sensory layer.
- **Deterministic Mirror-Palace Light Bounce (`UMirrorLightBounceComponent`)**:
  - Added `UMirrorLightBounceComponent` (also attached as default subobject on `ADungeoneerCharacter`) pre-allocating a secondary `USpotLightComponent` outside `TickComponent`.
  - Traces along the flashlight axis against `MirrorReflective` actors/HISM instances, applies $\mathbf{R} = \mathbf{I} - 2(\mathbf{I}\cdot\mathbf{N})\mathbf{N}$ reflection math, positions the bounce spotlight down the reflected corridor, and reveals struck `Phantasm` mirror seams (`CustomData[0] = 1.0f`).

### Verified
- Extended `--self-test` in `slice_normal_rig.py` to cover both `rig` and `dungeon` modes.
- Added `DarkDungeoneer.Dungeon.TileMapInstancerCutawayAndMirrorBounce` automation spec and updated `scripts/verify_architecture_invariants.py` (`85` tracked files).
- Built `DarkDungeoneerEditor Win64 Development` and `DarkDungeoneer Win64 Development`, passing all `8` headless UE `5.8.2` automation tests (`EXIT CODE: 0`).

---

## [0.9.0] - 2026-11-15

### Added
- **2.5D Hybrid Sprite Sheet Slicer & Emissive Extractor (`slice_normal_rig.py`)**:
  - Upgraded `.agents/skills/sprite-sheet-25d-cutter/scripts/slice_normal_rig.py` to strip `#FF00FF` magenta backgrounds cleanly to `(0, 0, 0, 0)` RGBA with vectorized zero-halo neighbor cleanup (`_D.png`).
  - Replaced plain Sobel luminance normals with a pure-NumPy Chamfer Euclidean distance-transform dome bevel combined with a 3x3 Sobel luminance gradient (`_N.png` RGB).
  - Added high-luminance saturated emissive extraction (`_E.png` RGB) isolating `#FFCC00` Player lantern glow, `#FF5500` Exploder magma belly, and `#00FFFF` Fast Thrower alchemical flasks.
  - Emitted `AtlasMetadata.json` containing normalized `(U0, V0, U1, V1)` sub-rectangles and bottom-center foot pivots for all 16 cells, regenerated all 6 rig packages under `Content/Art/Rigs/`, and updated `Content/AssetManifest.json` SHA-256 checksums.
- **Zero-Allocation 2.5D Runtime Animator (`U25DAnimOrchestratorComponent`)**:
  - Added distance-driven walk phase (`AccumulatedDistance / StrideLength`) preventing foot-sliding across Slo-Mo Chrono Gas speed transitions.
  - Fires deterministic `OnStepContact` events on transitions to contact frames `0` and `2` with floor `UPhysicalMaterial` (`PhysicsCore`) resolution and `UDungeoneerSensoryWorldSubsystem::BroadcastNoiseEvent` acoustic ripples.
  - Added 120Hz sub-stepped procedural springs for vertical stride hop, directional velocity lean, and pre-explosion belly-swell scaling.
  - Drives per-instance Custom Primitive Data (`CustomData[0]` UV cell index `0..15`, `CustomData[1]` Emissive Fuse Pulse, `CustomData[2..4]` Alchemical Flask Element RGB tint) directly on the cutout plane without creating per-actor MIDs, plus hidden volumetric shadow proxy capsule setup.
- **Split-Screen Player HUD, 2PC Trade Modal & Idempotent `.uasset` Bootstrap (`TASK-901` – `TASK-903`)**:
  - Added `UDungeoneerPlayerHUDWidget` attached once per-`ULocalPlayer` viewport (`ZOrder 5`) in `ADungeoneerCharacter::BeginPlay` displaying health, battery, live local compass move/aim octants, Chrono-Leech `S` gauge, and sensory perk tiers.
  - Added `UDungeoneerTradeModalWidget` binding `AVendorActor`, `UGibberishSpeechComponent`, `UTradeManagerComponent`, and `UDungeoneerInventoryComponent` with `OnTradeAckReceived` status feedback and catalog-selection gibberish chatter.
  - Added `UDungeoneerAssetBootstrapSubsystem` to import all 18 rig textures (`TF_Nearest`, `TEXTUREGROUP_Pixels2D`) and 50 WAV voice/phoneme stems, synthesize `MPC_SensoryState`, and serialize idempotent `.uasset` packages (`Content/Materials/MPC_SensoryState.uasset`, `Content/Materials/DA_25DMaterialConfig.uasset`, `Content/Core/DA_MovementConfig.uasset`).

### Verified
- Added `DarkDungeoneer.UI.HUDAndTradeModalAndAssetBootstrap` and `DarkDungeoneer.Rendering.25DAnimOrchestratorAndAtlasMetadata` to `DungeoneerVerificationSpecs.cpp`.
- Passed `scripts/verify_architecture_invariants.py` (`81` files verified), compiled both `DarkDungeoneerEditor Win64 Development` and `DarkDungeoneer Win64 Development`, and passed all `7` headless UE `5.8.2` automation specs (`EXIT CODE: 0`).

---

## [0.8.1] - 2026-11-15

### Fixed & Added
- Compiled standalone monolithic Win64 game executable `Binaries/Win64/DarkDungeoneer.exe` (`DarkDungeoneer Win64 Development`) alongside `DarkDungeoneerEditor Win64 Development` (`UnrealEditor-DarkDungeoneer.dll`).
- Guarded `UTexture2D::MipGenSettings` in `Dungeoneer25DMaterialBuilder.cpp` with `#if WITH_EDITORONLY_DATA` so `TargetType.Game` compiles with zero errors.
- Added programmatic C++ `RebuildWidget()` UMG trees (`UCanvasPanel`, `UBorder`, `UVerticalBox`, `UButton`, `UTextBlock`) to `UDungeoneerMainMenuWidget`, `UDungeoneerCoopLobbyWidget`, and `UDungeoneerMenuFooterWidget`, and wired `ADungeoneerGameMode::StartPlay()` to spawn `AMenuDioramaStageActor` and attach the menu widgets to the owning `ULocalPlayer` viewport (`ZOrder 10–12`).
- Added root `Launch-DarkDungeoneer.bat` to launch cooked `Binaries\Win64\DarkDungeoneer.exe` or fall back cleanly to `UnrealEditor.exe ... -game` before `.uasset` cooking.

---

## [0.8.0] - 2026-11-15

### Added
- **Main Menu & Co-Op Lobby UI (`TASK-801`–`TASK-804`)**:
  - `UDungeoneerMainMenuWidget`: 8-entry C++ navigation stack (`EnterCatacombs`, `ContinueExpedition`, `CoopSplitScreen`, `SensoryLoadout`, `MirrorTrial`, `RestingGround`, `Options`, `Quit`) with optional UMG `BindWidgetOptional` hooks and server/client map travel dispatch.
  - `AMenuDioramaStageActor`: Live `-60°` 2.5D diorama driving narrow-beam `USpotLightComponent` octant sweeps, Exploder CPD fuse pulse, Chrono-Gas haze toggle, Phantasm Mirror (`Stencil 104`), Decoy projection (`Stencil 106`), and deterministic phase-accumulated sonar pulses (`Stencil 105`).
  - `UDungeoneerCoopLobbyWidget`: 4-slot co-op squad card polling `ULocalPlayer`, `UDungeoneerGameViewportClient`, `UDungeoneerLocalPlayerSubsystem` sensory tiers, and `UDungeoneerSocialSubsystem` friends/parties (zero `GetFirstPlayerController()`).
  - `UDungeoneerMenuFooterWidget`: Live `0x%08X` replicated floor seed readout, deterministic `[R]` reroll, octant scalar speed telemetry, and Chrono-Leech conservation readout.
- **Prediction, Shadow Proxy, Portal Budget & Fog Net Relevancy (Rules `05` & `06`)**:
  - `FSavedMove_Dungeoneer` & `FNetworkPredictionData_Client_Dungeoneer` in `UDungeoneerMovementComponent`: `uint8` quantized `ChronoSlowRatio` client prediction & replay across Slo-Mo Gas boundaries.
  - `U25DDirectionalRigComponent::ConfigureVolumetricShadowProxy`: Enforces `bRenderInMainPass = false`, `bCastHiddenShadow = true`, `CastShadow = true` on 2.5D shadow proxy primitives.
  - `UMirrorCaptureBudgetSubsystem`: Round-robin `CaptureSceneDeferred()` mirror portal updates capped at `<= 2` portals/frame and scaled inversely by active local split-screen viewport count.
  - `ADungeoneerGameState` (`FDungeonFloorSeed`): Replicated LCG floor seed driving client-local `UDungeonGeneratorSubsystem` synthesis, wired into `ADungeoneerGameMode`.
  - `AZombieBase::IsNetRelevantFor` & `SetNetUpdateFrequency`: Culls fog-occluded zombies outside flashlight cones, `DarkVisionRadius`, and `HearingPulseRadius`, enforces non-replicated mirror decoys, and throttles occluded net frequency (`2.0f` vs `15.0f`).
- **Verification**:
  - Added `DarkDungeoneer.Social.TradeTwoPhaseCommitRollbackAndCommit` (`TradeTwoPhaseCommitTest`) and `DarkDungeoneer.UI.MenuNavigationAndFloorSeedDeterminism` in `DungeoneerVerificationSpecs.cpp`.
  - Expanded `scripts/verify_architecture_invariants.py` (`73` verified files, zero `Tick` allocations, zero banned symbols).
  - Compiled `DarkDungeoneerEditor (Win64 Development)` on UE `5.8.2` (`V7`) and passed all 5 headless `UnrealEditor-Cmd.exe` specs (`EXIT CODE: 0`).

---

## [0.7.1] - 2026-11-15

### Added
- `docs/MENU_MOCKUP.md` and `docs/assets/dark_dungeoneer_menu_concept.png` (`1920x1080` visual menu mockup composited from normal-lit Pro character sprites, flashlight cone lighting, Chrono-Gas mist, Phantasm Mirror stencil wall, and wrought-iron/brass UI framing) alongside the interactive HTML prototype (`docs/assets/dark_dungeoneer_menu_mockup.html`).
- `docs/TASKS.md`: Structured milestone breakdown (`v0.1.0`–`v1.0.0`) turning menu mockup and remaining vertical-slice modules into concrete engineering tickets (`TASK-801` through `TASK-1001`).
- `docs/DEVLOG.md`: Engineering log covering architecture choices, UE 5.8 compilation fixes, and UI direction.
- `VERSION`, `.gitignore`, and semantic commit/tag history (`v0.1.0` through `v0.7.1`).

---

## [0.7.0] - 2026-11-15

### Added
- `UDungeoneerSocialSubsystem`: `OnlineSubsystem` co-op party sessions reserving local split-screen + online slots, friend presence caching, and per-`ULocalPlayer` chat and voice controls (`Interfaces/VoiceInterface.h`, `Interfaces/OnlinePresenceInterface.h`).
- `UDungeoneerInventoryComponent` & `UTradeManagerComponent`: Server-authoritative Two-Phase Commit (`FTradeTransaction`: Lock Item GUIDs & Gold -> Validate -> Atomic Swap -> Client Ack) preventing duplication on packet drops or full inventories.
- `Source/DarkDungeoneer/Private/Tests/DungeoneerVerificationSpecs.cpp`: Headless UE Automation Specs (`DarkDungeoneer.Locomotion.DiagonalEqualsCardinalSpeed`, `DarkDungeoneer.AI.ChronoLeechConservationLaw`, `DarkDungeoneer.Audio.GibberishPhonemeDeterminism`).
- `scripts/verify_architecture_invariants.py`: Static invariant audit for hot-path zero allocations, banned APIs, math conservation, and `Content/AssetManifest.json` SHA-256 verification.

### Verified
- Compiled and linked `DarkDungeoneerEditor (Win64 Development)` on Unreal Engine `5.8.2` (`BuildSettingsVersion.V7`, `UnrealEditor-DarkDungeoneer.dll`).
- Passed all 3 headless `UnrealEditor-Cmd.exe` automation tests with `EXIT CODE: 0`.

---

## [0.6.0] - 2026-11-15

### Added
- `UDungeoneerTimedAudioComponent` & `UAnimNotify_DungeoneerTimedCue`: Deterministic distance-accumulated step-cycle and AnimNotify audio triggers (zero `FTimerHandle` loops).
- `UGibberishSpeechComponent`: Hero offline voice stem player and `MS_GibberishSynth` `CharCode % 24` phoneme mapper with `+3` / `-2` semitone `?` / `.` inflection.
- `.agents/skills/voice-gibberish-metasound/scripts/generate_phoneme_bank.py` + generated 24 micro-syllable `.wav` grains and hero greeting stems for `vendor_merchant` and `dungeon_lorekeeper`, registered in `Content/AssetManifest.json`.

---

## [0.5.0] - 2026-11-15

### Added
- `UDungeonGeneratorSubsystem` (`DungeonTypes.h`): Server-seeded deterministic `FRandomStream` floor DAG (`Entry -> Combat -> MirrorPalaceMaze -> Vendor/RestingGround -> DeepFogGauntlet -> MajorLootVault`).
- `AMirrorWall_Solid` (`CustomDepthStencil = 101`, flashlight bounce raycast), `AMirrorWall_Phantasm` (`CustomDepthStencil = 104`, half-res `USceneCaptureComponent2D`, pass-through collision, roughness divergence, phase-inverted ping), and `AMirrorDecoyNode` (`CustomDepthStencil = 106` zero-damage clones).
- `ARestingGroundActor`, `AVendorActor`, and `AMajorLootVaultActor`.

---

## [0.4.0] - 2026-11-15

### Added
- `UZombieSpatialSubsystem`: Pre-allocated 1024-cell (`32x32`) fixed spatial hash in `TUniquePtr<FSpatialGridBuffer>` plus pooled hazard rings (16 per hazard type, 32 scorch decals).
- `AZombieBase`, `AZombieExploder` (slow hunter, deterministic `IgnitionRadius` fuse with CPD belly pulse & pitch ticks), and `AZombieFastThrower` (`Fire`, `Poison`, `SloMoGas` throws + Chrono-Leech speed conservation).
- `AHazardZone_Fire`, `AHazardZone_Poison`, and `AHazardZone_ChronoGas` enforcing `V_player = V_base * (1 - S)` and `V_zombie = V_zombie_base * (1 + S * SpeedStealScalar)`.

---

## [0.3.0] - 2026-11-15

### Added
- `Shaders/Private/PixelLit25D.ush` & `UDungeoneer25DMaterialBuilder`: Master `M_25D_PixelLit` pipeline (texel-center UV snapping at `64 px / 100 UU`, Scharr tangent normal lighting, 4x4 Bayer dithered alpha).
- `U25DDirectionalRigComponent` (multi-plane sockets driven via Custom Primitive Data), `UDungeoneerSensoryWorldSubsystem` (32-noise / 128-scent ring buffers), `USensoryPerkComponent`, and `ADungeonAtmosphereActor`.
- `.agents/skills/sprite-sheet-25d-cutter/scripts/slice_normal_rig.py` + Pro-generated and sliced `_D.png`, `_N.png`, `_ORM.png` character atlases (`DungeoneerHero`, `ZombieExploder`, `ZombieFastThrower`).

---

## [0.2.0] - 2026-11-15

### Added
- `ADungeoneerCharacter`, `UDungeoneerMovementComponent`, `UDungeoneerMovementConfig`, `ADungeoneerGameMode`, and `DungeoneerTypes.h`: 4-axis (`8-compass`) movement quantization with `INV_SQRT_2` normalized diagonal velocity, decoupled flashlight aim octants, and deterministic step accumulator.
- `UDungeoneerGameViewportClient` & `UDungeoneerLocalPlayerSubsystem` (`FDungeoneerSensoryViewExtension`): 2–4 player local split-screen + online parity with per-`ULocalPlayer` post-process isolation.

---

## [0.1.0] - 2026-11-15

### Added
- UE 5.8 C++20 project scaffold (`DarkDungeoneer.uproject`, `DarkDungeoneer.Target.cs`, `DarkDungeoneerEditor.Target.cs`, `DarkDungeoneer.Build.cs`, `DarkDungeoneerModule.h/.cpp`).
- Engine, Game, and Enhanced Input configurations (`Config/DefaultEngine.ini`, `DefaultGame.ini`, `DefaultInput.ini`).
- Governance rules and domain skills (`AGENTS.md`, `.agents/rules/`, `.agents/skills/`).
