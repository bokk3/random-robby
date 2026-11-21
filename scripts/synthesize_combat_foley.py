#!/usr/bin/env python3
"""
synthesize_combat_foley.py

Builds the combat sound bank procedurally (no recorded samples) as mono 22.05 kHz 16-bit WAVs in
Content/Audio/Foley/Combat/, and records each file's SHA-256 in Content/AssetManifest.json under
"Combat_Foley_Bank". Output is deterministic: the same script always writes the same bytes.

The asset bootstrap imports every WAV under Content/Audio/Foley at startup, keyed by file name, so
the game refers to these as e.g. "Combat_Swing_0". Files with "Loop_" in the name loop.

Tone: glass, brass and candle-light (see docs/STORY.md). Hits ring a little like struck glass;
the parry is a lantern shutter clanging; blasts are kiln fire.
"""

from __future__ import annotations

import hashlib
import json
import sys
from datetime import datetime, timezone
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
from synthesize_ecosystem_foley import SAMPLE_RATE, _bandlimit_noise, _write_wav  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
COMBAT_DIR = ROOT / "Content" / "Audio" / "Foley" / "Combat"
MANIFEST_PATH = ROOT / "Content" / "AssetManifest.json"


def _t(seconds: float) -> np.ndarray:
    return np.arange(int(SAMPLE_RATE * seconds)) / SAMPLE_RATE


def _env(t: np.ndarray, attack: float, decay: float) -> np.ndarray:
    """Linear attack then exponential decay (decay = time constant in seconds)."""
    a = np.clip(t / max(attack, 1e-4), 0.0, 1.0)
    return a * np.exp(-np.maximum(t - attack, 0.0) / max(decay, 1e-4))


def _sweep(t: np.ndarray, f0: float, f1: float) -> np.ndarray:
    """Sine with an exponential pitch sweep from f0 to f1 over the length of t."""
    dur = max(t[-1], 1e-4)
    k = np.log(f1 / f0) / dur
    phase = 2.0 * np.pi * f0 * (np.exp(k * t) - 1.0) / k
    return np.sin(phase)


def _bells(t: np.ndarray, freqs: list[float], decays: list[float], gains: list[float]) -> np.ndarray:
    out = np.zeros_like(t)
    for f, d, g in zip(freqs, decays, gains):
        out += g * np.sin(2.0 * np.pi * f * t) * np.exp(-t / d)
    return out


def swing(seed: int) -> np.ndarray:
    rng = np.random.default_rng(1000 + seed)
    t = _t(0.24)
    low = _bandlimit_noise(rng, len(t), 0.02, 0.18)
    high = _bandlimit_noise(rng, len(t), 0.08, 0.6)
    # The whoosh brightens as the lantern passes, then falls away.
    x = t / t[-1]
    shape = np.sin(np.pi * np.clip(x * 1.15, 0.0, 1.0)) ** 2
    mix = low * (1.0 - x) + high * x
    return mix * shape * (0.9 + 0.1 * seed)


def hit(seed: int) -> np.ndarray:
    rng = np.random.default_rng(2000 + seed)
    t = _t(0.26)
    thump = _sweep(t, 110.0 - 10 * seed, 48.0) * _env(t, 0.002, 0.05)
    crack = _bandlimit_noise(rng, len(t), 0.15, 0.85) * _env(t, 0.0005, 0.012)
    glass = _bells(t, [2350.0 + 140 * seed, 3710.0], [0.05, 0.03], [0.22, 0.12])
    return thump * 1.0 + crack * 0.7 + glass


def heavy() -> np.ndarray:
    rng = np.random.default_rng(3000)
    t = _t(0.6)
    boom = _sweep(t, 75.0, 32.0) * _env(t, 0.003, 0.14)
    rubble = _bandlimit_noise(rng, len(t), 0.01, 0.12) * _env(t, 0.004, 0.18)
    crack = _bandlimit_noise(rng, len(t), 0.2, 0.9) * _env(t, 0.0005, 0.02)
    return boom * 1.2 + rubble * 0.55 + crack * 0.5


def parry() -> np.ndarray:
    rng = np.random.default_rng(4000)
    t = _t(0.95)
    # Brass shutter: inharmonic partials of a struck plate.
    ring = _bells(t, [523.0, 1347.0, 2211.0, 3119.0, 4410.0], [0.45, 0.3, 0.2, 0.12, 0.08], [0.6, 0.45, 0.35, 0.25, 0.12])
    click = _bandlimit_noise(rng, len(t), 0.3, 0.95) * _env(t, 0.0003, 0.006)
    return ring + click * 0.8


def perfect_dodge() -> np.ndarray:
    t = _t(0.7)
    shimmer = np.zeros_like(t)
    for i, f in enumerate([880.0, 1318.5, 1760.0, 2637.0]):
        shimmer += _sweep(t, f * 0.75, f) * (0.5 / (i + 1) + 0.15)
    trem = 0.65 + 0.35 * np.sin(2.0 * np.pi * 18.0 * t)
    return shimmer * trem * _env(t, 0.04, 0.22)


def hero_hurt() -> np.ndarray:
    rng = np.random.default_rng(5000)
    t = _t(0.22)
    thud = _sweep(t, 140.0, 70.0) * _env(t, 0.002, 0.05)
    cloth = _bandlimit_noise(rng, len(t), 0.03, 0.3) * _env(t, 0.002, 0.04)
    return thud + cloth * 0.5


def zombie_death() -> np.ndarray:
    rng = np.random.default_rng(6000)
    t = _t(0.65)
    body = _bandlimit_noise(rng, len(t), 0.005, 0.08) * _env(t, 0.01, 0.2)
    wobble = 0.6 + 0.4 * np.sin(2.0 * np.pi * 9.0 * t)
    groan = _sweep(t, 160.0, 70.0) * _env(t, 0.02, 0.18) * 0.35
    return body * wobble + groan


def windup() -> np.ndarray:
    """Enemy wind-up warning: a rising, breathy growl that peaks just before the strike."""
    rng = np.random.default_rng(13000)
    t = _t(0.55)
    rise = (t / t[-1]) ** 1.5
    growl = _sweep(t, 70.0, 150.0) * (0.6 + 0.4 * np.sin(2.0 * np.pi * 23.0 * t))
    breath = _bandlimit_noise(rng, len(t), 0.03, 0.35)
    return (growl * 0.8 + breath * 0.5) * rise


def fuse_tick() -> np.ndarray:
    t = _t(0.05)
    return _bells(t, [1620.0, 2450.0], [0.012, 0.008], [1.0, 0.4])


def blast() -> np.ndarray:
    rng = np.random.default_rng(7000)
    t = _t(1.15)
    roar = _bandlimit_noise(rng, len(t), 0.004, 0.1) * _env(t, 0.004, 0.32)
    boom = _sweep(t, 60.0, 28.0) * _env(t, 0.002, 0.25)
    crackle = _bandlimit_noise(rng, len(t), 0.25, 0.95) * _env(t, 0.001, 0.06)
    return roar * 0.9 + boom * 1.1 + crackle * 0.4


def shatter() -> np.ndarray:
    rng = np.random.default_rng(8000)
    t = _t(0.6)
    out = _bandlimit_noise(rng, len(t), 0.3, 0.95) * _env(t, 0.0005, 0.03) * 0.6
    for k in range(9):
        start = int(rng.uniform(0.0, 0.18) * SAMPLE_RATE)
        f = rng.uniform(2200.0, 6200.0)
        seg = t[: len(t) - start]
        out[start:] += np.sin(2.0 * np.pi * f * seg) * np.exp(-seg / rng.uniform(0.03, 0.09)) * rng.uniform(0.2, 0.45)
    return out


def gas_loop() -> np.ndarray:
    rng = np.random.default_rng(9000)
    t = _t(1.6)
    hiss = _bandlimit_noise(rng, len(t), 0.12, 0.7)
    breathe = 0.7 + 0.3 * np.sin(2.0 * np.pi * (1.0 / 1.6) * t)
    return hiss * breathe


def skill_kiln() -> np.ndarray:
    rng = np.random.default_rng(10000)
    t = _t(0.5)
    whoosh = swing(1)[: len(t)] if len(swing(1)) >= len(t) else np.pad(swing(1), (0, len(t) - len(swing(1))))
    fire = _bandlimit_noise(rng, len(t), 0.02, 0.4) * _env(t, 0.08, 0.15)
    return whoosh * 0.6 + fire * 0.7


def skill_hook() -> np.ndarray:
    rng = np.random.default_rng(11000)
    t = _t(0.45)
    out = np.zeros_like(t)
    for k in range(10):  # chain links rattling out
        start = int((0.02 + k * 0.03) * SAMPLE_RATE)
        seg = t[: len(t) - start]
        out[start:] += _bells(seg, [2900.0 + 37 * k, 4100.0], [0.02, 0.012], [0.5, 0.25])
    out += _bandlimit_noise(rng, len(t), 0.1, 0.6) * _env(t, 0.01, 0.1) * 0.25
    return out


def skill_shade() -> np.ndarray:
    t = _t(0.5)
    # A reversed swell: comes up out of nothing and cuts off, like stepping out of a reflection.
    swell = _sweep(t, 300.0, 900.0) * (t / t[-1]) ** 3
    return swell + _bells(t, [1200.0, 1800.0], [0.2, 0.15], [0.1, 0.08]) * (t / t[-1]) ** 2


def skill_wheel() -> np.ndarray:
    rng = np.random.default_rng(12000)
    t = _t(0.9)
    noise = _bandlimit_noise(rng, len(t), 0.05, 0.5)
    spin = 0.5 + 0.5 * np.sin(2.0 * np.pi * 7.0 * t) ** 2
    return noise * spin * _env(t, 0.05, 0.5)


def skill_noon() -> np.ndarray:
    t = _t(0.9)
    chord = _bells(t, [523.25, 659.25, 783.99, 1046.5], [0.5, 0.45, 0.4, 0.35], [0.5, 0.4, 0.35, 0.3])
    return chord * np.clip(t / 0.05, 0.0, 1.0)


def skill_slipped() -> np.ndarray:
    t = _t(0.8)
    return _bells(t, [1567.98, 2093.0, 2637.0, 3135.96], [0.35, 0.3, 0.25, 0.2], [0.4, 0.35, 0.3, 0.2])


def main() -> int:
    one_shots: dict[str, np.ndarray] = {}
    for i in range(3):
        one_shots[f"Combat_Swing_{i}"] = swing(i)
        one_shots[f"Combat_Hit_{i}"] = hit(i)
    one_shots.update({
        "Combat_Heavy_0": heavy(),
        "Combat_Parry_0": parry(),
        "Combat_PerfectDodge_0": perfect_dodge(),
        "Combat_HeroHurt_0": hero_hurt(),
        "Combat_ZombieDeath_0": zombie_death(),
        "Combat_FuseTick_0": fuse_tick(),
        "Combat_Windup_0": windup(),
        "Combat_Blast_0": blast(),
        "Combat_Shatter_0": shatter(),
        "Combat_Skill_Kiln": skill_kiln(),
        "Combat_Skill_Hook": skill_hook(),
        "Combat_Skill_Shade": skill_shade(),
        "Combat_Skill_Wheel": skill_wheel(),
        "Combat_Skill_Noon": skill_noon(),
        "Combat_Skill_Slipped": skill_slipped(),
    })
    loops = {"Combat_Loop_GasHiss": gas_loop()}

    content_root = ROOT / "Content"
    files: dict[str, dict] = {}
    for name, sig in sorted(one_shots.items()):
        path = COMBAT_DIR / f"{name}.wav"
        sha = _write_wav(path, sig.astype(np.float64), target_peak=0.7, loop=False)
        files[path.relative_to(content_root).as_posix()] = {"SHA256": sha, "Bytes": path.stat().st_size}
    for name, sig in sorted(loops.items()):
        path = COMBAT_DIR / f"{name}.wav"
        sha = _write_wav(path, sig.astype(np.float64), target_peak=0.5, loop=True)
        files[path.relative_to(content_root).as_posix()] = {"SHA256": sha, "Bytes": path.stat().st_size}

    manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))
    manifest.setdefault("Assets", {})
    previous = manifest["Assets"].get("Combat_Foley_Bank", {})
    # Keep the timestamp stable when nothing changed, so reruns don't dirty the manifest.
    timestamp = previous.get("Timestamp") if previous.get("Files") == files else datetime.now(timezone.utc).isoformat()
    manifest["Assets"]["Combat_Foley_Bank"] = {
        "Timestamp": timestamp,
        "Provenance": "ProceduralDSP-CombatFoley-v1 (scripts/synthesize_combat_foley.py)",
        "ComplianceVerified": True,
        "Files": files,
    }
    MANIFEST_PATH.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"[OK] Wrote {len(files)} combat WAVs to {COMBAT_DIR} and updated the asset manifest.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
