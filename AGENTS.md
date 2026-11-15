# Project: 2.5D Dark Dungeoneer (Unreal Engine 5.8)

## Core Identity & Architecture
- Engine Target: Unreal Engine 5.8 (C++20 primary, zero gameplay logic in Blueprints; Blueprints are restricted to data-only asset assignment and UMG visual layout).
- Perspective: Top-down fixed-pitch 2.5D camera with orthographic-feel perspective foreshortening.
- Memory & Performance: Zero heap allocations on the game-thread hot path (`TickComponent`, `ProcessEvent`). Pre-allocate entity pools and use deterministic fixed-step math where possible.

## Gameplay Pillars
1. **4-Axis Compass Locomotion**: Players move along 4 discrete axes (Horizontal, Vertical, Main Diagonal NE-SW, Anti-Diagonal NW-SE) yielding 8 compass headings (N, NE, E, SE, S, SW, W, NW).
2. **Darkness, Fog & Sensory Progression**: Pitch-black dungeons lit by a player cone flashlight and occluded by distance volumetric fog. Sensory upgrades (Dark Vision, Acute Hearing acoustic ripples, Thermal/Scent trails) dynamically drive Post-Process and Niagara parameters.
3. **Dungeon Loop**: Multi-floor procedural dungeon graphs containing Combat Rooms, Mirror-Palace Illusion Mazes, Resting Grounds, Vendors, and a terminal High-Tier Loot Vault.
4. **Zombie Ecosystem**: Slow proximity-exploding zombies and Fast elemental throwers (Fire, Poison, and Slo-Mo Gas with inverse velocity-steal math).
5. **Co-Op & Social**: Native Local Split-Screen combined with full Online Multiplayer (Parties, Friend Lists, Chat, and transactional Player/Vendor Trading).