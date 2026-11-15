#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MirrorLightBounceComponent.generated.h"

class USpotLightComponent;
class ADungeoneerTileMapInstancer;

/** Snapshot describing the active mirror-bounce secondary spotlight beam. */
USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API FMirrorLightBounceResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mirror|LightBounce")
	bool bDidBounce = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mirror|LightBounce")
	FVector HitLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mirror|LightBounce")
	FVector SurfaceNormal = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mirror|LightBounce")
	FVector ReflectedDirection = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mirror|LightBounce")
	float BouncedRangeUU = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mirror|LightBounce")
	bool bRevealedIllusionSeam = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mirror|LightBounce")
	int32 RevealedMirrorEntryIndex = INDEX_NONE;
};

/**
 * UMirrorLightBounceComponent
 *
 * Performs a deterministic raycast along a player's flashlight axis.
 * When striking an actor, HISM instance, or component tagged `"MirrorReflective"`,
 * computes `R = I - 2 * (I . N) * N` and aims a pre-allocated secondary `USpotLightComponent`
 * down the reflected corridor to reveal Mirror-Palace illusion seams with zero Tick allocations.
 */
UCLASS(ClassGroup = (Dungeoneer), meta = (BlueprintSpawnableComponent))
class DARKDUNGEONEER_API UMirrorLightBounceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMirrorLightBounceComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Pre-allocates and attaches the secondary reflected `USpotLightComponent` outside Tick. */
	UFUNCTION(BlueprintCallable, Category = "Mirror|LightBounce")
	void EnsureBounceLightInitialized();

	/** Binds an explicit tile-map instancer (optional; BeginPlay auto-discovers if unset). */
	UFUNCTION(BlueprintCallable, Category = "Mirror|LightBounce")
	void SetTargetTileMapInstancer(ADungeoneerTileMapInstancer* InInstancer);

	/**
	 * Deterministic law-of-reflection helper:
	 * Returns R = I - 2 * dot(I, N) * N using normalized incident and normal vectors.
	 */
	UFUNCTION(BlueprintPure, Category = "Mirror|LightBounce")
	static FVector ComputeReflectedVector(const FVector& IncidentDirection, const FVector& SurfaceNormal) noexcept;

	/**
	 * Traces along `(FlashlightOrigin, FlashlightDirection)` for reflective surfaces (`MirrorReflective`),
	 * aims the pre-allocated secondary spotlight along `R`, and reveals struck illusion seams.
	 */
	UFUNCTION(BlueprintCallable, Category = "Mirror|LightBounce")
	FMirrorLightBounceResult EvaluateFlashlightBounce(
		const FVector& FlashlightOrigin,
		const FVector& FlashlightDirection,
		float IncidentIntensity = 12000.0f);

	UFUNCTION(BlueprintPure, Category = "Mirror|LightBounce")
	USpotLightComponent* GetReflectedSpotLight() const noexcept { return ReflectedSpotLight.Get(); }

	UFUNCTION(BlueprintPure, Category = "Mirror|LightBounce")
	const FMirrorLightBounceResult& GetLastBounceResult() const noexcept { return LastBounceResult; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mirror|LightBounce", meta = (ClampMin = "100.0"))
	float MaxIncidentRangeUU = 1600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mirror|LightBounce", meta = (ClampMin = "100.0"))
	float MaxReflectedRangeUU = 1200.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mirror|LightBounce", meta = (ClampMin = "0.05", ClampMax = "2.0"))
	float BounceIntensityMultiplier = 0.78f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mirror|LightBounce", meta = (ClampMin = "5.0", ClampMax = "60.0"))
	float ReflectedInnerConeAngleDeg = 10.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mirror|LightBounce", meta = (ClampMin = "8.0", ClampMax = "75.0"))
	float ReflectedOuterConeAngleDeg = 24.0f;

private:
	UPROPERTY(Transient)
	TObjectPtr<USpotLightComponent> ReflectedSpotLight;

	UPROPERTY(Transient)
	TWeakObjectPtr<ADungeoneerTileMapInstancer> CachedInstancer;

	FMirrorLightBounceResult LastBounceResult;
};
