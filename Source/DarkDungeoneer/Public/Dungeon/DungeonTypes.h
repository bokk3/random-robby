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
enum class EDungeonBiomeType : uint8
{
	Crypt        = 0 UMETA(DisplayName = "Sunken Crypt"),
	MirrorPalace = 1 UMETA(DisplayName = "Obsidian Mirror-Palace"),
	SporeFens    = 2 UMETA(DisplayName = "Blighted Spore Fens"),
	EmberForge   = 3 UMETA(DisplayName = "Cinder Kiln")
};

UENUM(BlueprintType)
enum class EDungeonRoomShape : uint8
{
	SquareChamber        = 0 UMETA(DisplayName = "Square Chamber"),
	OctagonalRotunda     = 1 UMETA(DisplayName = "Octagonal Rotunda"),
	PillaredGallery      = 2 UMETA(DisplayName = "Pillared Gallery"),
	CrossAmbushVault     = 3 UMETA(DisplayName = "Cross Ambush Vault"),
	SecretPhantasmPocket = 4 UMETA(DisplayName = "Secret Phantasm Pocket")
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

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon|Biome")
	EDungeonBiomeType BiomeType = EDungeonBiomeType::Crypt;

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon|Biome")
	EDungeonBiomeType BleedBiomeType = EDungeonBiomeType::Crypt;

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon|Shape")
	EDungeonRoomShape RoomShape = EDungeonRoomShape::SquareChamber;

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon|Secret")
	bool bIsSecretRoom = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon|Secret")
	int32 ParentRoomNodeId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon|Loop")
	bool bHasDescendingStairs = false;

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

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon|Biome")
	EDungeonBiomeType DominantBiome = EDungeonBiomeType::Crypt;

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon|Biome")
	EDungeonBiomeType NextFloorPreviewBiome = EDungeonBiomeType::SporeFens;

	UPROPERTY(BlueprintReadOnly, Category = "Dungeon")
	TArray<FDungeonNode> Nodes;
};
