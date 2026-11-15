# Changelog

All notable changes to **Dark Dungeoneer (UE 5.8)** are documented in this file.
The project adheres to [Semantic Versioning 2.0.0](https://semver.org/) and [Conventional Commits](https://www.conventionalcommits.org/).

---

## [Unreleased]

- **v0.8.0 (Planned)**: UMG / CommonUI implementation of the Main Menu, live 2.5D Menu Diorama Stage, and Co-Op Lobby (tracked in [docs/TASKS.md](docs/TASKS.md)).
- **v0.9.0 (Planned)**: In-game per-viewport Split-Screen HUD, 2PC Resting Ground trade modal, and UE material/MetaSound editor wiring.
- **v1.0.0 (Planned)**: Full multi-floor vertical slice (`Entry -> Combat -> Mirror-Palace -> Resting Ground -> Deep Fog -> Loot Vault`).

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
