#pragma once

/**
 * DungeonFaunaSubsystem.h
 *
 * Pre-allocated Structure-of-Arrays (SoA) Ambient Fauna Simulation for UE 5.8 C++20:
 *  - Lumen-Moths / Fireflies (cell 0): Boids cohesion + tangential orbit around Tier 1-3 anchors;
 *    scatter with stretched emissive quads on sprint (>1.15x) or Fast Thrower (<250 UU);
 *    up to 3 roost near hero lantern if stationary (<120 UU/s for 3s).
 *  - Lantern-Beetles (cell 1): 4-compass wall-skittering critters registering moving Tier 1 light nodes.
 *  - Puddle-Newts (cell 2) & Crypt-Crabs (cell 3): inhabit ShallowPuddle / AnkleDeepStream cells;
 *    dart <180 UU with 4s cooldown spawning a water ripple + 0.25-loudness decoy ring (<1.55 lure threshold).
 *  - Single UInstancedStaticMeshComponent (ISM_Fauna, 256 pre-allocated slots, zero Tick heap allocs);
 *    instances >2000 UU from every local player are frozen cleanly.
 */

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"
#include "Dungeon/DungeonTypes.h"
#include "Ecosystem/DungeonEcosystemTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "DungeonFaunaSubsystem.generated.h"

class UInstancedStaticMeshComponent;
struct FDungeoneerWallInstanceEntry;

UENUM(BlueprintType)
enum class EFaunaState : uint8
{
	Idle    = 0,
	Orbit   = 1,
	Flee    = 2,
	Roost   = 3,
	Scuttle = 4,
	Dart    = 5
};

/** Pre-allocated SoA buffer holding all 256 ambient critters with zero AActor spawns. */
struct FFaunaSoA
{
	static constexpr int32 Capacity = 256;

	TStaticArray<FVector3f, Capacity> Pos;
	TStaticArray<FVector3f, Capacity> Vel;
	TStaticArray<FVector3f, Capacity> Home;
	TStaticArray<float, Capacity> StateTimer;
	TStaticArray<float, Capacity> NoiseCooldown;
	TStaticArray<float, Capacity> Phase;
	TStaticArray<float, Capacity> Emissive;
	TStaticArray<float, Capacity> EventTime;
	TStaticArray<float, Capacity> Stretch;
	TStaticArray<uint16, Capacity> AnchorLight; // Tier 1/2/3 candidate index (light anchor or moving node)
	TStaticArray<uint8, Capacity> Species;
	TStaticArray<uint8, Capacity> State;
	TStaticArray<uint8, Capacity> bActive;

	void ResetAll() noexcept
	{
		for (int32 i = 0; i < Capacity; ++i)
		{
			Pos[i] = FVector3f::ZeroVector;
			Vel[i] = FVector3f::ZeroVector;
			Home[i] = FVector3f::ZeroVector;
			StateTimer[i] = 0.0f;
			NoiseCooldown[i] = 0.0f;
			Phase[i] = 0.0f;
			Emissive[i] = 1.0f;
			EventTime[i] = 0.0f;
			Stretch[i] = 1.0f;
			AnchorLight[i] = MAX_uint16;
			Species[i] = 0;
			State[i] = 0;
			bActive[i] = 0;
		}
	}
};

/** Input geometry passed from ADungeoneerTileMapInstancer on floor build. */
struct FFaunaFloorBuildInput
{
	const FDungeonFloorGraph* FloorGraph = nullptr;
	const TArray<FDungeoneerWallInstanceEntry>* WallEntries = nullptr;
	const TArray<FVector>* PuddleLocations = nullptr;
	UInstancedStaticMeshComponent* FaunaISM = nullptr;
	float TileSizeUU = 256.0f;
};

UCLASS()
class DARKDUNGEONEER_API UDungeonFaunaSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxCritters = FFaunaSoA::Capacity;
	static constexpr int32 MaxTrackedPlayers = 16;

	UDungeonFaunaSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UDungeonFaunaSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return !IsTemplate(); }

	/** Clears, pre-allocates 256 ISM instances, and populates seeded moths, beetles, newts, and crabs. */
	void OnFloorBuilt(const FFaunaFloorBuildInput& Input);

	/** Deterministic test / scripted helper spawning one active critter into the next free SoA slot. */
	int32 SpawnScriptedCritter(
		EFaunaSpecies Species,
		const FVector& WorldPos,
		int32 LightCandidateId = INDEX_NONE,
		float SegmentHalfLength = 120.0f,
		int32 TangentAxis = 0);

	/** Advances SoA fauna simulation at fixed step (callable directly by headless automation specs). */
	void StepFaunaSimulation(float DeltaSeconds);

	UFUNCTION(BlueprintPure, Category = "Ecosystem|Fauna")
	int32 GetActiveCritterCount() const noexcept { return ActiveCritterCount; }

	/** Returns heap byte capacity of ScratchTransforms so tests prove zero allocation across ticks. */
	SIZE_T GetScratchTransformsAllocatedSize() const noexcept { return ScratchTransforms.GetAllocatedSize(); }

	EFaunaState GetCritterStateAt(int32 Index) const noexcept
	{
		return (Index >= 0 && Index < MaxCritters) ? static_cast<EFaunaState>(SoA.State[Index]) : EFaunaState::Idle;
	}

	EFaunaSpecies GetCritterSpeciesAt(int32 Index) const noexcept
	{
		return (Index >= 0 && Index < MaxCritters) ? static_cast<EFaunaSpecies>(SoA.Species[Index]) : EFaunaSpecies::LumenMoth;
	}

	float GetCritterNoiseCooldownAt(int32 Index) const noexcept
	{
		return (Index >= 0 && Index < MaxCritters) ? SoA.NoiseCooldown[Index] : 0.0f;
	}

	/** Zero-allocation read accessor finding nearest Lumen-Moth or Lantern-Beetle for acoustic flutter/clicks. */
	bool FindNearestAudibleBioEmitter(const FVector& ListenerPos, FVector& OutWorldPos, EFaunaSpecies& OutSpecies) const noexcept;

private:
	void ResetMetadataArrays() noexcept;
	void EnsureFaunaISMConfigured(UInstancedStaticMeshComponent* FaunaISM);
	float ResolveWrappedEcoTime() const noexcept;

	FFaunaSoA SoA;

	// Companion fixed metadata parallel to SoA (beetle wall tangent lengths & axes)
	TStaticArray<float, MaxCritters> SegmentHalfSpan;
	TStaticArray<uint8, MaxCritters> TangentAxisFlags;
	TStaticArray<uint8, MaxCritters> DirtyCustomFlags;

	// Tracked player stillness duration for moth lantern roosting (<= 16 local/coop players)
	TStaticArray<float, MaxTrackedPlayers> PlayerStillAccumSec;
	TStaticArray<uint8, MaxTrackedPlayers> RoostedMothsForPlayer;

	// Pre-allocated batch transform scratch buffer (SetNum(256) once; zero realloc in Tick)
	TArray<FTransform> ScratchTransforms;

	TWeakObjectPtr<UInstancedStaticMeshComponent> BoundFaunaISM;
	int32 ActiveCritterCount = 0;
	int32 BeetlesCount = 0;
	float SimAccumSec = 0.0f;
	float PendingEcoTimeFallback = 0.0f;
};
