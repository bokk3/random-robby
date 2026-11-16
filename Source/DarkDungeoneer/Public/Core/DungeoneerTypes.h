#pragma once

#include "CoreMinimal.h"
#include "DungeoneerTypes.generated.h"

UENUM(BlueprintType)
enum class ECompassOctant : uint8
{
	E  = 0 UMETA(DisplayName = "East (0 deg)"),
	NE = 1 UMETA(DisplayName = "North-East (45 deg)"),
	N  = 2 UMETA(DisplayName = "North (90 deg)"),
	NW = 3 UMETA(DisplayName = "North-West (135 deg)"),
	W  = 4 UMETA(DisplayName = "West (180 deg)"),
	SW = 5 UMETA(DisplayName = "South-West (225 deg)"),
	S  = 6 UMETA(DisplayName = "South (270 deg)"),
	SE = 7 UMETA(DisplayName = "South-East (315 deg)")
};

UENUM(BlueprintType)
enum class EDungeoneerSensoryPerk : uint8
{
	DarkVision   = 0 UMETA(DisplayName = "Dark Vision"),
	AcuteHearing = 1 UMETA(DisplayName = "Acute Hearing"),
	ScentThermal = 2 UMETA(DisplayName = "Scent / Thermal Sense")
};

UENUM(BlueprintType)
enum class EDungeoneerRelicId : uint8
{
	None                = 0 UMETA(DisplayName = "None"),
	SunGlassLens        = 1 UMETA(DisplayName = "Sun-Glass Lens"),
	ChronoWardBoots     = 2 UMETA(DisplayName = "Chrono-Ward Boots"),
	EchoTunedBlade      = 3 UMETA(DisplayName = "Echo-Tuned Blade"),
	SolarFlashlightCore = 4 UMETA(DisplayName = "Solar Flashlight Core"),
	VampireWick         = 5 UMETA(DisplayName = "Vampire Wick")
};

UENUM(BlueprintType)
enum class EDungeoneerConsumableId : uint8
{
	None            = 0 UMETA(DisplayName = "None"),
	LanternOilPhial = 1 UMETA(DisplayName = "Phial of Lantern Oil"),
	AntidoteDraught = 2 UMETA(DisplayName = "Antidote Draught")
};

UENUM(BlueprintType)
enum class EDungeoneerFloatingTextKind : uint8
{
	Damage     = 0,
	CritDamage = 1,
	Gold       = 2,
	Relic      = 3,
	Status     = 4
};

namespace DungeoneerMath
{
	inline constexpr float INV_SQRT_2 = 0.70710678f;
	inline constexpr float TEXELS_PER_METER = 64.0f;
	inline constexpr float UNREAL_UNITS_PER_METER = 100.0f;

	// 4-Axis (Horizontal, Vertical, Main Diagonal NE-SW, Anti-Diagonal NW-SE) unit vectors
	// guaranteeing diagonal speed matches cardinal speed to within < 1e-5f.
	inline const FVector2f GCompassUnitVectors[8] =
	{
		FVector2f( 1.0f,        0.0f),       // 0: E
		FVector2f( INV_SQRT_2,  INV_SQRT_2), // 1: NE
		FVector2f( 0.0f,        1.0f),       // 2: N
		FVector2f(-INV_SQRT_2,  INV_SQRT_2), // 3: NW
		FVector2f(-1.0f,        0.0f),       // 4: W
		FVector2f(-INV_SQRT_2, -INV_SQRT_2), // 5: SW
		FVector2f( 0.0f,       -1.0f),       // 6: S
		FVector2f( INV_SQRT_2, -INV_SQRT_2)  // 7: SE
	};

	inline constexpr float GCompassYawDegrees[8] =
	{
		  0.0f, // E
		 45.0f, // NE
		 90.0f, // N
		135.0f, // NW
		180.0f, // W
		225.0f, // SW
		270.0f, // S
		315.0f  // SE
	};

	/** Converts an octant into a normalized 3D XY direction vector without runtime sqrt. */
	[[nodiscard]] inline FVector OctantToDirection3D(ECompassOctant Octant) noexcept
	{
		const uint8 Index = static_cast<uint8>(Octant) & 7;
		const FVector2f& Vec = GCompassUnitVectors[Index];
		return FVector(static_cast<double>(Vec.X), static_cast<double>(Vec.Y), 0.0);
	}

	/** Maps an octant onto the 4 primary sprite-sheet directions (S, N, E, W) for asymmetric 4-dir rigs. */
	[[nodiscard]] constexpr uint8 OctantToCardinalRow(ECompassOctant Octant) noexcept
	{
		switch (Octant)
		{
		case ECompassOctant::S:
		case ECompassOctant::SW:
		case ECompassOctant::SE:
			return 0; // South row
		case ECompassOctant::N:
		case ECompassOctant::NW:
		case ECompassOctant::NE:
			return 1; // North row
		case ECompassOctant::E:
			return 2; // East row
		case ECompassOctant::W:
		default:
			return 3; // West row
		}
	}

	/**
	 * Maps an octant to the 4-row sprite sheet orientation matching our top-down camera (Yaw = 0 deg, looking +X):
	 *   -X (W)  = Screen Down (toward camera) -> Row 0 (Front)
	 *   +X (E)  = Screen Up (into dungeon)    -> Row 1 (Back)
	 *   +Y (N/NE/NW) = Screen Right           -> Row 2 (Right profile)
	 *   -Y (S/SE/SW) = Screen Left            -> Row 3 (Left profile)
	 */
	[[nodiscard]] constexpr uint8 OctantToScreenDirectionalRow(ECompassOctant Octant) noexcept
	{
		switch (Octant)
		{
		case ECompassOctant::W:
			return 0;
		case ECompassOctant::E:
			return 1;
		case ECompassOctant::N:
		case ECompassOctant::NE:
		case ECompassOctant::NW:
			return 2;
		case ECompassOctant::S:
		case ECompassOctant::SE:
		case ECompassOctant::SW:
		default:
			return 3;
		}
	}

	/**
	 * Maps an 8-compass world octant to the 4 screen-relative sprite sheet rows (0=Front, 1=Back, 2=Right, 3=Left)
	 * for an arbitrary camera boom Yaw angle (e.g. -45.0 deg isometric diorama framing).
	 */
	[[nodiscard]] inline uint8 OctantToScreenDirectionalRowForCameraYaw(ECompassOctant Octant, float CameraYawDeg) noexcept
	{
		const FVector2f WorldDir = GCompassUnitVectors[static_cast<uint8>(Octant) & 7];
		const float Rad = FMath::DegreesToRadians(-CameraYawDeg);
		const float C = FMath::Cos(Rad);
		const float S = FMath::Sin(Rad);
		const double ScreenUp = static_cast<double>(WorldDir.X) * static_cast<double>(C) - static_cast<double>(WorldDir.Y) * static_cast<double>(S);
		const double ScreenRight = static_cast<double>(WorldDir.X) * static_cast<double>(S) + static_cast<double>(WorldDir.Y) * static_cast<double>(C);
		if (FMath::Abs(ScreenRight) >= FMath::Abs(ScreenUp))
		{
			return (ScreenRight >= 0.0) ? 2 : 3;
		}
		return (ScreenUp < 0.0) ? 0 : 1;
	}

	/** Converts screen-space (Up, Right) stick/WASD/mouse aim into world XY for a given camera Yaw. */
	[[nodiscard]] inline FVector2D RotateScreenInputToWorldForCameraYaw(const FVector2D& ScreenUpRight, float CameraYawDeg) noexcept
	{
		const float Rad = FMath::DegreesToRadians(CameraYawDeg);
		const float C = FMath::Cos(Rad);
		const float S = FMath::Sin(Rad);
		return FVector2D(
			ScreenUpRight.X * static_cast<double>(C) - ScreenUpRight.Y * static_cast<double>(S),
			ScreenUpRight.X * static_cast<double>(S) + ScreenUpRight.Y * static_cast<double>(C));
	}
}

USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API FDungeoneerStepEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	ECompassOctant StepOctant = ECompassOctant::S;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	float AcousticLoudness = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	int32 StepSequenceIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Locomotion")
	bool bIsHostileSource = false;
};

USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API FDungeoneerNoiseEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sensory")
	FVector WorldOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Sensory")
	float MaxPulseRadius = 900.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Sensory")
	float CurrentRadius = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Sensory")
	float Loudness = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Sensory")
	float AgeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Sensory")
	float LifetimeSeconds = 0.85f;

	UPROPERTY(BlueprintReadOnly, Category = "Sensory")
	bool bIsIllusionPhaseInverted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sensory")
	bool bActive = false;
};

USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API FDungeoneerScentNode
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Sensory")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Sensory")
	float Intensity = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Sensory")
	float AgeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Sensory")
	float LifetimeSeconds = 10.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Sensory")
	bool bIsHostile = false;

	UPROPERTY(BlueprintReadOnly, Category = "Sensory")
	bool bActive = false;
};

USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API FDungeoneerSensoryViewState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Sensory")
	float DarkVisionRadius = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Sensory")
	float DarkVisionFogExtinctionScale = 1.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Sensory")
	float AcuteHearingIntensity = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Sensory")
	float ScentThermalIntensity = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Sensory")
	FVector PlayerWorldOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Sensory")
	FVector LatestHearingPulseOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Sensory")
	float LatestHearingPulseRadius = 0.0f;
};
