# Dark Dungeoneer — Development Task Tracker

- **Current Version**: `0.8.1` (tracked in [`VERSION`](../VERSION))
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

## Upcoming Subdivided Tasks

### Milestone `v0.9.0` — In-Game Split-Screen HUD, 2PC Trade UI & Editor Assets

- [ ] **`TASK-901` — Per-Viewport Player HUD (`UDungeoneerPlayerHUDWidget`)**
  - **Scope**: Bind per-`ULocalPlayer` health, flashlight battery, 8-compass octant indicator, and active Chrono-Leech slow gauge (`S` in `[0.0, MaxSlowRatio]`).

- [ ] **`TASK-902` — Resting Ground Vendor & 2PC Trade Modal (`UDungeoneerTradeModalWidget`)**
  - **Scope**: Visual UI driving `UTradeManagerComponent::Server_LockPlayerTradeOffer`, `Server_ConfirmPlayerTradeCommit`, and `Server_ExecuteVendorPurchase`, paired with `UGibberishSpeechComponent` chatter subtitle feed (`TradeTwoPhaseCommitTest` already verified in `DungeoneerVerificationSpecs.cpp`).

- [ ] **`TASK-903` — UE 5.8 Content Asset Creation & Material/MetaSound Wiring**
  - **Scope**: Import `Content/Art/Rigs/*` textures (`_D`, `_N`, `_ORM`) and `Content/Audio/Voices/*` `.wav` banks into `.uasset` packages; construct `M_25D_PixelLit`, `MPC_SensoryState`, `MID_SensoryOverlay`, and `MS_GibberishSynth` graph nodes in Unreal Editor, plus data-only UMG Blueprint shells for `TASK-801`–`TASK-902`.

---

### Milestone `v1.0.0` — Playable Cooperative Vertical Slice

- [ ] **`TASK-1001` — End-to-End 3-Floor Dungeon Loop & Package Verification**
  - **Scope**: Playable Entry Crypt -> Combat -> Mirror-Palace Illusion Maze -> Resting Ground Vendor/2PC Trade -> Deep Fog Gauntlet -> Major Loot Vault transition under 2P local split-screen + online client session, passing full `UnrealBuildTool` Game & Editor builds plus expanded `Automation RunTests DarkDungeoneer`.
