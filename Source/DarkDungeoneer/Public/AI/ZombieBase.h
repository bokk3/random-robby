#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/DungeoneerTypes.h"
#include "ZombieBase.generated.h"

class ADungeoneerCharacter;
class UZombieSpatialSubsystem;

/**
 * Base 2.5D Zombie Archetype sharing 4-axis/8-compass quantized stepping,
 * zero-allocation spatial registration, and deterministic step accumulator hooks.
 */
UCLASS(Abstract)
class DARKDUNGEONEER_API AZombieBase : public ACharacter
{
	GENERATED_BODY()

public:
	AZombieBase();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Zombie")
	ECompassOctant GetCurrentHeadingOctant() const noexcept { return CurrentHeadingOctant; }

	UFUNCTION(BlueprintPure, Category = "Zombie")
	bool IsZombieAlive() const noexcept { return CurrentHealth > 0.0f; }

	/** True zombies cast physical shadows and emit footsteps; mirror decoys do not. */
	UFUNCTION(BlueprintPure, Category = "Zombie")
	bool IsRealPhysicalZombie() const noexcept { return !bIsMirrorDecoy; }

	void ApplyZombieDamage(float DamageAmount, AActor* DamageInstigator);

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnZombieStepTriggered, const FDungeoneerStepEvent&);
	FOnZombieStepTriggered OnZombieStepTriggered;

protected:
	void MoveAlongOctant(ECompassOctant Octant, float Scale = 1.0f) noexcept;
	void AdvanceDeterministicStepAccumulator(float DeltaSeconds) noexcept;
	virtual void OnZombieDied();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config", meta = (ClampMin = "1.0"))
	float MaxHealth = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config", meta = (ClampMin = "50.0"))
	float BaseWalkSpeed = 240.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config", meta = (ClampMin = "100.0"))
	float PerceptionHearingRange = 1400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config", meta = (ClampMin = "10.0"))
	float StepCycleDistanceUU = 110.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	float CurrentHealth = 120.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	ECompassOctant CurrentHeadingOctant = ECompassOctant::S;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirror")
	bool bIsMirrorDecoy = false;

private:
	FVector PreviousLocation = FVector::ZeroVector;
	float StepDistanceAccumulator = 0.0f;
	int32 StepSequenceCounter = 0;
};
