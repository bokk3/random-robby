---
name: zombie-chrono-hazard-ai
description: Implements Zombie AI archetypes in UE 5.8 C++, including Slow Exploding Zombies and Fast Elemental Throwers (Fire, Poison, and Slo-Mo Gas with inverse speed-steal math). Use when creating enemy behaviors, hazard volumes, or status effects.
---

# Zombie Archetypes & Chrono-Leech Hazard Skill

## Archetype Specifications
1. **Slow Exploder (`AZombieExploder`)**:
   - Movement: Low base speed, high mass, relentless pathing toward nearest audible/lit player.
   - Trigger: Entering `IgnitionRadius` starts a deterministic fuse countdown (`FuseDuration`), pulsing emissive belly cracks and accelerating audio pitch ticks before dealing radial falloff damage and spawning a scorch decal.
2. **Fast Elemental / Chrono-Leech Thrower (`AZombieFastThrower`)**:
   - Movement: High base speed, strafes at `OptimalThrowRange` along the 8 compass axes.
   - Projectile Payload Selector (`EThrowPayload`):
     - `Fire`: Spawns `AHazardZone_Fire` (DoT + dynamic flickering orange point light that temporarily illuminates fog).
     - `Poison`: Spawns `AHazardZone_Poison` (stamina drain + green spore Niagara mist that reduces flashlight penetration).
     - `SloMoGas`: Spawns `AHazardZone_ChronoGas` linked via `TWeakObjectPtr<AZombieFastThrower>` to its thrower.

## Chrono-Leech Speed-Steal Protocol
1. Every fixed tick inside `AHazardZone_ChronoGas`:
   - Accumulate `SlowStack` on overlapping players up to `MaxSlowRatio` (e.g., `0.65f`).
   - Calculate total speed fraction drained across all affected players: `TotalDrainedRatio = Sum(PlayerSlowRatio_i)`.
   - Push `TotalDrainedRatio` to the owning `AZombieFastThrower`:
     - `ZombieMaxWalkSpeed = BaseWalkSpeed * (1.0f + TotalDrainedRatio * SpeedStealScalar)`
     - `ZombieThrowCooldown = BaseCooldown / (1.0f + TotalDrainedRatio * CadenceStealScalar)`
2. When the player exits the gas or kills the owning zombie, decay the leech buff over `DecayTime` with a time-warp audio down-sweep.