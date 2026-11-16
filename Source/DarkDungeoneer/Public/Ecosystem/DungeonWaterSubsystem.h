#pragma once

/**
 * DungeonWaterSubsystem.h
 *
 * Manages all 5 hydrological forms and surface resolution in the Living Ecosystem:
 *  1. Ceiling Drips (24 seeded accumulators falling by z = h - 0.5*g*t^2 -> 4-frame ripples)
 *  2. Shallow Puddles (SurfaceGrid marking + 4s fading wet footprints for 6 dry steps)
 *  3. Ankle-Deep Wading Streams (0.88x speed zone, UV flow scroll, 48 drifting petals, 4-frame splashes)
 *  4. Weeping Wall Cascades (vertical flipbook quads on N/E/W walls + cascade audio anchors)
 *  5. Bio & Thermal Geysers (pulsing vent pools in Sanctuary, SporeFens, and EmberForge)
 *
 * O(1) SurfaceGrid lookup (128 UU cells) and 256-slot pre-allocated ISM_WaterFX pool with zero Tick heap allocs.
 */

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"
#include "Core/DungeoneerTypes.h"
#include "Dungeon/DungeonTypes.h"
#include "Ecosystem/DungeonEcosystemTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "DungeonWaterSubsystem.generated.h"

class AActor;
class UHierarchicalInstancedStaticMeshComponent;
class UInstancedStaticMeshComponent;
class UDungeonLightBudgetSubsystem;
struct FDungeoneerWallInstanceEntry;

USTRUCT()
struct FWaterCeilingDripState
{
	GENERATED_BODY()

	FVector SurfacePos = FVector::ZeroVector;
	float CeilingHeightUU = 240.0f;
	float PeriodSec = 2.4f;
	float PhaseSec = 0.0f;
	bool bFalling = false;
	float FallElapsed = 0.0f;
	bool bActive = false;
};

USTRUCT()
struct FWaterFxSlotState
{
	GENERATED_BODY()

	FVector WorldPos = FVector::ZeroVector;
	FVector BaseScale = FVector::OneVector;
	FRotator Rotation = FRotator::ZeroRotator;
	EWaterFxKind Kind = EWaterFxKind::Ripple;
	float AgeSec = 0.0f;
	float LifetimeSec = 0.0f; // <= 0 means persistent / wrapping
	float DriftSpeed = 0.0f;
	FVector DriftDir = FVector::ZeroVector;
	FVector StreamMin = FVector::ZeroVector;
	FVector StreamMax = FVector::ZeroVector;
	bool bOccupied = false;
};

USTRUCT()
struct FWaterWalkerTrack
{
	GENERATED_BODY()

	TWeakObjectPtr<const AActor> Walker;
	int32 RemainingWetSteps = 0;
	EDungeonSurfaceKind LastSurface = EDungeonSurfaceKind::DryStone;
};

USTRUCT()
struct FWaterAudioEmitterRef
{
	GENERATED_BODY()

	FVector WorldPos = FVector::ZeroVector;
	EWaterForm Form = EWaterForm::WadingStream;
};

/** Input geometry record passed from ADungeoneerTileMapInstancer during floor build. */
struct FWaterFloorBuildInput
{
	const FDungeonFloorGraph* FloorGraph = nullptr;
	const TArray<FDungeoneerWallInstanceEntry>* WallEntries = nullptr;
	const TArray<FVector>* PuddleLocations = nullptr;
	const TArray<FVector>* CrystalLocations = nullptr;
	UHierarchicalInstancedStaticMeshComponent* WaterHISM = nullptr;
	UInstancedStaticMeshComponent* WaterFxISM = nullptr;
	float TileSizeUU = 256.0f;
	float WallHeightUU = 280.0f;
};

UCLASS()
class DARKDUNGEONEER_API UDungeonWaterSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr float GridCellSizeUU = 128.0f;
	static constexpr int32 MaxCeilingDrips = 24;
	static constexpr int32 MaxWaterFxSlots = 256;
	static constexpr int32 FootprintRingStart = 0;
	static constexpr int32 FootprintRingCount = 64;
	static constexpr int32 RippleRingStart = 64;
	static constexpr int32 RippleRingCount = 64;
	static constexpr int32 PetalPoolStart = 128;
	static constexpr int32 PetalPoolCount = 48;
	static constexpr int32 CascadeGeyserStart = 176;
	static constexpr int32 CascadeGeyserCount = 40;
	static constexpr int32 DropletPoolStart = 216;
	static constexpr int32 DropletPoolCount = 40;
	static constexpr int32 MaxTrackedWalkers = 64;

	UDungeonWaterSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UDungeonWaterSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return !IsTemplate(); }

	/** Rebuilds O(1) SurfaceGrid, stream/cascade/geyser instances, and pre-allocates 256 FX slots. */
	void OnFloorBuilt(const FWaterFloorBuildInput& Input);

	/** O(1) spatial surface category lookup replacing missing UPhysicalMaterial floor assets. */
	UFUNCTION(BlueprintPure, Category = "Ecosystem|Water")
	EDungeonSurfaceKind ResolveSurface(const FVector& WorldLocation) const noexcept;

	/** True when WorldLocation sits inside an AnkleDeepStream cell (triggers 0.88x wading speed + boot drop). */
	UFUNCTION(BlueprintPure, Category = "Ecosystem|Water")
	bool IsWadingLocation(const FVector& WorldLocation) const noexcept
	{
		return ResolveSurface(WorldLocation) == EDungeonSurfaceKind::AnkleDeepStream;
	}

	/** Stamps a world cell into the SurfaceGrid (used by Flora for SquishyMoss / CrystalShard). */
	void SetSurfaceCell(const FVector& WorldLocation, EDungeonSurfaceKind Kind) noexcept;

	/** Deterministic step hook called on walk column 1 & 3 transitions; spawns footprints & splashes. */
	void NotifyWalkerStep(const AActor* Walker, const FVector& WorldLocation, ECompassOctant MoveOctant, bool bIsPlayer);

	/** Spawns N drifting spore particles in ISM_WaterFX (used by Spore-Cap flora). */
	void SpawnSporePuffBurst(const FVector& Origin, int32 Count = 12, const FLinearColor& Color = FLinearColor(0.10f, 0.95f, 0.45f, 1.0f));

	/** Spawns a 4-frame expanding water ripple (used by PuddleNewts, CryptCrabs & drips). */
	void SpawnRippleAt(const FVector& Origin, const FLinearColor& Color = FLinearColor(0.20f, 0.85f, 1.00f, 1.0f), float Scale = 0.72f);

	/** Advances drips and pooled FX deterministically (also callable directly by automation specs). */
	void StepHydrologySimulation(float DeltaSeconds);

	UFUNCTION(BlueprintPure, Category = "Ecosystem|Water")
	int32 GetGridWidth() const noexcept { return GridWidth; }

	UFUNCTION(BlueprintPure, Category = "Ecosystem|Water")
	int32 GetGridHeight() const noexcept { return GridHeight; }

	UFUNCTION(BlueprintPure, Category = "Ecosystem|Water")
	int32 GetActiveFootprintCount() const noexcept { return ActiveFootprintCount; }

	UFUNCTION(BlueprintPure, Category = "Ecosystem|Water")
	int32 GetActiveCascadeCount() const noexcept { return ActiveCascadeCount; }

	UFUNCTION(BlueprintPure, Category = "Ecosystem|Water")
	int32 GetActiveGeyserCount() const noexcept { return ActiveGeyserCount; }

	UFUNCTION(BlueprintPure, Category = "Ecosystem|Water")
	int32 GetActiveStreamTileCount() const noexcept { return ActiveStreamTileCount; }

	const TArray<FWaterAudioEmitterRef>& GetWaterAudioEmitters() const noexcept { return WaterAudioEmitters; }

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnCeilingDripImpactSignature, const FVector& /*ImpactLocation*/, float /*CeilingHeightUU*/, EWaterForm /*SourceForm*/);
	FOnCeilingDripImpactSignature OnCeilingDripImpact;

private:
	int32 WorldToCellIndex(const FVector& WorldLocation) const noexcept;
	void PaintDiscOnGrid(const FVector& Center, float RadiusUU, EDungeonSurfaceKind Kind) noexcept;
	void SpawnWaterFxInSlot(
		int32 SlotIdx,
		EWaterFxKind Kind,
		int32 UVCell,
		const FVector& WorldPos,
		const FRotator& Rot,
		const FVector& Scale,
		const FLinearColor& Hue,
		float Emissive,
		float LifetimeSec);
	int32 AcquireRingSlot(int32 RingStart, int32 RingCount, int32& InOutCursor) noexcept;
	void FlushFxSlotToISM(int32 SlotIdx, float WrappedEcoTime, bool bMarkDirty = false);
	float ResolveWrappedEcoTime() const noexcept;

	TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent> BoundWaterHISM;
	TWeakObjectPtr<UInstancedStaticMeshComponent> BoundWaterFxISM;

	TArray<uint8> SurfaceGrid;
	FVector2D GridOriginXY = FVector2D::ZeroVector;
	int32 GridWidth = 0;
	int32 GridHeight = 0;

	TStaticArray<FWaterCeilingDripState, MaxCeilingDrips> Drips;
	int32 ActiveDripCount = 0;

	TStaticArray<FWaterFxSlotState, MaxWaterFxSlots> FxSlots;
	int32 FootprintCursor = 0;
	int32 RippleCursor = 0;
	int32 DropletCursor = 0;
	int32 ActiveFootprintCount = 0;
	int32 ActiveCascadeCount = 0;
	int32 ActiveGeyserCount = 0;
	int32 ActiveStreamTileCount = 0;

	TStaticArray<FWaterWalkerTrack, MaxTrackedWalkers> WalkerTracks;
	TArray<FWaterAudioEmitterRef> WaterAudioEmitters;
	float HydrologyAccumSec = 0.0f;
	float PendingEcoTimeFallback = 0.0f;
	bool bAnyFxDirty = false;
};
