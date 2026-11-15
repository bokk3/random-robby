---
trigger: model_decision
description: "Enforces semi-pixelated 2.5D shader standards, flashlight cone lighting, volumetric fog sensory reveals, and mirror-palace illusion rendering."
---

# 2.5D Rendering, Lighting, Fog & Mirror Illusions

1. **Semi-Pixelated Detailed Aesthetic**:
   - All 2D-to-2.5D cutout meshes must sample textures using texel-center UV snapping (`floor(UV * TexSize) + 0.5`) paired with tangent-space Normal Maps and Roughness/Metallic masks so pixel art reacts smoothly to dynamic 3D lights.
   - Maintain a unified texel density of 64 pixels per Unreal meter (100 UU).
2. **Flashlight & Sensory Fog Pipeline**:
   - The player flashlight is a narrow-cone `USpotLightComponent` with volumetric scattering enabled and inverse-square falloff.
   - Distance fog uses Exponential Height Fog + Local Volumetric Fog volumes.
   - Sensory upgrades must bind to a global Material Parameter Collection (`MPC_SensoryState`):
     - `DarkVisionRadius`: Expands an ambient desaturated rim-light sphere around the player.
     - `HearingPulseOrigin` & `HearingPulseRadius`: Renders high-contrast acoustic edge ripples through fog and walls when off-screen zombies step or shriek.
3. **Mirror Palace Maze Illusions**:
   - Use Custom Depth Stencil values (`101–108`) and planar `USceneCaptureComponent2D` render targets (capped at half-resolution with frustum culling) to render false corridors and reflected decoys.
   - Real vs. illusory walls and zombie reflections must diverge subtly in normal-map specular response or acoustic ping return so players with upgraded senses can spot the trick.
4. **Weather FX**:
   - Build ambient dungeon weather (draft dust, ceiling drips, toxic spore clouds, ember ash) in Niagara using GPU sprites with depth-buffer collisions enabled.