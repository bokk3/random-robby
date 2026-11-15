---
trigger: always_on
---

# Asset Provenance & Timed Audio Rules

1. **Generated-First Asset Policy**:
   - Prefer procedurally or AI-generated 2D sprite sheets, normal maps, SFX, and voice stems.
   - If any free-use / CC0 external asset is imported, it MUST pass through a mandatory modification pipeline (palette remapping, normal-map regeneration, mesh re-slicing, or pitch/formant synthesis) before commit. Log provenance in `Content/AssetManifest.json`.
2. **Frame-Accurate Timed Audio**:
   - Footsteps, zombie fuse ticks, gas hisses, and projectile throws must never fire on arbitrary timers. Trigger all gameplay audio via deterministic AnimNotifies or fixed-step distance accumulators synced to the 4-axis step cycle.