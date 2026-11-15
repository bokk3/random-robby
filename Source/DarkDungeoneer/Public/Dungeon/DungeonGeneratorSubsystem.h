#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Dungeon/DungeonTypes.h"
#include "DungeonGeneratorSubsystem.generated.h"

/**
 * Procedural Multi-Floor DAG Dungeon Generator Subsystem (`UDungeonGeneratorSubsystem`).
 * Uses server-seeded FRandomStream to produce deterministic floor graphs:
 *   Entry -> Combat/Exploration -> Mirror-Palace Illusion Maze ->
 *   Mid-Floor Branch (Vendor OR Resting Ground) -> Deep Fog Gauntlet -> Major Loot Vault.
 */
UCLASS()
class DARKDUNGEONEER_API UDungeonGeneratorSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Generates a deterministic Directed Acyclic Graph for a dungeon floor. */
	UFUNCTION(BlueprintCallable, Category = "Dungeon")
	FDungeonFloorGraph GenerateFloorGraph(int32 FloorIndex, int32 ServerSeed);

	/** Generates an orthogonal Mirror-Palace grid layout guaranteeing at least one traversable path. */
	UFUNCTION(BlueprintCallable, Category = "Dungeon|MirrorMaze")
	void GenerateMirrorMazeGrid(int32 RoomSeed, int32 GridWidth, int32 GridHeight, TArray<EMirrorCellType>& OutCells) const;

	/** Deterministic floor biome progression: Floor 1 Crypt, Floor 2 Spore Fens, Floor 3+ Ember Forge. */
	UFUNCTION(BlueprintPure, Category = "Dungeon|Biome")
	static EDungeonBiomeType ResolveFloorDominantBiome(int32 FloorIndex) noexcept;

	/** Next floor biome previewed in the final 2 transition nodes before the descending stairs. */
	UFUNCTION(BlueprintPure, Category = "Dungeon|Biome")
	static EDungeonBiomeType ResolveNextFloorBiome(int32 FloorIndex) noexcept;

	UFUNCTION(BlueprintPure, Category = "Dungeon")
	const FDungeonFloorGraph& GetActiveFloorGraph() const noexcept { return ActiveFloorGraph; }

	/** Returns true if the given world position lies inside a Resting Ground (safe zone disabling zombie spawns). */
	UFUNCTION(BlueprintPure, Category = "Dungeon")
	bool IsLocationInRestingGround(const FVector& WorldLocation) const noexcept;

	void RegisterRestingGroundBounds(const FBox& Bounds);

private:
	FDungeonFloorGraph ActiveFloorGraph;
	TArray<FBox> RestingGroundBounds;
};
