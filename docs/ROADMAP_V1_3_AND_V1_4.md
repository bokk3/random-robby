# Dark Dungeoneer — Roadmap: `v1.3.0` & `v1.4.0`

- **Baseline Release**: [`v1.2.1`](../CHANGELOG.md) (Living Dungeon Ecosystem + 2.5D Isometric Visual Overhaul, `16` passing headless UE `5.8.2` automation specs, `101` invariant-verified source files).
- **Guardrails Preserved Across Every Milestone**:
  - **C++20 Primary (`AGENTS.md`)**: Zero gameplay logic in Blueprints; widgets, materials, rigs, and procedural meshes built cleanly in C++.
  - **Hot-Path Discipline (`verify_architecture_invariants.py`)**: Zero heap allocations, zero `FTimerHandle`, and zero `GetAllActorsOfClass` / `GetFirstPlayerController` inside `Tick` / `TickComponent`.
  - **Intel HD 520 HISM Budget (Rule `02`)**: Keep HISM draw-call buckets tight, drive per-instance variations via `EEcoCPD` (`PerInstanceCustomData`), and compile with `bTreatWarningsAsErrors = true` across Editor and Standalone targets (Rule `07`).

---

## Target Trajectory Overview

| Version | Codename | Core Focus | Target Spec Count |
| :--- | :--- | :--- | :--- |
| **`v1.3.0`** | **Depth, Relics & Crypt Guardians** | Upgrade playable loop from baseline dungeon crawl into a tense tactical roguelite: real 2.5D Menu Diorama rigs, HUD minimap/popups, Relic & Consumable synergies, interactive Sanctuary/Vendor UI, Mirror Stalker + Floor 3 Biome Guardian Boss, and 3-hit melee combo / charged flare combat feel. | `20+` headless specs |
| **`v1.4.0`** | **Co-Op Hardening, Meta-Ledger & Cooked Shipping** | Complete local 2P split-screen drop-in & downed revive, packet-loss-resilient Online Co-Op + P2P trading, end-of-run summary & Sanctuary persistent unlock ledger (plus Daily Seed mode), and self-contained Windows `Shipping` packaging profiled to `< 16.6 ms` frame budget. | `24+` headless specs + soak & Shipping cook |

---

## Phase 1: `v1.3.0` — Tactical Depth, Relics & Boss Encounters

### `TASK-1301` — Menu Diorama Upgrade & Tactical HUD Overhaul
**Goal**: Replace prototype diorama cylinders/cubes visible on the Main Menu and expand the in-game HUD from telemetry text into readable dungeon navigation & combat instrumentation.
- **Upgrade `AMenuDioramaStageActor` ([MenuDioramaStageActor.cpp](../Source/DarkDungeoneer/Private/UI/MenuDioramaStageActor.cpp))**:
  - Replace `CylinderMesh` / `CubeMesh` proxy blocks (`ExploderShowcaseProxy`, `FastThrowerShowcaseProxy`, `PhantasmMirrorShowcaseProxy`, `DecoyProjectionProxy`) with camera-facing cutout sprite planes (`GetCameraFacingSpritePlaneRotation()`) displaying animated walk/pulse poses via `UDungeoneerAssetBootstrapSubsystem` masked MIDs so the Menu Diorama matches the live Entry Room quality.
  - Keep `GetExploderShowcaseProxy()` / `GetFastThrowerShowcaseProxy()` accessors non-null for `MenuNavigationAndFloorSeedDeterminism` compatibility.
- **Expand `UDungeoneerPlayerHUDWidget` ([DungeoneerPlayerHUDWidget.cpp](../Source/DarkDungeoneer/Private/UI/DungeoneerPlayerHUDWidget.cpp))**:
  - **2D Floor Minimap (`SLeafWidget` / Slate `FSlateDrawElement::MakeBox`)**: Render discovered DAG rooms (`SecretPhantasmPocket` rooms hidden until entered), Sanctuary/Vendor markers, Vault stairwell icon, and player 8-compass arrow from a fixed zero-alloc room buffer (`MaxRooms = 16`).
  - **Sensory Perk & Relic Strip + Quick-Consumable Bar (`[1]` / `[2]` / `[3]` or `[Q]`)**: Display live Dark Vision, Acute Hearing, and Scent/Thermal tier pips (`0..3`), active Relic badges, and remaining consumable charges.
  - **Pooled Floating Combat Text**: Pre-allocated ring buffer of `32` damage / gold / Chrono-slow world-to-screen labels (no per-hit Slate widget allocation).

### `TASK-1302` — Relic & Consumable Synergy System + Interactive Sanctuary/Vendor Flows
**Goal**: Make Gold, Secrets, Vendors, and Sensory Shrines drive meaningful build variety across every floor.
- **Relic & Consumable Registry (`DungeoneerTypes.h`, [DungeoneerInventoryComponent.h](../Source/DarkDungeoneer/Public/Social/DungeoneerInventoryComponent.h))**:
  - Add fixed-capacity `TStaticArray` / bounded passive Relic effects and active Consumables evaluated deterministically during movement, combat, and flora/light checks:
    - **Sun-Glass Lens**: Widens `FlashlightComponent` outer cone (`+8°`) and deals `+35%` cleave bonus damage to targets inside illuminated `PrismLily` bloom radii.
    - **Chrono-Ward Boots**: Halves `AHazardZone_ChronoGas` slow intake and improves stream wading multiplier (`0.88x` -> `0.96x`, replicated via `FSavedMove_Dungeoneer`).
    - **Echo-Tuned Lantern Blade**: Final cleave triggers an Acute Hearing shockwave (`BroadcastNoiseEvent`), briefly disarming/staggering Throwers and ringing nearby `EchoCrystal` chords.
    - **Phial of Lantern Oil / Antidote Draught**: Quick-use consumables (`[Q]` / `[1-2]`) refilling `+45%` battery or cleansing Poison/Chrono debuffs with custom foley cues.
- **World Loot Drops & Interactive Vendor / Sanctuary Modals ([DungeonEncounterActors.cpp](../Source/DarkDungeoneer/Private/Dungeon/DungeonEncounterActors.cpp), [DungeoneerTradeModalWidget.cpp](../Source/DarkDungeoneer/Private/UI/DungeoneerTradeModalWidget.cpp))**:
  - Spawn collectible glowing `ADungeoneerLootPickupActor` (pooled/reusable or bounded per floor) from `AMajorLootVaultActor` chests, shattered breakables, and Secret Phantasm pockets.
  - Extend `UDungeoneerTradeModalWidget` to open a 3-choice **Vendor Stall Catalogue** (`AVendorActor::ConsumeCatalogStock`) and a **Sensory Altar Selector** inside `ARestingGroundActor` letting players spend Gold/Shards to upgrade **Dark Vision**, **Acute Hearing**, or **Scent/Thermal Trails** (`USensoryPerkComponent::UpgradePerk`).

### `TASK-1303` — Obsidian Mirror Stalker & Floor 3 Biome Guardian Boss
**Goal**: Break combat predictability across rooms and give every 3-floor descent a signature culminating encounter.
- **New Enemy Archetype — `AZombieMirrorStalker` (`Public/AI/`, `Private/AI/`)**:
  - Paired primarily with `EDungeonBiomeType::MirrorPalace` and `SecretPhantasmPocket` encounters.
  - Starts shadowed/semi-transparent outside directional light until pierced by `FlashlightComponent`, a `UMirrorLightBounceComponent` reflection beam, or **Dark Vision** Tier `>= 2`.
  - Can step across `AMirrorWall_Phantasm` thresholds and triggers distinctive wet-obsidian footstep pings in `UDungeonAcousticSubsystem`.
  - Requires new 4x4 semi-pixelated sprite rig (`PROP` / `Rig_MirrorStalker_4x4`) processed via `slice_normal_rig.py --validate-stride`.
- **Floor 3 Guardian Boss Arena — `ABossGuardianActor` (`CinderColossus` / `BlightBroodMother`)**:
  - Guards the Floor 3 Terminal Vault stairwell inside a widened boss chamber before Vault unlock.
  - Deterministic attack phases driven by fixed-step accumulators (zero `FTimerHandle`):
    1. **Phase 1 (100%–55% HP)**: Telegraphed 8-compass radial shockwaves + aimed `AHazardZone_Fire` / `AHazardZone_ChronoGas` mortar volleys.
    2. **Phase 2 (< 55% HP)**: Faster cadence, summons `2` `AZombieExploder` adds, and emits acoustic screeches that flash arena `EchoCrystal` nodes.
  - Displays a dedicated **Boss Health & Phase Bar** at the top of `UDungeoneerPlayerHUDWidget`, guarantees a high-tier **Boss Core Relic** drop, and unlocks `AMajorLootVaultActor` upon defeat.

### `TASK-1304` — Tactile Combat Combos, Charged Lantern Flare & Hazard Chain Reactions
**Goal**: Elevate minute-to-minute melee & hazard combat responsiveness using zero-allocation C++20 pipelines.
- **3-Hit Lantern Blade Combo & Charged Flare ([DungeoneerCharacter.cpp](../Source/DarkDungeoneer/Private/Core/DungeoneerCharacter.cpp))**:
  - Chain `Quick Cleave -> Reverse Sweep -> Forward Lunge Thrust` within a `0.45s` deterministic input buffer window (`Col 1` / `Col 3` strike silhouettes + `SlashArcMesh` arc flip).
  - Add **Charged Lantern Flare** (`Hold RMB` / `R`): drains `25%` flashlight battery to emit a `420 UU` golden shock-flash that blooms every `PrismLily` in range, pushes back zombies, and momentarily clears dense `SporeFens` volumetric fog.
- **Hit-Stop, Micro Camera Impulse & Destructible Chain Synergies**:
  - Trigger `35 ms` hit-stop impulse dilation (local visual/animation pause without freezing global network simulation) + directional `CameraBoom` micro-offset kick (`FVector` damped spring on `Tick`, zero `UCameraShakeBase` allocation).
  - Wire `AHazardZone_Fire` / `EmberFire` floor vents to detonate oil/explosive barrels (`BreakableHISM`), chaining fire damage & `IgniteOrAccelerateFuse` across adjacent `AZombieExploder` clusters.

### `v1.3.0` Milestone Exit Criteria
- [ ] Zero prototype primitive cylinders/cubes remaining on `AMenuDioramaStageActor` while passing `MenuNavigationAndFloorSeedDeterminism`.
- [ ] All Relics, Consumables, 3-Hit Combo, Charged Flare, `AZombieMirrorStalker`, and Floor 3 Guardian Boss verified deterministically in `DungeoneerVerificationSpecs.cpp` (`20+` specs total).
- [ ] `slice_normal_rig.py --self-test`, `verify_architecture_invariants.py`, `DarkDungeoneerEditor`, and `DarkDungeoneer` compile cleanly with zero warnings (`bTreatWarningsAsErrors = true`).
- [ ] Live gameplay capture confirms readable Minimap HUD, Boss Arena, and Mirror Stalker light-reveal in action.

---

## Phase 2: `v1.4.0` — Co-Op Hardening, Persistent Meta-Ledger & Cooked Release

### `TASK-1401` — Complete 2-Player Local Split-Screen Drop-In & Co-Op Revive
**Goal**: Make couch co-op seamless end-to-end with strict per-viewport sensory isolation (Rule `03`).
- **Dynamic Drop-In/Out & Viewport Cutaway Isolation ([DungeoneerGameViewportClient.cpp](../Source/DarkDungeoneer/Private/Core/DungeoneerGameViewportClient.cpp), [DungeoneerLocalPlayerSubsystem.cpp](../Source/DarkDungeoneer/Private/Core/DungeoneerLocalPlayerSubsystem.cpp))**:
  - Bind Menu `Co-Op Split-Screen` + live P2 drop-in input (`F2` / Gamepad `Start`) to spawn Player 2 with an isolated `ULocalPlayer` split viewport, independent `FDungeoneerSensoryViewExtension` Dark Vision / Acute Hearing overlays, and per-camera wall cutaway alpha evaluation (`WallHISM`).
- **Co-Op Downed State, Rescue Revive & Party Sanctuary Sharing**:
  - Replace immediate respawn when `> 1` players are alive with a **`18s` Downed Bleeding State** where the ally emits faint flickering embers and can be revived via `[Hold F]` (`2.0s` channel inside the rescuer's flashlight cone).
  - Enable shared Sanctuary Stash deposits/withdrawals at `ARestingGroundActor` (`Co-Op Party Stash` cell from `PROP_Resting_Ground_Sanctuary_4x4`).

### `TASK-1402` — Online Co-Op Hardening, Seed-Parity Replication & P2P Trading
**Goal**: Validate deterministic multiplayer under non-ideal network conditions (Rules `03`, `06`, `07`).
- **Listen/Dedicated Client-Server Verification**:
  - Verify `ADungeoneerGameState` floor seed replication reproduces byte-identical `UDungeonGeneratorSubsystem`, `UDungeonWaterSubsystem` surface grids, `HISM_MagicalFlora`, and `EDungeonBiomeType` transitions across Server and Clients.
  - Validate `FSavedMove_Dungeoneer` (`bSavedWading`, `ChronoSlowFactor`, Dodge i-frames) rollback-free prediction under `PktLag=150` and `PktLoss=3` net emulation.
  - Audit `AZombieBase::IsNetRelevantFor` and `UMirrorCaptureBudgetSubsystem` fog-relevancy culling so unseen fog-occluded enemies and off-camera portals never consume bandwidth.
- **Full Online Party, Chat & 2PC Trading Flow ([DungeoneerSocialSubsystem.cpp](../Source/DarkDungeoneer/Private/Social/DungeoneerSocialSubsystem.cpp), [TradeManagerComponent.cpp](../Source/DarkDungeoneer/Private/Social/TradeManagerComponent.cpp))**:
  - Expose keybound `[T]` nearby-player Trade Request opening `UDungeoneerTradeModalWidget` wired cleanly to `UTradeManagerComponent`'s two-phase commit protocol.
  - Polish `UDungeoneerCoopLobbyWidget` host/join session browser, friend invite hooks, and in-game chat box (`Party`, `Local`, `System` channels).

### `TASK-1403` — Run Ledger, Crypt Bestiary & Sanctuary Meta-Progression
**Goal**: Reward both victorious runs and brutal wipes with persistent meta-progression and competitive seeded runs.
- **End-of-Run Summary Modal (`UDungeoneerRunSummaryWidget`)**:
  - Record per-run telemetry (`Floor Reached`, `Kills`, `Relics Collected`, `Secret Phantasms Found`, `Chrono-Slow Leeched`, `Downs/Revives`, `Run Duration`, `Deterministic Seed`) and display on Victory (Boss cleared + Vault descended) or Party Wipe with `[Retry Same Seed]` and `[Return to Sanctuary]` actions.
- **Persistent Meta-Save (`UDungeoneerSaveGame` in `Saved/SaveGames/`) & Daily Seed**:
  - Persist unlocked **Lantern Oil Trims** (cosmetic flame tints / starting Relic choice), **Crypt Bestiary** encounter logs, **Fastest Clear & Deepest Floor** records, and an optional deterministic **Daily Seed** toggle (`YYYYMMDD` seed) selectable from `UDungeoneerMainMenuWidget`.

### `TASK-1404` — Cooked Windows `Shipping` Packaging & Intel HD 520 Profiling
**Goal**: Produce a self-contained, distributable `Dark Dungeoneer` executable that launches cleanly without Editor binaries.
- **Full `BuildCookRun` Pipeline Verification**:
  - Configure default maps, splash/packaging settings, and cooked material inclusion (`/Game/Materials/M_Dungeoneer_EcoInstanced`, `MPC_SensoryState`, `Content/Art/*`, `Content/Audio/Foley/*`) in `Config/DefaultEngine.ini` and `Config/DefaultGame.ini` so `RunUAT.bat BuildCookRun -platform=Win64 -clientconfig=Shipping -cook -stage -pak` succeeds cleanly.
  - Update `Launch-DarkDungeoneer.bat` to prefer the staged `Shipping` executable seamlessly.
- **Integrated-GPU Budget Sign-Off**:
  - Capture `stat unit` and `stat scenerendering` telemetry in Floor 1 Crypt, Floor 3 Boss Arena, and 2-Player Split-Screen confirming base pass draw calls stay within budget (`<= 30` HISM + actor draw calls) and zero `Tick` frame spikes occur.

### `v1.4.0` Milestone Exit Criteria
- [ ] Verified 2-Player Split-Screen drop-in, Downed Revive, P2P `UDungeoneerTradeModalWidget` 2PC swap, and `SaveGame` round-trip specs (`24+` headless UE automation tests passing).
- [ ] Simulated multiplayer soak passes cleanly with deterministic Server/Client `UDungeonWaterSubsystem` & `UDungeonGeneratorSubsystem` parity.
- [ ] `RunUAT BuildCookRun` produces a clean Windows `Shipping` packaged build verified via clean launch test (both Main Menu and playable Crypt Floor 1 -> Boss Arena).
- [ ] Release docs updated (`VERSION 1.4.0`, `CHANGELOG.md`, `docs/TASKS.md`, `docs/DEVLOG.md`), release walkthrough finalized, and annotated `v1.4.0` tag created.
