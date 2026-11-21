#pragma once

#include "CoreMinimal.h"
#include "AI/ZombieBase.h"
#include "BossGuardianActor.generated.h"

class AMajorLootVaultActor;
class AZombieExploder;

/**
 * Floor 3 Terminal Biome Guardian Boss (`ABossGuardianActor` — Cinder Colossus).
 * - Guards AMajorLootVaultActor until defeated, locking vault descent while alive.
 * - Alternates telegraphed 8-compass radial slams, Fire/ChronoGas hazard volleys,
 *   and waking pre-allocated dormant Exploder adds in Phase 2 (< 55% HP).
 * - Strictly zero runtime SpawnActor inside Tick and zero FTimerHandle.
 */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API ABossGuardianActor : public AZombieBase
{
	GENERATED_BODY()

public:
	ABossGuardianActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	void SetLinkedVault(AMajorLootVaultActor* InVault) noexcept;
	AMajorLootVaultActor* GetLinkedVault() const noexcept { return LinkedVault.Get(); }

	UFUNCTION(BlueprintPure, Category = "Boss")
	int32 GetBossPhase() const noexcept { return CurrentBossPhase; }

	UFUNCTION(BlueprintPure, Category = "Boss")
	bool IsTelegraphingSlam() const noexcept { return SlamTelegraphRemaining > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Boss")
	float GetBossHealthPercent() const noexcept { return MaxHealth > 0.0f ? FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f) : 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Boss")
	FString GetBossDisplayTitle() const { return TEXT("CINDER COLOSSUS — VAULT GUARDIAN"); }

	/** Forces an immediate phase evaluation & radial slam trigger (used by combat & tests). */
	UFUNCTION(BlueprintCallable, Category = "Boss")
	void ExecuteRadialBossSlam();

protected:
	virtual void OnZombieDied() override;
	virtual const TCHAR* GetArchetypeKeyPrefix() const noexcept override { return TEXT("BossGuardian"); }
	virtual const TCHAR* GetAttackSheetPrefix() const noexcept override { return TEXT("BossGuardianAttack"); }
	virtual const TCHAR* GetRedesignedWalkPrefix() const noexcept override { return TEXT("BossGuardianWalk"); }
	virtual bool AttackSheetNeedsRedesignedWalk() const noexcept override { return true; }
	virtual bool UsesDeathFrame() const noexcept override { return true; }
	virtual bool ResolveAttackFrame(int32 FacingRow, int32& OutRow, int32& OutCol) const noexcept override;

	/** Seconds since the last slam landed; drives the impact and recovery frames. */
	float SlamAfterglow = 0.0f;
	virtual FName GetArchetypeDiffuseKey() const noexcept override { return FName(TEXT("BossGuardian_Row0_D")); }
	virtual FName GetArchetypeNormalKey() const noexcept override { return FName(TEXT("BossGuardian_Row0_N")); }
	virtual FLinearColor GetArchetypeVisualTint() const noexcept override { return FLinearColor(1.0f, 0.44f, 0.14f); }
	virtual FVector GetArchetypeVisualScale() const noexcept override { return FVector(2.55, 2.55, 2.55); }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Config", meta = (ClampMin = "100.0"))
	float SlamRadiusUU = 360.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Config", meta = (ClampMin = "10.0"))
	float SlamDamage = 28.0f;

private:
	void PreallocateDormantAdds();
	void WakePhase2DormantAdds();

	TWeakObjectPtr<AMajorLootVaultActor> LinkedVault;
	TStaticArray<TWeakObjectPtr<AZombieExploder>, 2> DormantAdds;
	int32 CurrentBossPhase = 1;
	float AttackCycleAccumulator = 0.0f;
	float SlamTelegraphRemaining = 0.0f;
	int32 PatternIndex = 0;
	bool bPhase2AddsWoken = false;
};
