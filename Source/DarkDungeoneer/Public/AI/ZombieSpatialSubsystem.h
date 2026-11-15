#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/DungeoneerTypes.h"
#include "ZombieSpatialSubsystem.generated.h"

class ADungeoneerCharacter;
class AZombieBase;
class AZombieFastThrower;
class AHazardZoneBase;
class AHazardZone_Fire;
class AHazardZone_Poison;
class AHazardZone_ChronoGas;
class ADecalActor;

UENUM(BlueprintType)
enum class EHazardPoolType : uint8
{
	Fire     = 0,
	Poison   = 1,
	ChronoGas = 2
};

struct FSpatialEntityRecord
{
	TWeakObjectPtr<AActor> Actor;
	FVector CachedLocation = FVector::ZeroVector;
	bool bIsPlayer = false;
	bool bIsLitByFlashlight = false;
	float AcousticVisibility = 0.0f;
};

struct FZombieSpatialCell
{
	static constexpr int32 MaxEntitiesPerCell = 8;
	TStaticArray<FSpatialEntityRecord, MaxEntitiesPerCell> Records;
	int32 Count = 0;

	void Reset() noexcept { Count = 0; }
};

/**
 * Zero-allocation Spatial Hash & Pre-Allocated Object Pool Subsystem for Zombie AI and Hazards.
 * Eliminates GetAllActorsOfClass and SpawnActor from the Tick hot path.
 */
UCLASS()
class DARKDUNGEONEER_API UZombieSpatialSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Registers a player character in the fixed-size tracked player array (called on BeginPlay). */
	void RegisterPlayer(ADungeoneerCharacter* Player);
	void UnregisterPlayer(ADungeoneerCharacter* Player);

	/** Registers a zombie in the fixed-size tracked zombie array (called on BeginPlay). */
	void RegisterZombie(AZombieBase* Zombie);
	void UnregisterZombie(AZombieBase* Zombie);

	/** Zero-allocation read-only accessors over registered players for visibility/net-relevancy culling. */
	int32 GetTrackedPlayerCount() const noexcept { return NumTrackedPlayers; }
	ADungeoneerCharacter* GetTrackedPlayerAt(int32 Index) const noexcept
	{
		return (Index >= 0 && Index < NumTrackedPlayers) ? TrackedPlayers[Index].Get() : nullptr;
	}

	/** Rebuilds the fixed 2D spatial hash grid in O(N) with zero heap allocations. */
	void RefreshSpatialGrid() noexcept;

	/** Finds the nearest audible or flashlight-lit player without calling GetAllActorsOfClass. */
	ADungeoneerCharacter* FindNearestTargetPlayer(const FVector& QueryOrigin, float MaxHearingRange, bool& bOutLitByFlashlight) const noexcept;

	/** Pre-allocates Fire, Poison, ChronoGas hazard zones and scorch decals at match startup. */
	void PreallocateHazardPools(int32 PerTypeCapacity = 16);

	/** Acquires a pre-allocated pooled hazard zone without calling SpawnActor at runtime. */
	AHazardZoneBase* AcquirePooledHazard(EHazardPoolType Type, const FVector& WorldLocation, AZombieFastThrower* OwningThrower) noexcept;

	/** Applies radial detonation damage using tracked actors with zero allocations. */
	void ApplyRadialDetonationDamage(const FVector& Epicenter, float Radius, float MaxDamage, AActor* InstigatorActor) noexcept;

	/** Activates a pooled scorch decal at the explosion epicenter. */
	void ActivatePooledScorchDecal(const FVector& WorldLocation) noexcept;

	static constexpr int32 GridDimension = 32; // 32 x 32 = 1024 fixed spatial cells
	static constexpr float CellSizeUU = 250.0f;
	static constexpr int32 MaxTrackedPlayers = 16;
	static constexpr int32 MaxTrackedZombies = 128;
	static constexpr int32 PoolPerHazardType = 16;
	static constexpr int32 MaxScorchDecals = 32;

private:
	static int32 ComputeCellIndex(const FVector& Location) noexcept;

	struct FSpatialGridBuffer
	{
		TStaticArray<FZombieSpatialCell, GridDimension * GridDimension> Cells;
	};

	TUniquePtr<FSpatialGridBuffer> SpatialGrid;
	TStaticArray<TWeakObjectPtr<ADungeoneerCharacter>, MaxTrackedPlayers> TrackedPlayers;
	int32 NumTrackedPlayers = 0;

	TStaticArray<TWeakObjectPtr<AZombieBase>, MaxTrackedZombies> TrackedZombies;
	int32 NumTrackedZombies = 0;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AHazardZone_Fire>> FirePool;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AHazardZone_Poison>> PoisonPool;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AHazardZone_ChronoGas>> ChronoGasPool;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ADecalActor>> ScorchDecalPool;

	int32 FirePoolCursor = 0;
	int32 PoisonPoolCursor = 0;
	int32 ChronoPoolCursor = 0;
	int32 DecalPoolCursor = 0;
	bool bPoolsPreallocated = false;
};
