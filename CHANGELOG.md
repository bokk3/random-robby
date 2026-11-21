# Changelog

All notable changes to **Dark Dungeoneer (UE 5.8)** are documented in this file.
The project adheres to [Semantic Versioning 2.0.0](https://semver.org/) and [Conventional Commits](https://www.conventionalcommits.org/).

---

## [Unreleased]

Work toward 1.6.0 (plan milestones M0, M1 and most of M2).

### Added (enemies that fight back, M3)
- Shared telegraphed melee for every zombie (`FZombieMeleeAttackDef`, `AZombieBase::TryStartMeleeAttack`):
  wind-up with a closing floor ring, red glow and a growl (`Combat_Windup_0`), then a lunge and one hit test,
  then a recovery opening. Strikes go through the hero's damage path, so parry and perfect dodge work on them.
- Attack tokens in `UZombieSpatialSubsystem` (two attackers per hero at a time), separation steering, and
  poise (hits cancel small enemies' wind-ups; stuns cancel any).
- Exploder: two shoulder-barges before the fuse (or the fuse early when badly hurt).
- Thrower: the flask is a visible projectile with a wind-up and a landing marker; parrying it in flight sends
  it back. New knife slash at close range.
- Mirror Stalker: the per-tick drain is now a parryable lunge-bite.
- Colossus: the slam shows its full area as a closing floor ring, with a warning growl.
- Elites (seeded, about 1 in 6) and per-floor health/damage scaling.
- `AI.EnemyTelegraphParryAndTokens` and `AI.ThrowerFlaskFlightDeflect` automation tests.

### Added (hero animation, M6)
- Five hero move sheets (combo, heavy, evade, skills, hurt/idle), generated with Gemini, cleaned and sliced
  into `Content/Art/Rigs/DungeoneerHeroMoves/`. The hero now shows real frames for every move: combo wind-up,
  swing, follow-through and recover; heavy charge, raise and slam (also the combo finisher); tucked dodge roll;
  dash lunge; parry brace; flask throw; hook cast; lantern spin; flare; flinch; downed; and an idle breath.
  `ADungeoneerCharacter::ResolvePoseFrame` picks the sheet and frame; a missing sheet falls back to walk frames.
- `Rendering.HeroMoveSheetFrames` automation test.

### Added (game feel, M2)
- `M_Dungeoneer_Sprite`: the character material, built in C++ (`FDungeoneerSpriteMaterialFactory`). Hit
  flash, warm glow (Exploder fuse) and a dithered fade-out, all driven by custom primitive data
  (`DungeoneerSpriteCPD`), so they survive walk-frame material swaps. Unset slots mean "normal".
- `M_Dungeoneer_FxAdditive` and `UDungeoneerFxPoolSubsystem`: 96 pooled additive quads for hit sparks, death
  bursts, blast rings, flare and heavy-smash rings, perfect-dodge rings and dodge dust. No Niagara assets needed.
- The slash arc and skill beam glow (additive) instead of reading as dark smears.
- Hit-stop on struck zombies (40-110 ms, longer for heavy hits and parries), stacking with time slow.
- Screen shake on the party camera from hero "trauma" (blasts, heavy smash, flare, taking damage).
- Taking damage: red sprite flash, red screen wash, a damage number and a camera kick.
- Damage numbers pop in large and settle; crits stay bigger.
- Combat sound bank: 21 procedural WAVs (`scripts/synthesize_combat_foley.py`) for swings, hits, heavy
  smash, parry, perfect dodge, hero hurt, zombie death, fuse ticks, blasts, flask shatter, gas and one
  stinger per skill, all fired from gameplay events.
- Slips dither in and out of the light instead of shrinking; afterimages and Slipped Self look like
  silvery reflections.
- Full-resolution rendering (`r.ScreenPercentage 100`, was 75) so pixel art stays crisp.
- `Combat.ImpactFeedback` automation test.

### Changed
- Local co-op shares one screen. `ADungeoneerPartyCamera` frames every local hero, zooms out as they spread
  (arm 1320 to 1750 UU) and leashes them to 820 UU apart; a downed hero is the anchor. Split-screen is gone.
- Player 2's card sits on player 1's HUD (top-right). Popups from both heroes draw on the one screen.
- Mouse aim now points at the spot under the cursor instead of using the screen centre.
- Player 2's keyboard keys are read from player 1's controller, where UE delivers them.
- A hero who bleeds out with a partner standing gets up beside the partner, not at the entry room.
- Slash and spin effects use a crescent texture instead of a flat rectangle.
- Slipped Self steps the hero 160 UU back out of the copy so the copy is visible.
- HUD: removed the move/aim, slow-ratio and sense-tier debug lines; slow and senses only show when they apply.
- Relic names, boss kill text and run-summary banners use the STORY.md names.
- Menu: "BRING A SECOND LANTERN" replaces the split-screen entry. `-AutoCoop` replaces `-AutoSplitScreen`.

### Fixed
- Starting or retrying a run now resets gold, relics, consumables, skills and senses.
- Secret pockets no longer spawn a second vault (and on floor 3 a second Colossus); they hold a Stalker,
  a relic and a scroll, and picking up the relic counts as a secret found.
- Vaults no longer give out Last Sunset (the Colossus relic) on floor 1.
- The Ledger bestiary records the actual enemy type killed.

### Added
- `-CaptureMove=<move>` for screenshot runs, and the `Coop.SharedCameraFramingAndLeash` automation test.
- Copy-ready image-generator prompts for 14 new sprite sheets in `docs/TO_GENERATE.md`.

## [1.5.0] - 2026-11-20

The game now plays as a run-based hack-and-slash, and the project docs were rewritten in plain language.

### Added
- **Combat kit.** The combo is now four hits with a stunning finisher, and presses during a swing are buffered.
  Hold attack to charge a heavy smash. Attack out of a roll for a piercing dash strike. Getting hit by a zombie
  mid-roll is a perfect dodge (slows nearby zombies, refunds the roll, empowers the next hit). New parry
  (L / V / MMB / B): stuns melee attackers and bats Thrower flasks back at them.
- **Skills.** `UDungeoneerSkillComponent` with two slots and six skills: Bottled Kiln (fire flask), Lamplighter's
  Hook (grapple), Their Own Shadow (blink-backstab), Censer's Wheel (spin), Borrowed Noon (the old charged flare)
  and Slipped Self (noise decoy). Skill scrolls drop in about a third of combat rooms and in every secret pocket.
- **Enemy status effects.** Zombies can be stunned, burned and time-slowed. The boss takes 25% of stun durations.
- **Gamepad support** for both players, plus a cleaner polled-input path.
- **HUD** shows equipped skills, cooldowns and the empowered-hit marker.
- **Story and world bible** in `docs/STORY.md`; game design in `docs/DESIGN.md`.
- Automation spec `DarkDungeoneer.Combat.HackAndSlashKit`.

### Changed
- `R` is now skill slot 2 (Borrowed Noon by default) and `Q` skill slot 1; oil moved to `1`.
- Player-facing strings no longer use engineering terms ("P2P", "2PC", "terminal vault").
- Agent setup moved from `.agents/` to `AGENTS.md` (+ `CLAUDE.md`) and `.claude/skills/`. The old rule files were
  merged into `AGENTS.md` and trimmed to what the code actually does.
- Completed plans, the task tracker and the devlog moved to `docs/archive/`.

---

## [1.4.0] - 2026-11-16

### Added
- **2-Player Local Split-Screen Drop-In, Downed Revive & Shared Party Stash (`ADungeoneerGameMode`, `ADungeoneerGameState`, `ADungeoneerCharacter`, `UZombieSpatialSubsystem`)**:
  - Wired `EDungeoneerMenuAction::CoopSplitScreen` and runtime `[F2]` drop-in (`RequestSplitScreenCoopJoin()`) spawning Player 2 on an isolated `ULocalPlayer` split viewport with independent `UDungeoneerPlayerHUDWidget`, distinct Lantern Trim tint, and dedicated Arrow / `RightCtrl` / `RightShift` / `Backslash` / `P` controls.
  - Added `18.0s` Co-Op Downed Bleeding when living allies remain (`GetAliveOtherPlayerCount > 0`) and `[F]` / `Backslash` Allied Revive (`ReviveByAlly` restoring `50%` HP, `>= 30%` battery, and incrementing `RevivesPerformed`).
  - Added replicated Shared Party Stash (`ReplicatedPartyStashGold` and `8`-slot `PartyStashItems` with `AuthorityDepositGoldToStash`, `AuthorityWithdrawGoldFromStash`, `AuthorityDepositItemToStash`, `AuthorityWithdrawItemFromStash`) on `ADungeoneerGameState`.
- **P2P Two-Phase Commit Supply Swap & Lobby Framing Polish (`ADungeoneerCharacter`, `UDungeoneerCoopLobbyWidget`)**:
  - Attached `UTradeManagerComponent` directly onto `ADungeoneerCharacter` and bound `[T]` / `[P]` nearby-ally P2P supply swaps (`TriggerAllyTradeWithNearest()`) executed atomically through `UTradeManagerComponent::AuthorityExecuteTwoPhaseCommit`.
  - Framed `UDungeoneerCoopLobbyWidget` in a matching obsidian-and-gold border (`400x168`) with calibrated `13pt` / `11pt` typography eliminating right-edge clipping in `1280x720`.
- **Persistent Meta-Ledger, Run Summary Modal & Daily Seed (`UDungeoneerSaveGame`, `UDungeoneerRunSummaryWidget`, `UDungeoneerMainMenuWidget`)**:
  - Added `UDungeoneerSaveGame` persisting unlocked Bestiary mask (`4/4`), Relic compendium (`5/5`), Highest Floor, Fastest Clear duration, `EquippedLanternTrimIndex` (`0..3` flame/light tints), and deterministic `ComputeDailySeedFromDate(Year, Month, Day)`.
  - Added obsidian-and-gold `UDungeoneerRunSummaryWidget` modal (`ShowRunSummaryOverlay`) displaying Victory/Defeat banner, Floor, Clear Time, Kills, Gold, Relics, Revives, Secrets, Seed telemetry, Meta-Ledger records, `[RETRY SAME SEED]`, and `[RETURN TO MENU]`.
  - Bound Main Menu `Sensory Perks & Loadout` to toggle deterministic Daily Seed mode and `Options & Display` to cycle saved Lantern Trims live in the Diorama.
- **Packaging Configuration & Launcher Readiness (`Config/DefaultGame.ini`, `Launch-DarkDungeoneer.bat`)**:
  - Added `[/Script/UnrealEd.ProjectPackagingSettings]` (`BuildConfiguration=PPBC_Shipping`, `Art`/`Audio`/`Materials` staging dirs, `/Engine/Maps/Entry`) and updated `Launch-DarkDungeoneer.bat` to prioritize staged builds cleanly.

### Changed & Verified
- Updated `scripts/verify_architecture_invariants.py` (`112` tracked files verified clean: zero `Tick` allocations, zero banned symbols, math invariants & `Content/AssetManifest.json` SHA-256 valid) and `slice_normal_rig.py --self-test`.
- Added 4 Phase-2 automation specs in `DungeoneerVerificationSpecs.cpp` (`SplitScreenDropInAndAllyRevive`, `SeedParityPredictionAndP2PTrade`, `SaveGameRoundTripAndRunSummary`, `DrawCallBudgetAndZeroTickAlloc`).
- Compiled `DarkDungeoneerEditor Win64 Development`, `DarkDungeoneer Win64 Development`, and `DarkDungeoneer Win64 Shipping` with zero project warnings (`bTreatWarningsAsErrors = true`), passed all `24 / 24` `DarkDungeoneer.*` headless automation tests (`EXIT CODE: 0`), and visually verified live `after_v1_4_0_menu.png` & `after_v1_4_0_splitscreen.png` captures.

---

## [1.3.0] - 2026-11-16

### Added
- **2.5D Rigs & Sanctuary Prop Pixel-Art Pipeline (`scripts/synthesize_v13_art.py`, `Content/AssetManifest.json`)**:
  - Generated and sliced crisp 4x4 semi-pixelated sprite rigs (`Rig_ZombieMirrorStalker_Raw`, `Rig_BossGuardian_Raw`) passing `--validate-stride` (`47.9%–52.4%` lower-body stride variance) alongside `PROP_Resting_Ground_Sanctuary_4x4` (Sanctuary Brazier, Third-Eye Sensory Monolith, Apothecary Merchant Stall, and Overflowing Vault Chest), importing `24` rig textures across 8 registered rigs cleanly in `UDungeoneerAssetBootstrapSubsystem`.
- **Menu Diorama Cutouts, Gothic Main Menu & Tactical HUD (`AMenuDioramaStageActor`, `UDungeoneerMainMenuWidget`, `UDungeoneerPlayerHUDWidget`)**:
  - Replaced prototype cylinder/cube diorama meshes on `AMenuDioramaStageActor` with animated camera-facing masked 2.5D sprite planes (`Exploder`, `Thrower`, `MirrorStalker`, `Mirror_Intact`).
  - Styled `UDungeoneerMainMenuWidget` with an obsidian slate & gold-trimmed gothic panel and active selection highlights.
  - Upgraded `UDungeoneerPlayerHUDWidget` with crimson HP & amber Lantern Oil `UProgressBar`s, a `MaxRooms = 16` zero-allocation custom `OnPaint` Floor Minimap (`SDungeoneerMinimapWidget`), automatic top-center Boss Health Plate, Bottom Supplies/Relics/Interact Dock, and `32`-slot pooled world-to-screen Floating Combat Popups (`SDungeoneerFloatingPopupWidget`).
- **Relic Synergies, Quick Consumables & World Drops (`UDungeoneerInventoryComponent`, `DungeonEncounterActors.h/.cpp`)**:
  - Added replicated passive Relics (`SunGlassLens` +8° cone & +35% bloom cleave damage, `ChronoWardBoots` 0.96x wading & halved ChronoSlow, `EchoTunedBlade` stun waves, `SolarFlashlightCore` +20% damage, `VampireWick` +6 HP/hit) and quick-use Consumables (`[Q]` `LanternOilPhial` +45% battery, `[C]` `AntidoteDraught` +35 HP & cleanses ChronoSlow).
  - Upgraded `ARestingGroundActor`, `AVendorActor`, and `AMajorLootVaultActor` from primitive blocks to masked 2.5D Sanctuary/Door sprite cutouts, wired Sanctuary Sensory Altar perk upgrades (`90g`), and added collectible `ADungeoneerLootPickupActor` drops in Secret Phantasm, Mirror-Palace, and Combat rooms.
- **Stealth Mirror Stalker & Floor 3 Guardian Boss (`AZombieMirrorStalker`, `ABossGuardianActor`)**:
  - Added `AZombieMirrorStalker` (310 UU/s cloaked stalker revealed by flashlight cones or Dark Vision `>= 2`).
  - Added `ABossGuardianActor` (*Cinder Colossus* guarding every 3rd-floor `AMajorLootVaultActor`, alternating Phase 1/2 radial 8-compass slams, `Fire`/`ChronoGas` pooled mortar hazards, pre-allocated Phase 2 dormant `AZombieExploder` adds, and guaranteed `SolarFlashlightCore` Vault unlock).
- **3-Hit Combo, Charged Lantern Flare & Chain Detonations (`ADungeoneerCharacter`, `UZombieSpatialSubsystem`, `ADungeoneerTileMapInstancer`)**:
  - Implemented buffered 3-hit Lantern Blade combos (`1.0x -> 1.15x reverse arc -> 1.45x lunge`), `[R]` Charged Lantern Flare (`25%` battery radial stun & room-wide `PrismLily` bloom via `IgnitePrismLiliesInRadius`), `35 ms` hit-stop dilation, critically damped `CameraBoom` micro-impulse, and `EmberFire`/radial chain ignition of adjacent breakables & `AZombieExploder` fuses.

### Changed & Verified
- Updated `scripts/verify_architecture_invariants.py` (`108` tracked core/subsystem/tool files verified clean: zero `Tick` heap allocations, zero banned symbols, math invariants & `AssetManifest.json` SHA-256 valid).
- Added 4 headless automation specs in `DungeoneerVerificationSpecs.cpp` (`RelicsConsumablesAndSanctuaryPerks`, `MirrorStalkerLightRevealAndBossPhases`, `ComboChargedFlareAndChainReactions`, `MinimapPopupsAndCutoutDiorama`); compiled `DarkDungeoneerEditor` and `DarkDungeoneer` (`Win64 Development`, zero project warnings), passed all `20 / 20` `DarkDungeoneer.*` tests (`EXIT CODE: 0`), and visually inspected live `-CaptureUI` Entry Room & Main Menu screenshots.

---

## [1.2.1] - 2026-11-16

### Added
- **Living Dungeon Ecosystem (`v1.2.0` M1–M3 Architecture)**:
  - **4-Tier Light Budget & Eco Shader (`UDungeonLightBudgetSubsystem`, `FDungeoneerEcoMaterialFactory`)**: Generated `/Game/Materials/M_Dungeoneer_EcoInstanced` (`EcoMatRev6`) with 8-float `EEcoCPD` per-instance custom data (`UVCell`, `Emissive`, `HueR`, `HueG`, `HueB`, `EventTime`, `ParamA`, `KindFlags`) and 32-bit `UnpackRGB24` shader distance-field glows (`MaxTier1 = 32` in `MPC_SensoryState`, `MaxTier2 = 16` unshadowed pooled lights, `MaxTier3World = 2` shadow-casting world lights, `MaxCandidates = 512`).
  - **Dungeon Hydrology (`UDungeonWaterSubsystem`)**: Added an `O(1)` `128 UU` surface grid (`EDungeonSurfaceKind`), ankle-deep wading streams (`0.88x` movement speed with `FSavedMove_Dungeoneer::bSavedWading` replication sync and boot occlusion offset), `24` seeded ceiling drips, `256`-slot `ISM_WaterFX` pool (wet footprints, boot/drip ripples, floating petals, weeping wall cascades, geysers), and `HISM_Water`.
  - **Interactive Flora (`UDungeonFloraManagerComponent`)**: Added `HISM_MagicalFlora` with photosensitive Prism-Lilies (blooming + `15s` Tier 2 light leases under flashlight or mirror-bounced beams), CSR BFS Pulse-Moss waves, Spore-Cap puffs, and serial-gated Echo-Crystal acoustic chords.
  - **SoA Critter Fauna (`UDungeonFaunaSubsystem`)**: Added pre-allocated `FFaunaSoA` (`Capacity = 256`) supporting `LumenMoth`, `LanternBeetle`, `PuddleNewt`, and `CryptCrab` via `ISM_Fauna` (`30 Hz` batch transform updates, sprint scatter, lantern roosting, and decoy `0.25f` puddle ripples).
  - **Surface-Layered Acoustics & Foley Pipeline (`UDungeonAcousticSubsystem`, `UDungeoneerPCMSoundWave`, `synthesize_ecosystem_foley.py`)**: Synthesized `42` deterministic `22.05 kHz` 16-bit mono WAV foley stems logged in `Content/AssetManifest.json`, drove Layers A/B/C via `FOnWalkColumnChanged` (`MaxWalkers = 64`, `TotalVoicePoolSize = 22` pooled voices, round-robin occlusion LPF), and implemented `UDungeoneerPCMSoundWave` for game-thread-safe procedural audio without `SoundWave.cpp` off-thread assertions.
- **2.5D Visual Overhaul & Release Integration (`v1.2.1` M4)**:
  - **Environment Atlases & De-Haloing (`slice_normal_rig.py`, `Content/Art/`)**: Generated pixel-art dark dungeon cobblestone/wall/archway atlases (`ATLAS_Dungeon_Floors_4x4`, `PROP_Dungeon_Walls_And_Cutaways_4x4`, `PROP_Doors_And_Level_Transitions_4x4`), upgraded `eco_cleanup_masks.json` flood-fill sticker-halo stripping, and replaced blocky procedural water waves with layered sine-ripple indigo streams and upright Entry cascades.
  - **Isometric `-45°` Diorama Camera, Exposure & Light Calibration (`ADungeoneerCharacter`, `AMenuDioramaStageActor`, `ADungeonAtmosphereActor`)**: Framed gameplay and Menu Diorama cameras at `Pitch = -42°, Yaw = -45°, FOV = 35°` with clamped auto-exposure (`0.42f`–`1.40f`), warm lantern `ELightUnits::Lumens` calibration (`650 lm` flashlight / `120 lm` fill in gameplay; `480 lm` / `95 lm` in `AMenuDioramaStageActor`), and violet-obsidian atmospheric fog.
  - **HISM Cluster Builds & Deterministic Screenshot Hook (`ADungeoneerTileMapInstancer`, `ADungeoneerGameMode`)**: Rebuilt HISM cluster trees and bounds (`BuildTreeIfOutdated`, `UpdateBounds`, `MarkRenderStateDirty`) across all floor/wall/water buckets, sealed room corners around tightened doorways, and added Tick-based `-CaptureShotAfterSec` / `-QuitAfterShot` capture flags (zero `FTimerHandle`).

### Changed & Verified
- Updated `scripts/verify_architecture_invariants.py` (`101` core/subsystem/tool files audited; zero `Tick` heap allocations, zero banned symbols, math invariants & SHA-256 `Content/AssetManifest.json` valid).
- Added 6 ecosystem automation specs in `Source/DarkDungeoneer/Private/Tests/DungeoneerVerificationSpecs.cpp` (`CustomDataPackingAndRGB24`, `LightBudgetTierCaps`, `FloraBloomMossWaveAndCrystalNoFeedback`, `FaunaSoAZeroAllocScatterNewtNoise`, `WaterSurfaceGridAndWadingDeterminism`, `AcousticSurfaceLayers`).
- Passed `slice_normal_rig.py --self-test`, rebuilt `DarkDungeoneerEditor` and `DarkDungeoneer` (`Win64 Development`, zero project warnings), passed all `16/16` headless UE `5.8.2` `DarkDungeoneer.*` automation tests (`EXIT CODE: 0`), and visually verified live Entry Room & Main Menu screenshots.

---

## [1.1.0] - 2026-11-15

### Added
- **Multi-Biome Procedural Floor Progression & Tactical Room Shapes (`DungeonTypes.h`, `UDungeonGeneratorSubsystem`)**:
  - Added `EDungeonBiomeType` (`Crypt`, `MirrorPalace`, `SporeFens`, `EmberForge`), `EDungeonRoomShape` (`SquareChamber`, `OctagonalRotunda`, `PillaredGallery`, `CrossAmbushVault`, `SecretPhantasmPocket`), transition-bleed fields (`BleedBiomeType`, `bHasDescendingStairs`), and `ResolveFloorDominantBiome` / `ResolveNextFloorBiome`.
  - Upgraded `GenerateFloorGraph` to **10 core tactical rooms + 2 `SecretPhantasmPocket` rooms** (`12` nodes total), pairing Floor 1 (*Sunken Crypt*), Floor 2 (*Blighted Spore Fens*), and Floor 3+ (*Cinder Kiln*) with an *Obsidian Mirror-Palace* branch, Sanctuary & Vendor nodes, ~25% next-biome transition bleed on the final 2 nodes before the descending stairwell, and illusory `AMirrorWall_Phantasm` secret seams.
- **Full `4x4` Sheet Extraction & Procedural Fallback Assets (`UDungeoneerAssetBootstrapSubsystem`)**:
  - Added batch single-decode `ExtractAllGridCellsFromPng` extracting all `16` cells (`Cell0..Cell15`) across existing 4x4 dungeon and rig atlases while preserving existing aliases.
  - Registered `PROP_Doors_And_Level_Transitions_4x4` (`Doors_Arch_D/N`, `Doors_Sconce_D/N`, `Doors_Stairs_D/N`) and `PROP_Resting_Ground_Sanctuary_4x4` (`Sanctuary_Brazier_D/N`, `Sanctuary_Rune_D/N`) with crisp procedural `64x64` pixel-art fallback synthesis (`RegisterProceduralTileFallback`) when optional PNGs are absent on disk.
- **8-HISM Bucketing & Systemic Floor Traps (`ADungeoneerTileMapInstancer`, `AZombieExploder`, `AZombieFastThrower`)**:
  - Expanded HISM rendering buckets to `8` components (`FloorHISM`, `WetFloorHISM` low-roughness puddles, `WallHISM`, `DiagonalWedgeHISM` 45° corners, `MirrorHISM`, `SconceHISM`, `BreakableHISM`, `ForegroundParallaxHISM` overhead beams/chains at `Z = +260 UU`) plus a pre-allocated `4`-slot shadowless flickering `SconceLightPool`.
  - Added interactive `FDungeoneerTrapVentEntry` floor hazards (`Steam`, `EmberFire`, `PoisonSpore`, `ChronoGas`, `AcousticChime`) via `RegisterTrapVentInstance` and `EvaluateTrapVentsForEntities` affecting both players and zombies; `EmberFire` vents ignite or accelerate `AZombieExploder::IgniteOrAccelerateFuse`.
  - Upgraded breakables (`1.95f` loudness, `1350 UU` radius) and `AcousticChime` wires (`2.35f` loudness, `1500 UU` radius) to lure nearby `AZombieExploder` and `AZombieFastThrower` enemies toward the sound origin via `UDungeoneerSensoryWorldSubsystem::QueryStrongestActivePulse`.
- **Zero-Allocation Biome Atmosphere & Subtle Eye Candy (`ADungeonAtmosphereActor`, `AVendorActor`, `ADungeoneerGameMode`)**:
  - Added pre-allocated `64`-instance `AmbientMotesHISM` around player 0 + smooth `0.6s` volumetric fog blending across dungeon biomes and Sanctuary safe zones, wired into `ADungeoneerGameMode`, alongside subtle `AVendorActor` lantern sway.

### Changed & Verified
- Added `DarkDungeoneer.Dungeon.BiomesOctagonalRoomsTrapsAndExploderLure` in `Source/DarkDungeoneer/Private/Tests/DungeoneerVerificationSpecs.cpp` (and updated bootstrap extraction & 12-node DAG assertions).
- `python scripts/verify_architecture_invariants.py` passed (`85` files verified, zero `Tick` allocations, zero banned symbols, math invariants & `AssetManifest.json` valid).
- All `10` headless UE `5.8.2` automation tests passed (`UE_TEST_EXIT=0`, `Result={Success}` for all 10 specs).

---

## [1.0.0] - 2026-11-15

### Added
- **Player Combat, Evasion & Procedural 2.5D Animations (`ADungeoneerCharacter`)**:
  - Added **Lantern Blade Cleave** (`TriggerPrimaryAttack` on `LeftMouseButton`, `SpaceBar`, `J`, or Enhanced Input `AttackAction`) sweeping a 130° melee cone (`UZombieSpatialSubsystem::ApplyMeleeArcDamage`) along `AimOctant`, shattering nearby breakables (`EvaluateEntityStepOnBreakables`), emitting an Acute Hearing acoustic ripple (`BroadcastNoiseEvent`), driving pre-allocated `SlashArcMesh` arc visuals + warm lantern flash + torso lunge/follow-through animation (`Col 1 -> Col 3`), and locking damage/pose against downed/staggered states.
  - Added **4-Axis Dodge Dash** (`TriggerDodgeDash` on `LeftShift`, `RightShift`, `K`, `RightMouseButton`, or `DodgeAction`) with `0.22s` invulnerability frames (`IsInvulnerable()`), clean horizontal dash impulse along the active movement/aim octant, and procedural squash/hop stride frames.
  - Added **Contextual Interaction & Dungeon Progression Loop** (`TriggerInteract` on `F`, `E`, or `InteractAction`): unlocks `AMajorLootVaultActor` vaults with seeded rewards (`+120` gold, high-tier relic item, `+25 HP` / `+30% Battery`), purchases seeded supplies & upgrades sensory perks at `AVendorActor` stalls (`Item_Lens_DarkVision`, `Item_Resonator_AcuteHearing`, `Item_Vial_ScentTracer`), and descends unlocked Vaults into the next deterministic procedural floor (`ADungeoneerGameMode::AdvanceToNextDungeonFloor()`).
  - Added player **Hit Stagger** knockback/shake and **Downed Respawn** flow (auto-revives with `65%` HP / `>= 50%` Battery back at the floor's `EntrySpawnLocation` without hard-locking runs).
- **Enemy Hit Feedback, Knockback & Slay Rewards (`UZombieSpatialSubsystem`, `AZombieBase`, `AZombieFastThrower`)**:
  - Added `UZombieSpatialSubsystem::ApplyMeleeArcDamage` (`GetTrackedZombieCount`, `GetTrackedZombieAt`) with zero `Tick` allocations or `GetAllActorsOfClass` calls.
  - Added `AZombieBase::TriggerHitFeedback` (knockback impulse, bright white hit flash, scale punch) and death collapse (`DeathHideCountdown = 0.65s`), granting gold (`+25g` Exploder / `+35g` Fast Thrower) and `+8%` flashlight battery siphon to the slaying hero (`RegisterEnemySlain`).
- **Live Split-Screen Combat HUD & Floor Loop (`UDungeoneerPlayerHUDWidget`, `ADungeoneerGameMode`)**:
  - Extended `UDungeoneerPlayerHUDWidget` with `Txt_CombatAndLoopReadout` (`FLOOR N | GOLD X | KILLS Y | [READY/SLASH!/DODGE!/STAGGER/DOWNED]`) and contextual `Txt_InteractHint` (`[F] UNLOCK TERMINAL LOOT VAULT`, `[F] DESCEND TO NEXT FLOOR`, `[F] BUY SUPPLY FROM VENDOR`).
  - Wired `ADungeoneerGameMode::EnterLiveDungeonFromMenu` to attach per-`ULocalPlayer` `ActivePlayerHUDWidgets` cleanly when entering the dungeon and added `ADungeoneerGameMode::AdvanceToNextDungeonFloor()` (`Floor N -> N+1`, deterministic seed progression, `RestingGroundBounds.Reset()`, encounter cleanup & respawn).

### Verified
- Expanded `Source/DarkDungeoneer/Private/Tests/DungeoneerVerificationSpecs.cpp` with `DarkDungeoneer.Gameplay.CombatDodgeVaultAndFloorLoop` verifying directional melee cone cleave, out-of-cone immunity, kill gold/battery rewards, dodge i-frames blocking incoming damage, HUD Floor/Gold/Kills telemetry, and `AdvanceToNextDungeonFloor()` Floor `1 -> 2` rebuilding.
- `python scripts/verify_architecture_invariants.py` passed (`85` files verified, zero `Tick` allocations, zero banned symbols, math invariants & `AssetManifest.json` SHA-256 valid).
- `DarkDungeoneerEditor` (`Win64 Development`) and `DarkDungeoneer` (`Win64 Development`, `DarkDungeoneer.exe`) compiled cleanly via `UnrealBuildTool`.
- All `9` headless UE `5.8.2` automation tests passed (`UE_TEST_EXIT=0`, `TEST COMPLETE. EXIT CODE: 0`).

---

## [0.9.7] - 2026-11-15

### Added
- **Standardized 4x4 (16-Cell) Walk-Cycle Kinematic Spec & Prompt Template (`.agents/skills/sprite-sheet-25d-cutter/SKILL.md`, `docs/TO_GENERATE.md`)**:
  - Embedded strict 4-Row (`0: South/SE`, `1: North/SW`, `2: East/NE`, `3: West/NW`) x 4-Column (`0: Contact / Idle Pose A`, `1: Full Left Step`, `2: Passing Pose B`, `3: Full Right Step`) specification and reusable generation prompt template so future sprite sheets never duplicate legs across columns.
- **Automated Stride Validation & Procedural Repair (`slice_normal_rig.py`)**:
  - Added `--validate-stride`, `--repair-stride`, and `--stride-threshold` (`0.15` default) CLI flags measuring lower-third leg bounding-box IoU, MSE, and foreground pixel variance between Column 1 (`Full Left Step`) and Column 3 (`Full Right Step`), recording `StrideValidation` metrics inside `AtlasMetadata.json`.
  - When a row fails with `< 15%` lower-body pixel variance and `--repair-stride` is enabled, automatically synthesizes alternating `[Col 0 Idle/Contact A, Col 1 Left Step, Col 2 Passing B, Col 3 Right Step]` poses via upper/lower-third splitting, horizontal/depth leg shear, torso height bobbing, and lower-body mirror warping while preserving asymmetrical upper-body props.
  - Added `EFoot::Left` / `EFoot::Right` and `OnFootstepContact` multicast delegate on `U25DAnimOrchestratorComponent` alongside `F25DStepContactEvent::Foot`.

### Changed
- **Re-Indexed `4x4` Sprite Atlases & Synchronized C++20 Runtime Column Playback**:
  - Re-ordered all 6 character/zombie sheets (`DungeoneerHero`, `ZombieExploder`, `ZombieFastThrower`, `Hero`, `Exploder`, `Thrower`) into `[Col 0 Contact/Idle A, Col 1 Full Left Step, Col 2 Passing B, Col 3 Full Right Step]`, re-sliced `_D`/`_N`/`_E`/`_ORM`/`AtlasMetadata.json`/`Content/AssetManifest.json`, and verified all rows pass `--validate-stride` (`22.4%–84.5%` lower-body variance).
  - Updated `U25DAnimOrchestratorComponent::AdvanceWalkDistance`, `ADungeoneerCharacter::AdvanceWalkFlipbookByDistance`, and `AZombieBase::AdvanceWalkFlipbookByDistance` to lock to **Column 0** when `Velocity.IsNearlyZero()` (idle), advance via `FMath::FloorToInt((AccumulatedDistance / StrideLength) * 4.0f) & 3` while moving, and trigger `OnFootstepContact(EFoot::Left)` on **Column 1** and `OnFootstepContact(EFoot::Right)` on **Column 3** + Acute Hearing fog ripples.

### Verified
- `python .agents/skills/sprite-sheet-25d-cutter/scripts/slice_normal_rig.py --self-test` passed Rig, Stride Validator/Repair (`0.0% -> 50.3%–69.4%`), and Dungeon modes.
- `python scripts/verify_architecture_invariants.py` passed (`85` files, `AssetManifest.json` SHA-256 valid).
- Updated `DungeoneerVerificationSpecs.cpp` (`HUDAndTradeModalAndAssetBootstrap` and `25DAnimOrchestratorAndAtlasMetadata`) to validate `Col 0` idle locking and alternating `EFoot::Left` (`Col 1`) -> `EFoot::Right` (`Col 3`) step events.
- `DarkDungeoneerEditor Win64 Development` (`8` actions) and `DarkDungeoneer Win64 Development` (`7` actions) compiled cleanly; all `8` headless UE `5.8.2` automation tests passed (`UE_TEST_EXIT=0`).

---

## [0.9.6] - 2026-11-15

### Fixed
- **Alternating 4-Frame Walk-Cycle Sprite Sheets (`DungeoneerHero`, `ZombieExploder`, `ZombieFastThrower`)**:
  - Replaced near-static duplicate stride poses in `Content/Art/Rigs/{DungeoneerHero,ZombieExploder,ZombieFastThrower}/Rig_*_Raw_D.png` with distinct 4-frame alternating locomotion cycles (`Col 0: Contact-L -> Col 1: Upright Passing A -> Col 2: Contact-R -> Col 3: Upright Passing B`) including opposing arm/lantern/chain counter-swing, hip bob, and front/back leg depth separation across all 4 directional rows.
  - Fixed `Rig_ZombieFastThrower_Raw_D.png` missing cardinal directions (adding authentic skull-masked South Front and North Back sprint rows alongside East & West sprints) and removed the purple glow halo around the raised flask.
  - Re-sliced `_D`, `_N`, `_E`, `_ORM`, `AtlasMetadata.json`, `rig_sockets.json`, and updated SHA-256 hashes in `Content/AssetManifest.json` via `slice_normal_rig.py`.
- **Distance-Locked 16-Cell Runtime Flipbook Stepping (`DungeoneerAssetBootstrapSubsystem`, `ADungeoneerCharacter`, `AZombieBase`)**:
  - Updated `UDungeoneerAssetBootstrapSubsystem::ExtractDefaultSingleCellTiles` to extract all `16` cells (`Row 0..3`, `Col 0..3`) with corresponding silhouette masks (`SpriteMaskTextures`) for `Hero`, `Exploder`, and `Thrower`, plus backward-compatible `*_Row{0..3}_*` and `*_Cell0_*` aliases (`136` cached entries total).
  - Expanded `DirectionalHeroMIDs` (`ADungeoneerCharacter`) and `DirectionalZombieMIDs` (`AZombieBase`) to pre-allocate all `16` MIDs at initialization (zero `Tick` heap allocations) and swap active material cell (`Row * 4 + Col`) deterministically from accumulated XY distance traveled via `AdvanceWalkFlipbookByDistance` (`WalkCycleStrideUU = 140 UU`, snapping cleanly to upright `Col 1` when idle and aligning zombie footstep triggers cleanly to `Col 0` and `Col 2`).

### Verified
- Expanded `DungeoneerVerificationSpecs.cpp` (`HUDAndTradeModalAndAssetBootstrap`) to verify all `136` extracted cells, all `48` diffuse/normal/mask walk-frame combinations, and deterministic distance-driven frame transitions (`Col 0 -> 1 -> 2 -> 3 -> idle Col 1`) on `ADungeoneerCharacter` and `AZombieFastThrower`.
- `DarkDungeoneerEditor Win64 Development` (`8` actions) and `DarkDungeoneer Win64 Development` (`7` actions) compiled cleanly, `python scripts/verify_architecture_invariants.py` exited `0` (`85` files verified), and all `8` headless UE `5.8.2` automation tests passed (`UE_TEST_EXIT=0`).

---

## [0.9.5] - 2026-11-15

### Fixed
- **Visible Character & Zombie 2.5D Cutout Sprites (`OpacityMask` Texture Extraction & Binding)**:
  - Diagnosed invisible `HeroVisualMesh` / `ZombieVisualMesh` cutouts from `v0.9.4`: both `/Engine/Engine_MI_Shaders/M_Shader_SimpleMaskedTwoSided` and `/Engine/Engine_MI_Shaders/M_ES_Phong_Masked_Master_2sided` drive `OpacityMask` from a separate `MaterialExpressionTextureSampleParameter2D` named `OpacityMask` (defaulting to solid black `/Engine/Engine_MI_Shaders/T_Emissive`), causing every masked pixel to evaluate `0.0 < OpacityMaskClipValue` and clip.
  - Added `UDungeoneerAssetBootstrapSubsystem::SpriteMaskTextures` and `GetSpriteMaskTexture(FName DiffuseKey)` in `DungeoneerAssetBootstrapSubsystem.h/.cpp`; `ExtractAtlasCellTexture` now synthesizes transient silhouette mask textures (`B = G = R = A = (Alpha >= 64) ? 255 : 0`) matching each extracted character row (including `bMirrorHorizontal` and `*_Cell0_D` aliases).
  - Updated `UDungeoneerAssetBootstrapSubsystem::CreateDungeonOrRigMaterial` to prefer `/Engine/Engine_MI_Shaders/M_Shader_SimpleMaskedTwoSided` first and bind `OpacityMask` to `MaskTex` across both masked two-sided engine shaders.
  - Updated `ComputeSpritePlaneOrientation` to tilt cutout planes at `45°` (`WorldUp = (0.7071, 0, 0.7071)`, `WorldNormal = (-0.7071, 0, 0.7071)`), disabled shadow casting on `HeroVisualMesh` (`ADungeoneerCharacter`) and `ZombieVisualMesh` (`AZombieBase`) to prevent plane self-shadowing, and positioned `HeroLanternFillLight` (`(-65, 0, 110)`) and `ZombieEmissiveLight` (`(-45, 0, 65)`) in front of the camera-facing side of the cutout billboards.

### Verified
- Added assertions in `DungeoneerVerificationSpecs.cpp` (`HUDAndTradeModalAndAssetBootstrap`) verifying non-null `Hero_Cell0_D` and `Hero_Row1_D` silhouette masks and confirming `HeroMaskedMID->K2_GetTextureParameterValue(TEXT("OpacityMask"))` matches `HeroRow1Mask`.
- `DarkDungeoneerEditor Win64 Development` (`10` actions) and `DarkDungeoneer Win64 Development` (`9` actions) compiled cleanly, `python scripts/verify_architecture_invariants.py` exited `0` (`85` files verified), and all `8` headless UE `5.8.2` automation tests passed (`EXIT CODE: 0`).

---

## [0.9.4] - 2026-11-15

### Fixed
- **Camera-Relative 2.5D Character & Zombie Facing**:
  - Added `DungeoneerMath::OctantToScreenDirectionalRow` in `DungeoneerTypes.h` mapping `W -> 0` (camera-facing Front), `E -> 1` (dungeon-facing Back/+X), `N/NE/NW -> 2` (Right/+Y), and `S/SE/SW -> 3` (Left/-Y), while preserving `OctantToCardinalRow` for the animation orchestrator.
  - Updated `UDungeoneerAssetBootstrapSubsystem::ExtractDefaultSingleCellTiles` to extract all 4 directional sprite rows (`Row0..Row3` at cells `0, 4, 8, 12`) for `Hero`, `Exploder`, and `Thrower` alongside backward-compatible `*_Cell0_*` aliases (`40` extracted tile entries total).
  - Solved `/Engine/BasicShapes/Plane.Plane` UV/tangent basis orientation (`ComputeSpritePlaneOrientation`, `GetCameraFacingSpritePlaneRotation`, `ShouldMirrorSpritePlaneU`, horizontally flipping albedo/normal red channel on `bFlipHorizontalForSpritePlane`) and bound masked 2-sided shaders (`M_ES_Phong_Masked_Master_2sided` / `M_Shader_SimpleMaskedTwoSided`) via `CreateDungeonOrRigMaterial(..., bMaskedSprite)`.
  - Changed default movement/aim octants to `ECompassOctant::E` (`+X` into the dungeon) and `CurrentFlashlightYawDeg = 0.0f` for the hero (`ECompassOctant::W` for zombies), pre-allocated `4` directional `MID` instances per character/zombie (`UpdateHeroDirectionalVisual`, `UpdateZombieDirectionalVisual`), and added `LastMouseSample` / `TimeSinceMouseMoved` tracking (`> 4.0 px²` movement threshold, `0.45s` idle window) in `ADungeoneerCharacter::PollDirectKeyboardAndMouseFallback` so resting cursor positions do not override `WASD` movement facing.

### Changed & Optimized
- **Integrated-GPU Dynamic Lighting Optimization (`Intel HD Graphics 520`)**:
  - Disabled Lumen GI/reflections, Virtual Shadow Maps (`r.Shadow.Virtual.Enable=0`, `r.ShadowQuality=2`, `r.Shadow.MaxResolution=1024`), DistanceField AO/Shadowing, Capsule Shadows, and TSR 200% in `Config/DefaultEngine.ini` and via runtime `ECVF_SetByCode` overrides in `ADungeoneerGameMode::ApplyLowSpecDynamicLightingCVars` so cached `Scalability@3` INI values cannot override low-spec rendering.
  - Preserved real-time dynamic flashlight shadows, light shafts, and Volumetric Fog using lighter froxel dimensions (`GridPixelSize=16`, `GridSizeZ=64`, `HistoryMissSupersampleCount=1`), FXAA (`r.AntiAliasingMethod=1`), `r.ScreenPercentage=75`, `t.MaxFPS=60`, and `1280x720` default window dimensions in `Launch-DarkDungeoneer.bat`.
  - Disabled dynamic shadow casting on secondary fill/campfire point lights (`ARestingGroundActor::CampfireLight`, `AMenuDioramaStageActor::HeroFlashlightSpot`) and enabled distance culling (`SetCullDistances(0, 3800)`) on all `ADungeoneerTileMapInstancer` HISM components (`FloorHISM`, `WallHISM`, `MirrorHISM`, `BreakableHISM`).

### Verified
- Expanded `DungeoneerVerificationSpecs.cpp` (`40` extracted cell textures, `OctantToScreenDirectionalRow`, all 12 `*_Row{0..3}_D` textures, `GetCameraFacingSpritePlaneRotation()`, and masked 2-sided Hero `MID`); `DarkDungeoneerEditor Win64 Development` (`15` actions) and `DarkDungeoneer Win64 Development` (`14` actions) compiled cleanly, `verify_architecture_invariants.py` passed (`85` files), and all `8` headless UE `5.8.2` automation tests passed (`EXIT CODE: 0`).

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
