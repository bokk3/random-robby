# Dark Dungeoneer — Development Task Tracker

- **Current Version**: `1.2.1` (tracked in [`VERSION`](../VERSION))
- **Versioning Policy**: Semantic Versioning (`MAJOR.MINOR.PATCH`) paired with atomic Conventional Commits (`feat(...)`, `fix(...)`, `docs(...)`, `test(...)`, `chore(...)`).

---

## Completed Milestones (`v0.1.0` – `v1.2.1`)

| SemVer Tag | Milestone | Status | Key Modules & Verification |
| :--- | :--- | :--- | :--- |
| `v1.2.1` | 2.5D Visual Overhaul, Exposure/HISM Fix & M4 Release Integration (`TASK-1202`) | Done | Pixel-art Dungeon Floors/Walls/Doors atlases, `eco_cleanup_masks.json` de-haloing, `-45°` isometric diorama camera (`Pitch=-42°`, `FOV=35°`), `ELightUnits::Lumens` calibration (`DungeoneerCharacter` & `MenuDioramaStageActor`), `M_Dungeoneer_EcoInstanced` `EcoMatRev6`, HISM `BuildTreeIfOutdated`/`UpdateBounds`, `-CaptureShotAfterSec` hook, `16` UE 5.8.2 specs (`EXIT CODE: 0`), verified Entry Room & Main Menu captures |
| `v1.2.0` | Living Dungeon Ecosystem — Lighting, Hydrology, Flora, Fauna & Acoustics (`TASK-1201`) | Done | `FDungeoneerEcoMaterialFactory` (`EEcoCPD::Count=8`, 32-bit `UnpackRGB24`), `UDungeonLightBudgetSubsystem` (`512` candidates / `32` Tier 1 / `16` Tier 2 / `2` Tier 3), `UDungeonWaterSubsystem` (`128 UU` O(1) surface grid, `0.88x` wading, `24` drips, `256`-slot `ISM_WaterFX`), `UDungeonFloraManagerComponent`, `UDungeonFaunaSubsystem` (`FFaunaSoA` `256`), `UDungeonAcousticSubsystem` + `UDungeoneerPCMSoundWave` (`42` foley WAVs) |
| `v1.1.0` | Multi-Biome Floor Plans, 8-HISM Bucketing, Systemic Traps & Atmosphere (`TASK-1101`) | Done | `EDungeonBiomeType`/`EDungeonRoomShape` (`10` core + `2` Secret Phantasm Pockets + transition bleed), full 16-cell `AssetBootstrap` + Doors/Sanctuary procedural fallbacks, `8`-HISM buckets (`WetFloorHISM`, `DiagonalWedgeHISM`, `SconceHISM`, `ForegroundParallaxHISM`) + `4`-light `SconceLightPool`, `FDungeoneerTrapVentEntry` + `IgniteOrAccelerateFuse` + acoustic noise lures, `AmbientMotesHISM` (`64` instances) & `0.6s` fog blend, `10` UE 5.8.2 specs (`UE_TEST_EXIT=0`) |
| `v0.1.0` | Project & Config Bootstrap | Done | UE 5.8 C++20 `.uproject`, `.Target.cs` (`V7`), `DarkDungeoneer.Build.cs`, `Default*.ini`, rules/skills |
| `v0.2.0` | Core 4-Axis & Split-Screen Isolation | Done | `ADungeoneerCharacter`, `UDungeoneerMovementComponent`, `UDungeoneerLocalPlayerSubsystem`, `FDungeoneerSensoryViewExtension` |
| `v0.3.0` | 2.5D Pixel-Lit Shader & Pro Sprite Rigs | Done | `PixelLit25D.ush`, `U25DDirectionalRigComponent`, `USensoryPerkComponent`, `slice_normal_rig.py`, `Content/Art/Rigs/` |
| `v0.4.0` | Zombie Spatial Hash & Chrono-Leech AI | Done | `UZombieSpatialSubsystem`, `AZombieExploder`, `AZombieFastThrower`, `AHazardZone_{Fire,Poison,ChronoGas}` |
| `v0.5.0` | Procedural Dungeon DAG & Mirror Illusions | Done | `UDungeonGeneratorSubsystem`, `AMirrorWall_Solid` (`101`), `AMirrorWall_Phantasm` (`104`), `AMirrorDecoyNode` (`106`), Encounter Actors |
| `v0.6.0` | Timed Step/Fuse Audio & Gibberish Synth | Done | `UDungeoneerTimedAudioComponent`, `UAnimNotify_DungeoneerTimedCue`, `UGibberishSpeechComponent`, `generate_phoneme_bank.py` |
| `v0.7.0` | Social Party, 2PC Trading & UE 5.8 Verification | Done | `UDungeoneerSocialSubsystem`, `UTradeManagerComponent`, `UnrealEditor-DarkDungeoneer.dll` built, 3 headless UE 5.8.2 specs passed |
| `v0.7.1` | Menu Mockup, SemVer Changelog & Devlog | Done | `docs/MENU_MOCKUP.md`, `docs/TASKS.md`, `docs/DEVLOG.md`, `CHANGELOG.md`, `VERSION` |
| `v0.8.0` | Main Menu UI, 2.5D Diorama, Prediction & Seed Replication | Done | `ADungeoneerGameState`, `FSavedMove_Dungeoneer`, `UMirrorCaptureBudgetSubsystem`, `AZombieBase::IsNetRelevantFor`, `TASK-801`–`TASK-804`, 5 UE 5.8.2 specs (`EXIT CODE: 0`) |
| `v0.8.1` | Standalone Executable & Programmatic UMG Bootstrap | Done | `DarkDungeoneer.exe`, `RebuildWidget()` UMG fallback trees, `Launch-DarkDungeoneer.bat` |
| `v0.9.0` | Split-Screen HUD, 2PC Trade Modal, `.uasset` Bootstrap & 2.5D Hybrid Anim/Slicer | Done | `UDungeoneerPlayerHUDWidget`, `UDungeoneerTradeModalWidget`, `UDungeoneerAssetBootstrapSubsystem`, `slice_normal_rig.py` (`_E.png` + `AtlasMetadata.json`), `U25DAnimOrchestratorComponent`, 7 UE 5.8.2 specs (`EXIT CODE: 0`) |
| `v0.9.1` | 2.5D Modular Dungeon HISM Assembly, South-Wall Cutaways & Mirror Light-Bounce | Done | `slice_normal_rig.py --mode dungeon`, `ADungeoneerTileMapInstancer`, `UMirrorLightBounceComponent`, 4 Dungeon Atlases/Props, 8 UE 5.8.2 specs (`EXIT CODE: 0`) |
| `v0.9.2` | Startup Black-Screen Fix, Visible Diorama/HISM Materials & In-Place Dungeon Play | Done | `/Engine/Maps/Entry`, `AMenuDioramaStageActor` meshes/fill light, `ADungeoneerCharacter` hero mesh + WASD/mouse fallback, `ADungeoneerGameMode::EnterLiveDungeonFromMenu`, 8 UE 5.8.2 specs (`EXIT CODE: 0`) |
| `v0.9.3` | Dungeon Texture Bootstrap, 4-Axis Corridor Carving & Live Room Encounters | Done | `UDungeoneerAssetBootstrapSubsystem` single-cell extraction + textured HISM/rig materials, carved doorways, deduplicated corridors, `AZombieBase`/encounter visuals & spawner, 8 UE 5.8.2 specs (`EXIT CODE: 0`) |
| `v0.9.4` | Camera-Relative 2.5D Facing & Integrated-GPU Dynamic Lighting | Done | `OctantToScreenDirectionalRow`, 4-row `Hero`/`Exploder`/`Thrower` extraction, masked 2-sided `Plane.Plane` cutouts, idle-mouse arbitration, Lumen/VSM/TSR removal preserving dynamic flashlight & Volumetric Fog, 8 UE 5.8.2 specs (`EXIT CODE: 0`) |
| `v0.9.5` | Cutout Silhouette `OpacityMask` Fix & Billboard Front-Fill Lighting | Done | `SpriteMaskTextures` transient silhouette extraction (`GetSpriteMaskTexture`), `OpacityMask` parameter binding on `M_Shader_SimpleMaskedTwoSided` / `M_ES_Phong_Masked_Master_2sided`, `45°` plane tilt, `SetCastShadow(false)` & front-positioned fill lights, 8 UE 5.8.2 specs (`EXIT CODE: 0`) |
| `v0.9.6` | Alternating 4-Frame Walk Cycles & Distance-Locked 16-Cell Flipbook | Done | Articulated 4x4 walk cycles (`Hero`, `Exploder`, `Thrower`), 136 extracted cells & masks (`ExtractDefaultSingleCellTiles`), pre-allocated 16 MIDs driven by `AdvanceWalkFlipbookByDistance`, 8 UE 5.8.2 specs (`UE_TEST_EXIT=0`) |
| `v0.9.7` | Standardized 16-Cell Kinematics, Stride Validator & `U25DAnimOrchestratorComponent` Footstep Sync | Done | `SKILL.md` / `TO_GENERATE.md` 4x4 spec + prompt template, `slice_normal_rig.py --validate-stride --repair-stride`, `EFoot::Left/Right` `OnFootstepContact` on Col 1/3, Idle Col 0 lock, 8 UE 5.8.2 specs (`UE_TEST_EXIT=0`) |
| `v1.0.0` | Playable Cooperative Combat, Dodge I-Frames & Multi-Floor Loop (`TASK-1001`) | Done | Lantern Blade cleave (`ApplyMeleeArcDamage` + `SlashArcMesh`), 4-axis Dodge Dash i-frames, Zombie knockback/hit-flash/death rewards, `[F]` Vendor/Vault interact, `AdvanceToNextDungeonFloor`, HUD Floor/Gold/Kills readout, 9 UE 5.8.2 specs (`UE_TEST_EXIT=0`) |

---

## Completed Subdivided Tasks (`v0.8.0`)

### Milestone `v0.8.0` — Main Menu, Diorama Stage & Co-Op Lobby (`MENU_MOCKUP.md`)

> Goal: Implement the C++20 UI viewmodels and live 2.5D diorama backing [`docs/MENU_MOCKUP.md`](MENU_MOCKUP.md) without putting gameplay logic inside Blueprints.

- [x] **`TASK-801` — Left Gothic Menu Stack & Mode Dispatcher (`UDungeoneerMainMenuWidget`)**
  - **Files**: `Source/DarkDungeoneer/Public/UI/DungeoneerMainMenuWidget.h`, `Private/UI/DungeoneerMainMenuWidget.cpp`
  - **Scope**: Bind Blueprint-assigned UMG text/button slots (`EnterCatacombs`, `ContinueExpedition`, `CoopSplitScreen`, `SensoryLoadout`, `MirrorTrial`, `RestingGround`, `Options`, `Quit`) to server travel / map transition logic in C++.
  - **Acceptance Criteria**: Compiles with zero warnings under `V7`; hovering/selecting buttons updates highlight state and triggers mode preview callbacks without dynamic `Tick` heap allocations.

- [x] **`TASK-802` — Live 2.5D Menu Diorama Stage (`AMenuDioramaStageActor`)**
  - **Files**: `Source/DarkDungeoneer/Public/UI/MenuDioramaStageActor.h`, `Private/UI/MenuDioramaStageActor.cpp`
  - **Scope**: Spawn/position showcase Hero, Exploder, Fast Thrower (inside idle Chrono-Gas pool), Phantasm Mirror (`Stencil 104`), and periodic deterministic distance/phase acoustic sonar ring pulses (`Stencil 105`) for the main menu background camera (`-60°`).
  - **Acceptance Criteria**: Smooth flashlight aim interpolation across showcase octants; zero `GetAllActorsOfClass` or `FTimerHandle` usage.

- [x] **`TASK-803` — Top-Right Co-Op & Split-Screen Squad Widget (`UDungeoneerCoopLobbyWidget`)**
  - **Files**: `Source/DarkDungeoneer/Public/UI/DungeoneerCoopLobbyWidget.h`, `Private/UI/DungeoneerCoopLobbyWidget.cpp`
  - **Scope**: Render the 4 party slots (`LOCAL #0..#3` + `OnlineSubsystem` friend invites), toggle `1P` / `2P HORIZ` / `3P` / `4P` split-screen layout on `UDungeoneerGameViewportClient`, and reflect per-`ULocalPlayer` sensory tiers from `UDungeoneerLocalPlayerSubsystem`.
  - **Acceptance Criteria**: Strictly queries `ULocalPlayer` / `APlayerState` (never `GetFirstPlayerController()`); verified with an automation test in `DungeoneerVerificationSpecs.cpp`.

- [x] **`TASK-804` — Bottom Seed Telemetry & Prompt Bar (`UDungeoneerMenuFooterWidget`)**
  - **Files**: `Source/DarkDungeoneer/Public/UI/DungeoneerMenuFooterWidget.h`, `Private/UI/DungeoneerMenuFooterWidget.cpp`
  - **Scope**: Display active `FRandomStream` seed hex, bind `[R]` seed reroll directly to `UDungeonGeneratorSubsystem`, and mirror active octant telemetry.
  - **Acceptance Criteria**: Seed reroll deterministically updates preview node metadata and replicates cleanly to joined party clients.

---

## Completed Subdivided Tasks (`v0.9.0`)

### Milestone `v0.9.0` — In-Game Split-Screen HUD, 2PC Trade UI, `.uasset` Bootstrap & 2.5D Hybrid Animation (`TASK-901`–`TASK-903`)

- [x] **`TASK-901` — Per-Viewport Player HUD (`UDungeoneerPlayerHUDWidget`)**
  - **Files**: `Source/DarkDungeoneer/Public/UI/DungeoneerPlayerHUDWidget.h`, `Private/UI/DungeoneerPlayerHUDWidget.cpp`, `Private/Core/DungeoneerCharacter.cpp`
  - **Scope**: Bind per-`ULocalPlayer` health, flashlight battery, live local 8-compass move/aim octant indicators, sensory tiers, and active Chrono-Leech slow gauge (`S` in `[0.0, MaxSlowRatio]`).

- [x] **`TASK-902` — Resting Ground Vendor & 2PC Trade Modal (`UDungeoneerTradeModalWidget`)**
  - **Files**: `Source/DarkDungeoneer/Public/UI/DungeoneerTradeModalWidget.h`, `Private/UI/DungeoneerTradeModalWidget.cpp`, `Public/Audio/GibberishSpeechComponent.h`
  - **Scope**: Programmatic modal driving `UTradeManagerComponent::Server_LockPlayerTradeOffer`, `Server_ConfirmPlayerTradeCommit`, `Server_CancelTrade`, and `Server_ExecuteVendorPurchase`, paired with `UGibberishSpeechComponent` chatter subtitle feed and `OnTradeAckReceived` ack updates.

- [x] **`TASK-903` — Runtime Content Bootstrap, `.uasset` Serialization & 2.5D Hybrid Slicer/Animator**
  - **Files**: `Source/DarkDungeoneer/Public/Core/DungeoneerAssetBootstrapSubsystem.h`, `Private/Core/DungeoneerAssetBootstrapSubsystem.cpp`, `Public/Rendering/25DAnimOrchestratorComponent.h`, `Private/Rendering/25DAnimOrchestratorComponent.cpp`, `.agents/skills/sprite-sheet-25d-cutter/scripts/slice_normal_rig.py`
  - **Scope**: Upgrade 4x4 magenta (`#FF00FF`) slicer to emit zero-halo `_D.png`, distance-bevel + 3x3 Sobel `_N.png`, high-luminance emissive `_E.png`, and 16-cell `AtlasMetadata.json`; drive distance-locked walk cycles, 120Hz springs, `OnStepContact` audio/sensory events, and Custom Primitive Data `[0..4]` in `U25DAnimOrchestratorComponent`; import all 18 rig PNGs + 50 WAV stems and serialize idempotent `MPC_SensoryState.uasset`, `DA_25DMaterialConfig.uasset`, and `DA_MovementConfig.uasset`.

## Completed Subdivided Tasks (`v0.9.1`)

### Milestone `v0.9.1` — Modular Dungeon Instancing, Per-Viewport Cutaways & Mirror Light-Bounce (`TASK-904`–`TASK-905`)

- [x] **`TASK-904` — Dungeon Atlas & Mirror Portal Mask Extractor (`slice_normal_rig.py --mode dungeon`)**
  - **Files**: `.agents/skills/sprite-sheet-25d-cutter/scripts/slice_normal_rig.py`, `Content/Art/Dungeon/*`, `Content/AssetManifest.json`
  - **Scope**: Add `--mode dungeon` processing borderless seamless `ATLAS_*` floor arrays (`_D`, `_N`, `_H` POM heightmap, `_R` puddle roughness `0.05`) and `#FF00FF`-keyed `PROP_*` walls/mirrors/breakables extracting `#00FF00` mirror panes into `_ORM` Red (`MirrorPortalMask = 255`).

- [x] **`TASK-905` — Zero-Draw-Call Dungeon HISM Builder, Cutaways & Mirror Light-Bounce**
  - **Files**: `Source/DarkDungeoneer/Public/Dungeon/DungeoneerTileMapInstancer.h`, `Private/Dungeon/DungeoneerTileMapInstancer.cpp`, `Public/Dungeon/MirrorLightBounceComponent.h`, `Private/Dungeon/MirrorLightBounceComponent.cpp`, `Private/Core/DungeoneerCharacter.cpp`
  - **Scope**: Assemble 4-axis (`N/S/E/W` + 45° `NE/NW/SE/SW`) procedural dungeon HISM rooms, toggle per-viewport South-wall cutaways via `SetCustomDataValue(0, 1.0f)` preserving `bCastHiddenShadow`, bounce character flashlight beams off `MirrorReflective` panes via `R = I - 2 * (I . N) * N` onto a pre-allocated secondary `USpotLightComponent` to reveal illusion seams, and shatter stepped-on breakables with `FDungeoneerNoiseEvent` Acute Hearing ripples.

## Completed Subdivided Tasks (`v0.9.2`)

### Milestone `v0.9.2` — Startup Black-Screen Fix, Visible Diorama/HISM Materials & In-Place Dungeon Play (`TASK-906`)

- [x] **`TASK-906` — Startup Map, Diorama Stage Meshes/Lighting, Hero Fallback Input & In-Place Dungeon Launch**
  - **Files**: `Config/DefaultEngine.ini`, `Source/DarkDungeoneer/Public/UI/MenuDioramaStageActor.h`, `Private/UI/MenuDioramaStageActor.cpp`, `Private/Dungeon/DungeoneerTileMapInstancer.cpp`, `Public/Core/DungeoneerCharacter.h`, `Private/Core/DungeoneerCharacter.cpp`, `Public/Core/DungeoneerGameMode.h`, `Private/Core/DungeoneerGameMode.cpp`, `Private/UI/DungeoneerMainMenuWidget.cpp`, `Private/Tests/DungeoneerVerificationSpecs.cpp`
  - **Scope**: Replace `/Engine/Maps/Templates/OpenWorld` with `/Engine/Maps/Entry`, populate all `AMenuDioramaStageActor`, `ADungeoneerTileMapInstancer`, and `ADungeoneerCharacter` meshes/materials/fill lights cleanly, prevent pawn view-target clobbering (`bAutoManageActiveCameraTarget = false`), and enable in-place menu-to-dungeon transition (`EnterLiveDungeonFromMenu`) plus direct `WASD`/Mouse aim fallback when Enhanced Input DataAssets are unset.

## Completed Subdivided Tasks (`v0.9.3`)

### Milestone `v0.9.3` — Dungeon Texture Bootstrap, 4-Axis Corridor Carving & Live Room Encounters (`TASK-907`–`TASK-908`)

- [x] **`TASK-907` — Dungeon Atlas Import, Single-Cell Cropping & Textured HISM/Rig Materials**
  - **Files**: `Source/DarkDungeoneer/DarkDungeoneer.Build.cs`, `Public/Core/DungeoneerAssetBootstrapSubsystem.h`, `Private/Core/DungeoneerAssetBootstrapSubsystem.cpp`, `Private/Core/DungeoneerCharacter.cpp`, `Private/UI/MenuDioramaStageActor.cpp`
  - **Scope**: Add `ImageWrapper` dependency; import all `16` dungeon sheets (`ATLAS_*`, `PROP_*`) with `TF_Nearest` + `TEXTUREGROUP_Pixels2D`; crop `16` single-cell tile textures from `4x4` sheets on startup (`ExtractAtlasCellTexture`) and bind pre-compiled textured engine shaders (`VertexPaint_2Tex_Color` / `M_Shader_Simple`) cleanly across HISM builders, characters, zombies, and menu diorama proxies.

- [x] **`TASK-908` — Doorway Carving, Deduplicated 4-Axis Corridors & Live Room Encounter Spawning**
  - **Files**: `Source/DarkDungeoneer/Public/Dungeon/DungeoneerTileMapInstancer.h`, `Private/Dungeon/DungeoneerTileMapInstancer.cpp`, `Public/AI/ZombieBase.h`, `Private/AI/ZombieBase.cpp`, `Public/AI/ZombieExploder.h`, `Private/AI/ZombieExploder.cpp`, `Public/AI/ZombieFastThrower.h`, `Public/Dungeon/DungeonEncounterActors.h`, `Private/Dungeon/DungeonEncounterActors.cpp`, `Private/Tests/DungeoneerVerificationSpecs.cpp`
  - **Scope**: Carve doorway apertures across open-void and flush-abutting rooms in `BuildDungeonFromFloorGraph`, merge T-junction corridor floor cells via `TSet<FIntPoint>` with cutaway-aware flanking corridor walls, attach visible meshes + beacon lights to zombies, Resting Ground, Vendor, and Major Loot Vault actors, and spawn/track room encounters deterministically.

### Milestone `v0.9.4` — Camera-Relative 2.5D Facing & Integrated-GPU Dynamic Lighting

- [x] **`TASK-909` — Camera-Relative 4-Row Sprite Extraction, Cutout Orientation & Input Aim Arbitration**
  - **Files**: `Source/DarkDungeoneer/Public/Core/DungeoneerTypes.h`, `Public/Core/DungeoneerAssetBootstrapSubsystem.h`, `Private/Core/DungeoneerAssetBootstrapSubsystem.cpp`, `Public/Core/DungeoneerMovementComponent.h`, `Public/Core/DungeoneerCharacter.h`, `Private/Core/DungeoneerCharacter.cpp`, `Public/AI/ZombieBase.h`, `Private/AI/ZombieBase.cpp`, `Public/AI/ZombieExploder.h`, `Public/AI/ZombieFastThrower.h`, `Private/Tests/DungeoneerVerificationSpecs.cpp`
  - **Scope**: Add `OctantToScreenDirectionalRow`, extract all 4 directional sprite rows (`Row0..Row3` = cells `0, 4, 8, 12`) for `Hero`/`Exploder`/`Thrower` (`40` cells total), align `/Engine/BasicShapes/Plane.Plane` UV/normal orientation (`bFlipHorizontalForSpritePlane`), bind masked 2-sided shaders (`M_ES_Phong_Masked_Master_2sided` / `M_Shader_SimpleMaskedTwoSided`), default hero facing East (`+X`) and zombies West (`-X`), swap pre-allocated directional MIDs (`UpdateHeroDirectionalVisual` / `UpdateZombieDirectionalVisual`), and gate mouse cursor aiming on motion (`> 4.0 px²` within `0.45s`) so idle cursors never override `WASD` facing.
- [x] **`TASK-910` — Integrated-GPU (`Intel HD 520`) Rendering Optimization Preserving Dynamic Lighting**
  - **Files**: `Config/DefaultEngine.ini`, `Source/DarkDungeoneer/Private/Core/DungeoneerGameMode.cpp`, `Private/Dungeon/DungeonEncounterActors.cpp`, `Private/UI/MenuDioramaStageActor.cpp`, `Private/Dungeon/DungeoneerTileMapInstancer.cpp`, `Launch-DarkDungeoneer.bat`
  - **Scope**: Disable Lumen GI/reflections, Virtual Shadow Maps, DistanceField AO/Shadowing, Capsule Shadows, and TSR in INI and via `ADungeoneerGameMode::ApplyLowSpecDynamicLightingCVars` (`ECVF_SetByCode`), preserve dynamic flashlight shadows & Volumetric Fog with lighter froxel budgets (`GridPixelSize=16`, `GridSizeZ=64`, `HistoryMissSupersampleCount=1`), FXAA, `r.ScreenPercentage=75`, `t.MaxFPS=60`, `1280x720` launcher resolution, disable shadows on `CampfireLight` and diorama fill spot, and cull HISM instances (`SetCullDistances(0, 3800)`).

### Milestone `v0.9.5` — Cutout Silhouette `OpacityMask` Fix & Billboard Front-Fill Lighting

- [x] **`TASK-911` — Silhouette Mask Texture Synthesis, `OpacityMask` Shader Binding & Cutout Lighting**
  - **Files**: `Source/DarkDungeoneer/Public/Core/DungeoneerAssetBootstrapSubsystem.h`, `Private/Core/DungeoneerAssetBootstrapSubsystem.cpp`, `Private/Core/DungeoneerCharacter.cpp`, `Private/AI/ZombieBase.cpp`, `Private/Tests/DungeoneerVerificationSpecs.cpp`
  - **Scope**: Add `SpriteMaskTextures` / `GetSpriteMaskTexture` in `UDungeoneerAssetBootstrapSubsystem` generating crisp binary alpha silhouette masks for extracted character rows (including mirrored cells and `Cell0` aliases), prefer `/Engine/Engine_MI_Shaders/M_Shader_SimpleMaskedTwoSided` and bind `OpacityMask` across both masked two-sided engine shaders so sprite cutouts no longer clip against black `T_Emissive`, tilt cutout planes to `45°`, set `SetCastShadow(false)` on hero/zombie cutout meshes, shift `HeroLanternFillLight` (`(-65, 0, 110)`) and `ZombieEmissiveLight` (`(-45, 0, 65)`) in front of billboards, and verify `OpacityMask` binding in `DungeoneerVerificationSpecs.cpp`.

### Milestone `v0.9.6` — Alternating 4-Frame Walk Cycles & Distance-Locked 16-Cell Flipbook (`TASK-912`)

- [x] **`TASK-912` — 4x4 Alternating Stride Sheets & Runtime 16-Frame Flipbook Stepping**
  - **Files**: `Content/Art/Rigs/{DungeoneerHero,ZombieExploder,ZombieFastThrower}/*`, `Content/AssetManifest.json`, `Source/DarkDungeoneer/Private/Core/DungeoneerAssetBootstrapSubsystem.cpp`, `Source/DarkDungeoneer/Public/Core/DungeoneerCharacter.h`, `Private/Core/DungeoneerCharacter.cpp`, `Public/AI/ZombieBase.h`, `Private/AI/ZombieBase.cpp`, `Private/Tests/DungeoneerVerificationSpecs.cpp`
  - **Scope**: Rebuild `Hero`, `Exploder`, and `Thrower` 4x4 sheets with unmistakable alternating `Contact-L -> Passing-A -> Contact-R -> Passing-B` footsteps (including true South/North cardinal rows and halo cleanup on `FastThrower`), re-slice normal/emissive/ORM atlases with updated SHA-256 manifest hashes, extract all `16` walk cells + silhouette masks (`136` total cache keys), pre-allocate `16` MIDs per character/zombie, and step `Row * 4 + Col` deterministically via XY distance traveled (`AdvanceWalkFlipbookByDistance`).

### Milestone `v0.9.7` — Standardized 16-Cell Kinematics, Stride Validator & Footstep Sync (`TASK-913`)

- [x] **`TASK-913` — 4x4 Kinematic Specification, `--validate-stride`/`--repair-stride` & `U25DAnimOrchestratorComponent` Footstep Sync**
  - **Files**: `.agents/skills/sprite-sheet-25d-cutter/SKILL.md`, `docs/TO_GENERATE.md`, `.agents/skills/sprite-sheet-25d-cutter/scripts/slice_normal_rig.py`, `Content/Art/Rigs/{DungeoneerHero,ZombieExploder,ZombieFastThrower,Hero,Exploder,Thrower}/*`, `Content/AssetManifest.json`, `Source/DarkDungeoneer/Public/Rendering/25DAnimOrchestratorComponent.h`, `Private/Rendering/25DAnimOrchestratorComponent.cpp`, `Public/Core/DungeoneerCharacter.h`, `Private/Core/DungeoneerCharacter.cpp`, `Public/AI/ZombieBase.h`, `Private/AI/ZombieBase.cpp`, `Private/Tests/DungeoneerVerificationSpecs.cpp`
  - **Scope**: Lock strict `Col 0: Contact/Idle A (+1..+2 px) -> Col 1: Full Left Step (-1..-2 px) -> Col 2: Passing B (+1..+2 px) -> Col 3: Full Right Step (-1..-2 px)` matrix across docs, prompt templates, slicer synthesis, `--validate-stride`/`--repair-stride`, re-sliced atlases, `Velocity.IsNearlyZero()` -> Col 0 locking, and `OnFootstepContact(EFoot::Left / EFoot::Right)` on Col 1 and Col 3.

---

## Upcoming Subdivided Tasks

### Milestone `v1.0.0` — Playable Cooperative Vertical Slice

- [x] **`TASK-1001` — End-to-End Dungeon Combat, Dodge, Loot Vault & Multi-Floor Loop**
  - **Files**: `Source/DarkDungeoneer/Public/Core/DungeoneerCharacter.h`, `Private/Core/DungeoneerCharacter.cpp`, `Public/Core/DungeoneerGameMode.h`, `Private/Core/DungeoneerGameMode.cpp`, `Public/AI/ZombieSpatialSubsystem.h`, `Private/AI/ZombieSpatialSubsystem.cpp`, `Public/AI/ZombieBase.h`, `Private/AI/ZombieBase.cpp`, `Private/AI/ZombieFastThrower.cpp`, `Public/Dungeon/DungeoneerTileMapInstancer.h`, `Private/Dungeon/DungeonGeneratorSubsystem.cpp`, `Public/UI/DungeoneerPlayerHUDWidget.h`, `Private/UI/DungeoneerPlayerHUDWidget.cpp`, `Private/Tests/DungeoneerVerificationSpecs.cpp`
  - **Scope**: Lantern Blade melee cleave + `SlashArcMesh` sweep & procedural lunge/follow-through, 4-axis Dodge Dash i-frames, Hit Stagger + entry respawn, Zombie hit flash/knockback/death gold+battery rewards, `[F]` Vendor purchase & Loot Vault unlock -> `AdvanceToNextDungeonFloor()` Floor `N -> N+1` progression, live HUD Floor/Gold/Kills/CombatState & `[F]` hints, plus full Editor/Game `UBT` builds and `9` passing UE 5.8.2 specs.

### Milestone `v1.2.0` — Living Dungeon Ecosystem (`TASK-1201`)

- [x] **`TASK-1201` — 4-Tier Light Budget, Hydrology, Interactive Flora, SoA Fauna & Surface Acoustics (M1–M3)**
  - **Files**: `Public/Ecosystem/DungeonEcosystemTypes.h`, `Public/Lighting/DungeonLightBudgetSubsystem.h`, `Private/Lighting/DungeonLightBudgetSubsystem.cpp`, `Public/Rendering/DungeoneerEcoMaterialFactory.h`, `Private/Rendering/DungeoneerEcoMaterialFactory.cpp`, `Public/Ecosystem/DungeonWaterSubsystem.h`, `Private/Ecosystem/DungeonWaterSubsystem.cpp`, `Public/Ecosystem/DungeonFloraManagerComponent.h`, `Private/Ecosystem/DungeonFloraManagerComponent.cpp`, `Public/Ecosystem/DungeonFaunaSubsystem.h`, `Private/Ecosystem/DungeonFaunaSubsystem.cpp`, `Public/Audio/DungeonAcousticSubsystem.h`, `Private/Audio/DungeonAcousticSubsystem.cpp`, `Public/Audio/DungeoneerPCMSoundWave.h`, `Private/Audio/DungeoneerPCMSoundWave.cpp`, `scripts/synthesize_ecosystem_foley.py`, `Content/Audio/Foley/*.wav`
  - **Scope**: Implement `M_Dungeoneer_EcoInstanced` with `EEcoCPD::Count == 8` and 32-bit `UnpackRGB24` Tier 1 shader distance-field glows (`MaxTier1 = 32`, `MaxTier2 = 16`, `MaxTier3World = 2`, `MaxCandidates = 512`), `UDungeonWaterSubsystem` (`128 UU` surface grid, `0.88x` wading speed + `FSavedMove_Dungeoneer::bSavedWading`, `24` ceiling drips, `256`-slot `ISM_WaterFX`), `UDungeonFloraManagerComponent` (`HISM_MagicalFlora`), `UDungeonFaunaSubsystem` (`FFaunaSoA::Capacity = 256`), `42` synthesized deterministic foley WAVs played safely via `UDungeoneerPCMSoundWave` (`MaxWalkers = 64`, `TotalVoicePoolSize = 22`), and `6` ecosystem automation specs in `DungeoneerVerificationSpecs.cpp`.

### Milestone `v1.2.1` — 2.5D Visual Overhaul & Release Integration (`TASK-1202`)

- [x] **`TASK-1202` — Pixel-Art Dungeon Atlases, De-Haloed Sprites, `-45°` Isometric Camera, HISM Builds & M4 Release**
  - **Files**: `.agents/skills/sprite-sheet-25d-cutter/scripts/slice_normal_rig.py`, `.agents/skills/sprite-sheet-25d-cutter/scripts/eco_cleanup_masks.json`, `Content/Art/Dungeon/*`, `Content/Art/Ecosystem/*`, `Private/Rendering/DungeoneerEcoMaterialFactory.cpp`, `Private/Dungeon/DungeoneerTileMapInstancer.cpp`, `Private/Core/DungeoneerCharacter.cpp`, `Private/UI/MenuDioramaStageActor.cpp`, `Private/Core/DungeoneerGameMode.cpp`, `Private/Ecosystem/DungeonWaterSubsystem.cpp`, `VERSION`, `CHANGELOG.md`, `README.md`, `docs/TASKS.md`, `docs/DEVLOG.md`
  - **Scope**: Replace geometric floor/wall/doorway sheets with generated pixel-art atlases + sticker-halo flooding cleanup, rebuild HISM cluster trees/bounds (`BuildTreeIfOutdated`, `UpdateBounds`), switch `SconceHISM` to upright cutout sprites and frame doorways cleanly, align gameplay and Menu Diorama cameras to `Pitch = -42°, Yaw = -45°, FOV = 35°` with calibrated `ELightUnits::Lumens` + clamped exposure (`0.42f`–`1.40f`), add Tick-based `-CaptureShotAfterSec`/`-QuitAfterShot`, verify both live Entry Room & Main Menu captures, pass `slice_normal_rig.py --self-test`, `verify_architecture_invariants.py` (`101` files), Editor & Standalone `Win64 Development` builds (`0` project warnings), all `16/16` headless UE `5.8.2` specs (`EXIT CODE: 0`), and cut `v1.2.1` release tag.

---

## Roadmap & Active Tasklist (`v1.3.0` – `v1.4.0`)

> Full architectural breakdown and exit criteria live in [`docs/ROADMAP_V1_3_AND_V1_4.md`](ROADMAP_V1_3_AND_V1_4.md).

### Milestone `v1.3.0` — Tactical Depth, Relics & Crypt Guardians

- [ ] **`TASK-1301` — Menu Diorama Cutout Sprites & Tactical Minimap/Popup HUD**
  - [ ] Replace `AMenuDioramaStageActor` `CylinderMesh` / `CubeMesh` showcase primitives with camera-facing 2.5D masked sprite planes (`Exploder`, `Thrower`, `Phantasm` mirror showcase) while keeping test accessors intact.
  - [ ] Add discovered-room Minimap (`MaxRooms = 16` fixed buffer), Sensory Perk tier pips (`0..3`), Relic & Consumable quick-bar, and `32`-slot pooled floating damage/gold text to `UDungeoneerPlayerHUDWidget`.
- [ ] **`TASK-1302` — Relic & Consumable Synergies + Interactive Vendor & Sanctuary Modals**
  - [ ] Implement pre-allocated Relics (*Sun-Glass Lens*, *Chrono-Ward Boots*, *Echo-Tuned Lantern Blade*) and quick-use Consumables (`[Q]` / `[1-2]`: *Lantern Oil Phial*, *Antidote Draught*) inside `UDungeoneerInventoryComponent` & combat/movement hooks.
  - [ ] Spawn collectible `ADungeoneerLootPickupActor` drops from Vaults, Secret Phantasm pockets, and breakables, and extend `UDungeoneerTradeModalWidget` for Vendor stock browsing and Sanctuary `USensoryPerkComponent::UpgradePerk` altar choices.
- [ ] **`TASK-1303` — `AZombieMirrorStalker` Archetype & Floor 3 Biome Guardian Boss**
  - [ ] Generate `4x4` pixel-art rig via `slice_normal_rig.py --validate-stride` and implement `AZombieMirrorStalker` (stealthed outside flashlight/mirror beams or Dark Vision `>= 2`, passes through `AMirrorWall_Phantasm`).
  - [ ] Implement Floor 3 Guardian Boss (`ABossGuardianActor` with deterministic phase-accumulated 8-compass slam rings, mortar hazard volleys, crystal-waking screech, boss HUD bar, and Boss Core Relic reward unlocking `AMajorLootVaultActor`).
- [ ] **`TASK-1304` — 3-Hit Lantern Blade Combo, Charged Flare & Destructible Chains**
  - [ ] Add 3-hit melee combo buffer (`0.45s` window) + Charged Lantern Flare (`25%` battery radial stun & room-wide `PrismLily` bloom), `35 ms` hit-stop dilation, damped `CameraBoom` micro-impulse, and barrel/fire/Exploder detonation chains.
  - [ ] Add `4+` new deterministic specs (`RelicSynergies`, `MirrorStalkerLightReveal`, `GuardianBossPhases`, `ComboFlareChain`) and verify all `20+` headless UE `5.8.2` tests pass (`EXIT CODE: 0`).

### Milestone `v1.4.0` — Co-Op Hardening, Persistent Meta-Ledger & Cooked Release

- [ ] **`TASK-1401` — Complete 2-Player Local Split-Screen Drop-In & Co-Op Revive**
  - [ ] Wire P2 dynamic split-screen drop-in (`Menu` + `[F2]`/Gamepad `Start`), isolated `FDungeoneerSensoryViewExtension` viewports, and per-camera `WallHISM` cutaway evaluation.
  - [ ] Add `18s` Co-Op Downed Bleeding state with `[Hold F]` `2.0s` ally rescue revive and shared Sanctuary Party Stash (`ARestingGroundActor`).
- [ ] **`TASK-1402` — Online Co-Op Hardening, Seed Parity & P2P Trading**
  - [ ] Verify Listen/Dedicated server deterministic replication parity across `UDungeonGeneratorSubsystem`, `UDungeonWaterSubsystem`, `UDungeonFloraManagerComponent`, and `FSavedMove_Dungeoneer` (`PktLag=150`, `PktLoss=3`), plus `AZombieBase::IsNetRelevantFor` fog culling.
  - [ ] Wire live `[T]` nearby-player Trade Request -> `UDungeoneerTradeModalWidget` 2PC swap via `UTradeManagerComponent`, plus `UDungeoneerCoopLobbyWidget` session/chat polish.
- [ ] **`TASK-1403` — End-of-Run Crypt Ledger, Bestiary & Sanctuary Persistent Save**
  - [ ] Implement `UDungeoneerRunSummaryWidget` displaying per-run telemetry (`Floor`, `Kills`, `Relics`, `Secrets`, `Seed`, `Duration`) with `[Retry Same Seed]` and `[Return to Sanctuary]`.
  - [ ] Implement `UDungeoneerSaveGame` (`Saved/SaveGames/`) persisting unlocked Bestiary entries, cosmetic/starting Lantern Trims, Best Records, and deterministic `Daily Seed` runs (`YYYYMMDD`).
- [ ] **`TASK-1404` — Cooked Windows `Shipping` Packaging, Profiling & Release**
  - [ ] Configure `DefaultEngine.ini` / `DefaultGame.ini` asset cooking rules and pass `RunUAT BuildCookRun -platform=Win64 -clientconfig=Shipping -cook -stage -pak`, updating `Launch-DarkDungeoneer.bat` to launch cooked binaries seamlessly.
  - [ ] Verify `stat unit` / `stat scenerendering` Intel HD 520 frame/draw-call budgets across Floor 3 Boss Arena and 2P Split-Screen, pass `24+` headless automation tests, update docs (`VERSION 1.4.0`), and tag `v1.4.0`.
