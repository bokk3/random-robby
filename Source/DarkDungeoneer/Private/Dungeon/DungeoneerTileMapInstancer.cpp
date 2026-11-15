#include "Dungeon/DungeoneerTileMapInstancer.h"
#include "Dungeon/DungeonGeneratorSubsystem.h"
#include "Dungeon/DungeonEncounterActors.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "AI/ZombieExploder.h"
#include "AI/ZombieFastThrower.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

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

	struct FComputedRoomLayout
	{
		int32 NodeId = INDEX_NONE;
		FVector RoomCenter = FVector::ZeroVector;
		int32 NumTilesX = 9;
		int32 NumTilesY = 9;
		float EdgeOffsetX = 1152.0f;
		float EdgeOffsetY = 1152.0f;
		FBox2D OuterBox = FBox2D(ForceInit);
		TArray<FVector2D> WestDoorPositions;
		TArray<FVector2D> EastDoorPositions;
	};

	inline bool IsNearAnyDoorway(const TArray<FVector2D>& DoorPositions, const double WorldX, const double WorldY, const double ClearanceRadius) noexcept
	{
		const double RadiusSq = ClearanceRadius * ClearanceRadius;
		for (const FVector2D& Door : DoorPositions)
		{
			if (FVector2D::DistSquared(FVector2D(WorldX, WorldY), Door) <= RadiusSq)
			{
				return true;
			}
		}
		return false;
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
	UMaterialInterface* BaseShapeMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UDungeoneerAssetBootstrapSubsystem* Bootstrap = GI ? GI->GetSubsystem<UDungeoneerAssetBootstrapSubsystem>() : nullptr;

	auto AssignHISMVisuals = [this, BaseShapeMat, Bootstrap](
		UHierarchicalInstancedStaticMeshComponent* Comp,
		FName DiffuseKey,
		FName NormalKey,
		const FLinearColor& TintColor,
		const float RoughnessVal)
	{
		if (!Comp)
		{
			return;
		}

		if (Bootstrap && !bTexturedHISMApplied)
		{
			if (UMaterialInstanceDynamic* TexturedMID = Bootstrap->CreateDungeonOrRigMaterial(this, DiffuseKey, NormalKey, TintColor, RoughnessVal, true))
			{
				Comp->SetMaterial(0, TexturedMID);
				return;
			}
		}

		if (BaseShapeMat && !Comp->GetMaterial(0))
		{
			if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseShapeMat, this))
			{
				MID->SetVectorParameterValue(FName(TEXT("Color")), TintColor);
				MID->SetScalarParameterValue(FName(TEXT("Roughness")), RoughnessVal);
				Comp->SetMaterial(0, MID);
			}
		}
	};

	if (FloorHISM)
	{
		FloorHISM->NumCustomDataFloats = 4;
		if (!FloorHISM->GetStaticMesh() && (PlaneMesh || BoxFallback))
		{
			FloorHISM->SetStaticMesh(PlaneMesh ? PlaneMesh : BoxFallback);
		}
		AssignHISMVisuals(FloorHISM.Get(), FName(TEXT("Floor_Tile0_D")), FName(TEXT("Floor_Tile0_N")), FLinearColor(0.22f, 0.25f, 0.34f), 0.72f);
	}

	auto ConfigureBoxHISM = [BoxFallback, &AssignHISMVisuals](
		UHierarchicalInstancedStaticMeshComponent* Comp,
		const bool bShadow,
		FName DiffuseKey,
		FName NormalKey,
		const FLinearColor& TintColor,
		const float RoughnessVal)
	{
		if (Comp)
		{
			Comp->NumCustomDataFloats = 4;
			Comp->bCastHiddenShadow = bShadow;
			if (!Comp->GetStaticMesh() && BoxFallback)
			{
				Comp->SetStaticMesh(BoxFallback);
			}
			AssignHISMVisuals(Comp, DiffuseKey, NormalKey, TintColor, RoughnessVal);
		}
	};

	ConfigureBoxHISM(WallHISM.Get(), true, FName(TEXT("Wall_Full0_D")), FName(TEXT("Wall_Full0_N")), FLinearColor(0.32f, 0.30f, 0.40f), 0.65f);
	ConfigureBoxHISM(MirrorHISM.Get(), true, FName(TEXT("Mirror_Intact_D")), FName(TEXT("Mirror_Intact_N")), FLinearColor(0.38f, 0.88f, 0.98f), 0.08f);
	ConfigureBoxHISM(BreakableHISM.Get(), true, FName(TEXT("Breakable_Intact_D")), FName(TEXT("Breakable_Intact_N")), FLinearColor(0.76f, 0.46f, 0.20f), 0.50f);

	if (Bootstrap)
	{
		bTexturedHISMApplied = true;
	}
}

void ADungeoneerTileMapInstancer::ClearAllInstances()
{
	if (FloorHISM)     { FloorHISM->ClearInstances(); }
	if (WallHISM)      { WallHISM->ClearInstances(); }
	if (MirrorHISM)    { MirrorHISM->ClearInstances(); }
	if (BreakableHISM) { BreakableHISM->ClearInstances(); }

	for (AActor* Encounter : SpawnedEncounterActors)
	{
		if (IsValid(Encounter) && !Encounter->IsActorBeingDestroyed())
		{
			Encounter->Destroy();
		}
	}
	SpawnedEncounterActors.Reset();

	WallEntries.Reset();
	MirrorEntries.Reset();
	BreakableEntries.Reset();
	PendingCutawayFlags.Reset();
	LastCorridorTileCount = 0;
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

void ADungeoneerTileMapInstancer::SpawnEncountersForFloorGraph(const FDungeonFloorGraph& FloorGraph)
{
	UWorld* World = GetWorld();
	if (!bSpawnRoomEncounters || !World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	for (const FDungeonNode& Node : FloorGraph.Nodes)
	{
		const FVector Center = Node.WorldCenter;

		switch (Node.RoomType)
		{
		case EDungeonRoomType::Combat:
		{
			if (AZombieExploder* Exploder = World->SpawnActor<AZombieExploder>(AZombieExploder::StaticClass(), Center + FVector(280.0, -180.0, 95.0), FRotator::ZeroRotator, SpawnParams))
			{
				SpawnedEncounterActors.Add(Exploder);
			}
			if (AZombieFastThrower* Thrower = World->SpawnActor<AZombieFastThrower>(AZombieFastThrower::StaticClass(), Center + FVector(340.0, 220.0, 95.0), FRotator::ZeroRotator, SpawnParams))
			{
				SpawnedEncounterActors.Add(Thrower);
			}
			break;
		}
		case EDungeonRoomType::DeepFogGauntlet:
		{
			for (int32 i = 0; i < 2; ++i)
			{
				const double OffsetY = (i == 0) ? -320.0 : 320.0;
				if (AZombieExploder* Exploder = World->SpawnActor<AZombieExploder>(AZombieExploder::StaticClass(), Center + FVector(180.0, OffsetY, 95.0), FRotator::ZeroRotator, SpawnParams))
				{
					SpawnedEncounterActors.Add(Exploder);
				}
				if (AZombieFastThrower* Thrower = World->SpawnActor<AZombieFastThrower>(AZombieFastThrower::StaticClass(), Center + FVector(-220.0, -OffsetY, 95.0), FRotator::ZeroRotator, SpawnParams))
				{
					SpawnedEncounterActors.Add(Thrower);
				}
			}
			break;
		}
		case EDungeonRoomType::RestingGround:
		{
			if (ARestingGroundActor* Sanctuary = World->SpawnActor<ARestingGroundActor>(ARestingGroundActor::StaticClass(), Center, FRotator::ZeroRotator, SpawnParams))
			{
				SpawnedEncounterActors.Add(Sanctuary);
			}
			break;
		}
		case EDungeonRoomType::Vendor:
		{
			if (AVendorActor* Merchant = World->SpawnActor<AVendorActor>(AVendorActor::StaticClass(), Center, FRotator::ZeroRotator, SpawnParams))
			{
				Merchant->InitializeSeededCatalog(Node.RoomSeed);
				SpawnedEncounterActors.Add(Merchant);
			}
			break;
		}
		case EDungeonRoomType::MajorLootVault:
		{
			if (AMajorLootVaultActor* Vault = World->SpawnActor<AMajorLootVaultActor>(AMajorLootVaultActor::StaticClass(), Center, FRotator::ZeroRotator, SpawnParams))
			{
				SpawnedEncounterActors.Add(Vault);
			}
			break;
		}
		default:
			break;
		}
	}
}

void ADungeoneerTileMapInstancer::BuildDungeonFromFloorGraph(const FDungeonFloorGraph& FloorGraph)
{
	EnsureMeshAssetsInitialized();
	ClearAllInstances();

	const UWorld* World = GetWorld();
	const UDungeonGeneratorSubsystem* GenSub = World ? World->GetSubsystem<UDungeonGeneratorSubsystem>() : nullptr;

	// Pre-reserve metadata storage cleanly outside Tick
	const int32 EstimatedRooms = FMath::Max(1, FloorGraph.Nodes.Num());
	WallEntries.Reserve(EstimatedRooms * 28);
	PendingCutawayFlags.Reserve(EstimatedRooms * 28);
	MirrorEntries.Reserve(EstimatedRooms * 8);
	BreakableEntries.Reserve(EstimatedRooms * 4);

	// 1. Pre-compute room bounding layouts and doorway / corridor topology
	TArray<TileMapDetail::FComputedRoomLayout> RoomLayouts;
	RoomLayouts.Reserve(FloorGraph.Nodes.Num());
	TMap<int32, int32> NodeIdToLayoutIdx;

	for (const FDungeonNode& Node : FloorGraph.Nodes)
	{
		TileMapDetail::FComputedRoomLayout Layout;
		Layout.NodeId = Node.NodeId;
		Layout.RoomCenter = Node.WorldCenter;
		const float HalfX = FMath::Max(TileSizeUU * 2.0f, Node.RoomExtents.X);
		const float HalfY = FMath::Max(TileSizeUU * 2.0f, Node.RoomExtents.Y);
		Layout.NumTilesX = FMath::Max(5, FMath::RoundToInt((HalfX * 2.0f) / TileSizeUU));
		Layout.NumTilesY = FMath::Max(5, FMath::RoundToInt((HalfY * 2.0f) / TileSizeUU));
		Layout.EdgeOffsetX = (static_cast<float>(Layout.NumTilesX) * 0.5f) * TileSizeUU;
		Layout.EdgeOffsetY = (static_cast<float>(Layout.NumTilesY) * 0.5f) * TileSizeUU;
		Layout.OuterBox = FBox2D(
			FVector2D(Node.WorldCenter.X - Layout.EdgeOffsetX, Node.WorldCenter.Y - Layout.EdgeOffsetY),
			FVector2D(Node.WorldCenter.X + Layout.EdgeOffsetX, Node.WorldCenter.Y + Layout.EdgeOffsetY));

		const int32 NewIdx = RoomLayouts.Add(Layout);
		NodeIdToLayoutIdx.Add(Node.NodeId, NewIdx);
	}

	// Collect deduplicated grid corridor tiles across all DAG edges so T-junctions merge cleanly
	TSet<FIntPoint> CorridorGridCells;
	TArray<FVector2D> AllDoorways2D;
	const double GridStep = static_cast<double>(TileSizeUU);

	auto AddCorridorStripXY = [&CorridorGridCells, GridStep](const FVector2D& P0, const FVector2D& P1)
	{
		const int32 GX0 = FMath::RoundToInt(P0.X / GridStep);
		const int32 GY0 = FMath::RoundToInt(P0.Y / GridStep);
		const int32 GX1 = FMath::RoundToInt(P1.X / GridStep);
		const int32 GY1 = FMath::RoundToInt(P1.Y / GridStep);
		const int32 Steps = FMath::Max(FMath::Abs(GX1 - GX0), FMath::Abs(GY1 - GY0));

		if (Steps == 0)
		{
			for (int32 ox = -1; ox <= 1; ++ox)
			{
				for (int32 oy = -1; oy <= 1; ++oy)
				{
					CorridorGridCells.Add(FIntPoint(GX0 + ox, GY0 + oy));
				}
			}
			return;
		}

		for (int32 s = 0; s <= Steps; ++s)
		{
			const float Alpha = static_cast<float>(s) / static_cast<float>(Steps);
			const int32 CGX = FMath::RoundToInt(FMath::Lerp(static_cast<float>(GX0), static_cast<float>(GX1), Alpha));
			const int32 CGY = FMath::RoundToInt(FMath::Lerp(static_cast<float>(GY0), static_cast<float>(GY1), Alpha));
			for (int32 ox = -1; ox <= 1; ++ox)
			{
				for (int32 oy = -1; oy <= 1; ++oy)
				{
					CorridorGridCells.Add(FIntPoint(CGX + ox, CGY + oy));
				}
			}
		}
	};

	for (const FDungeonNode& NodeA : FloorGraph.Nodes)
	{
		const int32* LayoutAIdx = NodeIdToLayoutIdx.Find(NodeA.NodeId);
		if (!LayoutAIdx)
		{
			continue;
		}

		for (const int32 OutId : NodeA.OutgoingNodeIds)
		{
			const int32* LayoutBIdx = NodeIdToLayoutIdx.Find(OutId);
			if (!LayoutBIdx)
			{
				continue;
			}

			TileMapDetail::FComputedRoomLayout& LayA = RoomLayouts[*LayoutAIdx];
			TileMapDetail::FComputedRoomLayout& LayB = RoomLayouts[*LayoutBIdx];

			const double GapX = LayB.OuterBox.Min.X - LayA.OuterBox.Max.X;
			if (GapX > GridStep * 0.4)
			{
				const FVector2D EastDoor(LayA.OuterBox.Max.X, LayA.RoomCenter.Y);
				const FVector2D WestDoor(LayB.OuterBox.Min.X, LayB.RoomCenter.Y);
				LayA.EastDoorPositions.Add(EastDoor);
				LayB.WestDoorPositions.Add(WestDoor);
				AllDoorways2D.Add(EastDoor);
				AllDoorways2D.Add(WestDoor);

				const double MidX = 0.5 * (EastDoor.X + WestDoor.X);
				AddCorridorStripXY(EastDoor, FVector2D(MidX, EastDoor.Y));
				AddCorridorStripXY(FVector2D(MidX, EastDoor.Y), FVector2D(MidX, WestDoor.Y));
				AddCorridorStripXY(FVector2D(MidX, WestDoor.Y), WestDoor);
			}
			else
			{
				// Rooms nearly touch or flush-abut (e.g. Combat -> MirrorPalaceMaze -> Vendor):
				// align a direct threshold aperture within the shared Y wall span.
				const double SafePad = GridStep * 1.35;
				const double OverlapMinY = FMath::Max(LayA.OuterBox.Min.Y + SafePad, LayB.OuterBox.Min.Y + SafePad);
				const double OverlapMaxY = FMath::Min(LayA.OuterBox.Max.Y - SafePad, LayB.OuterBox.Max.Y - SafePad);
				const double RawMidY = 0.5 * (LayA.RoomCenter.Y + LayB.RoomCenter.Y);
				const double SharedY = (OverlapMinY <= OverlapMaxY)
					? FMath::Clamp(RawMidY, OverlapMinY, OverlapMaxY)
					: (0.5 * (OverlapMinY + OverlapMaxY));

				const FVector2D EastDoor(LayA.OuterBox.Max.X, SharedY);
				const FVector2D WestDoor(LayB.OuterBox.Min.X, SharedY);
				LayA.EastDoorPositions.Add(EastDoor);
				LayB.WestDoorPositions.Add(WestDoor);
				AllDoorways2D.Add(EastDoor);
				AllDoorways2D.Add(WestDoor);
				AddCorridorStripXY(EastDoor, WestDoor);
			}
		}
	}

	// 2. Build Rooms with Carved Doorways
	const double DoorCarveRadius = static_cast<double>(TileSizeUU) * 1.45;
	for (int32 NodeIdx = 0; NodeIdx < FloorGraph.Nodes.Num(); ++NodeIdx)
	{
		const FDungeonNode& Node = FloorGraph.Nodes[NodeIdx];
		const TileMapDetail::FComputedRoomLayout& Lay = RoomLayouts[NodeIdx];

		FRandomStream RoomRand(Node.RoomSeed);
		const int32 NumTilesX = Lay.NumTilesX;
		const int32 NumTilesY = Lay.NumTilesY;
		const FVector RoomOrigin = Node.WorldCenter;

		// 2a. Room Floor Grid (`ATLAS_Dungeon_Floors_4x4`)
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
						int32 FloorUV = RoomRand.RandRange(0, 3);
						if (Node.RoomType == EDungeonRoomType::MirrorPalaceMaze)
						{
							FloorUV = 15;
						}
						else if (Node.RoomType == EDungeonRoomType::DeepFogGauntlet || ((ix + iy) % 5 == 0))
						{
							FloorUV = 4 + RoomRand.RandRange(0, 3);
						}
						FloorHISM->SetCustomDataValue(FloorIdx, 0, static_cast<float>(Node.NodeId), false);
						FloorHISM->SetCustomDataValue(FloorIdx, 1, static_cast<float>(FloorUV), false);
					}
				}
			}
		}

		// 2b. Room Perimeter 4-Axis Walls with Carved West/East Doorways
		const float EdgeOffsetX = Lay.EdgeOffsetX;
		const float EdgeOffsetY = Lay.EdgeOffsetY;
		const float WallZ = RoomOrigin.Z + WallHeightUU * 0.5f;
		const float SegSpan = TileSizeUU * 0.45f;
		const FVector NSExtents(SegSpan, 34.0f, WallHeightUU * 0.5f);
		const FVector EWExtents(34.0f, SegSpan, WallHeightUU * 0.5f);

		for (int32 ix = 1; ix < NumTilesX - 1; ++ix)
		{
			const float WX = RoomOrigin.X + (static_cast<float>(ix) - 0.5f * static_cast<float>(NumTilesX - 1)) * TileSizeUU;
			AddWallInstance(FVector(WX, RoomOrigin.Y + EdgeOffsetY, WallZ), EDungeoneerWallAxis::North, NSExtents, ix % 4, 4 + (ix % 4));
			AddWallInstance(FVector(WX, RoomOrigin.Y - EdgeOffsetY, WallZ), EDungeoneerWallAxis::South, NSExtents, ix % 4, 4 + (ix % 4));
		}

		for (int32 iy = 1; iy < NumTilesY - 1; ++iy)
		{
			const float WY = RoomOrigin.Y + (static_cast<float>(iy) - 0.5f * static_cast<float>(NumTilesY - 1)) * TileSizeUU;
			const double EastWX = RoomOrigin.X + EdgeOffsetX;
			const double WestWX = RoomOrigin.X - EdgeOffsetX;

			if (!TileMapDetail::IsNearAnyDoorway(Lay.EastDoorPositions, EastWX, WY, DoorCarveRadius))
			{
				AddWallInstance(FVector(EastWX, WY, WallZ), EDungeoneerWallAxis::East, EWExtents, iy % 4, 4 + (iy % 4));
			}
			if (!TileMapDetail::IsNearAnyDoorway(Lay.WestDoorPositions, WestWX, WY, DoorCarveRadius))
			{
				AddWallInstance(FVector(WestWX, WY, WallZ), EDungeoneerWallAxis::West, EWExtents, iy % 4, 4 + (iy % 4));
			}
		}

		// 45-degree Diagonal Corner Chamfers (skip if doorway falls near corner)
		const float DiagOffsetX = EdgeOffsetX - TileSizeUU * 0.25f;
		const float DiagOffsetY = EdgeOffsetY - TileSizeUU * 0.25f;
		const FVector DiagExtents(SegSpan, 34.0f, WallHeightUU * 0.5f);
		const double CornerDoorClearance = DoorCarveRadius * 0.9;

		const FVector CornerNE(RoomOrigin.X + DiagOffsetX, RoomOrigin.Y + DiagOffsetY, WallZ);
		const FVector CornerNW(RoomOrigin.X - DiagOffsetX, RoomOrigin.Y + DiagOffsetY, WallZ);
		const FVector CornerSE(RoomOrigin.X + DiagOffsetX, RoomOrigin.Y - DiagOffsetY, WallZ);
		const FVector CornerSW(RoomOrigin.X - DiagOffsetX, RoomOrigin.Y - DiagOffsetY, WallZ);

		if (!TileMapDetail::IsNearAnyDoorway(Lay.EastDoorPositions, CornerNE.X, CornerNE.Y, CornerDoorClearance))
		{
			AddWallInstance(CornerNE, EDungeoneerWallAxis::NorthEast, DiagExtents, 8, 4);
		}
		if (!TileMapDetail::IsNearAnyDoorway(Lay.WestDoorPositions, CornerNW.X, CornerNW.Y, CornerDoorClearance))
		{
			AddWallInstance(CornerNW, EDungeoneerWallAxis::NorthWest, DiagExtents, 9, 5);
		}
		if (!TileMapDetail::IsNearAnyDoorway(Lay.EastDoorPositions, CornerSE.X, CornerSE.Y, CornerDoorClearance))
		{
			AddWallInstance(CornerSE, EDungeoneerWallAxis::SouthEast, DiagExtents, 10, 6);
		}
		if (!TileMapDetail::IsNearAnyDoorway(Lay.WestDoorPositions, CornerSW.X, CornerSW.Y, CornerDoorClearance))
		{
			AddWallInstance(CornerSW, EDungeoneerWallAxis::SouthWest, DiagExtents, 11, 7);
		}

		// 2c. Mirror-Palace Maze Panels & 45-deg Bounce Prisms (`MirrorHISM`)
		if (Node.RoomType == EDungeonRoomType::MirrorPalaceMaze || Node.NodeId % 3 == 1)
		{
			if (GenSub && Node.RoomType == EDungeonRoomType::MirrorPalaceMaze)
			{
				TArray<EMirrorCellType> MazeCells;
				GenSub->GenerateMirrorMazeGrid(Node.RoomSeed, 5, 5, MazeCells);
				for (int32 idx = 0; idx < MazeCells.Num(); ++idx)
				{
					const EMirrorCellType CellType = MazeCells[idx];
					if (CellType == EMirrorCellType::SolidMirror || CellType == EMirrorCellType::PhantasmWall)
					{
						const int32 gx = idx % 5;
						const int32 gy = idx / 5;
						const FVector MPos = RoomOrigin + FVector((gx - 2.0f) * TileSizeUU * 0.9f, (gy - 2.0f) * TileSizeUU * 0.9f, WallHeightUU * 0.42f);
						// Keep entrance & exit doorway lanes traversable
						if (TileMapDetail::IsNearAnyDoorway(AllDoorways2D, MPos.X, MPos.Y, TileSizeUU * 1.5))
						{
							continue;
						}
						const FVector MNorm = ((gx + gy) % 2 == 0) ? FVector(0.0f, -1.0f, 0.0f) : FVector(-0.70710678f, -0.70710678f, 0.0f);
						const bool bPhantasm = (CellType == EMirrorCellType::PhantasmWall);
						RegisterMirrorInstance(MPos, MNorm, 96.0f, 115.0f, bPhantasm, bPhantasm ? 4 : (idx % 4));
					}
				}
			}
			else
			{
				RegisterMirrorInstance(
					RoomOrigin + FVector(0.0f, EdgeOffsetY * 0.55f, WallHeightUU * 0.42f),
					FVector(0.0f, -1.0f, 0.0f),
					96.0f,
					115.0f,
					false,
					0);
			}
		}

		// 2d. Deterministic Breakable Barrels/Urns (`PROP_Breakables_And_Traps_4x4`)
		const FVector BreakOffset(RoomRand.FRandRange(-160.0f, 160.0f), RoomRand.FRandRange(-140.0f, 140.0f), 45.0f);
		const int32 IntactCell = RoomRand.RandRange(0, 1);
		RegisterBreakableInstance(RoomOrigin + BreakOffset, IntactCell, IntactCell + 2, 85.0f, 1.35f);
	}

	// 3. Place Connecting Corridor Floor Tiles & Flanking Walls Outside Room Interiors
	auto IsStrictlyInsideAnyRoom = [&RoomLayouts](const double WX, const double WY, const double Margin) noexcept -> bool
	{
		for (const TileMapDetail::FComputedRoomLayout& Lay : RoomLayouts)
		{
			if (WX >= (Lay.OuterBox.Min.X + Margin) && WX <= (Lay.OuterBox.Max.X - Margin) &&
				WY >= (Lay.OuterBox.Min.Y + Margin) && WY <= (Lay.OuterBox.Max.Y - Margin))
			{
				return true;
			}
		}
		return false;
	};

	const FVector CorridorFloorScale(TileSizeUU / 100.0f, TileSizeUU / 100.0f, 1.0f);
	const float CorridorWallZ = WallHeightUU * 0.5f;
	const FVector CorridorNSExtents(TileSizeUU * 0.45f, 34.0f, WallHeightUU * 0.5f);
	const FVector CorridorEWExtents(34.0f, TileSizeUU * 0.45f, WallHeightUU * 0.5f);

	for (const FIntPoint& Cell : CorridorGridCells)
	{
		const double CellX = static_cast<double>(Cell.X) * GridStep;
		const double CellY = static_cast<double>(Cell.Y) * GridStep;

		if (IsStrictlyInsideAnyRoom(CellX, CellY, TileSizeUU * 0.4))
		{
			continue;
		}

		if (FloorHISM)
		{
			const int32 FloorIdx = FloorHISM->AddInstance(
				FTransform(FRotator::ZeroRotator, FVector(CellX, CellY, 0.0), CorridorFloorScale),
				true);
			if (FloorIdx != INDEX_NONE)
			{
				const int32 CorridorUV = 4 + ((FMath::Abs(Cell.X + Cell.Y)) % 4);
				FloorHISM->SetCustomDataValue(FloorIdx, 0, -1.0f, false);
				FloorHISM->SetCustomDataValue(FloorIdx, 1, static_cast<float>(CorridorUV), false);
				++LastCorridorTileCount;
			}
		}

		struct FNeighborSide
		{
			int32 DX;
			int32 DY;
			EDungeoneerWallAxis Axis;
			FVector HalfExt;
		};
		const FNeighborSide Sides[] =
		{
			{  0,  1, EDungeoneerWallAxis::North, CorridorNSExtents },
			{  0, -1, EDungeoneerWallAxis::South, CorridorNSExtents },
			{  1,  0, EDungeoneerWallAxis::East,  CorridorEWExtents },
			{ -1,  0, EDungeoneerWallAxis::West,  CorridorEWExtents }
		};

		for (const FNeighborSide& Side : Sides)
		{
			const FIntPoint NeighborKey(Cell.X + Side.DX, Cell.Y + Side.DY);
			if (CorridorGridCells.Contains(NeighborKey))
			{
				continue;
			}

			const double WallX = CellX + static_cast<double>(Side.DX) * (GridStep * 0.5);
			const double WallY = CellY + static_cast<double>(Side.DY) * (GridStep * 0.5);

			if (IsStrictlyInsideAnyRoom(WallX, WallY, -24.0) ||
				TileMapDetail::IsNearAnyDoorway(AllDoorways2D, WallX, WallY, TileSizeUU * 1.15))
			{
				continue;
			}

			AddWallInstance(FVector(WallX, WallY, CorridorWallZ), Side.Axis, Side.HalfExt, 0, 4);
		}
	}

	// 4. Spawn Room Encounters (Exploders, Fast Throwers, Sanctuary, Vendor, Vault)
	SpawnEncountersForFloorGraph(FloorGraph);

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
