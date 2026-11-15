#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/DungeoneerTypes.h"
#include "ZombieBase.generated.h"

class ADungeoneerCharacter;
class UPointLightComponent;
class UStaticMeshComponent;
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

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;

	/** Ensures the visible 2.5D placeholder/textured mesh and emissive light are initialized cleanly. */
	void EnsureVisualMeshInitialized();

	UFUNCTION(BlueprintPure, Category = "Zombie|Visual")
	UStaticMeshComponent* GetZombieVisualMesh() const noexcept { return ZombieVisualMesh.Get(); }

	/** Evaluates whether this zombie falls inside any flashlight cone, Dark Vision radius, or active noise pulse. */
	bool IsDetectedByViewer(const AActor* ViewerActor, const FVector& ViewerOrigin) const noexcept;

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
	virtual FName GetArchetypeDiffuseKey() const noexcept { return FName(TEXT("Exploder_Cell0_D")); }
	virtual FName GetArchetypeNormalKey() const noexcept { return FName(TEXT("Exploder_Cell0_N")); }
	virtual FLinearColor GetArchetypeVisualTint() const noexcept { return FLinearColor(0.85f, 0.30f, 0.12f); }
	virtual FVector GetArchetypeVisualScale() const noexcept { return FVector(0.75, 0.75, 1.45); }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zombie|Visual")
	TObjectPtr<UStaticMeshComponent> ZombieVisualMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zombie|Visual")
	TObjectPtr<UPointLightComponent> ZombieEmissiveLight;

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
	bool bTexturedVisualsApplied = false;
};
