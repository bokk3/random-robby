#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Dungeon/DungeonTypes.h"
#include "DungeoneerTileMapInstancer.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMesh;
struct FHitResult;

/**
 * 8-compass wall orientation axes supporting orthogonal (N/S/E/W) and
 * 45-degree diagonal corridor segments (NE/NW/SE/SW).
 */
UENUM(BlueprintType)
enum class EDungeoneerWallAxis : uint8
{
	North     UMETA(DisplayName = "North (+Y Back Wall)"),
	South     UMETA(DisplayName = "South (-Y Foreground Occluder)"),
	East      UMETA(DisplayName = "East (+X Right Wall)"),
	West      UMETA(DisplayName = "West (-X Left Wall)"),
	NorthEast UMETA(DisplayName = "North-East (45 deg Diagonal)"),
	NorthWest UMETA(DisplayName = "North-West (45 deg Diagonal)"),
	SouthEast UMETA(DisplayName = "South-East (45 deg Diagonal Occluder)"),
	SouthWest UMETA(DisplayName = "South-West (45 deg Diagonal Occluder)")
};

/** Pre-allocated metadata describing one wall HISM instance for zero-allocation Tick cutaway sweeps. */
USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API FDungeoneerWallInstanceEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|HISM")
	int32 HISMInstanceIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|HISM")
	EDungeoneerWallAxis WallAxis = EDungeoneerWallAxis::North;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|HISM")
	FVector WorldCenter = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|HISM")
	FVector WorldHalfExtents = FVector(120.0f, 36.0f, 140.0f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|HISM")
	int32 FullUVCellIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|HISM")
	int32 CutawayUVCellIndex = 4;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|HISM")
	bool bCanCutawayForCamera = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|HISM")
	bool bCurrentlyCutaway = false;
};

/** Pre-allocated metadata describing a mirror HISM instance for analytical & trace light-bounce math. */
USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API FDungeoneerMirrorInstanceEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Mirror")
	int32 HISMInstanceIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Mirror")
	FVector WorldCenter = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Mirror")
	FVector SurfaceNormal = FVector(0.0f, 1.0f, 0.0f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Mirror")
	FVector MirrorTangent = FVector(1.0f, 0.0f, 0.0f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Mirror")
	float HalfWidthUU = 96.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Mirror")
	float HalfHeightUU = 128.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Mirror")
	bool bIsIllusoryPhantasm = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Mirror")
	bool bIllusionSeamRevealed = false;
};

/** Pre-allocated metadata describing a breakable barrel/urn/trap instance that emits Acute Hearing ripples. */
USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API FDungeoneerBreakableInstanceEntry
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Breakables")
	int32 HISMInstanceIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Breakables")
	FVector WorldCenter = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Breakables")
	int32 IntactUVCellIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Breakables")
	int32 ShatteredUVCellIndex = 2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Breakables")
	float TriggerRadiusUU = 85.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Breakables")
	float ImpactLoudness = 1.25f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|Breakables")
	bool bIsShattered = false;
};

/**
 * ADungeoneerTileMapInstancer
 *
 * Zero-draw-call modular 2.5D dungeon assembler using UHierarchicalInstancedStaticMeshComponent
 * (HISM) along orthogonal (N/S/E/W) and 45-degree diagonal (NE/NW/SE/SW) compass axes.
 *
 * Features:
 *  - Per-viewport sphere-trace South-wall cutaway (`CustomData[0] = 1.0f`) preserving `bCastHiddenShadow`.
 *  - Deterministic mirror wall & 45-degree bounce prism registry tagged `"MirrorReflective"`.
 *  - Step-triggered breakables (`PROP_Breakables_And_Traps`) that swap to shattered UV cells
 *    and emit Acute Hearing noise ripples into `UDungeoneerSensoryWorldSubsystem`.
 */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API ADungeoneerTileMapInstancer : public AActor
{
	GENERATED_BODY()

public:
	ADungeoneerTileMapInstancer();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Rebuilds all modular HISM floor tiles, 4-axis walls, mirrors, and breakables from a procedural floor graph. */
	UFUNCTION(BlueprintCallable, Category = "Dungeon|Assembly")
	void BuildDungeonFromFloorGraph(const FDungeonFloorGraph& FloorGraph);

	/** Convenience builder fetching the active graph from UDungeonGeneratorSubsystem. */
	UFUNCTION(BlueprintCallable, Category = "Dungeon|Assembly")
	void BuildFromActiveGenerator();

	/**
	 * Performs a zero-allocation per-viewport sphere trace + segment check for every local split-screen
	 * viewport (`GetLocalPlayers()`), swapping occluded foreground walls to cutaway stubs (`CustomData[0] = 1.0f`).
	 */
	void UpdateSouthWallCutawaysForViewports() noexcept;

	/**
	 * Evaluates a single explicit camera-to-player segment and immediately applies dirty `CustomData[0]`
	 * cutaway updates. Useful for single-frame verification and unit testing.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dungeon|Cutaway")
	int32 EvaluateCameraToPlayerSegmentCutaway(const FVector& CameraLocation, const FVector& PlayerLocation);

	/**
	 * Tests whether an entity at `EntityWorldLocation` stepped onto intact `PROP_Breakables_And_Traps`
	 * instances; swaps shattered UV cells via `SetCustomDataValue` and broadcasts `NoiseEvent` ripples.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dungeon|Breakables")
	int32 EvaluateEntityStepOnBreakables(
		const FVector& EntityWorldLocation,
		float OverrideLoudness = -1.0f,
		float OverrideTriggerRadiusUU = -1.0f);

	/** Reveals a specific illusory mirror wall seam (`CustomData[0] = 1.0f`) when struck by bounced flashlight light. */
	UFUNCTION(BlueprintCallable, Category = "Dungeon|Mirror")
	void RevealMirrorIllusionSeam(int32 MirrorEntryIndex, bool bRevealed = true);

	/** Registers an explicit mirror panel instance (e.g. from scripted rooms or test fixtures). */
	int32 RegisterMirrorInstance(
		const FVector& WorldCenter,
		const FVector& SurfaceNormal,
		float HalfWidthUU,
		float HalfHeightUU,
		bool bIsIllusoryPhantasm,
		int32 UVCellIndex = 0);

	/** Registers an explicit breakable prop instance cleanly inside BreakableHISM. */
	int32 RegisterBreakableInstance(
		const FVector& WorldCenter,
		int32 IntactUVCellIndex = 0,
		int32 ShatteredUVCellIndex = 2,
		float TriggerRadiusUU = 85.0f,
		float ImpactLoudness = 1.25f);

	UFUNCTION(BlueprintPure, Category = "Dungeon|HISM")
	UHierarchicalInstancedStaticMeshComponent* GetFloorHISM() const noexcept { return FloorHISM.Get(); }

	UFUNCTION(BlueprintPure, Category = "Dungeon|HISM")
	UHierarchicalInstancedStaticMeshComponent* GetWallHISM() const noexcept { return WallHISM.Get(); }

	UFUNCTION(BlueprintPure, Category = "Dungeon|HISM")
	UHierarchicalInstancedStaticMeshComponent* GetMirrorHISM() const noexcept { return MirrorHISM.Get(); }

	UFUNCTION(BlueprintPure, Category = "Dungeon|HISM")
	UHierarchicalInstancedStaticMeshComponent* GetBreakableHISM() const noexcept { return BreakableHISM.Get(); }

	const TArray<FDungeoneerWallInstanceEntry>& GetWallEntries() const noexcept { return WallEntries; }
	const TArray<FDungeoneerMirrorInstanceEntry>& GetMirrorEntries() const noexcept { return MirrorEntries; }
	const TArray<FDungeoneerBreakableInstanceEntry>& GetBreakableEntries() const noexcept { return BreakableEntries; }

protected:
	void EnsureMeshAssetsInitialized();
	void ClearAllInstances();
	void ResetPendingCutaways() noexcept;
	void AccumulateCutawayAlongSegment(const FVector& CameraLocation, const FVector& PlayerLocation) noexcept;
	void ApplyPendingCutawayTransitions() noexcept;
	void AddWallInstance(
		const FVector& WorldCenter,
		EDungeoneerWallAxis Axis,
		const FVector& HalfExtents,
		int32 FullUVCell,
		int32 CutawayUVCell);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|HISM")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|HISM")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> FloorHISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|HISM")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> WallHISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|HISM")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> MirrorHISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Dungeon|HISM")
	TObjectPtr<UHierarchicalInstancedStaticMeshComponent> BreakableHISM;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Layout")
	float TileSizeUU = 256.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Layout")
	float WallHeightUU = 280.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon|Cutaway")
	float CutawaySphereRadiusUU = 80.0f;

private:
	UPROPERTY(Transient)
	TArray<FDungeoneerWallInstanceEntry> WallEntries;

	UPROPERTY(Transient)
	TArray<FDungeoneerMirrorInstanceEntry> MirrorEntries;

	UPROPERTY(Transient)
	TArray<FDungeoneerBreakableInstanceEntry> BreakableEntries;

	/** Pre-allocated byte mask parallel to WallEntries so Tick writes CustomData without heap allocation. */
	TArray<uint8> PendingCutawayFlags;

	/** Pre-allocated scratch buffer for World->SweepMultiByChannel inside Tick. */
	TArray<FHitResult> ScratchSweepHits;
};
