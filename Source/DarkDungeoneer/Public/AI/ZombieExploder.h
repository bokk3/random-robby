#pragma once

#include "CoreMinimal.h"
#include "AI/ZombieBase.h"
#include "ZombieExploder.generated.h"

/**
 * Slow Exploding Zombie Archetype (`AZombieExploder`).
 * - Low base speed, high mass, paths toward nearest audible/lit player via spatial subsystem.
 * - Entering IgnitionRadius begins a deterministic fixed-step fuse countdown (`FuseDuration`)
 *   with Custom Primitive Data emissive belly-crack pulse and accelerating pitch-tick phase crossings.
 * - Detonates with radial falloff damage and pooled scorch decal placement (zero SpawnActor).
 */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API AZombieExploder : public AZombieBase
{
	GENERATED_BODY()

public:
	AZombieExploder();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "Exploder")
	bool IsFuseIgnited() const noexcept { return bFuseIgnited; }

	UFUNCTION(BlueprintPure, Category = "Exploder")
	float GetFuseProgressNormalized() const noexcept;

	UFUNCTION(BlueprintPure, Category = "Exploder")
	float GetEmissivePulseScalar() const noexcept { return EmissivePulseScalar; }

	/** Fires whenever the deterministic fuse phase crosses a tick boundary (0..1 progress, pitch scalar). */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnExploderFuseTick, float /*FuseProgress*/, float /*PitchMultiplier*/);
	FOnExploderFuseTick OnExploderFuseTick;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnExploderDetonated, const FVector& /*Epicenter*/);
	FOnExploderDetonated OnExploderDetonated;

protected:
	void IgniteFuse() noexcept;
	void ExecuteDetonation();
	virtual FName GetArchetypeDiffuseKey() const noexcept override { return FName(TEXT("Exploder_Cell0_D")); }
	virtual FName GetArchetypeNormalKey() const noexcept override { return FName(TEXT("Exploder_Cell0_N")); }
	virtual FLinearColor GetArchetypeVisualTint() const noexcept override { return FLinearColor(0.95f, 0.36f, 0.08f); }
	virtual FVector GetArchetypeVisualScale() const noexcept override { return FVector(0.95, 0.95, 1.35); }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploder|Config", meta = (ClampMin = "50.0"))
	float IgnitionRadius = 280.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploder|Config", meta = (ClampMin = "0.2"))
	float FuseDuration = 2.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploder|Config", meta = (ClampMin = "100.0"))
	float DetonationRadius = 380.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploder|Config", meta = (ClampMin = "1.0"))
	float MaxDetonationDamage = 65.0f;

private:
	bool bFuseIgnited = false;
	bool bDetonated = false;
	float FuseElapsed = 0.0f;
	float NextTickPhase = 0.1f;
	float EmissivePulseScalar = 0.0f;
};
