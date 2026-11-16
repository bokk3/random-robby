# Dark Dungeoneer

A top-down **2.5D cooperative dark dungeon crawler** built in **Unreal Engine 5.8 (C++20)**.

Players navigate pitch-black procedural multi-floor dungeons armed with a directional flashlight cone, sensory upgrades, 8-compass 2.5D sprite animation, and mirror-bouncing light mechanics while surviving proximity-exploding and elemental-throwing undead.

> **Current Version:** `1.2.1` (Living Dungeon Ecosystem + 2.5D Visual Overhaul Release)

---

## Key Features

- **4-Axis / 8-Compass Locomotion & 2.5D Rigs (`DungeoneerMovementComponent`, `U25DAnimOrchestratorComponent`)**
  - Quantized movement along 4 discrete axes (Cardinal + Diagonal NE–SW / NW–SE) yielding 8 compass headings (`N`, `NE`, `E`, `SE`, `S`, `SW`, `W`, `NW`) with identical normalized speeds across cardinals and diagonals.
  - 4×4 (`16`-cell) semi-pixelated 2.5D sprite sheets with normal maps (`_D`, `_N`, `_ORM`, `_E`) and distance-locked stride playback.

- **Darkness, Flashlight Beams & Sensory Progression (`UDungeoneerSensoryWorldSubsystem`, `USensoryPerkComponent`)**
  - Pitch-black dungeons carved out by a directional flashlight/lantern cone and `UMirrorLightBounceComponent` beam reflections.
  - Upgradable sensory perks (**Dark Vision**, **Acute Hearing** acoustic ripples, and **Scent/Thermal Trails**) driving `MPC_SensoryState` and atmospheric fog.

- **Multi-Biome Procedural Dungeons & Mirror-Palace Mazes (`UDungeonGeneratorSubsystem`, `ADungeoneerTileMapInstancer`)**
  - Procedural 12-node floor graphs (**10 core rooms + 2 secret Phantasm pockets** hidden behind illusory `AMirrorWall_Phantasm` seams) across *Sunken Crypt*, *Blighted Spore Fens*, *Cinder Kiln (EmberForge)*, and *Obsidian Mirror-Palace* branches.
  - Pure C++ runtime HISM geometry bucketing (`FloorHISM`, `WetFloorHISM`, `WallHISM`, `DiagonalWedgeHISM`, `MirrorHISM`, `SconceHISM`, `BreakableHISM`, `ForegroundParallaxHISM`) with camera cutaway occlusion and PerInstanceCustomData atlas selection.

- **Zombie Ecosystem & Chrono-Leech Math (`UZombieSpatialSubsystem`, `AZombieExploder`, `AZombieFastThrower`)**
  - Slow proximity-exploding zombies lured by loud environmental noise (shattering breakables, acoustic chimes, EmberFire traps).
  - Fast elemental throwers launching Fire, Poison, and Slo-Mo Chrono Gas hazards obeying strict velocity conservation (player speed loss equals zombie speed gain).

- **Living Ecosystem, Fauna & Spatial Acoustics (`UDungeonLightBudgetSubsystem`, `UDungeonWaterSubsystem`, `UDungeonFloraManagerComponent`, `UDungeonFaunaSubsystem`, `UDungeonAcousticSubsystem`)**
  - 4-tier lighting hierarchy (`512` candidates: Tier 0 emissives, `32`-slot Tier 1 shader distance-field glows in `M_Dungeoneer_EcoInstanced`, `16` pooled unshadowed Tier 2 point lights, `2` Tier 3 shadow-casting world lights).
  - Dungeon hydrology (`128 UU` O(1) surface grid, ankle-deep wading streams with `0.88x` speed modifier, wet footprints, `24` ceiling drips, weeping wall cascades, and geysers), interactive bioluminescent flora (`HISM_MagicalFlora`), and zero-allocation `FFaunaSoA` critters (`256` capacity).
  - Surface-layered deterministic foley (`42` synthesized WAVs), game-thread-safe procedural voice streaming (`UDungeoneerPCMSoundWave`), and round-robin acoustic occlusion LPF (`64` tracked walkers, `22` pooled voices).

- **Co-Op Split-Screen, Social & Atomic Trading (`UDungeoneerSocialSubsystem`, `UTradeManagerComponent`)**
  - Local split-screen viewport management (`UDungeoneerGameViewportClient`) plus `OnlineSubsystem` parties, friend lists, chat, procedural gibberish vendor/NPC audio (`UGibberishSpeechComponent`), and two-phase commit player/vendor trading.

---

## Architecture Principles

- **C++20 Primary:** Zero gameplay logic in Blueprints (`AGENTS.md`); widgets, data assets, materials, and instanced dungeon meshes are constructed and driven from C++.
- **Zero Hot-Path Allocations:** `Tick` and `TickComponent` avoid heap allocations, `GetAllActorsOfClass`, `GetFirstPlayerController`, and `FTimerHandle`, relying instead on pre-allocated entity pools, spatial subsystems, and fixed-step accumulators.
- **Deterministic Provenance & Verification:** Every procedural system uses seeded streams paired with headless UE automation tests and static architectural invariant checks.

---

## Controls

| Action | Keys / Input |
| :--- | :--- |
| **Move (8-Compass)** | `W A S D` or Arrow Keys |
| **Lantern Blade Cleave** | `Left Mouse Button`, `Space`, or `J` |
| **Dodge Dash (i-frames)** | `Left/Right Shift`, `K`, or `Right Mouse Button` |
| **Interact / Buy / Descend** | `F` or `E` |

---

## Repository Layout

```text
├── Config/                  # UE 5.8 engine, game, and Enhanced Input configs
├── Content/                 # Runtime art atlases, foley WAVs, AssetManifest.json, generated .uassets
├── Shaders/Private/         # Custom HLSL 2.5D pixel-lit shader includes (PixelLit25D.ush)
├── Source/
│   ├── DarkDungeoneer.Target.cs
│   ├── DarkDungeoneerEditor.Target.cs
│   └── DarkDungeoneer/
│       ├── Public/          # Core, AI, Audio, Dungeon, Ecosystem, Lighting, Rendering, Social, UI
│       └── Private/         # Implementations + Source/DarkDungeoneer/Private/Tests/
├── docs/                    # Design docs, devlog, tasks, and asset generation specs
├── scripts/                 # Foley synthesizer & static invariant verification tools
├── AGENTS.md                # Project coding & performance rules
├── CHANGELOG.md             # Version history
├── DarkDungeoneer.uproject  # Unreal Engine 5.8 project descriptor
├── Launch-DarkDungeoneer.bat# Windows convenience launcher
└── VERSION                  # Current semantic version
```

---

## Prerequisites

- **Unreal Engine 5.8** (Windows x64 toolchain; `Launch-DarkDungeoneer.bat` expects UE at `D:\epuc\UE_5.8` by default)
- **Visual Studio 2022 / Build Tools** with C++20 and Windows 10/11 SDK
- **Python 3.10+** (for `scripts/verify_architecture_invariants.py`, `scripts/synthesize_ecosystem_foley.py`, and sprite-slicing utilities)

---

## Building & Running

### Quick Launch (Windows)

Run the included launcher script from the repository root:

```bat
Launch-DarkDungeoneer.bat
```

It automatically launches `Binaries\Win64\DarkDungeoneer.exe` if cooked content exists, or falls back to running the project directly via `UnrealEditor.exe -game -Windowed -ResX=1280 -ResY=720`.

### Build from Command Line (PowerShell)

Compile both the Editor and Standalone Game targets via UnrealBuildTool:

```powershell
& "D:\epuc\UE_5.8\Engine\Build\BatchFiles\Build.bat" DarkDungeoneerEditor Win64 Development "$PWD\DarkDungeoneer.uproject" -WaitMutex -FromMsBuild
& "D:\epuc\UE_5.8\Engine\Build\BatchFiles\Build.bat" DarkDungeoneer Win64 Development "$PWD\DarkDungeoneer.uproject" -WaitMutex -FromMsBuild
```

---

## Testing & Verification

### 1. Static Architecture, Math & Asset Provenance Audit

Checks `Tick` allocation bans, forbidden engine symbols, compass/chrono-leech invariants, and SHA-256 provenance hashes in `Content/AssetManifest.json`:

```powershell
python scripts/verify_architecture_invariants.py
```

### 2. Unreal Headless Automation Tests

Runs all `DarkDungeoneer.*` `IMPLEMENT_SIMPLE_AUTOMATION_TEST` suites cleanly without a window:

```powershell
& "D:\epuc\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$PWD\DarkDungeoneer.uproject" -ExecCmds="Automation RunTests DarkDungeoneer; Quit" -unattended -nullrhi -nop4 -nosplash -stdout -FullStdOutLogOutput
```

---

## Further Reading

- [AGENTS.md](AGENTS.md) — Core C++20 and performance architecture rules
- [CHANGELOG.md](CHANGELOG.md) — Detailed release notes across versions
- [docs/TASKS.md](docs/TASKS.md) & [docs/DEVLOG.md](docs/DEVLOG.md) — Development milestones and technical log
- [docs/TO_GENERATE.md](docs/TO_GENERATE.md), [docs/TO_GENERATE_DUNGEON.md](docs/TO_GENERATE_DUNGEON.md), [docs/TO_GENERATE_ECOSYSTEM.md](docs/TO_GENERATE_ECOSYSTEM.md) — 4×4 sprite sheet specifications and asset pipelines
