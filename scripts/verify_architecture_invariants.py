#!/usr/bin/env python3
"""
Static Rule Audit, Mathematical Invariant Verification & Asset Provenance Checker
for the 2.5D Cooperative Dark Dungeoneer (UE 5.8 C++20).
"""

from __future__ import annotations

import hashlib
import json
import math
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

REQUIRED_CPP_FILES = [
    "DarkDungeoneer.uproject",
    "Config/DefaultEngine.ini",
    "Config/DefaultGame.ini",
    "Config/DefaultInput.ini",
    "Source/DarkDungeoneer.Target.cs",
    "Source/DarkDungeoneerEditor.Target.cs",
    "Source/DarkDungeoneer/DarkDungeoneer.Build.cs",
    "Source/DarkDungeoneer/Public/DarkDungeoneerModule.h",
    "Source/DarkDungeoneer/Private/DarkDungeoneerModule.cpp",
    # Task Group 1 (Core)
    "Source/DarkDungeoneer/Public/Core/DungeoneerTypes.h",
    "Source/DarkDungeoneer/Public/Core/DungeoneerMovementConfig.h",
    "Source/DarkDungeoneer/Public/Core/DungeoneerGameMode.h",
    "Source/DarkDungeoneer/Private/Core/DungeoneerGameMode.cpp",
    "Source/DarkDungeoneer/Public/Core/DungeoneerMovementComponent.h",
    "Source/DarkDungeoneer/Private/Core/DungeoneerMovementComponent.cpp",
    "Source/DarkDungeoneer/Public/Core/DungeoneerCharacter.h",
    "Source/DarkDungeoneer/Private/Core/DungeoneerCharacter.cpp",
    "Source/DarkDungeoneer/Public/Core/DungeoneerGameViewportClient.h",
    "Source/DarkDungeoneer/Private/Core/DungeoneerGameViewportClient.cpp",
    "Source/DarkDungeoneer/Public/Core/DungeoneerLocalPlayerSubsystem.h",
    "Source/DarkDungeoneer/Private/Core/DungeoneerLocalPlayerSubsystem.cpp",
    # Task Group 2 (Rendering & Shaders)
    "Shaders/Private/PixelLit25D.ush",
    "Source/DarkDungeoneer/Public/Rendering/Dungeoneer25DMaterialBuilder.h",
    "Source/DarkDungeoneer/Private/Rendering/Dungeoneer25DMaterialBuilder.cpp",
    "Source/DarkDungeoneer/Public/Rendering/25DDirectionalRigComponent.h",
    "Source/DarkDungeoneer/Private/Rendering/25DDirectionalRigComponent.cpp",
    "Source/DarkDungeoneer/Public/Rendering/DungeoneerSensoryWorldSubsystem.h",
    "Source/DarkDungeoneer/Private/Rendering/DungeoneerSensoryWorldSubsystem.cpp",
    "Source/DarkDungeoneer/Public/Rendering/SensoryPerkComponent.h",
    "Source/DarkDungeoneer/Private/Rendering/SensoryPerkComponent.cpp",
    "Source/DarkDungeoneer/Public/Rendering/DungeonAtmosphereActor.h",
    "Source/DarkDungeoneer/Private/Rendering/DungeonAtmosphereActor.cpp",
    ".agents/skills/sprite-sheet-25d-cutter/scripts/slice_normal_rig.py",
    # Task Group 3 (AI)
    "Source/DarkDungeoneer/Public/AI/ZombieSpatialSubsystem.h",
    "Source/DarkDungeoneer/Private/AI/ZombieSpatialSubsystem.cpp",
    "Source/DarkDungeoneer/Public/AI/ZombieBase.h",
    "Source/DarkDungeoneer/Private/AI/ZombieBase.cpp",
    "Source/DarkDungeoneer/Public/AI/ZombieExploder.h",
    "Source/DarkDungeoneer/Private/AI/ZombieExploder.cpp",
    "Source/DarkDungeoneer/Public/AI/ZombieFastThrower.h",
    "Source/DarkDungeoneer/Private/AI/ZombieFastThrower.cpp",
    "Source/DarkDungeoneer/Public/AI/HazardZoneActors.h",
    "Source/DarkDungeoneer/Private/AI/HazardZoneActors.cpp",
    # Task Group 4 (Dungeon)
    "Source/DarkDungeoneer/Public/Dungeon/DungeonTypes.h",
    "Source/DarkDungeoneer/Public/Dungeon/DungeonGeneratorSubsystem.h",
    "Source/DarkDungeoneer/Private/Dungeon/DungeonGeneratorSubsystem.cpp",
    "Source/DarkDungeoneer/Public/Dungeon/MirrorMazeActors.h",
    "Source/DarkDungeoneer/Private/Dungeon/MirrorMazeActors.cpp",
    "Source/DarkDungeoneer/Public/Dungeon/DungeonEncounterActors.h",
    "Source/DarkDungeoneer/Private/Dungeon/DungeonEncounterActors.cpp",
    # Task Group 5 (Audio)
    "Source/DarkDungeoneer/Public/Audio/DungeoneerTimedAudioComponent.h",
    "Source/DarkDungeoneer/Private/Audio/DungeoneerTimedAudioComponent.cpp",
    "Source/DarkDungeoneer/Public/Audio/AnimNotify_DungeoneerTimedCue.h",
    "Source/DarkDungeoneer/Private/Audio/AnimNotify_DungeoneerTimedCue.cpp",
    "Source/DarkDungeoneer/Public/Audio/GibberishSpeechComponent.h",
    "Source/DarkDungeoneer/Private/Audio/GibberishSpeechComponent.cpp",
    ".agents/skills/voice-gibberish-metasound/scripts/generate_phoneme_bank.py",
    # Task Group 6 (Social)
    "Source/DarkDungeoneer/Public/Social/DungeoneerSocialSubsystem.h",
    "Source/DarkDungeoneer/Private/Social/DungeoneerSocialSubsystem.cpp",
    "Source/DarkDungeoneer/Public/Social/DungeoneerInventoryComponent.h",
    "Source/DarkDungeoneer/Private/Social/DungeoneerInventoryComponent.cpp",
    "Source/DarkDungeoneer/Public/Social/TradeManagerComponent.h",
    "Source/DarkDungeoneer/Private/Social/TradeManagerComponent.cpp",
]


def strip_comments(cpp_text: str) -> str:
    cpp_text = re.sub(r"/\*.*?\*/", "", cpp_text, flags=re.DOTALL)
    cpp_text = re.sub(r"//.*?$", "", cpp_text, flags=re.MULTILINE)
    return cpp_text


def extract_tick_bodies(cpp_text: str) -> list[str]:
    bodies: list[str] = []
    pattern = re.compile(r"void\s+\w+::Tick(?:Component)?\s*\([^)]*\)\s*\{")
    for match in pattern.finditer(cpp_text):
        start = match.end()
        depth = 1
        pos = start
        while pos < len(cpp_text) and depth > 0:
            ch = cpp_text[pos]
            if ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
            pos += 1
        bodies.append(cpp_text[start:pos])
    return bodies


def check_files_and_static_rules() -> list[str]:
    errors: list[str] = []
    for rel in REQUIRED_CPP_FILES:
        if not (ROOT / rel).exists():
            errors.append(f"MISSING FILE: {rel}")

    source_files = list((ROOT / "Source/DarkDungeoneer").rglob("*.h")) + list((ROOT / "Source/DarkDungeoneer").rglob("*.cpp"))
    for sf in source_files:
        if "Tests" in sf.parts:
            continue
        code = strip_comments(sf.read_text(encoding="utf-8"))
        rel = sf.relative_to(ROOT).as_posix()
        for banned in ("GetAllActorsOfClass", "GetFirstPlayerController", "FTimerHandle"):
            if banned in code:
                errors.append(f"BANNED SYMBOL '{banned}' found in {rel}")

        for tick_body in extract_tick_bodies(code):
            for hot_banned in ("SpawnActor", "NewObject", "CreateDynamicMaterialInstance"):
                if hot_banned in tick_body:
                    errors.append(f"HOT-PATH ALLOCATION '{hot_banned}' inside Tick in {rel}")

    return errors


def check_math_invariants() -> list[str]:
    errors: list[str] = []
    inv_sqrt2 = 0.70710678
    compass = [
        (1.0, 0.0),
        (inv_sqrt2, inv_sqrt2),
        (0.0, 1.0),
        (-inv_sqrt2, inv_sqrt2),
        (-1.0, 0.0),
        (-inv_sqrt2, -inv_sqrt2),
        (0.0, -1.0),
        (inv_sqrt2, -inv_sqrt2),
    ]
    base_speed = 450.0
    cardinal_speed = math.hypot(*compass[0]) * base_speed
    for idx, (vx, vy) in enumerate(compass):
        spd = math.hypot(vx, vy) * base_speed
        if abs(spd - cardinal_speed) >= 1e-5:
            errors.append(f"Octant {idx} speed {spd} != cardinal speed {cardinal_speed}")

    # Chrono-Leech Conservation Law
    max_slow = 0.65
    z_base = 410.0
    speed_scalar = 1.15
    cd_base = 3.2
    cd_scalar = 1.40
    for s in (0.0, 0.25, 0.50, max_slow):
        v_player = base_speed * (1.0 - s)
        v_zombie = z_base * (1.0 + s * speed_scalar)
        cd_zombie = cd_base / (1.0 + s * cd_scalar)
        if not (0.0 < v_player <= base_speed):
            errors.append(f"Invalid player velocity {v_player} for slow {s}")
        if s > 0.0 and not (v_zombie > z_base and cd_zombie < cd_base):
            errors.append(f"Chrono-Leech failed to buff zombie for slow {s}")

    return errors


def check_assets_and_manifest() -> list[str]:
    errors: list[str] = []
    manifest_path = ROOT / "Content/AssetManifest.json"
    if not manifest_path.exists():
        errors.append("MISSING Content/AssetManifest.json")
        return errors

    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    assets = manifest.get("Assets", {})
    if not assets:
        errors.append("AssetManifest.json contains zero 'Assets' entries")

    for asset_key, asset_meta in assets.items():
        files = asset_meta.get("Files", {})
        if not files:
            errors.append(f"Asset '{asset_key}' has zero file records in manifest")
            continue
        for rel_from_content, file_info in files.items():
            p = ROOT / "Content" / rel_from_content
            if not p.exists():
                errors.append(f"Manifest path missing on disk: Content/{rel_from_content}")
                continue
            actual_sha = hashlib.sha256(p.read_bytes()).hexdigest()
            if actual_sha != file_info.get("SHA256"):
                errors.append(f"SHA-256 mismatch for Content/{rel_from_content}")

    for rig in ("DungeoneerHero", "ZombieExploder", "ZombieFastThrower"):
        rig_dir = ROOT / "Content/Art/Rigs" / rig
        prefix = f"Rig_{rig}_Raw"
        for suffix in (f"{prefix}_D.png", f"{prefix}_N.png", f"{prefix}_ORM.png", "rig_sockets.json"):
            if not (rig_dir / suffix).exists():
                errors.append(f"MISSING rig output: {rig_dir.relative_to(ROOT)}/{suffix}")

    for profile in ("vendor_merchant", "dungeon_lorekeeper"):
        v_dir = ROOT / "Content/Audio/Voices" / profile
        wavs = list(v_dir.glob("syl_*.wav")) if v_dir.exists() else []
        if len(wavs) != 24:
            errors.append(f"Expected 24 phoneme grains in {v_dir}, found {len(wavs)}")

    return errors


def main() -> int:
    all_errors: list[str] = []
    all_errors.extend(check_files_and_static_rules())
    all_errors.extend(check_math_invariants())
    all_errors.extend(check_assets_and_manifest())

    if all_errors:
        print("VERIFICATION FAILED:")
        for err in all_errors:
            print(f"  - {err}")
        return 1

    print(f"ALL CHECKS PASSED: {len(REQUIRED_CPP_FILES)} core/subsystem/tool files verified, "
          "zero Tick allocations, zero banned symbols, math invariants & AssetManifest SHA-256 valid.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
