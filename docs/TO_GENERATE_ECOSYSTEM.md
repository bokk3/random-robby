# TO_GENERATE_ECOSYSTEM.md — 2.5D Living Ecosystem Sprite & Tile Sheets

## Technical Standards for Ecosystem Sheets
- **Resolution**: `2048x2048` raw generation (`512x512` per cell) → sliced via `slice_normal_rig.py --mode eco --cell-size 128` to **`512x512` (`128x128` per cell = 2m × 2m at 64 px/m)**, with fauna strips emitted at **`32x32` per cell (~0.5m)** via `--strip-row 3 --strip-cell-size 32`.
- **Pixel Grid & Outline Discipline**:
  - Crisp 1 px dark charcoal/obsidian outline (`#120E18`) around all silhouettes.
  - Glow halos must be painted **inside** the dark outline or rendered cleanly over solid `#FF00FF` without semi-transparent magenta-tinted outer airbrush fringes.
  - Pure `#FF00FF` `(255, 0, 255)` flat chroma background for all `PROP_*` sheets; zero magenta key on seamless `ATLAS_*` tile sheets.
  - Zero stray artist reference props (hands, disembodied boots, floating vials, text labels).
- **Mandatory Extracted Channels**:
  - `_D.png`: RGBA Diffuse with hard-edge flood-fill chroma keying (`alpha = 0` or `255`) and halo decontamination.
  - `_N.png`: Chamfer distance-bevel + 3×3 Sobel tangent-space Normal Map (`#8080FF` flat neutral in transparent regions).
  - `_E.png`: HSV high-saturation/luminance Emissive Mask on pure black `(0, 0, 0)` (no global magenta guard after keying so Prism-Lily magenta petals shine cleanly).
  - `AtlasMetadata.json`: 16-cell (or 4-cell strip) UV bounds & bottom-center anchor pivots + SHA-256 entry in `Content/AssetManifest.json`.

## Color Palette & Tier 0 Emissive Standards
- **Emerald Moss / Lantern Beetle**: `(0.10, 0.95, 0.45)`
- **Magenta Prism-Lily Bloom**: `(0.95, 0.15, 0.85)` + golden stamen `(1.00, 0.78, 0.25)`
- **Astral Cyan Crystals / Bio-Water**: `(0.20, 0.85, 1.00)`
- **Amber Fire / Magma Geyser**: `(1.00, 0.55, 0.18)` → `(1.00, 0.32, 0.05)`

---

## Reusable Generation Prompt Template
> 2D top-down 60-degree pitch dark gothic semi-pixel-art 4x4 sprite sheet, crisp 4px aligned pixel clusters (128px effective cell resolution), 1px dark `#120E18` pixel outlines, centered inside a strict 4x4 uniform grid on a completely flat uniform `#FF00FF` bright magenta background with zero border shadows, zero outer glow spill onto magenta, zero hands/boots/tools/labels, internal bioluminescent highlights for `_E` extraction.

---

## Sheet Specifications & C++ UV Cell Bindings

### 1. `PROP_Magical_Flora_And_Crystals_4x4.png`
- **Row 0 (`Cell 0..3`) — Photosensitive Prism-Lily**:
  - Col 0 (`0`): Dormant dark closed bud (`Emissive = 0.2`)
  - Col 1 (`1`): Half-parted bud showing gold-magenta seam
  - Col 2 (`2`): Opening blossom catching lantern bounce
  - Col 3 (`3`): Full radial magenta-gold bloom (`Emissive = 12.0`, 15s Tier 2 light lease)
- **Row 1 (`Cell 4..7`) — Violet/Emerald Spore-Cap Clusters**:
  - 4 cluster variants bursting with luminous lime-green gills (`120 UU` proximity burst trigger)
- **Row 2 (`Cell 8..11`) — Harmonic Echo-Crystals**:
  - 4 cyan-violet quartz formations (dormant → resonant acoustic flash)
- **Row 3 (`Cell 12..15`) — Kinetic Pulse-Moss Floor Patches**:
  - 4 seamless moss-veined slab overlays (radial wave propagation via `EEcoCPD::EventTime`)

### 2. `PROP_Fauna_Critters_4x4.png` (`32x32` px/cell = ~0.5m critters)
- **Row 0 (`Cell 0..3`) — Golden Lumen-Moth**: 4-frame wing-flap cycle (`Orbit` / `Flee` / `Roost`)
- **Row 1 (`Cell 4..7`) — Emerald Lantern-Beetle**: 4-frame leg-scuttle cycle (carries moving Tier 1 light node)
- **Row 2 (`Cell 8..11`) — Cyan Puddle-Newt**: 4-frame serpentine S-wriggle cycle (emits `0.25` decoy noise dart)
- **Row 3 (`Cell 12..15`) — Crystal-Back Crypt Crab**: 4-frame sideways pincer-scuttle cycle

*(Placeholder strip while single-frame fauna sheet is active: `PROP_Fauna_Critters_Strip_4x1`, Row 3 of `PROP_Water_And_Fauna_4x4` sliced at `32x32` per cell: `0 = Moth, 1 = Beetle, 2 = Newt, 3 = Crab`, with wing flap & scuttle driven procedurally in shader.)*

### 3. `ATLAS_Water_Surfaces_4x4.png` (Seamless, NO `#FF00FF` key)
- **Row 0 (`Cell 0..3`)**: 4 directional ankle-deep wading stream flow tiles
- **Row 1 (`Cell 4..7`)**: 4 shallow specular flagstone puddle tiles (`Roughness = 0.03`)
- **Row 2 (`Cell 8..11`)**: 4 wet dampened moss-to-stone transition curbs
- **Row 3 (`Cell 12..15`)**: 4 fading wet footprint overlays (N / NE / E / SE rotated across 8 compass headings for `4.0s`)

### 4. `PROP_Water_Cascades_And_Geysers_4x4.png` (`#FF00FF`)
- **Row 0 (`Cell 0..3`)**: 4-frame Weeping Wall vertical water cascade (N / E / W walls)
- **Row 1 (`Cell 4..7`)**: 4-frame wall-base floor drain grate & rising mist plume
- **Row 2 (`Cell 8..11`)**: 4-frame bubbling Cyan Bioluminescent Geyser pool (Sanctuary / Spore biomes)
- **Row 3 (`Cell 12..15`)**: 4-frame simmering Amber-Magma Thermal Geyser pool (EmberForge biome)

### 5. `PROP_Water_FX_4x4.png` (`#FF00FF`)
- **Row 0 (`Cell 0..3`)**: Falling ceiling condensation droplet → floor contact crown
- **Row 1 (`Cell 4..7`)**: 4-frame concentric normal-map water ripple ring
- **Row 2 (`Cell 8..11`)**: 4-frame ankle-deep wading boot splash sub-sprite
- **Row 3 (`Cell 12..15`)**: Drifting glowing lotus petals & luminous buoyant spores

### 6. `PROP_Wall_Bioluminescence_4x4.png` (`#FF00FF`)
- **Row 0 (`Cell 0..3`)**: Ceiling glow-worm silk strands (Tier 0 emissive micro-lights)
- **Row 1 (`Cell 4..7`)**: Glowing wall-fissure mineral veins (emerald / cyan)
- **Row 2 (`Cell 8..11`)**: Carved golden/cyan sanctuary ward runes
- **Row 3 (`Cell 12..15`)**: Small glowing ward stones (Tier 1 floor anchors)
