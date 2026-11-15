#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HazardZoneActors.generated.h"

class USphereComponent;
class UPointLightComponent;
class UNiagaraComponent;
class ADungeoneerCharacter;
class AZombieFastThrower;

/**
 * Pooled Hazard Zone Base Actor (zero SpawnActor calls during Tick).
 */
UCLASS(Abstract)
class DARKDUNGEONEER_API AHazardZoneBase : public AActor
{
	GENERATED_BODY()

public:
	AHazardZoneBase();

	virtual void Tick(float DeltaSeconds) override;

	void ActivateFromPool(const FVector& WorldLocation) noexcept;
	void DeactivateToPool() noexcept;

	bool IsHazardActive() const noexcept { return bHazardActive; }

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnHazardTickCue, const FVector& /*Location*/);
	FOnHazardTickCue OnHazardTickCue;

protected:
	UFUNCTION()
	void HandleSphereBeginOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleSphereEndOverlap(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	virtual void ApplyHazardTick(float DeltaSeconds) {}
	virtual void OnHazardDeactivated() {}

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	TObjectPtr<USphereComponent> ZoneBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard")
	TObjectPtr<UNiagaraComponent> HazardFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "0.5"))
	float ActiveDuration = 7.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard", meta = (ClampMin = "50.0"))
	float ZoneRadius = 240.0f;

	// Pre-allocated inline tracking array (zero heap allocations during gameplay)
	TArray<TWeakObjectPtr<ADungeoneerCharacter>, TInlineAllocator<8>> OverlappingPlayers;

	bool bHazardActive = false;
	float RemainingLifetime = 0.0f;
	float AudioCueAccumulator = 0.0f;
};

/** Fire Hazard Zone: Burn DoT + flickering orange point light illuminating volumetric fog. */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API AHazardZone_Fire : public AHazardZoneBase
{
	GENERATED_BODY()

public:
	AHazardZone_Fire();

protected:
	virtual void ApplyHazardTick(float DeltaSeconds) override;
	virtual void OnHazardDeactivated() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hazard|Fire")
	TObjectPtr<UPointLightComponent> FlickerLight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard|Fire")
	float BurnDamagePerSecond = 14.0f;

private:
	float FlickerPhase = 0.0f;
};

/** Poison Hazard Zone: Stamina drain + green spore mist increasing fog extinction. */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API AHazardZone_Poison : public AHazardZoneBase
{
	GENERATED_BODY()

public:
	AHazardZone_Poison();

protected:
	virtual void ApplyHazardTick(float DeltaSeconds) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard|Poison")
	float PoisonDamagePerSecond = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard|Poison")
	float FlashlightDimmingFactor = 0.45f;
};

/**
 * Chrono-Leech Slo-Mo Gas Zone (`AHazardZone_ChronoGas`).
 * - Accumulates SlowStack up to MaxSlowRatio (0.65f) on overlapping players.
 * - Computes TotalDrainedRatio = Sum(PlayerSlowRatio_i) and pushes it to OwningThrower.
 */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API AHazardZone_ChronoGas : public AHazardZoneBase
{
	GENERATED_BODY()

public:
	AHazardZone_ChronoGas();

	void SetOwningThrower(AZombieFastThrower* InThrower) noexcept { OwningThrower = InThrower; }

protected:
	virtual void ApplyHazardTick(float DeltaSeconds) override;
	virtual void OnHazardDeactivated() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard|Chrono")
	float SlowRampPerSecond = 0.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hazard|Chrono")
	float MaxSlowRatio = 0.65f;

private:
	TWeakObjectPtr<AZombieFastThrower> OwningThrower;
	TStaticArray<float, 8> PlayerSlowStacks;
};
