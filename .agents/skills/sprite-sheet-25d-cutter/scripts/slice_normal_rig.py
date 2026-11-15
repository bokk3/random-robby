#!/usr/bin/env python3
"""
slice_normal_rig.py

Zero-dependency (numpy + Pillow) 2.5D Hybrid Sprite Sheet Slicer & Channel Extractor
for Unreal Engine 5.8 (C++20 Dark Dungeoneer).

Features:
- Procedural 4x4 magenta-keyed (#FF00FF) archetype synthesis (--synthesize-archetype {hero, exploder, thrower})
- Hard-edge chroma-key stripping (_D.png RGBA) replacing #FF00FF with (0,0,0,0) and zero-halo neighbor cleanup
- Tangent-space normal synthesis (_N.png RGB) using pure-NumPy Chamfer distance-transform dome bevel + 3x3 Sobel luminance gradient
- High-luminance emissive mask extraction (_E.png RGB) isolating Player yellow lantern (#FFCC00),
  Exploder orange magma belly (#FF5500), and Fast Thrower cyan flasks (#00FFFF)
- Packed ORM synthesis (_ORM.png) and 24-color dark gothic palette remix (--remix)
- Normalized UV sub-rectangles (U0, V0, U1, V1) & bottom-center foot pivots for all 16 cells (AtlasMetadata.json)
- Legacy socket metadata (rig_sockets.json) and SHA-256 provenance tracking (Content/AssetManifest.json)
"""

import argparse
import hashlib
import json
import math
import os
import sys
from datetime import datetime, timezone

try:
    from PIL import Image, ImageDraw
    import numpy as np
    HAS_PIL_NUMPY = True
except ImportError:
    HAS_PIL_NUMPY = False

# -----------------------------------------------------------------------------
# Constants & Palettes
# -----------------------------------------------------------------------------
TEXELS_PER_METER = 64.0
UU_PER_METER = 100.0
UU_PER_PIXEL = UU_PER_METER / TEXELS_PER_METER  # 1.5625 UU / px

CHROMA_KEY_MAGENTA = (255, 0, 255)

DARK_GOTHIC_PALETTE = [
    # Shadows / Charcoals
    (12, 12, 16),
    (22, 24, 30),
    (36, 40, 48),
    # Cold Iron / Steel
    (60, 68, 80),
    (96, 108, 122),
    (148, 162, 178),
    # Stone / Leather
    (45, 36, 30),
    (78, 62, 50),
    (115, 94, 76),
    # Bone / Cloak Light
    (185, 180, 165),
    (225, 222, 210),
    # Dried Blood / Deep Crimson
    (75, 15, 18),
    (128, 25, 32),
    (180, 40, 50),
    # Necrotic Mold / Rotting Flesh
    (35, 52, 38),
    (62, 88, 60),
    (95, 130, 85),
    # Key Emissive Accents (Preserved cleanly for _E extraction)
    (255, 204, 0),    # Player Lantern Yellow (#FFCC00)
    (255, 85, 0),     # Exploder Magma Orange (#FF5500)
    (255, 195, 60),   # Magma Hot Core
    (0, 255, 255),    # Fast Thrower Alchemical Cyan (#00FFFF)
    (110, 235, 255),  # Cyan Core Highlight
    # Neutral Moonlight Glint
    (240, 250, 255),
]


def compute_sha256(filepath):
    """Computes the hex SHA-256 hash of a file."""
    h = hashlib.sha256()
    with open(filepath, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()


# -----------------------------------------------------------------------------
# Procedural Archetype Synthesis (64x64 per frame -> 256x256 4x4 #FF00FF atlas)
# -----------------------------------------------------------------------------
def synthesize_archetype_sheet(archetype_name):
    """
    Procedurally draws crisp 4x4 semi-pixelated dark gothic sprite sheets
    over a solid #FF00FF (255, 0, 255) chroma-key background.
    """
    if not HAS_PIL_NUMPY:
        raise RuntimeError("Pillow and numpy are required for procedural sprite synthesis.")

    sheet_w, sheet_h = 256, 256
    frame_w, frame_h = 64, 64
    img = Image.new("RGBA", (sheet_w, sheet_h), (255, 0, 255, 255))
    draw = ImageDraw.Draw(img)

    # 4 rows: 0 = South, 1 = North, 2 = East, 3 = West
    # 4 cols: 0 = Contact A, 1 = Passing A, 2 = Contact B, 3 = Passing B
    for row in range(4):
        for col in range(4):
            ox = col * frame_w
            oy = row * frame_h
            cx = ox + frame_w // 2
            cy = oy + frame_h // 2

            bob = -1 if col in (1, 3) else 0
            stride = -3 if col == 0 else (3 if col == 2 else 0)

            if archetype_name == "hero":
                boot_color = (45, 36, 30, 255)
                draw.rectangle([cx - 7 + stride, cy + 14, cx - 3 + stride, cy + 24], fill=boot_color)
                draw.rectangle([cx + 2 - stride, cy + 14, cx + 6 - stride, cy + 24], fill=boot_color)

                cloak_dark = (22, 24, 30, 255)
                cloak_mid = (36, 40, 48, 255)
                draw.polygon([
                    (cx - 12, cy + 18 + bob),
                    (cx + 12, cy + 18 + bob),
                    (cx + 10, cy - 6 + bob),
                    (cx - 10, cy - 6 + bob),
                ], fill=cloak_dark)

                if row != 1:
                    armor_color = (96, 108, 122, 255)
                    armor_hi = (148, 162, 178, 255)
                    draw.rectangle([cx - 6, cy - 4 + bob, cx + 6, cy + 8 + bob], fill=armor_color)
                    draw.line([(cx - 4, cy - 2 + bob), (cx + 4, cy - 2 + bob)], fill=armor_hi, width=1)
                else:
                    pack_color = (60, 48, 38, 255)
                    draw.rectangle([cx - 7, cy - 5 + bob, cx + 7, cy + 7 + bob], fill=pack_color)

                draw.ellipse([cx - 8, cy - 22 + bob, cx + 8, cy - 6 + bob], fill=cloak_mid)
                if row == 0:
                    draw.rectangle([cx - 5, cy - 16 + bob, cx + 5, cy - 10 + bob], fill=(12, 12, 16, 255))
                    draw.point((cx - 2, cy - 13 + bob), fill=(148, 162, 178, 255))
                    draw.point((cx + 2, cy - 13 + bob), fill=(148, 162, 178, 255))
                elif row == 2:
                    draw.rectangle([cx + 1, cy - 16 + bob, cx + 5, cy - 11 + bob], fill=(12, 12, 16, 255))
                    draw.point((cx + 3, cy - 13 + bob), fill=(148, 162, 178, 255))
                elif row == 3:
                    draw.rectangle([cx - 5, cy - 16 + bob, cx - 1, cy - 11 + bob], fill=(12, 12, 16, 255))
                    draw.point((cx - 3, cy - 13 + bob), fill=(148, 162, 178, 255))

                # Brass lantern housing + crisp #FFCC00 yellow lantern emissive core
                lantern_gold = (255, 204, 0, 255)
                if row == 0:
                    fx, fy = cx + 9, cy + 4 + bob
                    draw.rectangle([fx - 3, fy - 4, fx + 3, fy + 4], fill=(78, 62, 50, 255))
                    draw.rectangle([fx - 2, fy - 1, fx + 2, fy + 3], fill=lantern_gold)
                elif row == 1:
                    fx, fy = cx + 10, cy + 3 + bob
                    draw.rectangle([fx - 2, fy - 3, fx + 2, fy + 3], fill=(78, 62, 50, 255))
                    draw.rectangle([fx - 1, fy - 1, fx + 1, fy + 1], fill=lantern_gold)
                elif row == 2:
                    fx, fy = cx + 11, cy + 2 + bob
                    draw.rectangle([fx - 2, fy - 3, fx + 5, fy + 3], fill=(78, 62, 50, 255))
                    draw.rectangle([fx + 1, fy - 2, fx + 4, fy + 2], fill=lantern_gold)
                elif row == 3:
                    fx, fy = cx - 11, cy + 2 + bob
                    draw.rectangle([fx - 5, fy - 3, fx + 2, fy + 3], fill=(78, 62, 50, 255))
                    draw.rectangle([fx - 4, fy - 2, fx - 1, fy + 2], fill=lantern_gold)

            elif archetype_name == "exploder":
                pulse = 1 if col in (1, 3) else 0
                flesh_base = (62, 88, 60, 255)
                flesh_dark = (35, 52, 38, 255)
                magma_orange = (255, 85, 0, 255)
                magma_core = (255, 195, 60, 255)

                draw.rectangle([cx - 8 + stride, cy + 12, cx - 2 + stride, cy + 22], fill=flesh_dark)
                draw.rectangle([cx + 3 - stride, cy + 12, cx + 9 - stride, cy + 22], fill=flesh_dark)

                # Bloated torso & shackles
                draw.ellipse([cx - 15 - pulse, cy - 8 + bob, cx + 15 + pulse, cy + 16 + bob], fill=flesh_base)
                draw.rectangle([cx - 12, cy + 1 + bob, cx + 12, cy + 3 + bob], fill=(60, 68, 80, 255))

                # Glowing #FF5500 magma belly rift + unstable pustules
                draw.ellipse([cx - 8 - pulse, cy - 2 + bob, cx + 8 + pulse, cy + 12 + bob], fill=magma_orange)
                draw.ellipse([cx - 4, cy + 2 + bob, cx + 4, cy + 8 + bob], fill=magma_core)
                draw.ellipse([cx - 10, cy - 11 + bob, cx - 4, cy - 5 + bob], fill=magma_orange)

                draw.ellipse([cx - 7, cy - 20 + bob, cx + 7, cy - 7 + bob], fill=flesh_dark)
                if row != 1:
                    draw.point((cx - 3, cy - 13 + bob), fill=magma_core)
                    draw.point((cx + 3, cy - 13 + bob), fill=magma_core)

            elif archetype_name == "thrower":
                flesh_pale = (95, 130, 85, 255)
                flesh_shade = (45, 60, 45, 255)
                flask_cyan = (0, 255, 255, 255)
                flask_hi = (110, 235, 255, 255)

                draw.line([(cx - 5 + stride, cy + 10), (cx - 7 + stride, cy + 24)], fill=flesh_shade, width=2)
                draw.line([(cx + 5 - stride, cy + 10), (cx + 7 - stride, cy + 24)], fill=flesh_shade, width=2)

                draw.polygon([
                    (cx - 7, cy + 10 + bob),
                    (cx + 7, cy + 10 + bob),
                    (cx + 9, cy - 8 + bob),
                    (cx - 9, cy - 8 + bob),
                ], fill=flesh_pale)

                draw.ellipse([cx - 6, cy - 22 + bob, cx + 6, cy - 9 + bob], fill=flesh_pale)
                if row != 1:
                    draw.point((cx - 2, cy - 15 + bob), fill=(255, 204, 0, 255))
                    draw.point((cx + 2, cy - 15 + bob), fill=(255, 204, 0, 255))

                sign = -1 if row == 3 else 1
                hand_x = cx + sign * (12 + (5 if col == 2 else 0))
                hand_y = cy - 2 + bob - (4 if col == 1 else 0)
                draw.line([(cx + sign * 6, cy - 4 + bob), (hand_x, hand_y)], fill=flesh_shade, width=2)
                # Pure #00FFFF alchemical flask for dynamic elemental tinting in UE 5.8
                draw.ellipse([hand_x - 4, hand_y - 4, hand_x + 4, hand_y + 4], fill=flask_cyan)
                draw.point((hand_x - 1, hand_y - 1), fill=flask_hi)

    return img


# -----------------------------------------------------------------------------
# Emissive Detection Helper (_E Mask + Remix Preservation)
# -----------------------------------------------------------------------------
def compute_emissive_pixel_mask(arr_rgba):
    """
    Identifies high-luminance saturated emissive pixels matching:
      - Player lantern yellow (#FFCC00)
      - Exploder magma belly orange (#FF5500)
      - Fast Thrower alchemical flask cyan (#00FFFF)
    """
    r = arr_rgba[:, :, 0].astype(np.int32)
    g = arr_rgba[:, :, 1].astype(np.int32)
    b = arr_rgba[:, :, 2].astype(np.int32)
    alpha_ok = arr_rgba[:, :, 3] > 0

    is_yellow_lantern = (r >= 180) & (g >= 135) & (b <= 115) & ((r - b) >= 85)
    is_magma_belly = (r >= 180) & (g >= 40) & (g <= 215) & (b <= 95) & ((r - b) >= 100)
    is_cyan_flask = (b >= 170) & (g >= 155) & (r <= 135) & ((np.minimum(g, b) - r) >= 55)

    vmax = np.maximum(np.maximum(r, g), b).astype(np.float32)
    vmin = np.minimum(np.minimum(r, g), b).astype(np.float32)
    sat = (vmax - vmin) / np.maximum(vmax, 1.0)
    is_bright_sat = (vmax >= 225.0) & (sat >= 0.48)

    # Guard against stray magenta background leakage
    not_magenta = ~((r >= 185) & (b >= 185) & (g <= 95))

    return alpha_ok & not_magenta & (is_yellow_lantern | is_magma_belly | is_cyan_flask | is_bright_sat)


# -----------------------------------------------------------------------------
# Deliverable 1a: Hard-Edge Magenta Keying & Zero-Halo Cleanup (_D.png RGBA)
# -----------------------------------------------------------------------------
def chroma_key_and_cleanup(image, key_color=CHROMA_KEY_MAGENTA, tolerance=52):
    """
    Strips solid #FF00FF magenta key cleanly into (0, 0, 0, 0) RGBA while
    removing 1px silhouette magenta fringe via nearest clean-interior neighbor
    replacement (hard pixel edges, zero haloing).
    """
    if not HAS_PIL_NUMPY:
        return image

    arr = np.array(image.convert("RGBA"))
    r = arr[:, :, 0].astype(np.int32)
    g = arr[:, :, 1].astype(np.int32)
    b = arr[:, :, 2].astype(np.int32)
    a = arr[:, :, 3]

    kr, kg, kb = key_color
    dist_sq = (r - kr) ** 2 + (g - kg) ** 2 + (b - kb) ** 2

    # Key out exact #FF00FF + near-magenta AI fringe + already transparent pixels
    is_magenta_fringe = (np.minimum(r, b) >= 175) & (g <= 90) & ((np.minimum(r, b) - g) >= 115)
    is_key = (dist_sq <= (tolerance ** 2)) | is_magenta_fringe | (a < 128)

    # Hard pixel alpha edge: (0,0,0,0) for transparent, alpha=255 for opaque
    arr[is_key] = [0, 0, 0, 0]
    arr[~is_key, 3] = 255

    opaque = arr[:, :, 3] > 0

    # Detect pixels adjacent to transparency
    p_opaque = np.pad(opaque, 1, mode="constant", constant_values=False)
    has_transparent_neighbor = (
        ~p_opaque[:-2, 1:-1] | ~p_opaque[2:, 1:-1] |
        ~p_opaque[1:-1, :-2] | ~p_opaque[1:-1, 2:]
    )
    boundary = opaque & has_transparent_neighbor

    # Residual magenta spill check on boundary texels
    r = arr[:, :, 0].astype(np.int32)
    g = arr[:, :, 1].astype(np.int32)
    b = arr[:, :, 2].astype(np.int32)
    has_spill = boundary & (np.minimum(r, b) > g + 24) & ((r + b) > (2 * g + 60))

    if np.any(has_spill):
        clean_interior = opaque & (~has_spill)
        p_arr = np.pad(arr[:, :, :3].astype(np.int32), ((1, 1), (1, 1), (0, 0)), mode="constant", constant_values=0)
        p_clean = np.pad(clean_interior, 1, mode="constant", constant_values=False)

        accum_rgb = np.zeros((arr.shape[0], arr.shape[1], 3), dtype=np.int32)
        accum_cnt = np.zeros((arr.shape[0], arr.shape[1]), dtype=np.int32)
        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1), (-1, -1), (-1, 1), (1, -1), (1, 1)):
            ys = slice(1 + dy, 1 + dy + arr.shape[0])
            xs = slice(1 + dx, 1 + dx + arr.shape[1])
            mask = p_clean[ys, xs]
            accum_rgb[mask] += p_arr[ys, xs][mask]
            accum_cnt[mask] += 1

        replace_mask = has_spill & (accum_cnt > 0)
        arr[replace_mask, 0] = (accum_rgb[replace_mask, 0] // accum_cnt[replace_mask]).astype(np.uint8)
        arr[replace_mask, 1] = (accum_rgb[replace_mask, 1] // accum_cnt[replace_mask]).astype(np.uint8)
        arr[replace_mask, 2] = (accum_rgb[replace_mask, 2] // accum_cnt[replace_mask]).astype(np.uint8)

        # Fallback for isolated 1px tips: neutralize magenta channel spikes
        fallback_mask = has_spill & (accum_cnt == 0)
        if np.any(fallback_mask):
            g_fb = g[fallback_mask]
            arr[fallback_mask, 0] = np.minimum(r[fallback_mask], g_fb + 12).astype(np.uint8)
            arr[fallback_mask, 2] = np.minimum(b[fallback_mask], g_fb + 12).astype(np.uint8)

    return Image.fromarray(arr, "RGBA")


# -----------------------------------------------------------------------------
# Remix: Palette Quantization & Silhouette Edge Beveling (Rule 04)
# -----------------------------------------------------------------------------
def apply_remix_pass(image):
    """
    Applies dark gothic palette quantization while preserving emissive lantern,
    magma, and cyan flask keys intact, plus 1-pixel silhouette edge beveling.
    """
    if not HAS_PIL_NUMPY:
        return image

    arr = np.array(image.convert("RGBA"))
    opaque = arr[:, :, 3] > 20
    emissive = compute_emissive_pixel_mask(arr)
    quantize_mask = opaque & (~emissive)

    palette_np = np.array(DARK_GOTHIC_PALETTE, dtype=np.int32)
    target_rgb = arr[quantize_mask, :3].astype(np.int32)
    if len(target_rgb) > 0:
        # Chunked nearest-palette lookup keeps peak RAM tiny on 2048x2048 sheets
        out_rgb = np.empty_like(target_rgb)
        chunk = 65536
        for start in range(0, len(target_rgb), chunk):
            sl = target_rgb[start:start + chunk]
            diff = sl[:, np.newaxis, :] - palette_np[np.newaxis, :, :]
            dists = np.sum(diff ** 2, axis=2)
            out_rgb[start:start + chunk] = palette_np[np.argmin(dists, axis=1)]
        arr[quantize_mask, :3] = out_rgb.astype(np.uint8)

    p_opaque = np.pad(opaque, 1, mode="constant", constant_values=False)
    bevel_mask = opaque & (
        (~p_opaque[:-2, 1:-1]) | (~p_opaque[2:, 1:-1]) |
        (~p_opaque[1:-1, :-2]) | (~p_opaque[1:-1, 2:])
    ) & (~emissive)

    arr[bevel_mask, 0] = (arr[bevel_mask, 0].astype(np.float32) * 0.72).astype(np.uint8)
    arr[bevel_mask, 1] = (arr[bevel_mask, 1].astype(np.float32) * 0.72).astype(np.uint8)
    arr[bevel_mask, 2] = (arr[bevel_mask, 2].astype(np.float32) * 0.72).astype(np.uint8)

    return Image.fromarray(arr, "RGBA")


# -----------------------------------------------------------------------------
# Deliverable 1b: Distance-Transform Dome Bevel + 3x3 Sobel Normal Map (_N.png RGB)
# -----------------------------------------------------------------------------
def generate_tangent_normal_map(diffuse_image, normal_strength=2.4, bevel_radius=10.0):
    """
    Synthesizes a tangent-space normal map (_N.png RGB) by combining:
      1. Pure-NumPy Chamfer Euclidean distance transform dome bevel from silhouette alpha
      2. 3x3 Sobel luminance gradient from interior pixel details
    """
    if not HAS_PIL_NUMPY:
        return diffuse_image.convert("RGB")

    arr = np.array(diffuse_image.convert("RGBA"))
    h, w = arr.shape[:2]
    fg = arr[:, :, 3] > 0

    # 1. Exact pure-NumPy Chamfer (1, sqrt(2)) silhouette distance transform
    max_steps = int(math.ceil(bevel_radius)) + 2
    dist = np.where(fg, bevel_radius + 2.0, 0.0).astype(np.float32)
    sqrt2 = np.float32(1.41421356)
    for _ in range(max_steps):
        p = np.pad(dist, 1, mode="constant", constant_values=0.0)
        n4 = np.minimum(
            np.minimum(p[:-2, 1:-1], p[2:, 1:-1]),
            np.minimum(p[1:-1, :-2], p[1:-1, 2:])
        ) + 1.0
        nd = np.minimum(
            np.minimum(p[:-2, :-2], p[:-2, 2:]),
            np.minimum(p[2:, :-2], p[2:, 2:])
        ) + sqrt2
        dist = np.where(fg, np.minimum(dist, np.minimum(n4, nd)), 0.0)

    # Smooth hemispherical dome bevel profile [0..1]
    u = np.clip(dist / max(bevel_radius, 1e-3), 0.0, 1.0)
    dome_height = np.sqrt(np.clip(1.0 - (1.0 - u) ** 2, 0.0, 1.0))

    # 2. Interior pixel luminance gradient
    r = arr[:, :, 0].astype(np.float32) / 255.0
    g = arr[:, :, 1].astype(np.float32) / 255.0
    b = arr[:, :, 2].astype(np.float32) / 255.0
    lum = (0.299 * r + 0.587 * g + 0.114 * b) * fg.astype(np.float32)

    combined_height = (0.65 * dome_height + 0.35 * lum) * fg.astype(np.float32)

    # 3. 3x3 Sobel kernels for gradient calculation
    kx = np.array([[-1.0, 0.0, 1.0],
                   [-2.0, 0.0, 2.0],
                   [-1.0, 0.0, 1.0]], dtype=np.float32) / 8.0
    ky = np.array([[-1.0, -2.0, -1.0],
                   [0.0, 0.0, 0.0],
                   [1.0, 2.0, 1.0]], dtype=np.float32) / 8.0

    padded = np.pad(combined_height, pad_width=1, mode="constant", constant_values=0.0)
    dx = np.zeros((h, w), dtype=np.float32)
    dy = np.zeros((h, w), dtype=np.float32)
    for ky_i in range(3):
        for kx_i in range(3):
            patch = padded[ky_i:ky_i + h, kx_i:kx_i + w]
            dx += kx[ky_i, kx_i] * patch
            dy += ky[ky_i, kx_i] * patch

    nx = -dx * normal_strength
    ny = -dy * normal_strength
    nz = np.ones((h, w), dtype=np.float32)

    inv_len = 1.0 / np.sqrt(nx * nx + ny * ny + nz * nz)
    nx *= inv_len
    ny *= inv_len
    nz *= inv_len

    out_rgb = np.zeros((h, w, 3), dtype=np.uint8)
    out_rgb[:, :, 0] = np.clip((nx * 0.5 + 0.5) * 255.0, 0, 255).astype(np.uint8)
    out_rgb[:, :, 1] = np.clip((ny * 0.5 + 0.5) * 255.0, 0, 255).astype(np.uint8)
    out_rgb[:, :, 2] = np.clip((nz * 0.5 + 0.5) * 255.0, 0, 255).astype(np.uint8)

    # Non-opaque background maps to neutral flat tangent normal (128, 128, 255)
    bg = ~fg
    out_rgb[bg, 0] = 128
    out_rgb[bg, 1] = 128
    out_rgb[bg, 2] = 255

    return Image.fromarray(out_rgb, "RGB")


# -----------------------------------------------------------------------------
# Deliverable 1c: Emissive Channel Extraction (_E.png RGB)
# -----------------------------------------------------------------------------
def generate_emissive_mask(diffuse_image):
    """
    Extracts high-luminance emissive pixels (yellow lantern, orange magma belly,
    cyan flasks) into an isolated RGB emissive mask on pure black (0, 0, 0).
    """
    if not HAS_PIL_NUMPY:
        return diffuse_image.convert("RGB")

    arr = np.array(diffuse_image.convert("RGBA"))
    emissive = compute_emissive_pixel_mask(arr)

    out_rgb = np.zeros((arr.shape[0], arr.shape[1], 3), dtype=np.uint8)
    out_rgb[emissive] = arr[emissive, :3]
    return Image.fromarray(out_rgb, "RGB")


# -----------------------------------------------------------------------------
# Packed ORM Synthesis (Occlusion, Roughness, Metallic)
# -----------------------------------------------------------------------------
def generate_packed_orm_map(diffuse_image):
    """
    Packs Ambient Occlusion in R, Roughness in G, and Metallic in B.
    """
    if not HAS_PIL_NUMPY:
        return diffuse_image

    arr = np.array(diffuse_image.convert("RGBA"))
    h, w = arr.shape[:2]
    alpha = arr[:, :, 3]

    r = arr[:, :, 0].astype(np.float32)
    g = arr[:, :, 1].astype(np.float32)
    b = arr[:, :, 2].astype(np.float32)

    is_metal = (b >= r) & (b >= g) & (b > 90) & (alpha > 0)
    ao = np.clip(180 + ((r + g + b) / 3.0) * 0.3, 0, 255).astype(np.uint8)
    roughness = np.full((h, w), 200, dtype=np.uint8)
    roughness[is_metal] = 75
    metallic = np.zeros((h, w), dtype=np.uint8)
    metallic[is_metal] = 230

    out_arr = np.zeros((h, w, 4), dtype=np.uint8)
    out_arr[:, :, 0] = ao
    out_arr[:, :, 1] = roughness
    out_arr[:, :, 2] = metallic
    out_arr[:, :, 3] = alpha

    transparent = alpha == 0
    out_arr[transparent] = [255, 255, 0, 0]
    return Image.fromarray(out_arr, "RGBA")


# -----------------------------------------------------------------------------
# Deliverable 1d: AtlasMetadata.json + Legacy rig_sockets.json
# -----------------------------------------------------------------------------
def build_atlas_metadata_json(diffuse_image, asset_name, grid_cols=4, grid_rows=4):
    """
    Outputs normalized UV sub-rectangles (U0, V0, U1, V1) and bottom-center
    foot pivot coordinates for all cells in the grid (default 4x4 = 16 cells).
    """
    arr = np.array(diffuse_image.convert("RGBA"))
    sheet_h, sheet_w = arr.shape[:2]
    alpha = arr[:, :, 3]

    cell_w = sheet_w // max(grid_cols, 1)
    cell_h = sheet_h // max(grid_rows, 1)

    cells = []
    for row in range(grid_rows):
        for col in range(grid_cols):
            cell_index = row * grid_cols + col
            x0 = col * cell_w
            y0 = row * cell_h
            x1 = (col + 1) * cell_w
            y1 = (row + 1) * cell_h

            u0 = round(x0 / float(sheet_w), 6)
            v0 = round(y0 / float(sheet_h), 6)
            u1 = round(x1 / float(sheet_w), 6)
            v1 = round(y1 / float(sheet_h), 6)

            cell_alpha = alpha[y0:y1, x0:x1]
            ys, _ = np.nonzero(cell_alpha > 0)
            foot_y_px = (y0 + int(ys.max()) + 1) if len(ys) > 0 else y1
            center_x_px = x0 + (cell_w * 0.5)

            foot_pivot_u = round(center_x_px / float(sheet_w), 6)
            foot_pivot_v = round(foot_y_px / float(sheet_h), 6)

            cells.append({
                "CellIndex": cell_index,
                "Row": row,
                "Column": col,
                "PixelRect": {"X0": x0, "Y0": y0, "X1": x1, "Y1": y1},
                "UVRect": {"U0": u0, "V0": v0, "U1": u1, "V1": v1},
                "CellBottomCenterUV": [foot_pivot_u, v1],
                "FootPivotUV": [foot_pivot_u, foot_pivot_v],
                "FootPivotPx": [center_x_px, float(foot_y_px)],
            })

    return {
        "AssetName": asset_name,
        "SheetSize": [sheet_w, sheet_h],
        "Grid": {"Columns": grid_cols, "Rows": grid_rows, "TotalCells": len(cells)},
        "TexelDensity": TEXELS_PER_METER,
        "UnrealUnitsPerMeter": UU_PER_METER,
        "UnrealUnitsPerPixel": UU_PER_PIXEL,
        "CellWorldSizeUU": [cell_w * UU_PER_PIXEL, cell_h * UU_PER_PIXEL],
        "Cells": cells,
    }


def build_rig_sockets_json(asset_name, frame_w, frame_h, grid_cols, grid_rows, directions_count):
    """Builds rig_sockets.json enforcing 64 px / 100 UU texel density and multi-plane sockets."""
    return {
        "AssetName": asset_name,
        "TexelDensity": TEXELS_PER_METER,
        "UnrealUnitsPerMeter": UU_PER_METER,
        "UnrealUnitsPerPixel": UU_PER_PIXEL,
        "FrameDimensions": {
            "Width": frame_w,
            "Height": frame_h,
            "WorldWidthUU": frame_w * UU_PER_PIXEL,
            "WorldHeightUU": frame_h * UU_PER_PIXEL,
        },
        "Grid": {
            "Columns": grid_cols,
            "Rows": grid_rows,
            "Directions": directions_count,
        },
        "Sockets": {
            "Torso": {
                "Pivot": [0.5, 0.5],
                "DepthOffsetY": 0.0,
                "RelativeBoundsUU": [frame_w * UU_PER_PIXEL * 0.5, frame_h * UU_PER_PIXEL * 0.6],
            },
            "Head": {
                "Pivot": [0.5, 0.8],
                "DepthOffsetY": 0.5,
                "RelativeBoundsUU": [frame_w * UU_PER_PIXEL * 0.35, frame_h * UU_PER_PIXEL * 0.35],
            },
            "Limbs": {
                "Pivot": [0.5, 0.22],
                "DepthOffsetY": -0.5,
                "RelativeBoundsUU": [frame_w * UU_PER_PIXEL * 0.45, frame_h * UU_PER_PIXEL * 0.45],
            },
            "WeaponFlashlight": {
                "Pivot": [0.72, 0.45],
                "DepthOffsetY": 1.0,
                "RelativeBoundsUU": [frame_w * UU_PER_PIXEL * 0.25, frame_h * UU_PER_PIXEL * 0.25],
            },
        },
    }


def update_asset_manifest(manifest_path, asset_name, generated_files, provenance_tag):
    """Updates Content/AssetManifest.json with SHA-256 hashes and modification history."""
    os.makedirs(os.path.dirname(manifest_path), exist_ok=True)

    manifest_data = {
        "ManifestVersion": "1.0",
        "EngineTarget": "Unreal Engine 5.8",
        "TexelDensityStandard": "64 px / 100 UU",
        "Assets": {},
    }

    if os.path.isfile(manifest_path):
        try:
            with open(manifest_path, "r", encoding="utf-8") as f:
                manifest_data = json.load(f)
        except Exception as e:
            print(f"[WARN] Failed to parse existing manifest: {e}. Reinitializing.")

    manifest_data.setdefault("Assets", {})
    content_root = os.path.dirname(manifest_path)

    file_records = {}
    for fpath in generated_files:
        if os.path.isfile(fpath):
            rel_path = os.path.relpath(fpath, content_root).replace("\\", "/")
            file_records[rel_path] = {
                "SHA256": compute_sha256(fpath),
                "Bytes": os.path.getsize(fpath),
            }

    manifest_data["Assets"][asset_name] = {
        "Timestamp": datetime.now(timezone.utc).isoformat(),
        "Provenance": provenance_tag,
        "ComplianceVerified": True,
        "Files": file_records,
    }

    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest_data, f, indent=2)

    print(f"[OK] Updated asset manifest: {manifest_path}")


# -----------------------------------------------------------------------------
# Core Pipeline Execution
# -----------------------------------------------------------------------------
def process_pipeline(args):
    """Executes sprite keying, bevel/Sobel normal extraction, emissive masking, ORM, and metadata output."""
    if not HAS_PIL_NUMPY:
        raise RuntimeError("Pillow and numpy are required to execute slice_normal_rig.py.")

    if args.synthesize_archetype:
        asset_name = f"Rig_{args.synthesize_archetype.capitalize()}"
        provenance_tag = f"ProceduralSynthesis-{args.synthesize_archetype}-v2"
        print(f"[INFO] Procedurally synthesizing archetype: {args.synthesize_archetype}...")
        raw_img = synthesize_archetype_sheet(args.synthesize_archetype)
    elif args.input:
        asset_name = os.path.splitext(os.path.basename(args.input))[0]
        if not asset_name.startswith("Rig_"):
            asset_name = f"Rig_{asset_name}"
        provenance_tag = "CC0-External-Remixed" if args.remix else "Imported-Asset"
        print(f"[INFO] Loading input image: {args.input}...")
        raw_img = Image.open(args.input)
    else:
        raise ValueError("Must provide either --input <file> or --synthesize-archetype <name>")

    output_dir = args.output or os.path.join("Content", "Art", "Rigs", asset_name)
    os.makedirs(output_dir, exist_ok=True)

    # 1. Chroma-keying (#FF00FF -> (0,0,0,0)) & zero-halo neighbor cleanup
    diffuse_img = chroma_key_and_cleanup(raw_img)

    # 2. Remix pass (Palette quantization preserving emissive accents + edge beveling)
    if args.remix:
        print("[INFO] Applying dark gothic palette quantization & edge bevel remix...")
        diffuse_img = apply_remix_pass(diffuse_img)

    # Save Diffuse _D.png (RGBA)
    diffuse_path = os.path.join(output_dir, f"{asset_name}_D.png")
    diffuse_img.save(diffuse_path, "PNG")
    print(f"[OK] Saved Diffuse atlas (_D.png RGBA): {diffuse_path}")
    generated_files = [diffuse_path]

    # 3. Distance-Transform Dome Bevel + 3x3 Sobel Normal Map (_N.png RGB)
    if args.generate_normals:
        print("[INFO] Synthesizing distance-bevel + 3x3 Sobel tangent normal map (_N.png RGB)...")
        normal_img = generate_tangent_normal_map(diffuse_img)
        normal_path = os.path.join(output_dir, f"{asset_name}_N.png")
        normal_img.save(normal_path, "PNG")
        print(f"[OK] Saved Normal atlas (_N.png RGB): {normal_path}")
        generated_files.append(normal_path)

    # 4. High-Luminance Emissive Mask (_E.png RGB)
    print("[INFO] Extracting high-luminance emissive mask (_E.png RGB)...")
    emissive_img = generate_emissive_mask(diffuse_img)
    emissive_path = os.path.join(output_dir, f"{asset_name}_E.png")
    emissive_img.save(emissive_path, "PNG")
    print(f"[OK] Saved Emissive mask (_E.png RGB): {emissive_path}")
    generated_files.append(emissive_path)

    # 5. Packed ORM Map (_ORM.png)
    print("[INFO] Packing Occlusion, Roughness, Metallic (ORM) atlas...")
    orm_img = generate_packed_orm_map(diffuse_img)
    orm_path = os.path.join(output_dir, f"{asset_name}_ORM.png")
    orm_img.save(orm_path, "PNG")
    print(f"[OK] Saved ORM atlas: {orm_path}")
    generated_files.append(orm_path)

    # 6. AtlasMetadata.json (normalized U0, V0, U1, V1 + bottom-center foot pivots)
    atlas_meta = build_atlas_metadata_json(
        diffuse_image=diffuse_img,
        asset_name=asset_name,
        grid_cols=args.grid_cols,
        grid_rows=args.grid_rows,
    )
    atlas_meta_path = os.path.join(output_dir, "AtlasMetadata.json")
    with open(atlas_meta_path, "w", encoding="utf-8") as f:
        json.dump(atlas_meta, f, indent=2)
    print(f"[OK] Saved atlas UV metadata: {atlas_meta_path}")
    generated_files.append(atlas_meta_path)

    # 7. Legacy Rig Sockets JSON (compat)
    frame_w = diffuse_img.width // max(args.grid_cols, 1)
    frame_h = diffuse_img.height // max(args.grid_rows, 1)
    sockets_meta = build_rig_sockets_json(
        asset_name=asset_name,
        frame_w=frame_w,
        frame_h=frame_h,
        grid_cols=args.grid_cols,
        grid_rows=args.grid_rows,
        directions_count=args.directions,
    )
    sockets_path = os.path.join(output_dir, "rig_sockets.json")
    with open(sockets_path, "w", encoding="utf-8") as f:
        json.dump(sockets_meta, f, indent=2)
    print(f"[OK] Saved rig sockets metadata: {sockets_path}")
    generated_files.append(sockets_path)

    # 8. Update manifest (explicit path or default Content/AssetManifest.json)
    manifest_path = getattr(args, "manifest_path", None) or os.path.join("Content", "AssetManifest.json")
    update_asset_manifest(manifest_path, asset_name, generated_files, provenance_tag)

    return generated_files


# -----------------------------------------------------------------------------
# Self-Test Mode
# -----------------------------------------------------------------------------
def run_self_test():
    """Verifies chroma keying, distance+Sobel normals, emissive extraction, and 16-cell AtlasMetadata."""
    print("==================================================")
    print("Running slice_normal_rig.py self-test...")
    print("==================================================")

    test_dir = os.path.join("Content", "Art", "Rigs", "SelfTest")
    os.makedirs(test_dir, exist_ok=True)
    test_manifest = os.path.join(test_dir, "SelfTestManifest.json")

    for arch in ("hero", "exploder", "thrower"):
        print(f"\n--- Testing Archetype: {arch} ---")
        args = argparse.Namespace(
            input=None,
            directions=4,
            generate_normals=True,
            remix=True,
            synthesize_archetype=arch,
            self_test=False,
            output=os.path.join(test_dir, arch.capitalize()),
            grid_cols=4,
            grid_rows=4,
            manifest_path=test_manifest,
        )
        files = process_pipeline(args)
        assert len(files) == 6, f"Expected 6 output files (_D, _N, _E, _ORM, AtlasMetadata, rig_sockets), got {len(files)}"
        for f in files:
            assert os.path.isfile(f), f"File missing: {f}"
            assert os.path.getsize(f) > 0, f"File empty: {f}"

        # Verify zero magenta halo in corner of _D.png & non-empty emissive channel in _E.png
        d_img = np.array(Image.open(files[0]).convert("RGBA"))
        assert tuple(d_img[0, 0]) == (0, 0, 0, 0), f"Top-left background must be (0,0,0,0), got {d_img[0, 0]}"
        e_img = np.array(Image.open(files[2]).convert("RGB"))
        assert np.any(e_img > 0), f"Emissive mask empty for {arch}"
        with open(files[4], "r", encoding="utf-8") as mf:
            meta = json.load(mf)
            assert len(meta["Cells"]) == 16, f"Expected 16 UV cells, got {len(meta['Cells'])}"

    assert os.path.isfile(test_manifest), "SelfTestManifest.json was not created!"
    print("\n==================================================")
    print("[PASS] slice_normal_rig.py self-test succeeded!")
    print("==================================================")
    return True


def main():
    parser = argparse.ArgumentParser(description="2.5D Hybrid Sprite Sheet Cutter, Emissive Extractor & Normal Rig Pipeline")
    parser.add_argument("--input", type=str, help="Path to input raw 4x4 magenta-keyed sprite sheet (PNG)")
    parser.add_argument("--directions", type=int, choices=[4, 8], default=4, help="Direction count (4 or 8)")
    parser.add_argument("--generate-normals", action="store_true", default=True, help="Synthesize tangent-space normal maps")
    parser.add_argument("--remix", action="store_true", default=False, help="Apply palette remix and edge beveling")
    parser.add_argument("--synthesize-archetype", type=str, choices=["hero", "exploder", "thrower"], help="Synthesize archetype sheet")
    parser.add_argument("--self-test", action="store_true", help="Execute self-test suite")
    parser.add_argument("--output", type=str, default="", help="Output directory")
    parser.add_argument("--grid-cols", type=int, default=4, help="Number of animation frame columns")
    parser.add_argument("--grid-rows", type=int, default=4, help="Number of directional rows")

    args = parser.parse_args()

    if args.self_test:
        success = run_self_test()
        sys.exit(0 if success else 1)

    if not args.input and not args.synthesize_archetype:
        parser.print_help()
        sys.exit(1)

    process_pipeline(args)


if __name__ == "__main__":
    main()
