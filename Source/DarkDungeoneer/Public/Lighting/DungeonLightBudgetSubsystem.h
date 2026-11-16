#pragma once

/**
 * DungeonLightBudgetSubsystem.h
 *
 * 4-Tier Colorful Lighting & Bioluminescence Hierarchy (v1.2.0):
 *  - Tier 0: Emissive-only micro-lights driven solely via EEcoCPD[1..4] (0 GPU light cost).
 *  - Tier 1: Up to 32 (16 on sg.ShadowQuality <= 1) faint local glow nodes (75-160 UU)
 *            packed as (X, Y, Radius, PackRGB24) into MPC_SensoryState EcoGlow00..31.
 *  - Tier 2: Pre-allocated pool of 16 (capped at 8 on sg.ShadowQuality <= 1) unshadowed
 *            UPointLightComponents (250-450 UU) with 10% hysteresis and 0.25s intensity lerp.
 *  - Tier 3: Up to 2 shadow-casting + fog-piercing world hero point lights (600-1000 UU)
 *            for campfires, shrines, and loot vaults; overflow automatically demotes to Tier 2.
 */

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"
#include "Subsystems/WorldSubsystem.h"
#include "DungeonLightBudgetSubsystem.generated.h"

class AActor;
class UMaterialParameterCollection;
class UPointLightComponent;

UENUM(BlueprintType)
enum class EDungeonLightTier : uint8
{
	Tier0_Emissive = 0,
	Tier1_MPCGlow  = 1,
	Tier2_Pooled   = 2,
	Tier3_Hero     = 3
};

USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API FDungeonLightCandidate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	FVector WorldPosition = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	FLinearColor Color = FLinearColor(0.20f, 0.85f, 1.00f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	float RadiusUU = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	float BaseIntensity = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	float PriorityBoost = 0.0f;

	/** Absolute GetTimeSeconds() expiry; < 0 means permanent (no expiry). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lighting")
	double LeaseExpiryTime = -1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	EDungeonLightTier RequestedTier = EDungeonLightTier::Tier1_MPCGlow;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lighting")
	bool bMoving = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lighting")
	bool bEnabled = true;

	/** Runtime lease slot currently active (-1 = none). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lighting")
	int32 AssignedSlot = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lighting")
	EDungeonLightTier EffectiveAssignedTier = EDungeonLightTier::Tier0_Emissive;
};

UCLASS()
class DARKDUNGEONEER_API UDungeonLightBudgetSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxCandidates     = 512;
	static constexpr int32 MaxTier1          = 32;
	static constexpr int32 MaxTier2          = 16;
	static constexpr int32 MaxTier3World     = 2;
	static constexpr int32 MaxTransientSlots = 16;

	UDungeonLightBudgetSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UDungeonLightBudgetSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return !IsTemplate(); }

	/** Resets registry and allocates/attaches pooled Tier 2 & Tier 3 point lights outside Tick. */
	UFUNCTION(BlueprintCallable, Category = "Ecosystem|Lighting")
	void OnFloorBuilt(AActor* PoolOwnerActor);

	/** Clears all registered light candidates (called before repopulating a floor). */
	UFUNCTION(BlueprintCallable, Category = "Ecosystem|Lighting")
	void ClearCandidates() noexcept;

	/** Registers a static, moving, or leaseable candidate during floor build or setup. Returns CandidateId. */
	UFUNCTION(BlueprintCallable, Category = "Ecosystem|Lighting")
	int32 RegisterCandidate(const FDungeonLightCandidate& Candidate);

	/** Updates world position of a moving Tier 1/2 candidate (e.g. Lantern-Beetle) with zero allocation. */
	UFUNCTION(BlueprintCallable, Category = "Ecosystem|Lighting")
	void UpdateMovingCandidate(int32 CandidateId, const FVector& NewWorldPosition) noexcept;

	/** Activates or refreshes a timed Tier 2 light lease on an existing flora/prop candidate. */
	UFUNCTION(BlueprintCallable, Category = "Ecosystem|Lighting")
	bool RequestTier2Lease(int32 CandidateId, float DurationSeconds) noexcept;

	/** Enables/disables an existing candidate (e.g. dormant vs active) without removing it. */
	UFUNCTION(BlueprintCallable, Category = "Ecosystem|Lighting")
	void SetCandidateEnabled(int32 CandidateId, bool bInEnabled) noexcept;

	/** Activates a short-lived Tier 1 glow puff (e.g. Spore-Cap burst) using pre-allocated transient slots. */
	UFUNCTION(BlueprintCallable, Category = "Ecosystem|Lighting")
	void TriggerTransientTier1Glow(const FVector& WorldPosition, const FLinearColor& Color, float RadiusUU, float DurationSeconds) noexcept;

	/** Forces an immediate deterministic budget evaluation (useful in tests or right after floor build). */
	UFUNCTION(BlueprintCallable, Category = "Ecosystem|Lighting")
	void EvaluateLightBudgetNow(float DeltaTime = 0.1f);

	/** Explicit override (-1 = read sg.ShadowQuality CVar) for deterministic unit tests. */
	UFUNCTION(BlueprintCallable, Category = "Ecosystem|Lighting")
	void SetShadowQualityOverride(int32 InShadowQualityOverride) noexcept { ShadowQualityOverride = InShadowQualityOverride; }

	UFUNCTION(BlueprintPure, Category = "Ecosystem|Lighting")
	int32 GetEffectiveMaxTier1() const noexcept { return IsLowQualityMode() ? 16 : MaxTier1; }

	UFUNCTION(BlueprintPure, Category = "Ecosystem|Lighting")
	int32 GetEffectiveMaxTier2() const noexcept { return IsLowQualityMode() ? 8 : MaxTier2; }

	UFUNCTION(BlueprintPure, Category = "Ecosystem|Lighting")
	int32 GetActiveTier1Count() const noexcept { return ActiveTier1Count; }

	UFUNCTION(BlueprintPure, Category = "Ecosystem|Lighting")
	int32 GetActiveTier2Count() const noexcept { return ActiveTier2Count; }

	UFUNCTION(BlueprintPure, Category = "Ecosystem|Lighting")
	int32 GetActiveTier3WorldCount() const noexcept { return ActiveTier3WorldCount; }

	UFUNCTION(BlueprintPure, Category = "Ecosystem|Lighting")
	int32 GetCandidateCount() const noexcept { return Candidates.Num(); }

	const FDungeonLightCandidate* GetCandidate(int32 CandidateId) const noexcept
	{
		return Candidates.IsValidIndex(CandidateId) ? &Candidates[CandidateId] : nullptr;
	}

	const TStaticArray<FVector4f, MaxTier1>& GetPackedTier1Buffer() const noexcept { return PackedTier1; }

	bool IsLowQualityMode() const noexcept;

private:
	void GatherViewerLocations(TStaticArray<FVector, 4>& OutLocations, int32& OutCount) const noexcept;
	void ExpireTimedLeases(double NowSec) noexcept;
	void UpdatePointLightPools(float DeltaTime, double NowSec) noexcept;
	void PushTier1ToMPC() noexcept;

	TArray<FDungeonLightCandidate> Candidates;
	int32 TransientStartIndex = INDEX_NONE;
	int32 NextTransientOffset = 0;

	TStaticArray<TObjectPtr<UPointLightComponent>, MaxTier2> Tier2Pool;
	TStaticArray<TObjectPtr<UPointLightComponent>, MaxTier3World> Tier3Pool;
	TStaticArray<float, MaxTier2> Tier2SmoothedIntensity;
	TStaticArray<float, MaxTier3World> Tier3SmoothedIntensity;

	TStaticArray<FVector4f, MaxTier1> PackedTier1;
	TStaticArray<FName, MaxTier1> Tier1MPCNames;

	TArray<int32> ScratchTier2Chosen;
	TArray<int32> ScratchTier3Chosen;

	int32 ActiveTier1Count = 0;
	int32 ActiveTier2Count = 0;
	int32 ActiveTier3WorldCount = 0;
	int32 ShadowQualityOverride = -1;
	float AssignmentAccumulator = 0.0f;
	bool bBudgetDirty = true;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LightPoolOwner;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> CachedSensoryMPC;
};
