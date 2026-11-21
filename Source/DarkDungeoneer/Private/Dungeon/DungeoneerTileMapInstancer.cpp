#include "Dungeon/DungeoneerTileMapInstancer.h"
#include "Combat/DungeoneerSkillComponent.h"
#include "Dungeon/DungeonGeneratorSubsystem.h"
#include "Dungeon/DungeonEncounterActors.h"
#include "Audio/DungeonAcousticSubsystem.h"
#include "Ecosystem/DungeonEcosystemTypes.h"
#include "Ecosystem/DungeonFaunaSubsystem.h"
#include "Ecosystem/DungeonFloraManagerComponent.h"
#include "Ecosystem/DungeonWaterSubsystem.h"
#include "Lighting/DungeonLightBudgetSubsystem.h"
#include "Rendering/DungeoneerEcoMaterialFactory.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "Core/DungeoneerCharacter.h"
#include "AI/BossGuardianActor.h"
#include "AI/ZombieExploder.h"
#include "AI/ZombieFastThrower.h"
#include "AI/ZombieMirrorStalker.h"
#include "AI/ZombieGlazier.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
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
		TArray<FVector2D> NorthDoorPositions;
		TArray<FVector2D> SouthDoorPositions;
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
	FloorHISM->SetCullDistances(0, 3800);

	WetFloorHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("WetFloorHISM"));
	WetFloorHISM->SetupAttachment(SceneRoot);
	WetFloorHISM->NumCustomDataFloats = 4;
	WetFloorHISM->SetCastShadow(false);
	WetFloorHISM->bCastHiddenShadow = false;
	WetFloorHISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WetFloorHISM->SetCullDistances(0, 3800);

	WallHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("WallHISM"));
	WallHISM->SetupAttachment(SceneRoot);
	WallHISM->NumCustomDataFloats = 4;
	WallHISM->SetCastShadow(true);
	WallHISM->bCastHiddenShadow = true;
	WallHISM->SetCollisionProfileName(TEXT("BlockAll"));
	WallHISM->ComponentTags.Add(TEXT("DungeonWall"));
	WallHISM->SetCullDistances(0, 3800);

	DiagonalWedgeHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("DiagonalWedgeHISM"));
	DiagonalWedgeHISM->SetupAttachment(SceneRoot);
	DiagonalWedgeHISM->NumCustomDataFloats = 4;
	DiagonalWedgeHISM->SetCastShadow(true);
	DiagonalWedgeHISM->bCastHiddenShadow = true;
	DiagonalWedgeHISM->SetCollisionProfileName(TEXT("BlockAll"));
	DiagonalWedgeHISM->ComponentTags.Add(TEXT("DungeonWall"));
	DiagonalWedgeHISM->SetCullDistances(0, 3800);

	MirrorHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("MirrorHISM"));
	MirrorHISM->SetupAttachment(SceneRoot);
	MirrorHISM->NumCustomDataFloats = 4;
	MirrorHISM->SetCastShadow(true);
	MirrorHISM->bCastHiddenShadow = true;
	MirrorHISM->SetCollisionProfileName(TEXT("BlockAll"));
	MirrorHISM->ComponentTags.Add(TEXT("MirrorReflective"));
	MirrorHISM->SetCullDistances(0, 3800);

	SconceHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("SconceHISM"));
	SconceHISM->SetupAttachment(SceneRoot);
	SconceHISM->NumCustomDataFloats = 4;
	SconceHISM->SetCastShadow(false);
	SconceHISM->bCastHiddenShadow = false;
	SconceHISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SconceHISM->SetCullDistances(0, 3800);

	BreakableHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("BreakableHISM"));
	BreakableHISM->SetupAttachment(SceneRoot);
	BreakableHISM->NumCustomDataFloats = 4;
	BreakableHISM->SetCastShadow(true);
	BreakableHISM->bCastHiddenShadow = true;
	BreakableHISM->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	BreakableHISM->ComponentTags.Add(TEXT("BreakableNoiseProp"));
	BreakableHISM->SetCullDistances(0, 3800);

	ForegroundParallaxHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("ForegroundParallaxHISM"));
	ForegroundParallaxHISM->SetupAttachment(SceneRoot);
	ForegroundParallaxHISM->NumCustomDataFloats = EEcoCPD::Count;
	ForegroundParallaxHISM->SetCastShadow(false);
	ForegroundParallaxHISM->bCastHiddenShadow = false;
	ForegroundParallaxHISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ForegroundParallaxHISM->SetCullDistances(0, 3200);

	HISM_Water = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("HISM_Water"));
	HISM_Water->SetupAttachment(SceneRoot);
	HISM_Water->NumCustomDataFloats = EEcoCPD::Count;
	HISM_Water->SetCastShadow(false);
	HISM_Water->bCastHiddenShadow = false;
	HISM_Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HISM_Water->SetCullDistances(0, 3800);

	ISM_WaterFX = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ISM_WaterFX"));
	ISM_WaterFX->SetupAttachment(SceneRoot);
	ISM_WaterFX->NumCustomDataFloats = EEcoCPD::Count;
	ISM_WaterFX->SetCastShadow(false);
	ISM_WaterFX->bCastHiddenShadow = false;
	ISM_WaterFX->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISM_WaterFX->SetCullDistances(0, 3200);

	ISM_Fauna = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ISM_Fauna"));
	ISM_Fauna->SetupAttachment(SceneRoot);
	ISM_Fauna->NumCustomDataFloats = EEcoCPD::Count;
	ISM_Fauna->SetCastShadow(false);
	ISM_Fauna->bCastHiddenShadow = false;
	ISM_Fauna->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ISM_Fauna->SetCullDistances(0, 2800);

	FloraManager = CreateDefaultSubobject<UDungeonFloraManagerComponent>(TEXT("FloraManager"));

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
	UMaterialInterface* EcoMat = Bootstrap ? Bootstrap->EnsureEcoInstancedMaterial() : nullptr;

	auto AssignHISMVisuals = [this, BaseShapeMat, Bootstrap, EcoMat](
		UHierarchicalInstancedStaticMeshComponent* Comp,
		FName AtlasPrefix,
		FName SingleDiffuseKey,
		FName SingleNormalKey,
		EDungeoneerMaterialLayoutMode LayoutMode,
		const FLinearColor& TintColor,
		const float RoughnessVal)
	{
		if (!Comp)
		{
			return;
		}
		Comp->NumCustomDataFloats = EEcoCPD::Count;

		// D1b: Bind full 4x4 atlas onto M_Dungeoneer_EcoInstanced with Tier 1 glow reception
		if (Bootstrap && EcoMat && !bTexturedHISMApplied)
		{
			UTexture2D* AtlasD = Bootstrap->GetLoadedDungeonTexture(FName(*FString::Printf(TEXT("%s_D"), *AtlasPrefix.ToString())));
			UTexture2D* AtlasN = Bootstrap->GetLoadedDungeonTexture(FName(*FString::Printf(TEXT("%s_N"), *AtlasPrefix.ToString())));
			UTexture2D* AtlasE = Bootstrap->GetLoadedDungeonTexture(FName(*FString::Printf(TEXT("%s_E"), *AtlasPrefix.ToString())));
			if (AtlasD)
			{
				const float TexPx = static_cast<float>(FMath::Max(AtlasD->GetSizeX(), 128));
				if (UMaterialInstanceDynamic* EcoMID = FDungeoneerEcoMaterialFactory::CreateInstancedAtlasMID(
					Comp, EcoMat, AtlasD, AtlasN, AtlasE, LayoutMode, true, TexPx, 4, 4, RoughnessVal, 1.0f, 0.0f))
				{
					EcoMID->SetVectorParameterValue(FName(TEXT("DiffuseColor")), FMath::Lerp(FLinearColor::White, TintColor, 0.22f));
					Comp->SetMaterial(0, EcoMID);
					return;
				}
			}

			// Single-cell extracted fallback using Eco shader so Tier 1 glow still illuminates props/sconces
			UTexture2D* SingleD = Bootstrap->GetExtractedCellTexture(SingleDiffuseKey);
			UTexture2D* SingleN = Bootstrap->GetExtractedCellTexture(SingleNormalKey);
			if (SingleD)
			{
				if (UMaterialInstanceDynamic* EcoMID = FDungeoneerEcoMaterialFactory::CreateInstancedAtlasMID(
					Comp, EcoMat, SingleD, SingleN, nullptr, EDungeoneerMaterialLayoutMode::LegacyDirectCD0, true, 128.0f, 1, 1, RoughnessVal, 1.0f, 0.0f))
				{
					EcoMID->SetVectorParameterValue(FName(TEXT("DiffuseColor")), TintColor);
					Comp->SetMaterial(0, EcoMID);
					return;
				}
			}
		}

		if (Bootstrap && !bTexturedHISMApplied)
		{
			if (UMaterialInstanceDynamic* TexturedMID = Bootstrap->CreateDungeonOrRigMaterial(this, SingleDiffuseKey, SingleNormalKey, TintColor, RoughnessVal, true))
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

	auto ConfigurePlaneHISM = [PlaneMesh, BoxFallback, &AssignHISMVisuals](
		UHierarchicalInstancedStaticMeshComponent* Comp,
		FName AtlasPrefix,
		FName DiffuseKey,
		FName NormalKey,
		EDungeoneerMaterialLayoutMode LayoutMode,
		const FLinearColor& TintColor,
		const float RoughnessVal)
	{
		if (Comp)
		{
			Comp->NumCustomDataFloats = EEcoCPD::Count;
			if (!Comp->GetStaticMesh() && (PlaneMesh || BoxFallback))
			{
				Comp->SetStaticMesh(PlaneMesh ? PlaneMesh : BoxFallback);
			}
			AssignHISMVisuals(Comp, AtlasPrefix, DiffuseKey, NormalKey, LayoutMode, TintColor, RoughnessVal);
		}
	};

	auto ConfigureBoxHISM = [BoxFallback, &AssignHISMVisuals](
		UHierarchicalInstancedStaticMeshComponent* Comp,
		const bool bShadow,
		FName AtlasPrefix,
		FName DiffuseKey,
		FName NormalKey,
		EDungeoneerMaterialLayoutMode LayoutMode,
		const FLinearColor& TintColor,
		const float RoughnessVal)
	{
		if (Comp)
		{
			Comp->NumCustomDataFloats = EEcoCPD::Count;
			Comp->bCastHiddenShadow = bShadow;
			if (!Comp->GetStaticMesh() && BoxFallback)
			{
				Comp->SetStaticMesh(BoxFallback);
			}
			AssignHISMVisuals(Comp, AtlasPrefix, DiffuseKey, NormalKey, LayoutMode, TintColor, RoughnessVal);
		}
	};

	ConfigurePlaneHISM(FloorHISM.Get(),             FName(TEXT("ATLAS_Dungeon_Floors_4x4")),            FName(TEXT("Floor_Tile0_D")),    FName(TEXT("Floor_Tile0_N")),    EDungeoneerMaterialLayoutMode::LegacyDirectCD1, FLinearColor(0.85f, 0.88f, 0.96f), 0.72f);
	ConfigurePlaneHISM(WetFloorHISM.Get(),          FName(TEXT("ATLAS_Dungeon_Floors_4x4")),            FName(TEXT("Floor_Wet0_D")),     FName(TEXT("Floor_Wet0_N")),     EDungeoneerMaterialLayoutMode::LegacyDirectCD1, FLinearColor(0.55f, 0.80f, 0.96f), 0.03f);
	ConfigureBoxHISM(WallHISM.Get(),                true,  FName(TEXT("ATLAS_Dungeon_Floors_4x4")),            FName(TEXT("Floor_Tile0_D")),    FName(TEXT("Floor_Tile0_N")),    EDungeoneerMaterialLayoutMode::LegacyDualCell,  FLinearColor(0.42f, 0.48f, 0.64f), 0.68f);
	ConfigureBoxHISM(DiagonalWedgeHISM.Get(),       true,  FName(TEXT("ATLAS_Dungeon_Floors_4x4")),            FName(TEXT("Floor_Tile0_D")),    FName(TEXT("Floor_Tile0_N")),    EDungeoneerMaterialLayoutMode::LegacyDualCell,  FLinearColor(0.38f, 0.44f, 0.60f), 0.62f);
	ConfigureBoxHISM(MirrorHISM.Get(),              true,  FName(TEXT("PROP_Mirror_Palace_Maze_4x4")),         FName(TEXT("Mirror_Intact_D")),  FName(TEXT("Mirror_Intact_N")),  EDungeoneerMaterialLayoutMode::LegacyMirror,    FLinearColor(0.75f, 0.96f, 1.00f), 0.08f);
	ConfigurePlaneHISM(SconceHISM.Get(),              FName(TEXT("PROP_Doors_And_Level_Transitions_4x4")),FName(TEXT("Doors_Sconce_D")),   FName(TEXT("Doors_Sconce_N")),   EDungeoneerMaterialLayoutMode::LegacyDirectCD1, FLinearColor(0.95f, 0.90f, 0.85f), 0.62f);
	ConfigurePlaneHISM(BreakableHISM.Get(),                 FName(TEXT("PROP_Breakables_And_Traps_4x4")),       FName(TEXT("Breakable_Intact_D")), FName(TEXT("Breakable_Intact_N")), EDungeoneerMaterialLayoutMode::LegacyDualCell, FLinearColor(0.95f, 0.85f, 0.72f), 0.50f);
	ConfigureBoxHISM(ForegroundParallaxHISM.Get(),  false, FName(TEXT("PROP_Dungeon_Walls_And_Cutaways_4x4")), FName(TEXT("Wall_Chain_D")),     FName(TEXT("Wall_Chain_N")),     EDungeoneerMaterialLayoutMode::LegacyDirectCD1, FLinearColor(0.70f, 0.72f, 0.80f), 0.45f);

	if (Bootstrap)
	{
		bTexturedHISMApplied = true;
	}
}

void ADungeoneerTileMapInstancer::ClearAllInstances()
{
	if (FloorHISM)              { FloorHISM->ClearInstances(); }
	if (WetFloorHISM)           { WetFloorHISM->ClearInstances(); }
	if (WallHISM)               { WallHISM->ClearInstances(); }
	if (DiagonalWedgeHISM)      { DiagonalWedgeHISM->ClearInstances(); }
	if (MirrorHISM)             { MirrorHISM->ClearInstances(); }
	if (SconceHISM)             { SconceHISM->ClearInstances(); }
	if (BreakableHISM)          { BreakableHISM->ClearInstances(); }
	if (ForegroundParallaxHISM) { ForegroundParallaxHISM->ClearInstances(); }
	if (HISM_Water)             { HISM_Water->ClearInstances(); }
	if (ISM_WaterFX)            { ISM_WaterFX->ClearInstances(); }
	if (ISM_Fauna)              { ISM_Fauna->ClearInstances(); }

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
	TrapVentEntries.Reset();
	SconceWorldPositions.Reset();
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
	const bool bIsDiagonalAxis =
		(Axis == EDungeoneerWallAxis::NorthEast ||
		 Axis == EDungeoneerWallAxis::NorthWest ||
		 Axis == EDungeoneerWallAxis::SouthEast ||
		 Axis == EDungeoneerWallAxis::SouthWest);

	UHierarchicalInstancedStaticMeshComponent* TargetHISM = bIsDiagonalAxis && DiagonalWedgeHISM
		? DiagonalWedgeHISM.Get()
		: WallHISM.Get();
	if (!TargetHISM)
	{
		return;
	}

	const float YawDeg = TileMapDetail::WallAxisToYawDeg(Axis);
	const FVector MeshScale = FVector(
		FMath::Max(0.2f, HalfExtents.X * 2.0f / 100.0f),
		FMath::Max(0.2f, HalfExtents.Y * 2.0f / 100.0f),
		FMath::Max(0.2f, HalfExtents.Z * 2.0f / 100.0f));
	const FTransform InstanceTransform(FRotator(0.0f, YawDeg, 0.0f), WorldCenter, MeshScale);

	const int32 InstIdx = TargetHISM->AddInstance(InstanceTransform, true);
	if (InstIdx != INDEX_NONE)
	{
		TargetHISM->SetCustomDataValue(InstIdx, 0, 0.0f, false); // 0 = Full standing wall, 1 = Row 2 cutaway stub
		TargetHISM->SetCustomDataValue(InstIdx, 1, static_cast<float>(FullUVCell), false);
		TargetHISM->SetCustomDataValue(InstIdx, 2, static_cast<float>(CutawayUVCell), false);
		TargetHISM->SetCustomDataValue(InstIdx, 3, static_cast<float>(static_cast<uint8>(Axis)), false);

		// Note: WallEntries only records primary WallHISM instances when checking PerInstanceSMCustomData on WallHISM
		if (TargetHISM == WallHISM.Get())
		{
			FDungeoneerWallInstanceEntry Entry;
			Entry.HISMInstanceIndex = InstIdx;
			Entry.WallAxis = Axis;
			Entry.WorldCenter = WorldCenter;
			Entry.WorldHalfExtents = HalfExtents;
			Entry.FullUVCellIndex = FullUVCell;
			Entry.CutawayUVCellIndex = CutawayUVCell;
			Entry.bCanCutawayForCamera =
				(Axis == EDungeoneerWallAxis::South ||
				 Axis == EDungeoneerWallAxis::North ||
				 Axis == EDungeoneerWallAxis::West);
			Entry.bCurrentlyCutaway = false;

			WallEntries.Add(Entry);
			PendingCutawayFlags.Add(0);
		}
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

	FRotator SpriteRot = FRotator(60.0f, 180.0f, 0.0f);
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			if (const UDungeoneerAssetBootstrapSubsystem* Bootstrap = GI->GetSubsystem<UDungeoneerAssetBootstrapSubsystem>())
			{
				SpriteRot = Bootstrap->GetCameraFacingSpritePlaneRotation();
			}
		}
	}

	const FTransform InstanceTransform(SpriteRot, WorldCenter, FVector(0.88f, 0.88f, 1.0f));
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

int32 ADungeoneerTileMapInstancer::RegisterTrapVentInstance(
	const FVector& WorldCenter,
	const EDungeoneerTrapHazardType HazardType,
	const float TriggerRadiusUU,
	const float DamageAmount,
	const int32 UVCellIndex)
{
	if (!BreakableHISM)
	{
		return INDEX_NONE;
	}

	EnsureMeshAssetsInitialized();

	const FTransform VentTransform(FRotator::ZeroRotator, WorldCenter, FVector(1.35f, 1.35f, 0.14f));
	const int32 InstIdx = BreakableHISM->AddInstance(VentTransform, true);
	if (InstIdx != INDEX_NONE)
	{
		BreakableHISM->SetCustomDataValue(InstIdx, 0, 2.0f, true); // 2 = active floor hazard vent
		BreakableHISM->SetCustomDataValue(InstIdx, 1, static_cast<float>(UVCellIndex), true);
		BreakableHISM->SetCustomDataValue(InstIdx, 2, static_cast<float>(static_cast<uint8>(HazardType)), true);
		BreakableHISM->SetCustomDataValue(InstIdx, 3, 0.5f, true);

		FDungeoneerTrapVentEntry Vent;
		Vent.HISMInstanceIndex = InstIdx;
		Vent.HazardType = HazardType;
		Vent.WorldCenter = WorldCenter;
		Vent.TriggerRadiusUU = TriggerRadiusUU;
		Vent.DamageAmount = DamageAmount;
		Vent.CooldownSeconds = 1.6f;
		Vent.ActiveCooldown = 0.0f;
		Vent.FuseAccelerateSeconds = 1.25f;
		return TrapVentEntries.Add(Vent);
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
	const int32 FirstSpawnedIndex = SpawnedEncounterActors.Num();

	for (const FDungeonNode& Node : FloorGraph.Nodes)
	{
		const FVector Center = Node.WorldCenter;

		// Secret pockets reuse vault/gauntlet room types for their floor art, but their encounter is always
		// the secret-pocket one (a Stalker, a relic and a scroll). Never a second vault or boss.
		const EDungeonRoomType EncounterType = Node.bIsSecretRoom ? EDungeonRoomType::Combat : Node.RoomType;
		switch (EncounterType)
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
			if (Node.bIsSecretRoom)
			{
				if (AZombieMirrorStalker* Stalker = World->SpawnActor<AZombieMirrorStalker>(AZombieMirrorStalker::StaticClass(), Center + FVector(160.0, -140.0, 95.0), FRotator::ZeroRotator, SpawnParams))
				{
					SpawnedEncounterActors.Add(Stalker);
				}
				if (ADungeoneerLootPickupActor* SecretRelic = World->SpawnActor<ADungeoneerLootPickupActor>(ADungeoneerLootPickupActor::StaticClass(), Center + FVector(-140.0, 120.0, 0.0), FRotator::ZeroRotator, SpawnParams))
				{
					static constexpr EDungeoneerRelicId SecretPool[4] =
					{
						EDungeoneerRelicId::SunGlassLens,
						EDungeoneerRelicId::ChronoWardBoots,
						EDungeoneerRelicId::EchoTunedBlade,
						EDungeoneerRelicId::VampireWick
					};
					SecretRelic->ConfigurePickup(EDungeoneerLootDropKind::Relic, 0, EDungeoneerConsumableId::None, SecretPool[Node.RoomSeed & 3]);
					SecretRelic->MarkAsSecretFind();
					SpawnedEncounterActors.Add(SecretRelic);
				}
				// Secret pockets always hold a skill scroll.
				if (ADungeoneerLootPickupActor* SecretScroll = World->SpawnActor<ADungeoneerLootPickupActor>(ADungeoneerLootPickupActor::StaticClass(), Center + FVector(140.0, 120.0, 0.0), FRotator::ZeroRotator, SpawnParams))
				{
					SecretScroll->ConfigureSkillScroll(DungeoneerSkills::SkillFromRoll(static_cast<uint32>(Node.RoomSeed) >> 3));
					SpawnedEncounterActors.Add(SecretScroll);
				}
			}
			else if ((Node.RoomSeed & 1) == 0)
			{
				if (ADungeoneerLootPickupActor* Supply = World->SpawnActor<ADungeoneerLootPickupActor>(ADungeoneerLootPickupActor::StaticClass(), Center + FVector(-220.0, 180.0, 0.0), FRotator::ZeroRotator, SpawnParams))
				{
					const EDungeoneerConsumableId Kind = (Node.RoomSeed & 2) ? EDungeoneerConsumableId::AntidoteDraught : EDungeoneerConsumableId::LanternOilPhial;
					Supply->ConfigurePickup(EDungeoneerLootDropKind::Consumable, 0, Kind, EDungeoneerRelicId::None);
					SpawnedEncounterActors.Add(Supply);
				}
			}
			// About one combat room in three drops a skill scroll.
			if (!Node.bIsSecretRoom && ((static_cast<uint32>(Node.RoomSeed) >> 2) % 3u) == 0u)
			{
				if (ADungeoneerLootPickupActor* Scroll = World->SpawnActor<ADungeoneerLootPickupActor>(ADungeoneerLootPickupActor::StaticClass(), Center + FVector(220.0, -180.0, 0.0), FRotator::ZeroRotator, SpawnParams))
				{
					Scroll->ConfigureSkillScroll(DungeoneerSkills::SkillFromRoll(static_cast<uint32>(Node.RoomSeed) >> 5));
					SpawnedEncounterActors.Add(Scroll);
				}
			}
			break;
		}
		case EDungeonRoomType::MirrorPalaceMaze:
		{
			if (AZombieMirrorStalker* Stalker = World->SpawnActor<AZombieMirrorStalker>(AZombieMirrorStalker::StaticClass(), Center + FVector(190.0, 110.0, 95.0), FRotator::ZeroRotator, SpawnParams))
			{
				SpawnedEncounterActors.Add(Stalker);
			}
			// The Mirror Palace's own dead: a glass-setter guarding the maze with a mirror pane.
			if (AZombieGlazier* Glazier = World->SpawnActor<AZombieGlazier>(AZombieGlazier::StaticClass(), Center + FVector(-160.0, 140.0, 95.0), FRotator::ZeroRotator, SpawnParams))
			{
				SpawnedEncounterActors.Add(Glazier);
			}
			if (ADungeoneerLootPickupActor* MirrorGold = World->SpawnActor<ADungeoneerLootPickupActor>(ADungeoneerLootPickupActor::StaticClass(), Center + FVector(-180.0, -140.0, 0.0), FRotator::ZeroRotator, SpawnParams))
			{
				MirrorGold->ConfigurePickup(EDungeoneerLootDropKind::GoldPouch, 65, EDungeoneerConsumableId::None, EDungeoneerRelicId::None);
				SpawnedEncounterActors.Add(MirrorGold);
			}
			break;
		}
		case EDungeonRoomType::DeepFogGauntlet:
		{
			for (int32 i = 0; i < 2; ++i)
			{
				const double OffsetY = (i == 0) ? -320.0 : 320.0;
				if (i == 0 && FloorGraph.FloorIndex >= 2)
				{
					// From the second floor a Glazier holds the line in the gauntlet.
					if (AZombieGlazier* Glazier = World->SpawnActor<AZombieGlazier>(AZombieGlazier::StaticClass(), Center + FVector(180.0, OffsetY, 95.0), FRotator::ZeroRotator, SpawnParams))
					{
						SpawnedEncounterActors.Add(Glazier);
					}
				}
				else if (AZombieExploder* Exploder = World->SpawnActor<AZombieExploder>(AZombieExploder::StaticClass(), Center + FVector(180.0, OffsetY, 95.0), FRotator::ZeroRotator, SpawnParams))
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
			AMajorLootVaultActor* Vault = World->SpawnActor<AMajorLootVaultActor>(AMajorLootVaultActor::StaticClass(), Center, FRotator::ZeroRotator, SpawnParams);
			if (Vault)
			{
				SpawnedEncounterActors.Add(Vault);
			}
			if (FloorGraph.FloorIndex >= 3 && (FloorGraph.FloorIndex % 3) == 0)
			{
				if (ABossGuardianActor* Boss = World->SpawnActor<ABossGuardianActor>(ABossGuardianActor::StaticClass(), Center + FVector(140.0, 0.0, 95.0), FRotator::ZeroRotator, SpawnParams))
				{
					Boss->SetLinkedVault(Vault);
					SpawnedEncounterActors.Add(Boss);
				}
			}
			break;
		}
		default:
			break;
		}
	}

	// Deeper floors are tougher, and about one regular enemy in six is an elite (seeded, so a floor
	// always rolls the same elites). The Colossus has its own tuning.
	uint32 EliteRoll = static_cast<uint32>(FloorGraph.FloorIndex) * 2654435761u + 0x9E37u;
	for (int32 i = FirstSpawnedIndex; i < SpawnedEncounterActors.Num(); ++i)
	{
		AZombieBase* Zombie = Cast<AZombieBase>(SpawnedEncounterActors[i].Get());
		if (!Zombie || Zombie->IsA<ABossGuardianActor>())
		{
			continue;
		}
		Zombie->ApplyFloorScaling(FloorGraph.FloorIndex);
		EliteRoll = EliteRoll * 1664525u + 1013904223u;
		if (((EliteRoll >> 16) % 6u) == 0u)
		{
			Zombie->MakeElite();
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
	BreakableEntries.Reserve(EstimatedRooms * 6);
	TrapVentEntries.Reserve(EstimatedRooms * 4);
	SconceWorldPositions.Reserve(EstimatedRooms * 6);

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
	TArray<FVector> PuddleLocations;
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

	FRotator DoorSpriteFacingRot(48.0f, -45.0f, 0.0f);
	if (const UWorld* WorldPtr = GetWorld())
	{
		if (const UGameInstance* GI = WorldPtr->GetGameInstance())
		{
			if (const UDungeoneerAssetBootstrapSubsystem* Bootstrap = GI->GetSubsystem<UDungeoneerAssetBootstrapSubsystem>())
			{
				DoorSpriteFacingRot = Bootstrap->GetCameraFacingSpritePlaneRotation();
			}
		}
	}

	auto RecordSconceNearDoor = [this, &DoorSpriteFacingRot](const FVector2D& Door2D, const float BaseZ, const float LateralOffsetY)
	{
		const FVector SconcePos(Door2D.X, Door2D.Y + LateralOffsetY, BaseZ + WallHeightUU * 0.56f);
		if (SconceHISM)
		{
			// Upright stone gothic archway framing East/West corridor threshold (Cell 0 on PROP_Doors_And_Level_Transitions_4x4)
			const FVector ArchPos(Door2D.X, Door2D.Y, BaseZ + 115.0f);
			const int32 ArchIdx = SconceHISM->AddInstance(FTransform(DoorSpriteFacingRot, ArchPos, FVector(2.15f, 2.15f, 1.0f)), false);
			if (ArchIdx != INDEX_NONE)
			{
				SconceHISM->SetCustomDataValue(ArchIdx, 0, 0.0f, false);
				SconceHISM->SetCustomDataValue(ArchIdx, 1, 0.0f, false); // R0C0: Gothic Stone Archway
			}

			// Upright wall torch/lantern sconce (Row 3: Cell 12 Torch / Cell 14 Lantern)
			const int32 SconceCell = (FMath::RoundToInt(FMath::Abs(Door2D.Y)) % 2 == 0) ? 12 : 14;
			const int32 SconceIdx = SconceHISM->AddInstance(FTransform(DoorSpriteFacingRot, SconcePos, FVector(0.82f, 0.82f, 1.0f)), false);
			if (SconceIdx != INDEX_NONE)
			{
				SconceHISM->SetCustomDataValue(SconceIdx, 0, 1.0f, false);
				SconceHISM->SetCustomDataValue(SconceIdx, 1, static_cast<float>(SconceCell), false);
			}
		}
		SconceWorldPositions.Add(SconcePos);
	};

	for (const FDungeonNode& NodeA : FloorGraph.Nodes)
	{
		const int32* LayoutAIdx = NodeIdToLayoutIdx.Find(NodeA.NodeId);
		if (!LayoutAIdx)
		{
			continue;
		}

		// Handle secret Phantasm pockets attached along North (+Y) or South (-Y) parent seam
		if (NodeA.bIsSecretRoom && NodeA.ParentRoomNodeId != INDEX_NONE)
		{
			if (const int32* ParentIdx = NodeIdToLayoutIdx.Find(NodeA.ParentRoomNodeId))
			{
				TileMapDetail::FComputedRoomLayout& SecretLay = RoomLayouts[*LayoutAIdx];
				TileMapDetail::FComputedRoomLayout& ParentLay = RoomLayouts[*ParentIdx];
				const bool bAttachedSouth = (SecretLay.RoomCenter.Y < ParentLay.RoomCenter.Y);
				const double SeamY = bAttachedSouth ? ParentLay.OuterBox.Min.Y : ParentLay.OuterBox.Max.Y;
				const FVector2D SecretSeam2D(SecretLay.RoomCenter.X, SeamY);
				if (bAttachedSouth)
				{
					ParentLay.SouthDoorPositions.Add(SecretSeam2D);
					SecretLay.NorthDoorPositions.Add(SecretSeam2D);
				}
				else
				{
					ParentLay.NorthDoorPositions.Add(SecretSeam2D);
					SecretLay.SouthDoorPositions.Add(SecretSeam2D);
				}
				AllDoorways2D.Add(SecretSeam2D);
				AddCorridorStripXY(SecretSeam2D, SecretSeam2D);

				// Hide the secret threshold behind an illusory AMirrorWall_Phantasm seam
				const FVector PhantasmNormal = bAttachedSouth ? FVector(0.0, 1.0, 0.0) : FVector(0.0, -1.0, 0.0);
				RegisterMirrorInstance(
					FVector(SecretSeam2D.X, SecretSeam2D.Y, WallHeightUU * 0.44f),
					PhantasmNormal,
					128.0f,
					WallHeightUU * 0.45f,
					true,
					4);
			}
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
				RecordSconceNearDoor(EastDoor, LayA.RoomCenter.Z, -135.0f);
				RecordSconceNearDoor(WestDoor, LayB.RoomCenter.Z, 135.0f);

				const double MidX = 0.5 * (EastDoor.X + WestDoor.X);
				AddCorridorStripXY(EastDoor, FVector2D(MidX, EastDoor.Y));
				AddCorridorStripXY(FVector2D(MidX, EastDoor.Y), FVector2D(MidX, WestDoor.Y));
				AddCorridorStripXY(FVector2D(MidX, WestDoor.Y), WestDoor);
			}
			else
			{
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
				RecordSconceNearDoor(EastDoor, LayA.RoomCenter.Z, -135.0f);
				AddCorridorStripXY(EastDoor, WestDoor);
			}
		}
	}

	auto SelectBiomeFloorUV = [](const EDungeonBiomeType Biome, FRandomStream& Rand) noexcept -> int32
	{
		switch (Biome)
		{
		case EDungeonBiomeType::MirrorPalace: return 12 + Rand.RandRange(0, 3);
		case EDungeonBiomeType::SporeFens:    return 8 + Rand.RandRange(0, 3);
		case EDungeonBiomeType::EmberForge:   return 12 + Rand.RandRange(0, 1);
		case EDungeonBiomeType::Crypt:
		default:                              return Rand.RandRange(0, 3);
		}
	};

	auto SelectBiomeHazard = [](const EDungeonBiomeType Biome, const EDungeonRoomType RoomType) noexcept -> EDungeoneerTrapHazardType
	{
		if (RoomType == EDungeonRoomType::MirrorPalaceMaze)
		{
			return EDungeoneerTrapHazardType::AcousticChime;
		}
		switch (Biome)
		{
		case EDungeonBiomeType::SporeFens:  return EDungeoneerTrapHazardType::PoisonSpore;
		case EDungeonBiomeType::EmberForge: return EDungeoneerTrapHazardType::EmberFire;
		case EDungeonBiomeType::Crypt:
		default:                            return EDungeoneerTrapHazardType::Steam;
		}
	};

	// 2. Build Rooms with Carved Doorways, Biome Atlases, Shapes, Puddles, Pillars & Traps
	const double DoorCarveRadius = static_cast<double>(TileSizeUU) * 0.65;
	for (int32 NodeIdx = 0; NodeIdx < FloorGraph.Nodes.Num(); ++NodeIdx)
	{
		const FDungeonNode& Node = FloorGraph.Nodes[NodeIdx];
		const TileMapDetail::FComputedRoomLayout& Lay = RoomLayouts[NodeIdx];

		FRandomStream RoomRand(Node.RoomSeed);
		const int32 NumTilesX = Lay.NumTilesX;
		const int32 NumTilesY = Lay.NumTilesY;
		const FVector RoomOrigin = Node.WorldCenter;

		// 2a. Room Floor Grid & Wet Specular Puddle Overlays (`FloorHISM` + `WetFloorHISM`)
		for (int32 iy = 0; iy < NumTilesY; ++iy)
		{
			for (int32 ix = 0; ix < NumTilesX; ++ix)
			{
				const float LocalX = (static_cast<float>(ix) - 0.5f * static_cast<float>(NumTilesX - 1)) * TileSizeUU;
				const float LocalY = (static_cast<float>(iy) - 0.5f * static_cast<float>(NumTilesY - 1)) * TileSizeUU;
				const FVector TileCenter = RoomOrigin + FVector(LocalX, LocalY, 0.0f);
				const FVector FloorScale(TileSizeUU / 100.0f, TileSizeUU / 100.0f, 1.0f);

				// Transition Bleed (~25% on the East side when BleedBiomeType differs from primary)
				const bool bInBleedZone = (Node.BleedBiomeType != Node.BiomeType) && (ix >= (NumTilesX * 3) / 4);
				const EDungeonBiomeType TileBiome = bInBleedZone ? Node.BleedBiomeType : Node.BiomeType;

				if (FloorHISM)
				{
					const int32 FloorIdx = FloorHISM->AddInstance(FTransform(FRotator::ZeroRotator, TileCenter, FloorScale), false);
					if (FloorIdx != INDEX_NONE)
					{
						int32 FloorUV = SelectBiomeFloorUV(TileBiome, RoomRand);
						if (Node.RoomType == EDungeonRoomType::MirrorPalaceMaze)
						{
							FloorUV = 15;
						}
						FloorHISM->SetCustomDataValue(FloorIdx, 0, static_cast<float>(Node.NodeId), false);
						FloorHISM->SetCustomDataValue(FloorIdx, 1, static_cast<float>(FloorUV), false);
					}
				}

				// Deterministic wet specular puddle clusters in Crypt / SporeFens rooms
				const bool bPlaceWetPuddle = (TileBiome == EDungeonBiomeType::Crypt || TileBiome == EDungeonBiomeType::SporeFens)
					&& Node.RoomType != EDungeonRoomType::RestingGround
					&& ix > 0 && ix < (NumTilesX - 1) && iy > 0 && iy < (NumTilesY - 1)
					&& (((ix * 7 + iy * 13 + Node.NodeId) % 9) == 0);
				if (bPlaceWetPuddle && WetFloorHISM)
				{
					const int32 WetIdx = WetFloorHISM->AddInstance(
						FTransform(FRotator::ZeroRotator, TileCenter + FVector(0.0, 0.0, 1.25), FloorScale * 0.85f),
						false);
					if (WetIdx != INDEX_NONE)
					{
						WetFloorHISM->SetCustomDataValue(WetIdx, 0, static_cast<float>(Node.NodeId), false);
						WetFloorHISM->SetCustomDataValue(WetIdx, 1, static_cast<float>(4 + RoomRand.RandRange(0, 3)), false);
					}
					PuddleLocations.Add(TileCenter);
				}
			}
		}

		// 2b. Room Perimeter 4-Axis Walls with Carved N/S/E/W Doorways + Secret Seams
		const float EdgeOffsetX = Lay.EdgeOffsetX;
		const float EdgeOffsetY = Lay.EdgeOffsetY;
		const float WallZ = RoomOrigin.Z + WallHeightUU * 0.5f;
		const float SegSpan = TileSizeUU * 0.45f;
		const FVector NSExtents(SegSpan, 34.0f, WallHeightUU * 0.5f);
		const FVector EWExtents(34.0f, SegSpan, WallHeightUU * 0.5f);

		for (int32 ix = 0; ix < NumTilesX; ++ix)
		{
			const float WX = RoomOrigin.X + (static_cast<float>(ix) - 0.5f * static_cast<float>(NumTilesX - 1)) * TileSizeUU;
			const double NorthWY = RoomOrigin.Y + EdgeOffsetY;
			const double SouthWY = RoomOrigin.Y - EdgeOffsetY;

			if (!TileMapDetail::IsNearAnyDoorway(Lay.NorthDoorPositions, WX, NorthWY, DoorCarveRadius))
			{
				AddWallInstance(FVector(WX, NorthWY, WallZ), EDungeoneerWallAxis::North, NSExtents, ix % 4, 4 + (ix % 4));
			}
			if (!TileMapDetail::IsNearAnyDoorway(Lay.SouthDoorPositions, WX, SouthWY, DoorCarveRadius))
			{
				AddWallInstance(FVector(WX, SouthWY, WallZ), EDungeoneerWallAxis::South, NSExtents, ix % 4, 4 + (ix % 4));
			}
		}

		for (int32 iy = 0; iy < NumTilesY; ++iy)
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

		// 45-degree Diagonal Corner Wedges (`DiagonalWedgeHISM`) — deeper bevel on OctagonalRotunda
		const bool bOctagonal = (Node.RoomShape == EDungeonRoomShape::OctagonalRotunda);
		const float CornerInsetRatio = bOctagonal ? 0.52f : 0.25f;
		const float DiagOffsetX = EdgeOffsetX - TileSizeUU * CornerInsetRatio;
		const float DiagOffsetY = EdgeOffsetY - TileSizeUU * CornerInsetRatio;
		const FVector DiagExtents(bOctagonal ? TileSizeUU * 0.62f : SegSpan, 34.0f, WallHeightUU * 0.5f);
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

		// 2b-bis. Shape-specific Interior Pillars & Overhead Foreground Parallax Beams/Chains
		if (Node.RoomShape == EDungeonRoomShape::PillaredGallery || Node.RoomShape == EDungeonRoomShape::CrossAmbushVault)
		{
			const float PillarDist = TileSizeUU * 1.45f;
			const FVector PillarExtents(44.0f, 44.0f, WallHeightUU * 0.52f);
			AddWallInstance(RoomOrigin + FVector(-PillarDist, -PillarDist, WallZ), EDungeoneerWallAxis::North, PillarExtents, 12, 4);
			AddWallInstance(RoomOrigin + FVector( PillarDist, -PillarDist, WallZ), EDungeoneerWallAxis::North, PillarExtents, 12, 4);
			AddWallInstance(RoomOrigin + FVector(-PillarDist,  PillarDist, WallZ), EDungeoneerWallAxis::North, PillarExtents, 12, 4);
			AddWallInstance(RoomOrigin + FVector( PillarDist,  PillarDist, WallZ), EDungeoneerWallAxis::North, PillarExtents, 12, 4);

			if (ForegroundParallaxHISM)
			{
				const FVector BeamPos = RoomOrigin + FVector(0.0f, 0.0f, 260.0f);
				const int32 BeamIdx = ForegroundParallaxHISM->AddInstance(FTransform(FRotator::ZeroRotator, BeamPos, FVector(3.8f, 0.28f, 0.28f)), false);
				if (BeamIdx != INDEX_NONE)
				{
					ForegroundParallaxHISM->SetCustomDataValue(BeamIdx, 0, static_cast<float>(Node.NodeId), false);
					ForegroundParallaxHISM->SetCustomDataValue(BeamIdx, 1, 12.0f, false);
				}
			}
		}

		// Sanctuary Brazier / Descending Stairs Visual Landmarks (`SconceHISM`)
		if (Node.RoomType == EDungeonRoomType::RestingGround && SconceHISM)
		{
			const int32 BrazierIdx = SconceHISM->AddInstance(FTransform(DoorSpriteFacingRot, RoomOrigin + FVector(0.0f, 0.0f, 75.0f), FVector(1.35f, 1.35f, 1.0f)), false);
			if (BrazierIdx != INDEX_NONE)
			{
				SconceHISM->SetCustomDataValue(BrazierIdx, 0, 1.0f, false);
				SconceHISM->SetCustomDataValue(BrazierIdx, 1, 13.0f, false); // R3C1 Stone Brazier
			}
		}
		else if (Node.bHasDescendingStairs && SconceHISM)
		{
			const FVector StairPos = RoomOrigin + FVector(EdgeOffsetX * 0.45f, 0.0f, 88.0f);
			const int32 StairIdx = SconceHISM->AddInstance(FTransform(DoorSpriteFacingRot, StairPos, FVector(1.85f, 1.85f, 1.0f)), false);
			if (StairIdx != INDEX_NONE)
			{
				SconceHISM->SetCustomDataValue(StairIdx, 0, 2.0f, false); // 2 = Descending stairwell
				SconceHISM->SetCustomDataValue(StairIdx, 1, 8.0f, false);  // R2C0 Stone Stairs
			}
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

		// 2d. Breakable Acoustic Props (loud 1.95f lures zombies + Acute Hearing ripples) & Systemic Biome Trap Vents
		const FVector BreakOffset = (Node.RoomType == EDungeonRoomType::Entry)
			? FVector(340.0f, 320.0f, 45.0f)
			: FVector(RoomRand.FRandRange(-160.0f, 160.0f), RoomRand.FRandRange(-140.0f, 140.0f), 45.0f);
		const int32 IntactCell = RoomRand.RandRange(0, 1);
		RegisterBreakableInstance(RoomOrigin + BreakOffset, IntactCell, IntactCell + 2, 85.0f, 1.95f);

		if (Node.RoomType == EDungeonRoomType::Combat || Node.RoomType == EDungeonRoomType::DeepFogGauntlet || Node.RoomType == EDungeonRoomType::MirrorPalaceMaze)
		{
			const EDungeoneerTrapHazardType PrimaryTrap = SelectBiomeHazard(Node.BiomeType, Node.RoomType);
			const FVector VentPos = RoomOrigin + FVector(RoomRand.FRandRange(-240.0f, 240.0f), RoomRand.FRandRange(-220.0f, 220.0f), 2.5f);
			RegisterTrapVentInstance(VentPos, PrimaryTrap, 120.0f, 18.0f, 8 + (static_cast<int32>(PrimaryTrap) % 4));

			// Transition bleed secondary hazard grate on final nodes
			if (Node.BleedBiomeType != Node.BiomeType)
			{
				const EDungeoneerTrapHazardType BleedTrap = SelectBiomeHazard(Node.BleedBiomeType, Node.RoomType);
				const FVector BleedVentPos = RoomOrigin + FVector(EdgeOffsetX * 0.45f, RoomRand.FRandRange(-180.0f, 180.0f), 2.5f);
				RegisterTrapVentInstance(BleedVentPos, BleedTrap, 120.0f, 20.0f, 12 + (static_cast<int32>(BleedTrap) % 4));
			}
		}
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

	// 5. Register 4-Tier Living Ecosystem Light Candidates, Hydrology & Interactive Flora
	UDungeonLightBudgetSubsystem* LightBudget = World ? World->GetSubsystem<UDungeonLightBudgetSubsystem>() : nullptr;
	if (LightBudget)
	{
		LightBudget->ClearCandidates();

		for (const FVector& SconcePos : SconceWorldPositions)
		{
			FDungeonLightCandidate C;
			C.WorldPosition = SconcePos;
			C.Color = EcoPalette::AmberFire;
			C.RadiusUU = 360.0f;
			C.BaseIntensity = 360.0f;
			C.RequestedTier = EDungeonLightTier::Tier2_Pooled;
			LightBudget->RegisterCandidate(C);
		}

		for (const FDungeonNode& Node : FloorGraph.Nodes)
		{
			if (Node.RoomType == EDungeonRoomType::RestingGround)
			{
				FDungeonLightCandidate C;
				C.WorldPosition = Node.WorldCenter + FVector(0.0, 0.0, 95.0);
				C.Color = EcoPalette::AmberFire;
				C.RadiusUU = 720.0f;
				C.BaseIntensity = 260.0f;
				C.PriorityBoost = 2.0f;
				C.RequestedTier = EDungeonLightTier::Tier3_Hero;
				LightBudget->RegisterCandidate(C);
			}
			else if (Node.RoomType == EDungeonRoomType::MajorLootVault)
			{
				FDungeonLightCandidate C;
				C.WorldPosition = Node.WorldCenter + FVector(0.0, 0.0, 110.0);
				C.Color = EcoPalette::GoldenRune;
				C.RadiusUU = 680.0f;
				C.BaseIntensity = 240.0f;
				C.PriorityBoost = 1.5f;
				C.RequestedTier = EDungeonLightTier::Tier3_Hero;
				LightBudget->RegisterCandidate(C);
			}
			else if (Node.RoomType == EDungeonRoomType::Vendor)
			{
				FDungeonLightCandidate C;
				C.WorldPosition = Node.WorldCenter + FVector(0.0, 0.0, 60.0);
				C.Color = EcoPalette::GoldenRune;
				C.RadiusUU = 155.0f;
				C.BaseIntensity = 380.0f;
				C.RequestedTier = EDungeonLightTier::Tier1_MPCGlow;
				LightBudget->RegisterCandidate(C);
			}
		}

		for (const FDungeoneerTrapVentEntry& Vent : TrapVentEntries)
		{
			FDungeonLightCandidate C;
			C.WorldPosition = Vent.WorldCenter + FVector(0.0, 0.0, 24.0);
			C.Color = (Vent.HazardType == EDungeoneerTrapHazardType::EmberFire)
				? EcoPalette::MagmaOrange
				: (Vent.HazardType == EDungeoneerTrapHazardType::PoisonSpore ? EcoPalette::EmeraldMoss : EcoPalette::AstralCyan);
			C.RadiusUU = 135.0f;
			C.BaseIntensity = 320.0f;
			C.RequestedTier = EDungeonLightTier::Tier1_MPCGlow;
			LightBudget->RegisterCandidate(C);
		}

		for (int32 m = 0; m < MirrorEntries.Num(); m += 2)
		{
			FDungeonLightCandidate C;
			C.WorldPosition = MirrorEntries[m].WorldCenter;
			C.Color = EcoPalette::AstralCyan;
			C.RadiusUU = 130.0f;
			C.BaseIntensity = 290.0f;
			C.RequestedTier = EDungeonLightTier::Tier1_MPCGlow;
			LightBudget->RegisterCandidate(C);
		}
	}

	if (UDungeonWaterSubsystem* WaterSub = World ? World->GetSubsystem<UDungeonWaterSubsystem>() : nullptr)
	{
		FWaterFloorBuildInput WaterInput;
		WaterInput.FloorGraph = &FloorGraph;
		WaterInput.WallEntries = &WallEntries;
		WaterInput.PuddleLocations = &PuddleLocations;
		WaterInput.WaterHISM = HISM_Water.Get();
		WaterInput.WaterFxISM = ISM_WaterFX.Get();
		WaterInput.TileSizeUU = TileSizeUU;
		WaterInput.WallHeightUU = WallHeightUU;
		WaterSub->OnFloorBuilt(WaterInput);
	}

	if (FloraManager)
	{
		FloraManager->PopulateForFloor(FloorGraph, &MirrorEntries, TileSizeUU);
	}

	if (UDungeonFaunaSubsystem* FaunaSub = World ? World->GetSubsystem<UDungeonFaunaSubsystem>() : nullptr)
	{
		FFaunaFloorBuildInput FaunaInput;
		FaunaInput.FloorGraph = &FloorGraph;
		FaunaInput.WallEntries = &WallEntries;
		FaunaInput.PuddleLocations = &PuddleLocations;
		FaunaInput.FaunaISM = ISM_Fauna.Get();
		FaunaInput.TileSizeUU = TileSizeUU;
		FaunaSub->OnFloorBuilt(FaunaInput);
	}

	if (UDungeonAcousticSubsystem* AcousticSub = World ? World->GetSubsystem<UDungeonAcousticSubsystem>() : nullptr)
	{
		AcousticSub->OnFloorBuilt(this, FloorGraph.DominantBiome, FloorGraph.MasterSeed, &FloorGraph);
	}

	if (FloorHISM)              { FloorHISM->MarkRenderStateDirty(); }
	if (FloorHISM)              { FloorHISM->BuildTreeIfOutdated(false, true); FloorHISM->UpdateBounds(); FloorHISM->MarkRenderStateDirty(); }
	if (WetFloorHISM)           { WetFloorHISM->BuildTreeIfOutdated(false, true); WetFloorHISM->UpdateBounds(); WetFloorHISM->MarkRenderStateDirty(); }
	if (WallHISM)               { WallHISM->BuildTreeIfOutdated(false, true); WallHISM->UpdateBounds(); WallHISM->MarkRenderStateDirty(); }
	if (DiagonalWedgeHISM)      { DiagonalWedgeHISM->BuildTreeIfOutdated(false, true); DiagonalWedgeHISM->UpdateBounds(); DiagonalWedgeHISM->MarkRenderStateDirty(); }
	if (MirrorHISM)             { MirrorHISM->BuildTreeIfOutdated(false, true); MirrorHISM->UpdateBounds(); MirrorHISM->MarkRenderStateDirty(); }
	if (SconceHISM)             { SconceHISM->BuildTreeIfOutdated(false, true); SconceHISM->UpdateBounds(); SconceHISM->MarkRenderStateDirty(); }
	if (BreakableHISM)          { BreakableHISM->BuildTreeIfOutdated(false, true); BreakableHISM->UpdateBounds(); BreakableHISM->MarkRenderStateDirty(); }
	if (ForegroundParallaxHISM) { ForegroundParallaxHISM->BuildTreeIfOutdated(false, true); ForegroundParallaxHISM->UpdateBounds(); ForegroundParallaxHISM->MarkRenderStateDirty(); }
	if (HISM_Water)             { HISM_Water->BuildTreeIfOutdated(false, true); HISM_Water->UpdateBounds(); HISM_Water->MarkRenderStateDirty(); }

	if (LightBudget)
	{
		LightBudget->OnFloorBuilt(this);
		LightBudget->EvaluateLightBudgetNow(0.25f);
	}

	UE_LOG(LogTemp, Display,
		TEXT("[TileMap] Floor=%d Wet=%d Wall=%d Sconce=%d Water=%d"),
		FloorHISM ? FloorHISM->GetInstanceCount() : -1,
		WetFloorHISM ? WetFloorHISM->GetInstanceCount() : -1,
		WallHISM ? WallHISM->GetInstanceCount() : -1,
		SconceHISM ? SconceHISM->GetInstanceCount() : -1,
		HISM_Water ? HISM_Water->GetInstanceCount() : -1);
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
				SensorySub->BroadcastNoiseEvent(Entry.WorldCenter, Loudness, 1350.0f, false);
			}

			++ShatteredThisCall;
		}
	}

	return ShatteredThisCall;
}

int32 ADungeoneerTileMapInstancer::EvaluateTrapVentsForEntities(const float DeltaSeconds)
{
	int32 TriggeredCount = 0;
	UWorld* World = GetWorld();
	if (!World || TrapVentEntries.Num() == 0)
	{
		return 0;
	}

	UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>();
	UDungeoneerSensoryWorldSubsystem* SensorySub = World->GetSubsystem<UDungeoneerSensoryWorldSubsystem>();
	if (!SpatialSys)
	{
		return 0;
	}

	const int32 PlayerCount = SpatialSys->GetTrackedPlayerCount();
	const int32 ZombieCount = SpatialSys->GetTrackedZombieCount();

	for (FDungeoneerTrapVentEntry& Vent : TrapVentEntries)
	{
		if (Vent.ActiveCooldown > 0.0f)
		{
			Vent.ActiveCooldown = FMath::Max(0.0f, Vent.ActiveCooldown - DeltaSeconds);
			if (Vent.ActiveCooldown <= 0.0f && BreakableHISM && Vent.HISMInstanceIndex != INDEX_NONE)
			{
				BreakableHISM->SetCustomDataValue(Vent.HISMInstanceIndex, 3, 0.5f, false);
			}
			continue;
		}

		const double RadiusSq = static_cast<double>(Vent.TriggerRadiusUU) * static_cast<double>(Vent.TriggerRadiusUU);
		bool bTriggeredThisVent = false;

		for (int32 p = 0; p < PlayerCount; ++p)
		{
			if (ADungeoneerCharacter* Player = SpatialSys->GetTrackedPlayerAt(p))
			{
				const FVector PLoc = Player->GetActorLocation();
				if (FVector::DistSquared2D(PLoc, Vent.WorldCenter) <= RadiusSq && FMath::Abs(PLoc.Z - Vent.WorldCenter.Z) <= 200.0)
				{
					if (Vent.HazardType == EDungeoneerTrapHazardType::AcousticChime)
					{
						if (SensorySub)
						{
							SensorySub->BroadcastNoiseEvent(Vent.WorldCenter, 2.35f, 1500.0f, false);
						}
					}
					else
					{
						Player->ApplyDungeoneerDamage(Vent.DamageAmount, this);
					}
					bTriggeredThisVent = true;
				}
			}
		}

		for (int32 z = 0; z < ZombieCount; ++z)
		{
			if (AZombieBase* Zombie = SpatialSys->GetTrackedZombieAt(z))
			{
				if (!Zombie->IsZombieAlive())
				{
					continue;
				}
				const FVector ZLoc = Zombie->GetActorLocation();
				if (FVector::DistSquared2D(ZLoc, Vent.WorldCenter) <= RadiusSq && FMath::Abs(ZLoc.Z - Vent.WorldCenter.Z) <= 200.0)
				{
					if (Vent.HazardType == EDungeoneerTrapHazardType::AcousticChime)
					{
						if (SensorySub)
						{
							SensorySub->BroadcastNoiseEvent(Vent.WorldCenter, 2.35f, 1500.0f, false);
						}
					}
					else
					{
						Zombie->ApplyZombieDamage(Vent.DamageAmount, this);
						Zombie->TriggerHitFeedback((ZLoc - Vent.WorldCenter).GetSafeNormal2D(), 90.0f);
						if (Vent.HazardType == EDungeoneerTrapHazardType::EmberFire)
						{
							if (AZombieExploder* Exploder = Cast<AZombieExploder>(Zombie))
							{
								Exploder->IgniteOrAccelerateFuse(Vent.FuseAccelerateSeconds);
							}
						}
					}
					bTriggeredThisVent = true;
				}
			}
		}

		if (bTriggeredThisVent)
		{
			Vent.ActiveCooldown = Vent.CooldownSeconds;
			if (Vent.HazardType == EDungeoneerTrapHazardType::EmberFire)
			{
				EvaluateEntityStepOnBreakables(Vent.WorldCenter, 2.0f, 175.0f);
				SpatialSys->ApplyRadialDetonationDamage(Vent.WorldCenter, 200.0f, 12.0f, this);
			}
			if (BreakableHISM && Vent.HISMInstanceIndex != INDEX_NONE)
			{
				BreakableHISM->SetCustomDataValue(Vent.HISMInstanceIndex, 3, 2.0f, true);
			}
			++TriggeredCount;
		}
	}

	return TriggeredCount;
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

	TrapPollAccumulator += DeltaSeconds;
	if (TrapPollAccumulator >= 0.15f)
	{
		EvaluateTrapVentsForEntities(TrapPollAccumulator);
		TrapPollAccumulator = 0.0f;

		if (const UWorld* World = GetWorld())
		{
			if (const UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
			{
				const int32 PlayerCount = SpatialSys->GetTrackedPlayerCount();
				for (int32 e = 0; e < SpawnedEncounterActors.Num(); ++e)
				{
					ADungeoneerLootPickupActor* Pickup = Cast<ADungeoneerLootPickupActor>(SpawnedEncounterActors[e].Get());
					if (!Pickup || Pickup->IsCollected())
					{
						continue;
					}
					const FVector PickupLoc = Pickup->GetActorLocation();
					for (int32 p = 0; p < PlayerCount; ++p)
					{
						if (ADungeoneerCharacter* Player = SpatialSys->GetTrackedPlayerAt(p))
						{
							if (FVector::DistSquared2D(Player->GetActorLocation(), PickupLoc) <= FMath::Square(95.0))
							{
								Pickup->CollectByPlayer(Player);
								break;
							}
						}
					}
				}
			}
		}
	}
}
