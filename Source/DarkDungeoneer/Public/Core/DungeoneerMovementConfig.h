#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DungeoneerMovementConfig.generated.h"

/**
 * Data-only configuration asset for 4-axis (8-compass) locomotion, flashlight beam, and step cadence.
 * All gameplay logic remains strictly in C++20.
 */
UCLASS(BlueprintType)
class DARKDUNGEONEER_API UDungeoneerMovementConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Base horizontal movement speed along any of the 4 compass axes (UU/s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion", meta = (ClampMin = "50.0"))
	float BaseWalkSpeed = 450.0f;

	/** Squared 2D input deadzone threshold before quantizing to an octant. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Locomotion", meta = (ClampMin = "0.0001"))
	float InputDeadZoneSq = 0.04f;

	/** Fixed horizontal travel distance (UU) required to advance one deterministic footstep cycle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "10.0"))
	float StepCycleDistanceUU = 120.0f;

	/** Maximum Chrono-Leech slow fraction S in [0.0, MaxChronoSlowRatio]. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "0.0", ClampMax = "0.95"))
	float MaxChronoSlowRatio = 0.65f;

	/** Exponential interpolation speed for flashlight yaw tracking toward the target aim octant. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flashlight", meta = (ClampMin = "1.0"))
	float FlashlightInterpSpeed = 18.0f;

	/** Narrow inner cone half-angle (degrees) for the player's volumetric flashlight. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flashlight", meta = (ClampMin = "1.0", ClampMax = "45.0"))
	float FlashlightInnerConeAngleDeg = 12.0f;

	/** Narrow outer cone half-angle (degrees) for the player's volumetric flashlight. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flashlight", meta = (ClampMin = "2.0", ClampMax = "60.0"))
	float FlashlightOuterConeAngleDeg = 22.5f;

	/** Effective range of the inverse-square flashlight beam (UU). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flashlight", meta = (ClampMin = "100.0"))
	float FlashlightAttenuationRadius = 1800.0f;

	/** Volumetric fog scattering multiplier for the flashlight cone. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flashlight", meta = (ClampMin = "0.0"))
	float FlashlightVolumetricScattering = 2.5f;
};
