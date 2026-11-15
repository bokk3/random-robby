#include "Dungeon/DungeonGeneratorSubsystem.h"

void UDungeonGeneratorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RestingGroundBounds.Reset();
}

FDungeonFloorGraph UDungeonGeneratorSubsystem::GenerateFloorGraph(int32 FloorIndex, int32 ServerSeed)
{
	RestingGroundBounds.Reset();
	FDungeonFloorGraph Graph;
	Graph.FloorIndex = FloorIndex;
	Graph.MasterSeed = ServerSeed ^ (FloorIndex * 7919);

	FRandomStream Stream(Graph.MasterSeed);
	constexpr float RoomPitchX = 2800.0f;
	constexpr float RoomPitchY = 2600.0f;

	auto MakeNode = [&Stream, RoomPitchX, RoomPitchY](int32 Id, int32 Depth, float LateralOffset, EDungeonRoomType Type) -> FDungeonNode
	{
		FDungeonNode Node;
		Node.NodeId = Id;
		Node.DepthLayer = Depth;
		Node.RoomType = Type;
		Node.WorldCenter = FVector(static_cast<float>(Depth) * RoomPitchX, LateralOffset * RoomPitchY, 0.0f);
		Node.RoomExtents = (Type == EDungeonRoomType::MirrorPalaceMaze)
			? FVector(1600.0f, 1600.0f, 450.0f)
			: FVector(1200.0f, 1200.0f, 400.0f);
		Node.RoomSeed = Stream.RandHelper(INT32_MAX);
		return Node;
	};

	// Layer 0: Entry
	FDungeonNode EntryNode = MakeNode(0, 0, 0.0f, EDungeonRoomType::Entry);

	// Layer 1: 2 Combat / Exploration Branches
	FDungeonNode CombatA = MakeNode(1, 1, -0.6f, EDungeonRoomType::Combat);
	FDungeonNode CombatB = MakeNode(2, 1,  0.6f, EDungeonRoomType::Combat);
	EntryNode.OutgoingNodeIds.Add(1);
	EntryNode.OutgoingNodeIds.Add(2);

	// Layer 2: Mirror-Palace Illusion Maze (convergence bottleneck)
	FDungeonNode MirrorNode = MakeNode(3, 2, 0.0f, EDungeonRoomType::MirrorPalaceMaze);
	CombatA.OutgoingNodeIds.Add(3);
	CombatB.OutgoingNodeIds.Add(3);

	// Layer 3: Mid-Floor Branch (Vendor OR Resting Ground)
	const bool bVendorTop = Stream.RandRange(0, 1) == 0;
	FDungeonNode MidA = MakeNode(4, 3, -0.65f, bVendorTop ? EDungeonRoomType::Vendor : EDungeonRoomType::RestingGround);
	FDungeonNode MidB = MakeNode(5, 3,  0.65f, bVendorTop ? EDungeonRoomType::RestingGround : EDungeonRoomType::Vendor);
	MirrorNode.OutgoingNodeIds.Add(4);
	MirrorNode.OutgoingNodeIds.Add(5);

	// Layer 4: Deep Fog Gauntlet
	FDungeonNode GauntletNode = MakeNode(6, 4, 0.0f, EDungeonRoomType::DeepFogGauntlet);
	MidA.OutgoingNodeIds.Add(6);
	MidB.OutgoingNodeIds.Add(6);

	// Layer 5: Terminal High-Tier Major Loot Vault
	FDungeonNode VaultNode = MakeNode(7, 5, 0.0f, EDungeonRoomType::MajorLootVault);
	GauntletNode.OutgoingNodeIds.Add(7);

	Graph.Nodes.Reserve(8);
	Graph.Nodes.Add(MoveTemp(EntryNode));
	Graph.Nodes.Add(MoveTemp(CombatA));
	Graph.Nodes.Add(MoveTemp(CombatB));
	Graph.Nodes.Add(MoveTemp(MirrorNode));
	Graph.Nodes.Add(MoveTemp(MidA));
	Graph.Nodes.Add(MoveTemp(MidB));
	Graph.Nodes.Add(MoveTemp(GauntletNode));
	Graph.Nodes.Add(MoveTemp(VaultNode));

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
