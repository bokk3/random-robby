# Handoff

State of Dark Dungeoneer for whoever picks it up next. Rules and commands are in [AGENTS.md](AGENTS.md);
what the game is, in [docs/DESIGN.md](docs/DESIGN.md).

## Where things stand (v1.5.0 + plan M0/M1, uncommitted on `main`)

The playability plan (milestones M0–M6: bugs, shared camera, game feel, enemy telegraphs, run structure,
story, art) is being worked through in order. M0, M1 and most of M2 are done and verified on 2026-11-21:
build clean, static checks pass (120 files), 27 automation specs pass, and captures show the shared camera,
the new sprite/FX materials, effect rings and sparks. Details are in CHANGELOG [Unreleased].

Still open in M2: HUD restyle (skill icons with radial cooldowns, boss bar phase notch, biome title card).
In editor `-game` runs the first effect of a session hitches about 3 s while a shader compiles ("Preparing
Shaders (1)" stays on screen); cooked builds precompile shaders. M3 (telegraphed enemy attacks, elites, floor scaling)
is done and verified: 30 automation specs pass. Not yet done in M3: the new "Glazier" shield enemy (waiting on
its sprite sheets) and a Colossus sweep/charge. Enemies only notice a hero who makes noise or lights them,
so a hero standing still in a combat room is left alone; that is the existing stealth design. Next is M4
(branching floors, arena locks, waves, choices, weapons). M6 has started: the five hero move sheets are in and used in game (see CHANGELOG); enemy, NPC
and effect sheets from docs/TO_GENERATE.md are still to come. Slice each sheet into its own folder.

Machine note: drive C: was nearly full (and refilled to about 1 GB during M2 from something outside this
project). If a build fails with "C1076 internal heap limit" / "Failed to create virtual memory for PCH", add
`-MaxParallelActions=1` to the build command. This machine's shader/derived-data cache now lives on
`D:/UnrealCache/Zen/Data` via `Saved/Config/WindowsEditor/Engine.ini` (not committed).

Known issue: a dark textured streak sits next to the hero in most captures (it predates v1.5). Its source
isn't found yet; it is not a fauna, flora or hero mesh shadow.

### v1.5.0

The project is turning from a dark exploration crawler into a run-based hack-and-slash in the
Dead Cells mould (original setting and story).

Done in this pass:
- **Combat kit** on `ADungeoneerCharacter`: 4-hit combo with buffering, hold-to-charge heavy smash,
  dash strike out of a roll, perfect dodge, parry (also deflects Thrower flasks). Gamepad input for both players.
- **Skills**: `UDungeoneerSkillComponent` (`Source/DarkDungeoneer/*/Combat/`), two slots, six skills, skill scrolls as loot.
- **Enemy status effects** on `AZombieBase`: stun, burn, time slow.
- **Story** in `docs/STORY.md` (setting: the sealed glass city of Senwick and its "Held Hour"; skill names come from it).
- **Docs/agent setup cleaned up**: `.agents/rules` merged into `AGENTS.md`; skills moved to `.claude/skills/`;
  finished plans/devlog/task tracker moved to `docs/archive/`; README, CHANGELOG entry, VERSION 1.5.0.

Verified on 2026-11-20: `DarkDungeoneerEditor Win64 Development` builds with no project warnings, the static
checks pass (114 files), and all 25 automation specs pass (new one: `DarkDungeoneer.Combat.HackAndSlashKit`).
Not yet done: playing it by hand to tune feel, and rebuilding the standalone Development/Shipping targets.

## Next tasks, in order of value

1. **Telegraphed enemy attacks.** Zombies rarely make direct melee attacks, so parry and perfect dodge lack
   clear moments. Give Exploders a lunge with a wind-up flash, Stalkers a visible strike, the Colossus readable slams.
2. **Hero attack sprites.** The hero sheet only has walk frames. Specs for four combat sheets are in
   `docs/TO_GENERATE.md` (last section). Hook them into `UpdateNewMovePoses` once sliced.
3. **Weapon variety.** A second weapon slot with its own combo table (data-driven, like `GSkillDefs`).
4. **Skill pickup choice.** Scrolls auto-swap today; add a keep/swap prompt.
5. **Replicate combat poses** for online play (damage is server-side already; visuals aren't replicated).
6. Older polish items: vault chest roughness hot spot under the flashlight, P2 spawn offset overlapping P1,
   rebuild Win64 Development/Shipping and package with `RunUAT BuildCookRun`.
7. Later: UE 5.9's experimental WebAssembly/WebGPU target. It runs the mobile renderer, so volumetric fog and
   online play would need fallbacks.
