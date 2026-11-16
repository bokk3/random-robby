#pragma once

/**
 * DungeonFloraManagerComponent.h
 *
 * Interactive Dungeon Flora System (`HISM_MagicalFlora`) owned by ADungeoneerTileMapInstancer:
 *  - Prism-Lily (Row 0, cells 0..3): dormant 0.2 -> full bloom 12.0 magenta/gold via flashlight or
 *    UMirrorLightBounceComponent ray (<60 UU), acquiring a 15s Tier 2 point light lease.
 *  - Spore-Cap (Row 1, cells 4..7): entity proximity <120 UU (6s cooldown) emits 12 spores into
 *    ISM_WaterFX and a 1.2s emerald Tier 1 glow puff.
 *  - Echo-Crystal (Row 2, cells 8..11): observes SensoryWorld->GetNoiseEventSerial() and flashes cyan
 *    with harmonic chord index when covered by an active noise pulse (never re-emits noise events).
 *  - Pulse-Moss (Row 3, cells 12..15): steps trigger a fixed-queue CSR BFS wave writing CD5 + hop*0.08.
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/StaticArray.h"
#include "Dungeon/DungeonTypes.h"
#include "Ecosystem/DungeonEcosystemTypes.h"
#include "DungeonFloraManagerComponent.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
struct FDungeoneerMirrorInstanceEntry;

UCLASS(ClassGroup = (Dungeoneer), meta = (BlueprintSpawnableComponent))
class DARKDUNGEONEER_API UDungeonFloraManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDungeonFloraManagerComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Clears, populates biome-seeded interactive flora, builds Pulse-Moss CSR graph, and binds material. */
	void PopulateForFloor(
		const FDungeonFloorGraph& FloorGraph,
		const TArray<FDungeoneerMirrorInstanceEntry>* MirrorEntries = nullptr,
		float TileSizeUU = 256.0f);

	/** Deterministic test / scripted helper adding a single flora instance and returning its SoA index. */
	int32 AddFloraInstance(EFloraKind Kind, const FVector& WorldPos, const FLinearColor& Hue, float Scale = 1.0f);

	/** Rebuilds CSR Pulse-Moss adjacency graph cleanly outside Tick (called at end of PopulateForFloor). */
	void RebuildPulseMossCSRGraph(float MaxEdgeDistanceUU = 280.0f);

	/** Lights a Prism-Lily by index (transitions cell to open bloom 3, CD1=12, requests 15s Tier 2 lease). */
	void IgnitePrismLily(int32 FloraIndex, float LeaseSeconds = 15.0f);

	/** Runs zero-allocation CSR BFS from nearest Pulse-Moss node within TriggerRadiusUU, setting wave phase. */
	int32 TriggerPulseMossWaveNear(const FVector& WorldPos, float TriggerRadiusUU = 95.0f);

	/** Fires a Spore-Cap puff burst + 1.2s Tier 1 transient glow if cooldown elapsed. */
	bool TriggerSporeCapNear(const FVector& WorldPos, float TriggerRadiusUU = 120.0f);

	/** Scans latest SensoryWorld noise events and flashes covered Echo-Crystals (never emits new noise). */
	int32 EvaluateEchoCrystalsAgainstNoise();

	/** Advances flora simulation step cleanly at 30 Hz (callable directly by automation tests). */
	void StepFloraSimulation(float DeltaSeconds);

	UFUNCTION(BlueprintPure, Category = "Ecosystem|Flora")
	UHierarchicalInstancedStaticMeshComponent* GetFloraHISM() const noexcept { return FloraHISM.Get(); }

	UFUNCTION(BlueprintPure, Category = "Ecosystem|Flora")
	int32 GetFloraCount() const noexcept { return Kinds.Num(); }

	EFloraKind GetFloraKindAt(int32 Index) const noexcept
	{
		return Kinds.IsValidIndex(Index) ? Kinds[Index] : EFloraKind::PrismLily;
	}

	uint8 GetFloraStateAt(int32 Index) const noexcept
	{
		return States.IsValidIndex(Index) ? States[Index] : 0;
	}

	float GetFloraEmissiveAt(int32 Index) const noexcept
	{
		return Emissives.IsValidIndex(Index) ? Emissives[Index] : 0.0f;
	}

	float GetFloraEventTimeAt(int32 Index) const noexcept
	{
		return EventTimes.IsValidIndex(Index) ? EventTimes[Index] : 0.0f;
	}

	int32 GetFloraLightCandidateAt(int32 Index) const noexcept
	{
		return LightCandidateIds.IsValidIndex(Index) ? LightCandidateIds[Index] : INDEX_NONE;
	}

	/** Exposed so crystal & mirror queries can inspect crystal locations after PopulateForFloor. */
	void GatherCrystalPositions(TArray<FVector>& OutPositions) const;

	/** Zero-allocation proximity check for Layer C continuous Flora Brush Chime cross-fade. */
	bool IsPositionNearAnyFlora(const FVector& WorldPos, float RadiusUU = 110.0f) const noexcept;

	/** Returns nearest Echo-Crystal location to QueryPos, or false if none exist. */
	bool FindNearestEchoCrystal(const FVector& QueryPos, FVector& OutCrystalPos) const noexcept;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnEchoCrystalChimeSignature, const FVector& /*CrystalPos*/, int32 /*ChordIndex*/);
	FOnEchoCrystalChimeSignature OnEchoCrystalChime;

private:
	void EnsureFloraHISMVisuals();
	void PushCustomDataIfBudget(int32 FloraIdx, int32& InOutWritesRemaining, bool& bOutAnyDirty);
	static float PointToSegmentDistSq(const FVector& Pt, const FVector& SegA, const FVector& SegB) noexcept;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ecosystem|Flora", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FloraHISM;

	// Parallel SoA storage pre-allocated at floor build (zero Tick allocations)
	TArray<EFloraKind> Kinds;
	TArray<FVector> WorldPositions;
	TArray<FLinearColor> Hues;
	TArray<uint8> States;          // 0 = dormant/idle, 1 = bloomed/charged/rippling
	TArray<float> StateTimers;     // remaining bloom/flash or cooldown seconds
	TArray<float> Emissives;
	TArray<float> EventTimes;
	TArray<int32> UVCells;
	TArray<int32> LightCandidateIds;
	TArray<int32> HISMInstanceIndices;

	// Compact CSR adjacency for Pulse-Moss clusters
	TArray<int32> MossIndices;     // maps dense moss idx [0..M-1] -> FloraIdx
	TArray<int32> FloraToDenseMoss;// maps FloraIdx -> dense moss idx or INDEX_NONE
	TArray<int32> MossOffsets;     // size M + 1
	TArray<int32> MossAdj;         // dense moss indices
	TArray<uint32> MossVisitedEpoch;
	uint32 CurrentBfsEpoch = 1;

	TStaticArray<int32, 64> BfsQueue;
	TStaticArray<uint8, 64> BfsHops;

	uint64 LastSeenNoiseSerial = 0;
	float SimAccumSec = 0.0f;
	float PendingEcoTimeFallback = 0.0f;
};
