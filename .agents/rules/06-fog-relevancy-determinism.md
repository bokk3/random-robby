---
trigger: model_decision
description: "Governs deterministic seed replication for procedural dungeons and network relevancy culling for fog-occluded enemies and mirror illusions."
---

# Procedural Seed Determinism & Sensory Network Relevancy

1. **Seed-Only Dungeon & Mirror Replication**:
   - Never replicate individual procedural wall actors or static room transforms over the network.
   - Replicate only the floor's `FDungeonFloorSeed` (`int64` master seed + mutated room bitmask) via `ADungeoneerGameState`. All clients must construct the exact same DAG, Mirror-Palace geometry, and static props locally using deterministic `FRandomStream` calls.

2. **Sensory & Flashlight Network Relevancy (`IsNetRelevantFor`)**:
   - Override `AActor::IsNetRelevantFor` on `AZombieBase` and hidden loot actors.
   - An enemy buried in deep Volumetric Fog outside of all players' `FlashlightCone`, `DarkVisionRadius`, and `HearingPulseRadius` must drop replication frequency (`NetUpdateFrequency = 2.0f`) or cull until a sensory threshold is crossed.
   - Mirror-Palace decoy clones must be driven deterministically on clients from the single authoritative parent zombie's transform and stencil ID rather than replicating separate decoy actors.