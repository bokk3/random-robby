#pragma once

/**
 * DungeonEcosystemTypes.h
 *
 * Shared POD types, 8-float Per-Instance Custom Data (EEcoCPD) layout constants,
 * linear RGB palettes, 24-bit color packer, surface acoustic multipliers,
 * and atlas UV cell bindings for the Living Ecosystem (v1.2.0).
 */

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

/** Locked 8-float Per-Instance Custom Data layout for all ecosystem HISM/ISM buckets. */
namespace EEcoCPD
{
	inline constexpr int32 UVCell    = 0; // 0..15 atlas cell (or 0..3 strip cell); flipbook base cell
	inline constexpr int32 Emissive  = 1; // Emissive multiplier (Tier 0). Dormant flora 0.2 -> bloom 12.0
	inline constexpr int32 HueR      = 2; // Linear RGB R [0..1]
	inline constexpr int32 HueG      = 3; // Linear RGB G [0..1]
	inline constexpr int32 HueB      = 4; // Linear RGB B [0..1]
	inline constexpr int32 EventTime = 5; // Wrapped EcoTime (s, mod 512) of last event trigger (bloom/wave/spawn)
	inline constexpr int32 ParamA    = 6; // Kind-specific scalar (bloom target 0/1, wave ring, flow octant, lifetime)
	inline constexpr int32 KindFlags = 7; // (Kind << 4) | StateBits encoded as exact integer float
	inline constexpr int32 Count     = 8;

	/** Single-precision safe wrap modulus matching MPC EcoTime (s). */
	inline constexpr double EcoTimeWrapSeconds = 512.0;

	/** Maximum event-driven SetCustomDataValue writes per frame across eco buckets. */
	inline constexpr int32 MaxCPDUpdatesPerFrame = 64;
}

/** Vibrant magical linear RGB palette constants contrasted against pitch-black fog. */
namespace EcoPalette
{
	inline constexpr FLinearColor EmeraldMoss  { 0.10f, 0.95f, 0.45f, 1.0f };
	inline constexpr FLinearColor MagentaBloom { 0.95f, 0.15f, 0.85f, 1.0f };
	inline constexpr FLinearColor AstralCyan   { 0.20f, 0.85f, 1.00f, 1.0f };
	inline constexpr FLinearColor GoldenRune   { 1.00f, 0.78f, 0.25f, 1.0f };
	inline constexpr FLinearColor AmberFire    { 1.00f, 0.55f, 0.18f, 1.0f };
	inline constexpr FLinearColor MagmaOrange  { 1.00f, 0.32f, 0.05f, 1.0f };
}

/** Exact 24-bit integer RGB packing/unpacking (< 2^24 -> exact IEEE-754 single-precision float). */
namespace EcoColors
{
	[[nodiscard]] FORCEINLINE constexpr float PackRGB24(uint8 R, uint8 G, uint8 B) noexcept
	{
		const uint32 Packed = (static_cast<uint32>(R) << 16) | (static_cast<uint32>(G) << 8) | static_cast<uint32>(B);
		return static_cast<float>(Packed);
	}

	[[nodiscard]] FORCEINLINE float PackLinearColorRGB24(const FLinearColor& Linear) noexcept
	{
		const uint8 R = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Linear.R * 255.0f), 0, 255));
		const uint8 G = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Linear.G * 255.0f), 0, 255));
		const uint8 B = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Linear.B * 255.0f), 0, 255));
		return PackRGB24(R, G, B);
	}

	FORCEINLINE void UnpackRGB24(float PackedFloat, uint8& OutR, uint8& OutG, uint8& OutB) noexcept
	{
		const uint32 Packed = static_cast<uint32>(FMath::Clamp(FMath::RoundToInt32(PackedFloat), 0, 16777215));
		OutR = static_cast<uint8>((Packed >> 16) & 0xFFu);
		OutG = static_cast<uint8>((Packed >> 8) & 0xFFu);
		OutB = static_cast<uint8>(Packed & 0xFFu);
	}

	[[nodiscard]] FORCEINLINE float PackKindFlags(uint8 Kind, uint8 StateBits) noexcept
	{
		return static_cast<float>((static_cast<uint32>(Kind) << 4) | (static_cast<uint32>(StateBits) & 0x0Fu));
	}

	[[nodiscard]] FORCEINLINE float WrapEcoTime(double WorldTimeSeconds) noexcept
	{
		return static_cast<float>(FMath::Fmod(FMath::Max(WorldTimeSeconds, 0.0), EEcoCPD::EcoTimeWrapSeconds));
	}
}

/** 7 physical floor surface categories resolved by UDungeonWaterSubsystem::ResolveSurface. */
UENUM(BlueprintType)
enum class EDungeonSurfaceKind : uint8
{
	DryStone        = 0,
	ShallowPuddle   = 1,
	AnkleDeepStream = 2,
	SquishyMoss     = 3,
	CrystalShard    = 4,
	SanctuaryRug    = 5,
	ObsidianMirror  = 6,
	Count           UMETA(Hidden)
};

/** Interactive dungeon flora species in HISM_MagicalFlora (rows 0..3 of PROP_Magical_Flora_And_Crystals_4x4). */
UENUM(BlueprintType)
enum class EFloraKind : uint8
{
	PrismLily   = 0,
	SporeCap    = 1,
	EchoCrystal = 2,
	PulseMoss   = 3
};

/** Pre-allocated SoA fauna species in UDungeonFaunaSubsystem (cells 0..3 of PROP_Fauna_Critters_Strip_4x1). */
UENUM(BlueprintType)
enum class EFaunaSpecies : uint8
{
	LumenMoth     = 0,
	LanternBeetle = 1,
	PuddleNewt    = 2,
	CryptCrab     = 3
};

/** 5 physical + visual hydrology states in UDungeonWaterSubsystem. */
UENUM(BlueprintType)
enum class EWaterForm : uint8
{
	CeilingDrip     = 0,
	ShallowPuddle   = 1,
	WadingStream    = 2,
	WeepingCascade  = 3,
	BioThermalGeyser = 4
};

/** Pooled ISM micro-particle / overlay kinds in ISM_WaterFX. */
UENUM(BlueprintType)
enum class EWaterFxKind : uint8
{
	Droplet       = 0,
	Ripple        = 1,
	Splash        = 2,
	Footprint     = 3,
	FloatingPetal = 4,
	CascadeMist   = 5,
	SporePuff     = 6
};

/** Acoustic metadata & zombie lure multipliers per EDungeonSurfaceKind (locked D6 values). */
struct FSurfaceAcoustics
{
	const TCHAR* ImpactBank;
	float NoiseRadiusMultiplier;
	float AcousticReflectivity;
};

inline constexpr FSurfaceAcoustics EcoSurfaceTable[static_cast<int32>(EDungeonSurfaceKind::Count)] =
{
	/* DryStone        */ { TEXT("Step_Stone"),    1.00f, 0.55f },
	/* ShallowPuddle   */ { TEXT("Step_Puddle"),   1.40f, 0.80f },
	/* AnkleDeepStream */ { TEXT("Step_Wade"),     1.25f, 0.70f },
	/* SquishyMoss     */ { TEXT("Step_Moss"),     0.70f, 0.20f },
	/* CrystalShard    */ { TEXT("Step_Crystal"),  1.40f, 0.95f },
	/* SanctuaryRug    */ { TEXT("Step_Rug"),      0.60f, 0.10f },
	/* ObsidianMirror  */ { TEXT("Step_Obsidian"), 1.15f, 0.90f }
};

/** Locked atlas UV cell indices verified against sliced PROP_Magical_Flora_And_Crystals_4x4, PROP_Water_And_Fauna_4x4 & Strip 4x1. */
namespace EcoCells
{
	// PROP_Magical_Flora_And_Crystals_4x4 (512x512, 4x4 grid of 128px cells)
	inline constexpr int32 PrismLilyBloom[4]    = { 0, 1, 2, 3 };
	inline constexpr int32 SporeCapVariants[4]  = { 4, 5, 6, 7 };
	inline constexpr int32 EchoCrystalCharge[4] = { 8, 9, 10, 11 };
	inline constexpr int32 PulseMossStage[4]    = { 12, 13, 14, 15 };

	// PROP_Water_And_Fauna_4x4 (512x512, 4x4 grid of 128px cells)
	inline constexpr int32 WaterCascadeBase = 0; // Cells 0..3
	inline constexpr int32 WaterRippleBase  = 4; // Cells 4..7
	inline constexpr int32 WaterGeyserBase  = 8; // Cells 8..11

	// PROP_Fauna_Critters_Strip_4x1 (128x32, 4x1 strip of 32px cells)
	inline constexpr int32 FaunaStrip[4] = { 0, 1, 2, 3 }; // 0=Moth, 1=Beetle, 2=Newt, 3=Crab

	// Gameplay constants shared with static architecture invariants
	inline constexpr float WadingSpeedScalar = 0.88f;
	inline constexpr float WadingBootDropUU  = 6.25f; // 4 px at 64 px / 100 UU
}
