#pragma once

#include "CoreMinimal.h"
#include "DungeonTypes.generated.h"

UENUM(BlueprintType)
enum class EDungeonRoomType : uint8
{
	Entry            = 0 UMETA(DisplayName = "Entry Chamber"),
	Combat           = 1 UMETA(DisplayName = "Combat Room"),
	MirrorPalaceMaze = 2 UMETA(DisplayName = "Mirror-Palace Illusion Maze"),
	Vendor           = 3 UMETA(DisplayName = "Merchant Alcove"),
	RestingGround    = 4 UMETA(DisplayName = "Resting Ground Sanctuary"),
	DeepFogGauntlet  = 5 UMETA(DisplayName = "Deep Fog Gauntlet"),
	MajorLootVault   = 6 UMETA(DisplayName = "Terminal Major Loot Vault")
};

UENUM(BlueprintType)
enum class EMirrorCellType : uint8
{
	OpenFloor    = 0,
	SolidMirror  = 1,
	PhantasmWall = 2,
	DecoyAnchor  = 3
};

USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API FDungeonNode
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon")
	int32 NodeId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon")
	int32 DepthLayer = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon")
	EDungeonRoomType RoomType = EDungeonRoomType::Combat;

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon")
	FVector WorldCenter = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon")
	FVector RoomExtents = FVector(1200.0f, 1200.0f, 400.0f);

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon")
	int32 RoomSeed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon")
	TArray<int32> OutgoingNodeIds;
};

USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API FDungeonFloorGraph
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon")
	int32 FloorIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon")
	int32 MasterSeed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon")
	TArray<FDungeonNode> Nodes;
};
