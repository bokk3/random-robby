# Dark Dungeoneer — Game Design

A fast, run-based hack-and-slash roguelite seen from a fixed top-down 2.5D camera. You go down into a
pitch-black, reshuffling dungeon with a lantern that is your light, your fuel and your weapon. You die,
you come back at the top with what you learned, and you go down again. Up to two players can play
together on one shared screen or online. Single player is the main mode.

The fiction is in [STORY.md](STORY.md). This document covers how the game plays.

## Pillars

1. **Fast, readable melee.** Every fight is decided by spacing, timing and the moves you bring:
   combos, a heavy smash, rolls, parries and two skills. Mistakes are punished, but good play is
   rewarded with empowered hits and crowd control.
2. **Light is a resource.** The lantern lights a cone in front of you; everything else is dark.
   Lantern oil feeds the light and some skills. Enemies like the Mirror Stalker only exist when lit.
3. **The dungeon is alive.** Water slows you, flowers bloom in light, noise draws Exploders, Throwers
   steal your speed. Use the environment as a weapon.
4. **Every run is different, the knowledge carries over.** Floors reshuffle from a seed. Skills and
   relics are found during the run. The Ledger (meta save) keeps records, the bestiary and cosmetics.
5. **Better together.** Drop-in second player on the same screen, revives, a shared stash and supply swaps.
   The camera frames both heroes and zooms out as they spread; they can't get more than 900 UU apart.

## Combat

### Core moves (always available)

| Move | Input (P1 keyboard / gamepad) | What it does |
| --- | --- | --- |
| Lamplighter's Round (combo) | LMB, J or Space / X | Four hits: 1.0x, 1.1x, 1.25x, then a 1.8x finisher that knocks back hard and stuns for 0.5s. Presses during a swing are buffered for 0.3s. |
| Banked Fire (heavy smash) | Hold attack / hold X | After the current swing, keep holding to charge. Release after 0.35s to smash: 1.8x–2.6x damage in a wide arc, big knockback, 0.8–1.4s stun. Auto-releases at 1.4s. Releasing early cancels. |
| Dodge roll | Shift, K or RMB / A | 0.22s of invulnerability and a burst of speed. |
| Moth to Flame (dash strike) | Attack during a roll or within 0.18s after | Lunge forward, piercing everything in a narrow line for 1.5x damage. |
| Stolen Second (perfect dodge) | Get hit by a zombie mid-roll | No damage, roll cooldown refunded, zombies within 650 UU slowed to 30% speed for 1s, next hit does 2x. |
| Shutter Guard (parry) | L, V or MMB / B | 0.22s guard window. A zombie hit from the front is negated; the attacker is knocked back and stunned for 1.4s, and your next hit does 2x. A Thrower's flask that lands near you during the window flies back and hurts, stuns and burns or slows the thrower. A successful parry resets the parry cooldown. |

"Empowered" hits (after a parry, perfect dodge or Their Own Shadow) last 1.3–1.5s and are consumed by the next
melee swing. The HUD shows `xN NEXT HIT` while one is ready.

### Skills (two slots, found during runs)

Skill scrolls drop in about one combat room in three and always in secret Phantasm pockets. Picking one up
fills an empty slot, or replaces the slot that was replaced least recently. Slot 1: Q or U / LB or LT.
Slot 2: R or I / RB or RT. A new run starts with Bottled Kiln and Borrowed Noon.

| ID | In-world name | Cooldown | Effect |
| --- | --- | --- | --- |
| `EmberFlask` | Bottled Kiln | 7s | Lobs fire at the nearest enemy in front (or 380 UU ahead): 30 damage in 190 UU, burn 16/s for 4s, lights Exploder fuses. |
| `GrappleChain` | Lamplighter's Hook | 6s | Yanks the nearest enemy in front (760 UU) to you, 20 damage, 0.9s stun. Bosses pull you to them instead. Needs a target. |
| `ShadeStep` | Their Own Shadow | 8s | Blinks behind the nearest enemy, stuns it briefly, and makes your next hit a 2.5x backstab. Blinks forward if nothing is near. |
| `WhirlingLantern` | Censer's Wheel | 9s, 10 oil | Spins for 1.4s, hitting everything within 230 UU every 0.2s for 16. You can move at 80% speed. |
| `SolarFlare` | Borrowed Noon | 6s, 25 oil | Radial flash: damage, 1.2s stun on everything within 420 UU, blooms Prism-Lilies. |
| `MirrorDecoy` | Slipped Self | 12s | You step 160 UU backward out of yourself, leaving a copy that rings out loud noise every 0.5s for 5s (Exploders hunt noise), then shatters for 45 damage and a 0.6s stun. |

Skill rules live in `Source/DarkDungeoneer/Private/Combat/DungeoneerSkillComponent.cpp` (the `GSkillDefs` table).

### Status effects on enemies

`AZombieBase` supports stun (AI frozen; bosses take 25% of the duration), burn (damage over time) and
time slow (`CustomTimeDilation`, which also slows fuses and throws). Timers run on real time.

### Enemies

Every enemy attack is telegraphed: a red ring on the floor closes in during the wind-up, the enemy glows red
and a growl rises, and the blow lands when the ring closes. Strikes count as zombie hits, so a parry or a perfect
dodge answers them. At most two enemies can wind up on one hero at a time (attack tokens), packs push apart
instead of stacking, and any hit cancels a small enemy's wind-up (the Colossus ignores light hits).

| Enemy | Threat | Counter |
| --- | --- | --- |
| Exploder ("Tapper") | Shoulder-barges twice (0.6 s wind-up, 14 damage), then lights its fuse and blows up (65 in 380 UU). Lights up early when badly hurt. Hunts noise. | Parry the barge, then back off from the fuse; or lure it into others. Bottled Kiln lights its fuse early. |
| Thrower ("Runner") | Plants its feet (0.35 s), then lobs a flask that flies for about half a second to a marked spot: fire, poison or stillsmoke. Knife-slashes (0.4 s, 10 damage) anyone who gets close. | Step off the marker, or parry the flask as it reaches you to send it back (stun, 35 damage, its own payload). |
| Mirror Stalker ("Slip") | Invisible unless lit; lunge-bites about every 1.4 s (0.45 s wind-up, 12 damage). | Keep it in your cone; parry the bite. |
| Cinder Colossus (Floor 3 boss) | Slams with a floor ring showing the whole area (0.75 s, 0.55 s in phase 2), fire/gas mortars, summons Exploders. | Hard to stun, too heavy to Hook. Get out of the ring, then dash strikes and heavy smashes. |

**Elites** (about one regular enemy in six, seeded per floor) glow gold: +60% health, +25% damage, 25% faster
wind-ups, double gold. **Deeper floors** add +20% health and +10% damage per floor past the first.

## Run structure

Three floors per run (Sunken Crypt, Blighted Spore Fens, Cinder Kiln), with Mirror-Palace mazes appearing
throughout. Each floor is a seeded room graph: entry, combat rooms, a mirror maze, a vendor or sanctuary,
a fog gauntlet, and the vault. The floor 3 vault is guarded by the Cinder Colossus. Death ends the run unless
a partner is alive to revive you within 18s.

## Controls summary

| Action | P1 keyboard | P2 keyboard | Gamepad |
| --- | --- | --- | --- |
| Move | WASD (arrows when solo) | Arrows | Left stick |
| Aim | Mouse | follows movement | Right stick |
| Attack / hold to smash | LMB, J, Space | Right Ctrl, Enter, Num0 | X |
| Roll | Shift, K, RMB | Right Shift, Num1 | A |
| Parry | L, V, MMB | ;, Num6 | B |
| Skill 1 / Skill 2 | Q/U, R/I | [/Num5, ]/Num4 | LB/LT, RB/RT |
| Interact | F, E | \, Num2 | Y |
| Oil / Antidote | 1 / C, 2 | Num7 / Num8 | D-pad up / down |
| Supply swap | T | P, Num3 | D-pad right |
| Add player 2 | F2 | — | Start (P1 pad) |

## Feedback and feel

- **Hits:** the struck zombie freezes for 40-110 ms (longer for heavy hits and parries), flashes white, sparks
  fly from the hit, and a damage number pops in and settles. Crits are gold and larger.
- **Big moments** (blasts, heavy smash, Borrowed Noon, taking damage) shake the shared camera. Blasts and smashes
  leave a ring on the floor; a perfect dodge rings blue across the slow radius.
- **Getting hit:** the hero flashes red, the screen edge washes red, a number shows and the camera kicks.
- **Sound:** every swing, hit, parry, perfect dodge, hurt, death, fuse tick, blast, flask shatter and skill has a
  cue (Content/Audio/Foley/Combat, made by `scripts/synthesize_combat_foley.py`).

## Art status and what's missing

The hero sheet (`Content/Art/Rigs/DungeoneerHero`) has only a 4-direction × 4-frame walk cycle, holding a
lantern and no visible weapon. Attacks are sold procedurally: lunges, squash and stretch, the slash-arc quad,
afterimages, and cycling facing rows for the spin. Proper attack frames would help a lot; the sheet specs
are in [TO_GENERATE.md](TO_GENERATE.md#hero-combat-sheets-needed-for-the-hack-and-slash-kit).

## Known gaps / next steps

- **Enemy attacks are not telegraphed.** Zombies mostly hurt you through explosions, hazards and the
  Stalker's drain, so parry and perfect dodge have few clear moments to shine. Next: give each enemy a wind-up
  (flash + pose) before a melee lunge, and give the boss readable slams.
- **Online clients only see their own combat poses.** Combat state is not replicated, so remote players
  don't see each other's swings (damage is correct). Same-screen co-op is unaffected.
- **Skill choice.** Scrolls auto-swap; a "keep or swap" prompt would be better.
- **Weapons.** There is one weapon (the lantern). Dead-Cells-style runs want weapon variety: a second weapon
  slot with different combos (e.g. a hook-pike with long reach, twin wicks with fast hits).
- **Scaling.** No per-run stat growth yet beyond relics and senses.
