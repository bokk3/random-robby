#include "Dungeon/DungeoneerTileMapInstancer.h"
#include "Dungeon/DungeonGeneratorSubsystem.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

namespace TileMapDetail
{
	/** Slab-method segment vs expanded AABB intersection test with zero heap allocation. */
	inline bool SegmentIntersectsExpandedBox(
		const FVector& SegStart,
		const FVector& SegEnd,
		const FVector& BoxCenter,
		const FVector& ExpandedHalfExtents) noexcept
	{
		const FVector MinB = BoxCenter - ExpandedHalfExtents;
		const FVector MaxB = BoxCenter + ExpandedHalfExtents;
		const FVector Delta = SegEnd - SegStart;

		double TMin = 0.0;
		double TMax = 1.0;

		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const double D = Delta[Axis];
			const double S = SegStart[Axis];
			if (FMath::Abs(D) < 1e-6)
			{
				if (S < MinB[Axis] || S > MaxB[Axis])
				{
					return false;
				}
			}
			else
			{
				const double InvD = 1.0 / D;
				double T1 = (MinB[Axis] - S) * InvD;
				double T2 = (MaxB[Axis] - S) * InvD;
				if (T1 > T2)
				{
					Swap(T1, T2);
				}
				TMin = FMath::Max(TMin, T1);
				TMax = FMath::Min(TMax, T2);
				if (TMin > TMax)
				{
					return false;
				}
			}
		}

		return true;
	}

	inline float WallAxisToYawDeg(const EDungeoneerWallAxis Axis) noexcept
	{
		switch (Axis)
		{
		case EDungeoneerWallAxis::North:     return 0.0f;
		case EDungeoneerWallAxis::South:     return 180.0f;
		case EDungeoneerWallAxis::East:      return 90.0f;
		case EDungeoneerWallAxis::West:      return -90.0f;
		case EDungeoneerWallAxis::NorthEast: return 45.0f;
		case EDungeoneerWallAxis::NorthWest: return -45.0f;
		case EDungeoneerWallAxis::SouthEast: return 135.0f;
		case EDungeoneerWallAxis::SouthWest: return -135.0f;
		default:                             return 0.0f;
		}
	}
}

ADungeoneerTileMapInstancer::ADungeoneerTileMapInstancer()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	FloorHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("FloorHISM"));
	FloorHISM->SetupAttachment(SceneRoot);
	FloorHISM->NumCustomDataFloats = 4;
	FloorHISM->SetCastShadow(false);
	FloorHISM->bCastHiddenShadow = false;
	FloorHISM->SetCollisionProfileName(TEXT("BlockAll"));

	WallHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("WallHISM"));
	WallHISM->SetupAttachment(SceneRoot);
	WallHISM->NumCustomDataFloats = 4;
	WallHISM->SetCastShadow(true);
	WallHISM->bCastHiddenShadow = true;
	WallHISM->SetCollisionProfileName(TEXT("BlockAll"));
	WallHISM->ComponentTags.Add(TEXT("DungeonWall"));

	MirrorHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("MirrorHISM"));
	MirrorHISM->SetupAttachment(SceneRoot);
	MirrorHISM->NumCustomDataFloats = 4;
	MirrorHISM->SetCastShadow(true);
	MirrorHISM->bCastHiddenShadow = true;
	MirrorHISM->SetCollisionProfileName(TEXT("BlockAll"));
	MirrorHISM->ComponentTags.Add(TEXT("MirrorReflective"));

	BreakableHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("BreakableHISM"));
	BreakableHISM->SetupAttachment(SceneRoot);
	BreakableHISM->NumCustomDataFloats = 4;
	BreakableHISM->SetCastShadow(true);
	BreakableHISM->bCastHiddenShadow = true;
	BreakableHISM->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	BreakableHISM->ComponentTags.Add(TEXT("BreakableNoiseProp"));

	ScratchSweepHits.Reserve(32);
}

void ADungeoneerTileMapInstancer::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EnsureMeshAssetsInitialized();
}

void ADungeoneerTileMapInstancer::BeginPlay()
{
	Super::BeginPlay();
	EnsureMeshAssetsInitialized();

	if (WallEntries.Num() == 0)
	{
		BuildFromActiveGenerator();
	}
}

void ADungeoneerTileMapInstancer::EnsureMeshAssetsInitialized()
{
	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UStaticMesh* BoxFallback = CubeMesh ? CubeMesh : PlaneMesh;

	if (FloorHISM)
	{
		FloorHISM->NumCustomDataFloats = 4;
		if (!FloorHISM->GetStaticMesh() && (PlaneMesh || BoxFallback))
		{
			FloorHISM->SetStaticMesh(PlaneMesh ? PlaneMesh : BoxFallback);
		}
	}

	auto ConfigureBoxHISM = [BoxFallback](UHierarchicalInstancedStaticMeshComponent* Comp, const bool bShadow)
	{
		if (Comp)
		{
			Comp->NumCustomDataFloats = 4;
			Comp->bCastHiddenShadow = bShadow;
			if (!Comp->GetStaticMesh() && BoxFallback)
			{
				Comp->SetStaticMesh(BoxFallback);
			}
		}
	};

	ConfigureBoxHISM(WallHISM.Get(), true);
	ConfigureBoxHISM(MirrorHISM.Get(), true);
	ConfigureBoxHISM(BreakableHISM.Get(), true);
}

void ADungeoneerTileMapInstancer::ClearAllInstances()
{
	if (FloorHISM)     { FloorHISM->ClearInstances(); }
	if (WallHISM)      { WallHISM->ClearInstances(); }
	if (MirrorHISM)    { MirrorHISM->ClearInstances(); }
	if (BreakableHISM) { BreakableHISM->ClearInstances(); }

	WallEntries.Reset();
	MirrorEntries.Reset();
	BreakableEntries.Reset();
	PendingCutawayFlags.Reset();
}

void ADungeoneerTileMapInstancer::AddWallInstance(
	const FVector& WorldCenter,
	const EDungeoneerWallAxis Axis,
	const FVector& HalfExtents,
	const int32 FullUVCell,
	const int32 CutawayUVCell)
{
	if (!WallHISM)
	{
		return;
	}

	const float YawDeg = TileMapDetail::WallAxisToYawDeg(Axis);
	const FVector MeshScale = FVector(
		FMath::Max(0.2f, HalfExtents.X * 2.0f / 100.0f),
		FMath::Max(0.2f, HalfExtents.Y * 2.0f / 100.0f),
		FMath::Max(0.2f, HalfExtents.Z * 2.0f / 100.0f));
	const FTransform InstanceTransform(FRotator(0.0f, YawDeg, 0.0f), WorldCenter, MeshScale);

	const int32 InstIdx = WallHISM->AddInstance(InstanceTransform, true);
	if (InstIdx != INDEX_NONE)
	{
		WallHISM->SetCustomDataValue(InstIdx, 0, 0.0f, false); // 0 = Full standing wall, 1 = Row 2 cutaway stub
		WallHISM->SetCustomDataValue(InstIdx, 1, static_cast<float>(FullUVCell), false);
		WallHISM->SetCustomDataValue(InstIdx, 2, static_cast<float>(CutawayUVCell), false);
		WallHISM->SetCustomDataValue(InstIdx, 3, static_cast<float>(static_cast<uint8>(Axis)), false);

		FDungeoneerWallInstanceEntry Entry;
		Entry.HISMInstanceIndex = InstIdx;
		Entry.WallAxis = Axis;
		Entry.WorldCenter = WorldCenter;
		Entry.WorldHalfExtents = HalfExtents;
		Entry.FullUVCellIndex = FullUVCell;
		Entry.CutawayUVCellIndex = CutawayUVCell;
		Entry.bCanCutawayForCamera =
			(Axis == EDungeoneerWallAxis::South ||
			 Axis == EDungeoneerWallAxis::SouthEast ||
			 Axis == EDungeoneerWallAxis::SouthWest);
		Entry.bCurrentlyCutaway = false;

		WallEntries.Add(Entry);
		PendingCutawayFlags.Add(0);
	}
}

int32 ADungeoneerTileMapInstancer::RegisterMirrorInstance(
	const FVector& WorldCenter,
	const FVector& SurfaceNormal,
	const float HalfWidthUU,
	const float HalfHeightUU,
	const bool bIsIllusoryPhantasm,
	const int32 UVCellIndex)
{
	if (!MirrorHISM)
	{
		return INDEX_NONE;
	}

	EnsureMeshAssetsInitialized();

	const FVector CleanNormal = SurfaceNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector(0.0f, 1.0f, 0.0f));
	const FVector Tangent = FVector::CrossProduct(FVector::UpVector, CleanNormal).GetSafeNormal(UE_SMALL_NUMBER, FVector(1.0f, 0.0f, 0.0f));
	const FRotator FacingRot = CleanNormal.Rotation();
	const FVector Scale(0.24f, FMath::Max(0.4f, HalfWidthUU * 2.0f / 100.0f), FMath::Max(0.4f, HalfHeightUU * 2.0f / 100.0f));

	const int32 InstIdx = MirrorHISM->AddInstance(FTransform(FacingRot, WorldCenter, Scale), true);
	if (InstIdx != INDEX_NONE)
	{
		MirrorHISM->SetCustomDataValue(InstIdx, 0, 0.0f, true); // Illusion seam reveal flag
		MirrorHISM->SetCustomDataValue(InstIdx, 1, static_cast<float>(UVCellIndex), true);
		MirrorHISM->SetCustomDataValue(InstIdx, 2, bIsIllusoryPhantasm ? 1.0f : 0.0f, true);
		MirrorHISM->SetCustomDataValue(InstIdx, 3, static_cast<float>(MirrorEntries.Num()), true);

		FDungeoneerMirrorInstanceEntry Entry;
		Entry.HISMInstanceIndex = InstIdx;
		Entry.WorldCenter = WorldCenter;
		Entry.SurfaceNormal = CleanNormal;
		Entry.MirrorTangent = Tangent;
		Entry.HalfWidthUU = HalfWidthUU;
		Entry.HalfHeightUU = HalfHeightUU;
		Entry.bIsIllusoryPhantasm = bIsIllusoryPhantasm;
		Entry.bIllusionSeamRevealed = false;
		return MirrorEntries.Add(Entry);
	}

	return INDEX_NONE;
}

int32 ADungeoneerTileMapInstancer::RegisterBreakableInstance(
	const FVector& WorldCenter,
	const int32 IntactUVCellIndex,
	const int32 ShatteredUVCellIndex,
	const float TriggerRadiusUU,
	const float ImpactLoudness)
{
	if (!BreakableHISM)
	{
		return INDEX_NONE;
	}

	EnsureMeshAssetsInitialized();

	const FTransform InstanceTransform(FRotator::ZeroRotator, WorldCenter, FVector(0.95f, 0.95f, 1.1f));
	const int32 InstIdx = BreakableHISM->AddInstance(InstanceTransform, true);
	if (InstIdx != INDEX_NONE)
	{
		BreakableHISM->SetCustomDataValue(InstIdx, 0, 0.0f, true); // 0 = intact, 1 = shattered
		BreakableHISM->SetCustomDataValue(InstIdx, 1, static_cast<float>(IntactUVCellIndex), true);
		BreakableHISM->SetCustomDataValue(InstIdx, 2, static_cast<float>(ShatteredUVCellIndex), true);
		BreakableHISM->SetCustomDataValue(InstIdx, 3, ImpactLoudness, true);

		FDungeoneerBreakableInstanceEntry Entry;
		Entry.HISMInstanceIndex = InstIdx;
		Entry.WorldCenter = WorldCenter;
		Entry.IntactUVCellIndex = IntactUVCellIndex;
		Entry.ShatteredUVCellIndex = ShatteredUVCellIndex;
		Entry.TriggerRadiusUU = TriggerRadiusUU;
		Entry.ImpactLoudness = ImpactLoudness;
		Entry.bIsShattered = false;
		return BreakableEntries.Add(Entry);
	}

	return INDEX_NONE;
}

void ADungeoneerTileMapInstancer::BuildDungeonFromFloorGraph(const FDungeonFloorGraph& FloorGraph)
{
	EnsureMeshAssetsInitialized();
	ClearAllInstances();

	const UWorld* World = GetWorld();
	const UDungeonGeneratorSubsystem* GenSub = World ? World->GetSubsystem<UDungeonGeneratorSubsystem>() : nullptr;

	// Pre-reserve metadata storage cleanly outside Tick
	const int32 EstimatedRooms = FMath::Max(1, FloorGraph.Nodes.Num());
	WallEntries.Reserve(EstimatedRooms * 24);
	PendingCutawayFlags.Reserve(EstimatedRooms * 24);
	MirrorEntries.Reserve(EstimatedRooms * 8);
	BreakableEntries.Reserve(EstimatedRooms * 4);

	for (const FDungeonNode& Node : FloorGraph.Nodes)
	{
		FRandomStream RoomRand(Node.RoomSeed);
		const float HalfX = FMath::Max(TileSizeUU * 2.0f, Node.RoomExtents.X);
		const float HalfY = FMath::Max(TileSizeUU * 2.0f, Node.RoomExtents.Y);
		const int32 NumTilesX = FMath::Max(3, FMath::RoundToInt((HalfX * 2.0f) / TileSizeUU));
		const int32 NumTilesY = FMath::Max(3, FMath::RoundToInt((HalfY * 2.0f) / TileSizeUU));
		const FVector RoomOrigin = Node.WorldCenter;

		// 1. Floor Grid (ATLAS_Dungeon_Floors_4x4)
		for (int32 iy = 0; iy < NumTilesY; ++iy)
		{
			for (int32 ix = 0; ix < NumTilesX; ++ix)
			{
				const float LocalX = (static_cast<float>(ix) - 0.5f * static_cast<float>(NumTilesX - 1)) * TileSizeUU;
				const float LocalY = (static_cast<float>(iy) - 0.5f * static_cast<float>(NumTilesY - 1)) * TileSizeUU;
				const FVector TileCenter = RoomOrigin + FVector(LocalX, LocalY, 0.0f);
				const FVector FloorScale(TileSizeUU / 100.0f, TileSizeUU / 100.0f, 1.0f);

				if (FloorHISM)
				{
					const int32 FloorIdx = FloorHISM->AddInstance(FTransform(FRotator::ZeroRotator, TileCenter, FloorScale), true);
					if (FloorIdx != INDEX_NONE)
					{
						int32 FloorUV = RoomRand.RandRange(0, 3); // Dry crypt slate default
						if (Node.RoomType == EDungeonRoomType::MirrorPalaceMaze)
						{
							FloorUV = 15; // Shattered glass / mirror transition row
						}
						else if (Node.RoomType == EDungeonRoomType::DeepFogGauntlet || ((ix + iy) % 5 == 0))
						{
							FloorUV = 4 + RoomRand.RandRange(0, 3); // Wet puddle tiles (0.05 roughness)
						}
						FloorHISM->SetCustomDataValue(FloorIdx, 0, static_cast<float>(Node.NodeId), false);
						FloorHISM->SetCustomDataValue(FloorIdx, 1, static_cast<float>(FloorUV), false);
					}
				}
			}
		}

		// 2. 4-Axis Walls: Orthogonal N/S/E/W + 45-degree Diagonal NE/NW/SE/SW Corner Wedges
		const float EdgeOffsetX = (static_cast<float>(NumTilesX) * 0.5f) * TileSizeUU;
		const float EdgeOffsetY = (static_cast<float>(NumTilesY) * 0.5f) * TileSizeUU;
		const float WallZ = RoomOrigin.Z + WallHeightUU * 0.5f;
		const float SegSpan = TileSizeUU * 0.45f;
		const FVector NSExtents(SegSpan, 34.0f, WallHeightUU * 0.5f);
		const FVector EWExtents(34.0f, SegSpan, WallHeightUU * 0.5f);

		for (int32 ix = 1; ix < NumTilesX - 1; ++ix)
		{
			const float WX = RoomOrigin.X + (static_cast<float>(ix) - 0.5f * static_cast<float>(NumTilesX - 1)) * TileSizeUU;
			// North back wall (+Y)
			AddWallInstance(FVector(WX, RoomOrigin.Y + EdgeOffsetY, WallZ), EDungeoneerWallAxis::North, NSExtents, ix % 4, 4 + (ix % 4));
			// South foreground occluder wall (-Y) -> cutaway stub Row 1 (cells 4..7)
			AddWallInstance(FVector(WX, RoomOrigin.Y - EdgeOffsetY, WallZ), EDungeoneerWallAxis::South, NSExtents, ix % 4, 4 + (ix % 4));
		}

		for (int32 iy = 1; iy < NumTilesY - 1; ++iy)
		{
			const float WY = RoomOrigin.Y + (static_cast<float>(iy) - 0.5f * static_cast<float>(NumTilesY - 1)) * TileSizeUU;
			// East (+X) and West (-X) walls
			AddWallInstance(FVector(RoomOrigin.X + EdgeOffsetX, WY, WallZ), EDungeoneerWallAxis::East, EWExtents, iy % 4, 4 + (iy % 4));
			AddWallInstance(FVector(RoomOrigin.X - EdgeOffsetX, WY, WallZ), EDungeoneerWallAxis::West, EWExtents, iy % 4, 4 + (iy % 4));
		}

		// 45-degree Diagonal Corner Chamfers (NE, NW, SE, SW) supporting 8-compass navigation without stair-stepping
		const float DiagOffsetX = EdgeOffsetX - TileSizeUU * 0.25f;
		const float DiagOffsetY = EdgeOffsetY - TileSizeUU * 0.25f;
		const FVector DiagExtents(SegSpan, 34.0f, WallHeightUU * 0.5f);
		AddWallInstance(FVector(RoomOrigin.X + DiagOffsetX, RoomOrigin.Y + DiagOffsetY, WallZ), EDungeoneerWallAxis::NorthEast, DiagExtents, 8, 4);
		AddWallInstance(FVector(RoomOrigin.X - DiagOffsetX, RoomOrigin.Y + DiagOffsetY, WallZ), EDungeoneerWallAxis::NorthWest, DiagExtents, 9, 5);
		AddWallInstance(FVector(RoomOrigin.X + DiagOffsetX, RoomOrigin.Y - DiagOffsetY, WallZ), EDungeoneerWallAxis::SouthEast, DiagExtents, 10, 6);
		AddWallInstance(FVector(RoomOrigin.X - DiagOffsetX, RoomOrigin.Y - DiagOffsetY, WallZ), EDungeoneerWallAxis::SouthWest, DiagExtents, 11, 7);

		// 3. Mirror-Palace Maze Panels & 45-deg Bounce Prisms (`MirrorHISM`)
		if (Node.RoomType == EDungeonRoomType::MirrorPalaceMaze || Node.NodeId % 3 == 1)
		{
			if (GenSub && Node.RoomType == EDungeonRoomType::MirrorPalaceMaze)
			{
				TArray<EMirrorCellType> MazeCells;
				GenSub->GenerateMirrorMazeGrid(Node.RoomSeed, 4, 4, MazeCells);
				for (int32 idx = 0; idx < MazeCells.Num(); ++idx)
				{
					const EMirrorCellType CellType = MazeCells[idx];
					if (CellType == EMirrorCellType::SolidMirror || CellType == EMirrorCellType::PhantasmWall)
					{
						const int32 gx = idx % 4;
						const int32 gy = idx / 4;
						const FVector MPos = RoomOrigin + FVector((gx - 1.5f) * TileSizeUU * 0.85f, (gy - 1.5f) * TileSizeUU * 0.85f, WallHeightUU * 0.42f);
						const FVector MNorm = ((gx + gy) % 2 == 0) ? FVector(0.0f, -1.0f, 0.0f) : FVector(-0.70710678f, -0.70710678f, 0.0f);
						const bool bPhantasm = (CellType == EMirrorCellType::PhantasmWall);
						RegisterMirrorInstance(MPos, MNorm, 96.0f, 115.0f, bPhantasm, bPhantasm ? 4 : (idx % 4));
					}
				}
			}
			else
			{
				// Corridor reflective mirror & 45-degree prism
				RegisterMirrorInstance(
					RoomOrigin + FVector(0.0f, EdgeOffsetY * 0.55f, WallHeightUU * 0.42f),
					FVector(0.0f, -1.0f, 0.0f),
					96.0f,
					115.0f,
					false,
					0);
			}
		}

		// 4. Deterministic Breakable Barrels/Urns (`PROP_Breakables_And_Traps_4x4`)
		const FVector BreakOffset(RoomRand.FRandRange(-160.0f, 160.0f), RoomRand.FRandRange(-140.0f, 140.0f), 45.0f);
		const int32 IntactCell = RoomRand.RandRange(0, 1); // Cell 0 = Barrel, Cell 1 = Urn
		RegisterBreakableInstance(RoomOrigin + BreakOffset, IntactCell, IntactCell + 2, 85.0f, 1.35f);
	}

	if (FloorHISM)     { FloorHISM->MarkRenderStateDirty(); }
	if (WallHISM)      { WallHISM->MarkRenderStateDirty(); }
	if (MirrorHISM)    { MirrorHISM->MarkRenderStateDirty(); }
	if (BreakableHISM) { BreakableHISM->MarkRenderStateDirty(); }
}

void ADungeoneerTileMapInstancer::BuildFromActiveGenerator()
{
	if (const UWorld* World = GetWorld())
	{
		if (const UDungeonGeneratorSubsystem* GenSub = World->GetSubsystem<UDungeonGeneratorSubsystem>())
		{
			const FDungeonFloorGraph& Graph = GenSub->GetActiveFloorGraph();
			if (Graph.Nodes.Num() > 0)
			{
				BuildDungeonFromFloorGraph(Graph);
			}
		}
	}
}

void ADungeoneerTileMapInstancer::ResetPendingCutaways() noexcept
{
	if (PendingCutawayFlags.Num() > 0)
	{
		FMemory::Memzero(PendingCutawayFlags.GetData(), PendingCutawayFlags.Num() * sizeof(uint8));
	}
}

void ADungeoneerTileMapInstancer::AccumulateCutawayAlongSegment(
	const FVector& CameraLocation,
	const FVector& PlayerLocation) noexcept
{
	const UWorld* World = GetWorld();
	if (World && WallHISM)
	{
		ScratchSweepHits.Reset();
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(TileMapSouthWallCutaway), false);
		QueryParams.bTraceComplex = false;

		World->SweepMultiByChannel(
			ScratchSweepHits,
			CameraLocation,
			PlayerLocation,
			FQuat::Identity,
			ECC_Visibility,
			FCollisionShape::MakeSphere(CutawaySphereRadiusUU),
			QueryParams);

		for (const FHitResult& Hit : ScratchSweepHits)
		{
			if (Hit.GetComponent() == WallHISM.Get() && Hit.Item != INDEX_NONE)
			{
				for (int32 Idx = 0; Idx < WallEntries.Num(); ++Idx)
				{
					if (WallEntries[Idx].HISMInstanceIndex == Hit.Item && WallEntries[Idx].bCanCutawayForCamera)
					{
						PendingCutawayFlags[Idx] = 1;
						break;
					}
				}
			}
		}
	}

	// Deterministic segment-vs-box check guarantees zero missed occluders even without Chaos rebuilds
	const FVector Pad(CutawaySphereRadiusUU, CutawaySphereRadiusUU, CutawaySphereRadiusUU * 0.5f);
	for (int32 Idx = 0; Idx < WallEntries.Num(); ++Idx)
	{
		if (PendingCutawayFlags[Idx] != 0)
		{
			continue;
		}

		const FDungeoneerWallInstanceEntry& Entry = WallEntries[Idx];
		if (!Entry.bCanCutawayForCamera)
		{
			continue;
		}

		if (TileMapDetail::SegmentIntersectsExpandedBox(
			CameraLocation,
			PlayerLocation,
			Entry.WorldCenter,
			Entry.WorldHalfExtents + Pad))
		{
			PendingCutawayFlags[Idx] = 1;
		}
	}
}

void ADungeoneerTileMapInstancer::ApplyPendingCutawayTransitions() noexcept
{
	if (!WallHISM)
	{
		return;
	}

	bool bAnyDirty = false;
	for (int32 Idx = 0; Idx < WallEntries.Num(); ++Idx)
	{
		FDungeoneerWallInstanceEntry& Entry = WallEntries[Idx];
		const bool bShouldCutaway = (PendingCutawayFlags[Idx] != 0);
		if (bShouldCutaway != Entry.bCurrentlyCutaway)
		{
			Entry.bCurrentlyCutaway = bShouldCutaway;
			WallHISM->SetCustomDataValue(Entry.HISMInstanceIndex, 0, bShouldCutaway ? 1.0f : 0.0f, false);
			WallHISM->SetCustomDataValue(
				Entry.HISMInstanceIndex,
				1,
				static_cast<float>(bShouldCutaway ? Entry.CutawayUVCellIndex : Entry.FullUVCellIndex),
				false);
			bAnyDirty = true;
		}
	}

	if (bAnyDirty)
	{
		// Keep bCastHiddenShadow enabled so cutaway walls still project ceiling/doorway silhouettes
		WallHISM->bCastHiddenShadow = true;
		WallHISM->MarkRenderStateDirty();
	}
}

void ADungeoneerTileMapInstancer::UpdateSouthWallCutawaysForViewports() noexcept
{
	const UWorld* World = GetWorld();
	const UGameInstance* GameInst = World ? World->GetGameInstance() : nullptr;
	if (!World || !GameInst || WallEntries.Num() == 0)
	{
		return;
	}

	ResetPendingCutaways();

	const TArray<ULocalPlayer*>& LocalPlayers = GameInst->GetLocalPlayers();
	for (ULocalPlayer* LP : LocalPlayers)
	{
		const APlayerController* PC = LP ? LP->GetPlayerController(World) : nullptr;
		const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!PC || !Pawn)
		{
			continue;
		}

		FVector ViewLoc = FVector::ZeroVector;
		FRotator ViewRot = FRotator::ZeroRotator;
		PC->GetPlayerViewPoint(ViewLoc, ViewRot);
		const FVector PawnLoc = Pawn->GetActorLocation();

		AccumulateCutawayAlongSegment(ViewLoc, PawnLoc);
		EvaluateEntityStepOnBreakables(PawnLoc, -1.0f, -1.0f);
	}

	ApplyPendingCutawayTransitions();
}

int32 ADungeoneerTileMapInstancer::EvaluateCameraToPlayerSegmentCutaway(
	const FVector& CameraLocation,
	const FVector& PlayerLocation)
{
	ResetPendingCutaways();
	AccumulateCutawayAlongSegment(CameraLocation, PlayerLocation);
	ApplyPendingCutawayTransitions();

	int32 ActiveCutaways = 0;
	for (const FDungeoneerWallInstanceEntry& Entry : WallEntries)
	{
		if (Entry.bCurrentlyCutaway)
		{
			++ActiveCutaways;
		}
	}
	return ActiveCutaways;
}

int32 ADungeoneerTileMapInstancer::EvaluateEntityStepOnBreakables(
	const FVector& EntityWorldLocation,
	const float OverrideLoudness,
	const float OverrideTriggerRadiusUU)
{
	int32 ShatteredThisCall = 0;
	UWorld* World = GetWorld();
	UDungeoneerSensoryWorldSubsystem* SensorySub = World ? World->GetSubsystem<UDungeoneerSensoryWorldSubsystem>() : nullptr;

	for (FDungeoneerBreakableInstanceEntry& Entry : BreakableEntries)
	{
		if (Entry.bIsShattered)
		{
			continue;
		}

		const float EffectiveRadius = OverrideTriggerRadiusUU > 0.0f ? OverrideTriggerRadiusUU : Entry.TriggerRadiusUU;
		const double DistSq2D = FVector::DistSquared2D(EntityWorldLocation, Entry.WorldCenter);
		const double DeltaZ = FMath::Abs(EntityWorldLocation.Z - Entry.WorldCenter.Z);

		if (DistSq2D <= static_cast<double>(EffectiveRadius) * static_cast<double>(EffectiveRadius) && DeltaZ <= 200.0)
		{
			Entry.bIsShattered = true;
			if (BreakableHISM && Entry.HISMInstanceIndex != INDEX_NONE)
			{
				BreakableHISM->SetCustomDataValue(Entry.HISMInstanceIndex, 0, 1.0f, true);
				BreakableHISM->SetCustomDataValue(
					Entry.HISMInstanceIndex,
					1,
					static_cast<float>(Entry.ShatteredUVCellIndex),
					true);
			}

			if (SensorySub)
			{
				const float Loudness = OverrideLoudness > 0.0f ? OverrideLoudness : Entry.ImpactLoudness;
				SensorySub->BroadcastNoiseEvent(Entry.WorldCenter, Loudness, 950.0f, false);
			}

			++ShatteredThisCall;
		}
	}

	return ShatteredThisCall;
}

void ADungeoneerTileMapInstancer::RevealMirrorIllusionSeam(const int32 MirrorEntryIndex, const bool bRevealed)
{
	if (!MirrorEntries.IsValidIndex(MirrorEntryIndex))
	{
		return;
	}

	FDungeoneerMirrorInstanceEntry& Entry = MirrorEntries[MirrorEntryIndex];
	Entry.bIllusionSeamRevealed = bRevealed;

	if (MirrorHISM && Entry.HISMInstanceIndex != INDEX_NONE)
	{
		MirrorHISM->SetCustomDataValue(Entry.HISMInstanceIndex, 0, bRevealed ? 1.0f : 0.0f, true);
	}
}

void ADungeoneerTileMapInstancer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateSouthWallCutawaysForViewports();
}
