---
name: mirror-maze-dungeon-gen
description: Builds procedural dungeon levels in UE 5.8 featuring combat rooms, mirror-palace illusion mazes, resting grounds, vendors, and terminal loot vaults. Use when working on dungeon generation, level progression, mirror illusions, or vendor/resting encounters.
---

# Procedural Dungeon & Mirror-Palace Maze Skill

## Dungeon Graph Progression
1. Generate a directed acyclic graph (DAG) per floor with deterministic `FRandomStream` seeded by the server:
   - **Entry Node** -> **Combat / Exploration Nodes** -> **Mirror-Palace Illusion Node** -> **Mid-Floor Branch (Vendor OR Resting Ground)** -> **Deep Fog Gauntlet** -> **Boss / Major Loot Vault Node**.
2. **Resting Ground Node**: Disables zombie spawns, provides campfire dynamic warm lighting, restores health/battery, and unlocks Party Stash / Ability Loadout tuning.
3. **Vendor Node**: Spawns an NPC merchant with a seeded inventory table and hooks into the server-authoritative `UTradeManagerComponent`.

## Mirror-Palace Maze Construction
1. Construct hex or orthogonal grid cells containing three wall types:
   - **True Mirror (`AMirrorWall_Solid`)**: Reflects player flashlight beams (via bounce spot light raycast) and reflects characters, blocking physical movement.
   - **Illusory Pass-Through (`AMirrorWall_Phantasm`)**: Renders a fake reflection using stencil portal capture (`CustomDepthStencil = 104`), but allows players to step diagonally or cardinally right through it.
   - **Decoy Spawner (`AMirrorDecoyNode`)**: Projects reflected zombie clones that deal zero damage except for the single true zombie holding a physical shadow and acoustic footstep signature.