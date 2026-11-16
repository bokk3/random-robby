#!/usr/bin/env python3
"""
Deterministic 2x-pixel crisp dark-fantasy sheet generator for v1.3.0:
  1. Content/Art/Raw/ZombieMirrorStalker_Raw.png            (4x4 rig, #FF00FF keyed)
  2. Content/Art/Raw/BossGuardian_Raw.png                   (4x4 rig, #FF00FF keyed)
  3. Content/Art/Raw/PROP_Resting_Ground_Sanctuary_4x4.png  (4x4 sanctuary & vendor props, #FF00FF keyed)

Rendered on a 64x64 logical pixel canvas per cell and nearest-upscaled 2x -> 128x128 per cell (512x512 sheet)
with multi-tone shading, weathered grain, emissive accents, and crisp 1px dark outlines.
"""

from __future__ import annotations

import os
from pathlib import Path
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parent.parent
RAW_DIR = ROOT / "Content/Art/Raw"
CHROMA = (255, 0, 255, 255)
OUTLINE = (9, 10, 16, 255)


def _new_cell() -> tuple[Image.Image, ImageDraw.ImageDraw]:
    img = Image.new("RGBA", (64, 64), CHROMA)
    return img, ImageDraw.Draw(img)


def _outline_and_paste(sheet: Image.Image, cell: Image.Image, col: int, row: int) -> None:
    """Adds a crisp 1px dark outline around non-chroma pixels, then nearest-upscales 2x into 512x512."""
    w, h = cell.size
    src = cell.load()
    outlined = cell.copy()
    dst = outlined.load()
    for y in range(h):
        for x in range(w):
            if src[x, y] != CHROMA:
                continue
            for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                nx, ny = x + dx, y + dy
                if 0 <= nx < w and 0 <= ny < h and src[nx, ny] != CHROMA:
                    dst[x, y] = OUTLINE
                    break
    up = outlined.resize((128, 128), Image.Resampling.NEAREST)
    sheet.paste(up, (col * 128, row * 128))


def draw_mirror_stalker_sheet() -> Path:
    sheet = Image.new("RGBA", (512, 512), CHROMA)
    shroud_deep = (15, 18, 30, 255)
    shroud_dark = (22, 27, 44, 255)
    shroud_mid = (38, 48, 76, 255)
    shroud_hi = (72, 92, 134, 255)
    shard_dark = (36, 108, 152, 255)
    shard_mid = (96, 224, 255, 255)
    shard_hi = (228, 252, 255, 255)
    eye_cyan = (0, 255, 255, 255)

    for row in range(4):
        for col in range(4):
            cell, d = _new_cell()
            cx, cy = 32, 33
            bob = -2 if col in (0, 2) else 1
            stride = -5 if col == 1 else (5 if col == 3 else 0)
            ldrop = 3 if col == 1 else (-2 if col == 3 else 0)
            rdrop = 3 if col == 3 else (-2 if col == 1 else 0)

            # Spectral booted legs with crystal shin highlights
            if row in (0, 1):
                lx = cx - 5 + (stride // 2)
                rx = cx + 5 - (stride // 2)
                d.rectangle([lx - 2, cy + 12 + ldrop, lx + 2, cy + 23 + ldrop], fill=shroud_deep)
                d.rectangle([rx - 2, cy + 12 + rdrop, rx + 2, cy + 23 + rdrop], fill=shroud_dark)
                d.line([(lx - 1, cy + 13 + ldrop), (lx - 1, cy + 20 + ldrop)], fill=shard_dark)
                d.line([(rx + 1, cy + 13 + rdrop), (rx + 1, cy + 20 + rdrop)], fill=shroud_hi)
            else:
                fx = cx + stride
                bx = cx - stride
                d.rectangle([bx - 2, cy + 12 + rdrop, bx + 2, cy + 23 + rdrop], fill=shroud_deep)
                d.rectangle([fx - 2, cy + 12 + ldrop, fx + 3, cy + 24 + ldrop], fill=shroud_hi)

            # Layered tattered obsidian shroud
            d.polygon([
                (cx - 11, cy + 17 + bob), (cx - 7, cy + 11 + bob), (cx - 1, cy + 18 + bob),
                (cx + 5, cy + 11 + bob), (cx + 11, cy + 17 + bob),
                (cx + 9, cy - 6 + bob), (cx - 9, cy - 6 + bob),
            ], fill=shroud_deep)
            d.polygon([
                (cx - 8, cy + 12 + bob), (cx + 8, cy + 12 + bob),
                (cx + 7, cy - 5 + bob), (cx - 7, cy - 5 + bob),
            ], fill=shroud_mid)
            d.line([(cx - 4, cy - 4 + bob), (cx - 5, cy + 10 + bob)], fill=shroud_hi)
            d.line([(cx + 3, cy - 3 + bob), (cx + 4, cy + 9 + bob)], fill=shroud_dark)

            # Jagged ice-blue mirror shards on shoulders & ribcage
            if row == 1:
                d.polygon([(cx - 5, cy - 8 + bob), (cx, cy - 17 + bob), (cx + 5, cy - 4 + bob)], fill=shard_mid)
                d.polygon([(cx - 1, cy - 15 + bob), (cx + 2, cy - 10 + bob), (cx, cy - 6 + bob)], fill=shard_hi)
                d.polygon([(cx - 13, cy - 12 + bob), (cx - 5, cy - 14 + bob), (cx - 6, cy - 1 + bob)], fill=shard_hi)
                d.polygon([(cx + 13, cy - 12 + bob), (cx + 5, cy - 14 + bob), (cx + 6, cy - 1 + bob)], fill=shard_dark)
            else:
                d.polygon([(cx - 14, cy - 13 + bob), (cx - 5, cy - 6 + bob), (cx - 8, cy + 2 + bob)], fill=shard_hi)
                d.polygon([(cx + 14, cy - 13 + bob), (cx + 5, cy - 6 + bob), (cx + 8, cy + 2 + bob)], fill=shard_mid)
                d.polygon([(cx - 1, cy - 3 + bob), (cx + 4, cy + 4 + bob), (cx - 3, cy + 7 + bob)], fill=shard_mid)
                d.point((cx, cy), fill=shard_hi)

            # Pointed spectral cowl & glowing cyan eyes
            hx = cx + (2 if row == 2 else (-2 if row == 3 else 0))
            d.polygon([
                (hx, cy - 23 + bob), (hx + 8, cy - 16 + bob), (hx + 7, cy - 6 + bob),
                (hx - 7, cy - 6 + bob), (hx - 8, cy - 16 + bob),
            ], fill=shroud_hi)
            d.ellipse([hx - 5, cy - 19 + bob, hx + 5, cy - 8 + bob], fill=shroud_deep)
            if row == 0:
                d.rectangle([hx - 4, cy - 15 + bob, hx - 2, cy - 13 + bob], fill=eye_cyan)
                d.rectangle([hx + 2, cy - 15 + bob, hx + 4, cy - 13 + bob], fill=eye_cyan)
            elif row == 2:
                d.rectangle([hx + 1, cy - 15 + bob, hx + 4, cy - 13 + bob], fill=eye_cyan)
            elif row == 3:
                d.rectangle([hx - 4, cy - 15 + bob, hx - 1, cy - 13 + bob], fill=eye_cyan)

            # Clawed limbs swinging in counter-phase
            arm_l_y = cy + (2 if col in (1, 2) else -2) + bob
            arm_r_y = cy + (-2 if col in (1, 2) else 2) + bob
            if row in (0, 1):
                d.line([(cx - 8, cy - 3 + bob), (cx - 14, arm_l_y + 5)], fill=shroud_hi, width=2)
                d.line([(cx + 8, cy - 3 + bob), (cx + 14, arm_r_y + 5)], fill=shroud_hi, width=2)
                d.point((cx - 15, arm_l_y + 6), fill=shard_hi)
                d.point((cx + 15, arm_r_y + 6), fill=shard_hi)
            else:
                sgn = 1 if row == 2 else -1
                reach = 15 + (3 if col == 1 else 0)
                d.line([(cx + sgn * 5, cy - 2 + bob), (cx + sgn * reach, arm_l_y + 2)], fill=shroud_hi, width=2)
                d.point((cx + sgn * (reach + 1), arm_l_y + 2), fill=shard_hi)

            _outline_and_paste(sheet, cell, col, row)

    out_path = RAW_DIR / "ZombieMirrorStalker_Raw.png"
    sheet.save(out_path, "PNG")
    return out_path


def draw_boss_guardian_sheet() -> Path:
    sheet = Image.new("RGBA", (512, 512), CHROMA)
    basalt_deep = (18, 17, 22, 255)
    basalt_dark = (30, 28, 36, 255)
    basalt_mid = (58, 54, 68, 255)
    basalt_hi = (104, 98, 114, 255)
    lava_dark = (220, 60, 10, 255)
    lava_mid = (255, 138, 22, 255)
    lava_core = (255, 236, 108, 255)
    chain_steel = (138, 150, 170, 255)

    for row in range(4):
        for col in range(4):
            cell, d = _new_cell()
            cx, cy = 32, 34
            bob = -2 if col in (0, 2) else 2
            stride = -6 if col == 1 else (6 if col == 3 else 0)
            ldrop = 3 if col == 1 else (-2 if col == 3 else 0)
            rdrop = 3 if col == 3 else (-2 if col == 1 else 0)

            # Heavy basalt legs with ember knee runes
            d.rectangle([cx - 11 + (stride // 2), cy + 10 + ldrop, cx - 3 + (stride // 2), cy + 25 + ldrop], fill=basalt_deep)
            d.rectangle([cx + 3 - (stride // 2), cy + 10 + rdrop, cx + 11 - (stride // 2), cy + 25 + rdrop], fill=basalt_dark)
            d.rectangle([cx - 10 + (stride // 2), cy + 12 + ldrop, cx - 6 + (stride // 2), cy + 15 + ldrop], fill=lava_mid)
            d.rectangle([cx + 6 - (stride // 2), cy + 12 + rdrop, cx + 10 - (stride // 2), cy + 15 + rdrop], fill=lava_dark)

            # Layered basalt cuirass + stone crack highlights
            d.polygon([
                (cx - 17, cy + 12 + bob), (cx + 17, cy + 12 + bob),
                (cx + 20, cy - 10 + bob), (cx - 20, cy - 10 + bob),
            ], fill=basalt_dark)
            d.polygon([
                (cx - 14, cy + 8 + bob), (cx + 14, cy + 8 + bob),
                (cx + 16, cy - 8 + bob), (cx - 16, cy - 8 + bob),
            ], fill=basalt_mid)
            d.line([(cx - 14, cy - 7 + bob), (cx + 14, cy - 7 + bob)], fill=basalt_hi)
            # Riveted shoulder pauldrons
            d.ellipse([cx - 23, cy - 14 + bob, cx - 10, cy - 2 + bob], fill=basalt_mid)
            d.arc([cx - 22, cy - 13 + bob, cx - 11, cy - 3 + bob], start=190, end=350, fill=basalt_hi)
            d.ellipse([cx + 10, cy - 14 + bob, cx + 23, cy - 2 + bob], fill=basalt_mid)
            d.arc([cx + 11, cy - 13 + bob, cx + 22, cy - 3 + bob], start=190, end=350, fill=basalt_hi)

            # Molten magma chest furnace or glowing spinal fissure
            if row != 1:
                d.ellipse([cx - 8, cy - 5 + bob, cx + 8, cy + 7 + bob], fill=lava_dark)
                d.ellipse([cx - 5, cy - 3 + bob, cx + 5, cy + 5 + bob], fill=lava_mid)
                d.ellipse([cx - 2, cy - 1 + bob, cx + 2, cy + 3 + bob], fill=lava_core)
                d.line([(cx - 12, cy - 5 + bob), (cx - 6, cy + bob), (cx + 12, cy - 4 + bob)], fill=lava_mid)
            else:
                d.line([(cx, cy - 9 + bob), (cx, cy + 9 + bob)], fill=lava_core, width=2)
                d.line([(cx - 7, cy - 2 + bob), (cx + 7, cy + 2 + bob)], fill=lava_mid, width=2)

            # Horned obsidian helm
            hx = cx + (3 if row == 2 else (-3 if row == 3 else 0))
            d.rectangle([hx - 7, cy - 21 + bob, hx + 7, cy - 9 + bob], fill=basalt_mid)
            d.rectangle([hx - 6, cy - 20 + bob, hx + 6, cy - 18 + bob], fill=basalt_hi)
            d.polygon([(hx - 7, cy - 19 + bob), (hx - 15, cy - 28 + bob), (hx - 4, cy - 22 + bob)], fill=basalt_hi)
            d.polygon([(hx + 7, cy - 19 + bob), (hx + 15, cy - 28 + bob), (hx + 4, cy - 22 + bob)], fill=basalt_hi)
            if row == 0:
                d.rectangle([hx - 4, cy - 15 + bob, hx - 2, cy - 13 + bob], fill=lava_core)
                d.rectangle([hx + 2, cy - 15 + bob, hx + 4, cy - 13 + bob], fill=lava_core)
            elif row == 2:
                d.rectangle([hx + 1, cy - 15 + bob, hx + 5, cy - 13 + bob], fill=lava_core)
            elif row == 3:
                d.rectangle([hx - 5, cy - 15 + bob, hx - 1, cy - 13 + bob], fill=lava_core)

            # Heavy chained gauntlets (Col 2 raises fists for smash silhouette)
            raise_y = -8 if col == 2 else 3
            d.rectangle([cx - 24, cy + raise_y + bob, cx - 15, cy + raise_y + 10 + bob], fill=basalt_hi)
            d.rectangle([cx + 15, cy + raise_y + bob, cx + 24, cy + raise_y + 10 + bob], fill=basalt_hi)
            d.rectangle([cx - 23, cy + raise_y + 2 + bob, cx - 16, cy + raise_y + 5 + bob], fill=chain_steel)
            d.rectangle([cx + 16, cy + raise_y + 2 + bob, cx + 23, cy + raise_y + 5 + bob], fill=chain_steel)

            _outline_and_paste(sheet, cell, col, row)

    out_path = RAW_DIR / "BossGuardian_Raw.png"
    sheet.save(out_path, "PNG")
    return out_path


def draw_sanctuary_props_sheet() -> Path:
    sheet = Image.new("RGBA", (512, 512), CHROMA)
    stone_deep = (26, 29, 38, 255)
    stone_dark = (40, 46, 60, 255)
    stone_mid = (72, 82, 102, 255)
    stone_hi = (122, 136, 162, 255)
    iron = (44, 48, 60, 255)
    gold = (224, 174, 48, 255)
    gold_hi = (255, 234, 136, 255)
    flame_out = (255, 86, 18, 255)
    flame_mid = (255, 168, 34, 255)
    flame_in = (255, 242, 148, 255)
    cyan_glow = (30, 238, 255, 255)
    violet_glow = (176, 86, 255, 255)
    wood_dark = (58, 38, 24, 255)
    wood_mid = (96, 64, 40, 255)
    wood_hi = (132, 92, 58, 255)
    cloth_indigo = (42, 48, 96, 255)
    cloth_hi = (78, 88, 152, 255)

    for row in range(4):
        for col in range(4):
            cell, d = _new_cell()
            cx, cy = 32, 36
            jitter = (col % 2) * 2 - 1

            if row == 0:
                # Row 0: Ornate Sanctuary Campfire Brazier with carved pedestal, iron ribs & roaring fire
                d.ellipse([cx - 19, cy + 9, cx + 19, cy + 21], fill=stone_deep)
                d.ellipse([cx - 16, cy + 10, cx + 16, cy + 17], fill=stone_dark)
                d.rectangle([cx - 14, cy + 2, cx + 14, cy + 12], fill=iron)
                for rx in (-10, -4, 4, 10):
                    d.line([(cx + rx, cy + 2), (cx + rx, cy + 12)], fill=stone_hi)
                d.ellipse([cx - 17, cy - 3, cx + 17, cy + 7], fill=stone_mid)
                d.arc([cx - 16, cy - 3, cx + 16, cy + 6], start=185, end=355, fill=stone_hi)
                d.ellipse([cx - 12, cy, cx + 12, cy + 5], fill=flame_out)
                # Multi-tongue fire & embers
                d.polygon([
                    (cx - 11, cy + 2), (cx - 6 + jitter, cy - 17), (cx, cy - 26 - (col % 2) * 2),
                    (cx + 6 - jitter, cy - 16), (cx + 11, cy + 2),
                ], fill=flame_out)
                d.polygon([
                    (cx - 7, cy + 2), (cx - 2, cy - 13), (cx + 1 + jitter, cy - 20),
                    (cx + 6, cy - 11), (cx + 7, cy + 2),
                ], fill=flame_mid)
                d.ellipse([cx - 4, cy - 8, cx + 4, cy + 2], fill=flame_in)
                d.point((cx - 8 + col * 3, cy - 25), fill=flame_in)
                d.point((cx + 8 - col * 3, cy - 22), fill=flame_mid)

            elif row == 1:
                # Row 1: Mossy Obsidian Sensory Upgrade Monolith Altar + Floating Third-Eye Rune
                d.rectangle([cx - 17, cy + 11, cx + 17, cy + 21], fill=stone_deep)
                d.rectangle([cx - 15, cy + 11, cx + 15, cy + 14], fill=stone_dark)
                d.polygon([
                    (cx - 11, cy + 13), (cx + 11, cy + 13),
                    (cx + 8, cy - 12), (cx, cy - 21), (cx - 8, cy - 12),
                ], fill=stone_mid)
                d.line([(cx - 7, cy - 11), (cx - 7, cy + 11)], fill=stone_hi)
                d.line([(cx + 6, cy - 11), (cx + 6, cy + 11)], fill=stone_dark)
                d.line([(cx - 4, cy - 3), (cx + 4, cy - 3)], fill=stone_deep)
                # Vibrant emerald moss clusters on plinth corners
                d.ellipse([cx - 14, cy + 8, cx - 5, cy + 15], fill=(38, 168, 76, 255))
                d.ellipse([cx + 5, cy + 9, cx + 14, cy + 15], fill=(56, 206, 98, 255))
                # Floating cyan/violet Third-Eye sigil
                ry = cy - 4 + (col % 2) - 1
                d.polygon([(cx, ry - 9), (cx + 7, ry), (cx, ry + 9), (cx - 7, ry)], fill=violet_glow)
                d.polygon([(cx, ry - 6), (cx + 4, ry), (cx, ry + 6), (cx - 4, ry)], fill=cyan_glow)
                d.point((cx, ry), fill=(255, 255, 255, 255))

            elif row == 2:
                # Row 2: Ornate Apothecary Merchant Stall with IndigoAwning, Bottle Necks & Teal Lantern
                d.rectangle([cx - 21, cy + 5, cx + 21, cy + 19], fill=wood_dark)
                d.rectangle([cx - 21, cy + 4, cx + 21, cy + 7], fill=wood_hi)
                for px in range(-16, 17, 8):
                    d.line([(cx + px, cy + 7), (cx + px, cy + 18)], fill=wood_mid)
                d.rectangle([cx - 18, cy - 12, cx - 15, cy + 5], fill=wood_mid)
                d.rectangle([cx + 15, cy - 12, cx + 18, cy + 5], fill=wood_mid)
                # Indigo awning with scalloped cloth stripes
                d.polygon([(cx - 23, cy - 10), (cx + 23, cy - 10), (cx + 19, cy - 22), (cx - 19, cy - 22)], fill=cloth_indigo)
                for sx in range(-16, 18, 8):
                    d.line([(cx + sx, cy - 21), (cx + sx - 1, cy - 11)], fill=cloth_hi, width=2)
                # Bottled elixirs with corks & specular shine
                d.rectangle([cx - 13, cy - 1, cx - 8, cy + 4], fill=(38, 212, 118, 255))
                d.rectangle([cx - 11, cy - 3, cx - 10, cy - 1], fill=stone_hi)
                d.rectangle([cx - 5, cy - 2, cx, cy + 4], fill=violet_glow)
                d.rectangle([cx - 3, cy - 4, cx - 2, cy - 2], fill=gold_hi)
                d.rectangle([cx + 3, cy, cx + 8, cy + 4], fill=gold)
                # Swaying cyan glass lantern hanging from right beam
                lx = cx + 12 + jitter
                d.line([(lx, cy - 10), (lx, cy - 7)], fill=iron)
                d.rectangle([lx - 3, cy - 7, lx + 3, cy], fill=iron)
                d.rectangle([lx - 2, cy - 6, lx + 2, cy - 1], fill=cyan_glow)

            else:
                # Row 3: Gold-Bound Vault Treasure Chest Overflowing with Coins & Emerald Gems
                d.rectangle([cx - 20, cy + 12, cx + 20, cy + 20], fill=stone_deep)
                d.rectangle([cx - 17, cy, cx + 17, cy + 14], fill=wood_dark)
                for py in (4, 9):
                    d.line([(cx - 15, cy + py), (cx + 15, cy + py)], fill=wood_mid)
                d.polygon([(cx - 18, cy), (cx + 18, cy), (cx + 15, cy - 12), (cx - 15, cy - 12)], fill=wood_mid)
                d.line([(cx - 15, cy - 10), (cx + 15, cy - 10)], fill=wood_hi)
                # Burnished gold straps & ornate clasp
                d.rectangle([cx - 17, cy - 2, cx + 17, cy + 1], fill=gold)
                d.rectangle([cx - 14, cy - 12, cx - 11, cy + 14], fill=gold)
                d.rectangle([cx + 11, cy - 12, cx + 14, cy + 14], fill=gold)
                d.rectangle([cx - 3, cy - 3, cx + 3, cy + 5], fill=gold_hi)
                d.point((cx, cy + 1), fill=stone_deep)
                # Radiant treasure hoard
                d.ellipse([cx - 10, cy - 8, cx + 10, cy - 2], fill=flame_mid)
                d.ellipse([cx - 6, cy - 7, cx + 6, cy - 3], fill=gold_hi)
                d.point((cx - 4 + col, cy - 5), fill=(40, 255, 160, 255))
                d.point((cx + 5 - col, cy - 6), fill=cyan_glow)

            _outline_and_paste(sheet, cell, col, row)

    out_path = RAW_DIR / "PROP_Resting_Ground_Sanctuary_4x4.png"
    sheet.save(out_path, "PNG")
    return out_path


def main() -> None:
    os.makedirs(RAW_DIR, exist_ok=True)
    p1 = draw_mirror_stalker_sheet()
    p2 = draw_boss_guardian_sheet()
    p3 = draw_sanctuary_props_sheet()
    print(f"Generated raw sheets:\n  {p1}\n  {p2}\n  {p3}")


if __name__ == "__main__":
    main()
