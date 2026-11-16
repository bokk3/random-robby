#include "Dungeon/DungeonGeneratorSubsystem.h"

void UDungeonGeneratorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RestingGroundBounds.Reset();
}

EDungeonBiomeType UDungeonGeneratorSubsystem::ResolveFloorDominantBiome(const int32 FloorIndex) noexcept
{
	const int32 CycleIdx = FMath::Max(0, FloorIndex - 1) % 3;
	switch (CycleIdx)
	{
	case 0:  return EDungeonBiomeType::Crypt;
	case 1:  return EDungeonBiomeType::SporeFens;
	default: return EDungeonBiomeType::EmberForge;
	}
}

EDungeonBiomeType UDungeonGeneratorSubsystem::ResolveNextFloorBiome(const int32 FloorIndex) noexcept
{
	return ResolveFloorDominantBiome(FloorIndex + 1);
}

FDungeonFloorGraph UDungeonGeneratorSubsystem::GenerateFloorGraph(int32 FloorIndex, int32 ServerSeed)
{
	RestingGroundBounds.Reset();
	FDungeonFloorGraph Graph;
	Graph.FloorIndex = FloorIndex;
	Graph.MasterSeed = ServerSeed ^ (FloorIndex * 7919);
	Graph.DominantBiome = ResolveFloorDominantBiome(FloorIndex);
	Graph.NextFloorPreviewBiome = ResolveNextFloorBiome(FloorIndex);

	FRandomStream Stream(Graph.MasterSeed);
	constexpr float RoomPitchX = 2800.0f;
	constexpr float RoomPitchY = 2600.0f;

	auto MakeNode = [&Stream, &Graph, RoomPitchX, RoomPitchY](
		int32 Id,
		int32 Depth,
		float LateralOffset,
		EDungeonRoomType Type,
		EDungeonRoomShape Shape,
		EDungeonBiomeType PrimaryBiome,
		EDungeonBiomeType BleedBiome) -> FDungeonNode
	{
		FDungeonNode Node;
		Node.NodeId = Id;
		Node.DepthLayer = Depth;
		Node.RoomType = Type;
		Node.RoomShape = Shape;
		Node.BiomeType = PrimaryBiome;
		Node.BleedBiomeType = BleedBiome;
		Node.bIsSecretRoom = (Shape == EDungeonRoomShape::SecretPhantasmPocket);
		Node.bHasDescendingStairs = (Type == EDungeonRoomType::MajorLootVault);
		Node.WorldCenter = FVector(static_cast<float>(Depth) * RoomPitchX, LateralOffset * RoomPitchY, 0.0f);
		if (Type == EDungeonRoomType::Entry)
		{
			Node.RoomExtents = FVector(640.0f, 640.0f, 400.0f);
		}
		else if (Type == EDungeonRoomType::MirrorPalaceMaze)
		{
			Node.RoomExtents = FVector(1280.0f, 1152.0f, 450.0f);
		}
		else if (Shape == EDungeonRoomShape::CrossAmbushVault)
		{
			Node.RoomExtents = FVector(1152.0f, 1024.0f, 400.0f);
		}
		else
		{
			Node.RoomExtents = FVector(1024.0f, 1024.0f, 400.0f);
		}
		Node.RoomSeed = Stream.RandHelper(INT32_MAX);
		return Node;
	};

	const EDungeonBiomeType DomBiome = Graph.DominantBiome;
	const EDungeonBiomeType NextBiome = Graph.NextFloorPreviewBiome;

	// 10 core tactical rooms + 2 secret Phantasm pockets hidden behind illusory mirrors:
	// Layer 0: Entry Chamber
	FDungeonNode EntryNode = MakeNode(0, 0, 0.0f, EDungeonRoomType::Entry, EDungeonRoomShape::SquareChamber, DomBiome, DomBiome);

	// Layer 1: Dual Combat Wings (Pillared Gallery South, Octagonal Rotunda North)
	FDungeonNode CombatA = MakeNode(1, 1, -0.68f, EDungeonRoomType::Combat, EDungeonRoomShape::PillaredGallery, DomBiome, DomBiome);
	FDungeonNode CombatB = MakeNode(2, 1,  0.68f, EDungeonRoomType::Combat, EDungeonRoomShape::OctagonalRotunda, DomBiome, DomBiome);
	EntryNode.OutgoingNodeIds.Add(1);
	EntryNode.OutgoingNodeIds.Add(2);

	// Layer 2: Central Ambush Convergence Hall
	FDungeonNode ConvergenceHall = MakeNode(3, 2, 0.0f, EDungeonRoomType::Combat, EDungeonRoomShape::CrossAmbushVault, DomBiome, DomBiome);
	CombatA.OutgoingNodeIds.Add(3);
	CombatB.OutgoingNodeIds.Add(3);

	// Layer 3: Obsidian Mirror-Palace Puzzle Branch (South) & Sanctuary Resting Ground (North)
	FDungeonNode MirrorNode = MakeNode(4, 3, -0.72f, EDungeonRoomType::MirrorPalaceMaze, EDungeonRoomShape::OctagonalRotunda, EDungeonBiomeType::MirrorPalace, DomBiome);
	FDungeonNode SanctuaryNode = MakeNode(5, 3, 0.72f, EDungeonRoomType::RestingGround, EDungeonRoomShape::OctagonalRotunda, DomBiome, DomBiome);
	ConvergenceHall.OutgoingNodeIds.Add(4);
	ConvergenceHall.OutgoingNodeIds.Add(5);

	// Layer 4: Merchant Alcove + Mid-Floor Hazard Wing (begins Transition Bleed)
	const bool bVendorLower = Stream.RandRange(0, 1) == 0;
	FDungeonNode Layer4A = MakeNode(
		6, 4, -0.58f,
		bVendorLower ? EDungeonRoomType::Vendor : EDungeonRoomType::Combat,
		bVendorLower ? EDungeonRoomShape::SquareChamber : EDungeonRoomShape::PillaredGallery,
		DomBiome, NextBiome);
	FDungeonNode Layer4B = MakeNode(
		7, 4,  0.58f,
		bVendorLower ? EDungeonRoomType::Combat : EDungeonRoomType::Vendor,
		bVendorLower ? EDungeonRoomShape::PillaredGallery : EDungeonRoomShape::SquareChamber,
		DomBiome, NextBiome);
	MirrorNode.OutgoingNodeIds.Add(6);
	MirrorNode.OutgoingNodeIds.Add(7);
	SanctuaryNode.OutgoingNodeIds.Add(6);
	SanctuaryNode.OutgoingNodeIds.Add(7);

	// Layer 5 & 6: Final 2 Transition Bleed Nodes -> Deep Fog Gauntlet -> Terminal Major Loot Vault + Descending Stairs
	FDungeonNode GauntletNode = MakeNode(8, 5, 0.0f, EDungeonRoomType::DeepFogGauntlet, EDungeonRoomShape::CrossAmbushVault, DomBiome, NextBiome);
	Layer4A.OutgoingNodeIds.Add(8);
	Layer4B.OutgoingNodeIds.Add(8);

	FDungeonNode VaultNode = MakeNode(9, 6, 0.0f, EDungeonRoomType::MajorLootVault, EDungeonRoomShape::OctagonalRotunda, DomBiome, NextBiome);
	GauntletNode.OutgoingNodeIds.Add(9);

	// Secret Phantasm Pockets (flush against MirrorNode South wall and CombatB North wall behind AMirrorWall_Phantasm)
	FDungeonNode SecretMirrorPocket = MakeNode(10, 3, -1.36f, EDungeonRoomType::MajorLootVault, EDungeonRoomShape::SecretPhantasmPocket, EDungeonBiomeType::MirrorPalace, DomBiome);
	SecretMirrorPocket.RoomExtents = FVector(512.0f, 512.0f, 400.0f);
	SecretMirrorPocket.WorldCenter = MirrorNode.WorldCenter + FVector(0.0f, -(MirrorNode.RoomExtents.Y + 512.0f), 0.0f);
	SecretMirrorPocket.ParentRoomNodeId = MirrorNode.NodeId;
	SecretMirrorPocket.bHasDescendingStairs = false;

	FDungeonNode SecretCryptPocket = MakeNode(11, 1, 1.36f, EDungeonRoomType::DeepFogGauntlet, EDungeonRoomShape::SecretPhantasmPocket, NextBiome, DomBiome);
	SecretCryptPocket.RoomExtents = FVector(512.0f, 512.0f, 400.0f);
	SecretCryptPocket.WorldCenter = CombatB.WorldCenter + FVector(0.0f, (CombatB.RoomExtents.Y + 512.0f), 0.0f);
	SecretCryptPocket.ParentRoomNodeId = CombatB.NodeId;
	SecretCryptPocket.bHasDescendingStairs = false;

	Graph.Nodes.Reserve(12);
	Graph.Nodes.Add(MoveTemp(EntryNode));
	Graph.Nodes.Add(MoveTemp(CombatA));
	Graph.Nodes.Add(MoveTemp(CombatB));
	Graph.Nodes.Add(MoveTemp(ConvergenceHall));
	Graph.Nodes.Add(MoveTemp(MirrorNode));
	Graph.Nodes.Add(MoveTemp(SanctuaryNode));
	Graph.Nodes.Add(MoveTemp(Layer4A));
	Graph.Nodes.Add(MoveTemp(Layer4B));
	Graph.Nodes.Add(MoveTemp(GauntletNode));
	Graph.Nodes.Add(MoveTemp(VaultNode));
	Graph.Nodes.Add(MoveTemp(SecretMirrorPocket));
	Graph.Nodes.Add(MoveTemp(SecretCryptPocket));

	ActiveFloorGraph = Graph;
	return ActiveFloorGraph;
}

void UDungeonGeneratorSubsystem::GenerateMirrorMazeGrid(int32 RoomSeed, int32 GridWidth, int32 GridHeight, TArray<EMirrorCellType>& OutCells) const
{
	const int32 W = FMath::Max(5, GridWidth);
	const int32 H = FMath::Max(5, GridHeight);
	OutCells.SetNumUninitialized(W * H);

	FRandomStream Stream(RoomSeed);
	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			const int32 Idx = Y * W + X;
			if (X == 0 || Y == 0 || X == W - 1 || Y == H - 1)
			{
				OutCells[Idx] = EMirrorCellType::SolidMirror;
				continue;
			}

			// Carve guaranteed pass-through spine using PhantasmWalls and OpenFloor
			if (X == W / 2 || Y == H / 2)
			{
				OutCells[Idx] = (Stream.FRand() < 0.28f) ? EMirrorCellType::PhantasmWall : EMirrorCellType::OpenFloor;
				continue;
			}

			const float Roll = Stream.FRand();
			if (Roll < 0.25f)
			{
				OutCells[Idx] = EMirrorCellType::SolidMirror;
			}
			else if (Roll < 0.45f)
			{
				OutCells[Idx] = EMirrorCellType::PhantasmWall;
			}
			else if (Roll < 0.55f)
			{
				OutCells[Idx] = EMirrorCellType::DecoyAnchor;
			}
			else
			{
				OutCells[Idx] = EMirrorCellType::OpenFloor;
			}
		}
	}

	// Keep center entry and exit cells clear
	OutCells[(H / 2) * W + 1] = EMirrorCellType::OpenFloor;
	OutCells[(H / 2) * W + (W - 2)] = EMirrorCellType::OpenFloor;
}

void UDungeonGeneratorSubsystem::RegisterRestingGroundBounds(const FBox& Bounds)
{
	RestingGroundBounds.Add(Bounds);
}

bool UDungeonGeneratorSubsystem::IsLocationInRestingGround(const FVector& WorldLocation) const noexcept
{
	for (const FBox& Box : RestingGroundBounds)
	{
		if (Box.IsInsideOrOn(WorldLocation))
		{
			return true;
		}
	}
	return false;
}
