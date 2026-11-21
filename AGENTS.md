# Dark Dungeoneer — agent guide

Top-down 2.5D co-op roguelite hack-and-slash in Unreal Engine 5.8, written in C++20.
Read `docs/DESIGN.md` for what the game is and `docs/STORY.md` for the fiction.

## Build, test, run (PowerShell, engine at `D:\epuc\UE_5.8`)

```powershell
# Editor target (needed for tests and captures)
& "D:\epuc\UE_5.8\Engine\Build\BatchFiles\Build.bat" DarkDungeoneerEditor Win64 Development "$PWD\DarkDungeoneer.uproject" -waitmutex -NoHotReload

# Static checks: banned symbols, Tick allocations, math invariants, asset hashes
python scripts/verify_architecture_invariants.py

# Headless automation tests (all DarkDungeoneer.* specs)
& "D:\epuc\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$PWD\DarkDungeoneer.uproject" -ExecCmds="Automation RunTests DarkDungeoneer; Quit" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput

# Play straight into a room (flags handled in DungeoneerGameMode.cpp)
& "D:\epuc\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "$PWD\DarkDungeoneer.uproject" -game -Windowed -ResX=1280 -ResY=720 -AutoPlaySolo -StartFloor=1 -StartRoom=Combat
```

Other capture flags: `-AutoCoop` (second local player on the same screen), `-CaptureMove=<Combo|Dodge|DashStrike|Parry|Flare|Kiln|Hook|Shade|Whirl|Decoy|Spread>`, `-StartRoom=<Combat|Mirror|Gauntlet|Sanctuary|Vendor|Vault|Boss>`,
`-RenderOffScreen -CaptureUI -CaptureShotAfterSec=2 -QuitAfterShot` (screenshots land in `Saved\Screenshots\`).

A change is done when it compiles with zero warnings (`bTreatWarningsAsErrors`), the static checks pass,
and the automation tests pass. Add or extend a spec in `Source/DarkDungeoneer/Private/Tests/DungeoneerVerificationSpecs.cpp`
for new gameplay rules.

## Hard rules

1. **No gameplay logic in Blueprints.** Gameplay, widgets (`RebuildWidget()`), materials and meshes are built in C++.
   Blueprints/assets are only for data and layout.
2. **No heap allocation in `Tick`/`TickComponent`.** Use fixed arrays (`TStaticArray`), pre-allocated pools
   (`UZombieSpatialSubsystem` owns hazard and decal pools) and the tracked player/zombie lists.
3. **Banned symbols** (enforced by the static check): `GetAllActorsOfClass`, `GetFirstPlayerController`, `FTimerHandle`.
   Use the spatial subsystem's tracked lists, iterate `ULocalPlayer`s, and use time accumulators instead of timers.
4. **One screen, several players.** Local co-op shares one full-screen view: every local controller looks through
   `ADungeoneerPartyCamera`, which frames all local heroes and leashes them (`DungeoneerParty::LeashRadiusUU`).
   Only player 1's view is drawn, so other players' HUD cards are added to player 1's screen. Keyboard events
   arrive on player 1's controller; gamepads on each player's own. Never assume there is only one hero.
5. **Server authority.** Damage, loot, inventory and trades run on the authority. Clients call a `Server_` RPC.
   Trades go through `UTradeManagerComponent`'s two-phase commit.
6. **Lights use lumens.** Call `SetIntensityUnits(ELightUnits::Lumens)` on every light, and keep counts inside the
   `UDungeonLightBudgetSubsystem` tiers. Target hardware is an Intel HD 520-class iGPU.
7. **Sprites are camera-facing planes.** `/Engine/BasicShapes/Plane.Plane`, nearest filtering, masked two-sided
   material. 64 texels = 100 UU.
8. **Asset provenance.** Every generated or modified asset is listed with its SHA-256 in `Content/AssetManifest.json`.
   Gameplay sounds fire from step/frame accumulators, not timers.
9. **Movement is 8-directional.** Input snaps to the nearest compass octant; diagonals move at the same speed as
   cardinals. Aim is separate from movement.

## Code map (`Source/DarkDungeoneer/`)

| Folder | What lives there |
| --- | --- |
| `Core/` | Hero (`ADungeoneerCharacter`: input, combo, dodge, parry, heavy smash), movement component, party camera (shared co-op view and leash), game mode (floors, CLI flags), game state, save game, asset bootstrap (PNG/WAV import and sprite slicing at startup) |
| `Combat/` | `UDungeoneerSkillComponent`: two skill slots, cooldowns, skill effects, skill table |
| `AI/` | Zombie base class (status effects: stun, burn, time slow), Exploder, Thrower, Mirror Stalker, Cinder Colossus boss, hazards, spatial hash and pools |
| `Dungeon/` | Floor graph generator, tile/HISM instancer and room encounters, loot pickups (incl. skill scrolls), mirror maze actors |
| `Ecosystem/` | Water, flora and fauna systems |
| `Rendering/`, `Lighting/`, `Audio/` | Sprite materials, sensory perks, light budget, foley and gibberish voice |
| `Social/` | Inventory, trading, online party/friends |
| `UI/` | Menus and HUD, all built in C++ |
| `Tests/` | Automation specs |

## Skills for agents

`.claude/skills/` has task guides: sprite-sheet slicing (`sprite-sheet-25d-cutter`, includes `slice_normal_rig.py`),
dungeon generation, compass movement, zombie AI, and voice/gibberish audio.

## Writing style

Plain, specific English in comments, docs and commit messages. Say what the code does and why; skip adjectives
like "cleanly", "strictly", "robust". Player-facing text should sound like the game world (see `docs/STORY.md`),
not like engineering notes.
