# Dark Dungeoneer — Development Task Tracker

- **Current Version**: `0.9.2` (tracked in [`VERSION`](../VERSION))
- **Versioning Policy**: Semantic Versioning (`MAJOR.MINOR.PATCH`) paired with atomic Conventional Commits (`feat(...)`, `fix(...)`, `docs(...)`, `test(...)`, `chore(...)`).

---

## Completed Milestones (`v0.1.0` – `v0.7.1`)

| SemVer Tag | Milestone | Status | Key Modules & Verification |
| :--- | :--- | :--- | :--- |
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

---

## Upcoming Subdivided Tasks

### Milestone `v1.0.0` — Playable Cooperative Vertical Slice

- [ ] **`TASK-1001` — End-to-End 3-Floor Dungeon Loop & Package Verification**
  - **Scope**: Playable Entry Crypt -> Combat -> Mirror-Palace Illusion Maze -> Resting Ground Vendor/2PC Trade -> Deep Fog Gauntlet -> Major Loot Vault transition under 2P local split-screen + online client session, passing full `UnrealBuildTool` Game & Editor builds plus expanded `Automation RunTests DarkDungeoneer`.
