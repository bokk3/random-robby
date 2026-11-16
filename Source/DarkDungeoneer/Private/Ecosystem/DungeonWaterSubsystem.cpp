#include "Ecosystem/DungeonWaterSubsystem.h"
#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "Dungeon/DungeoneerTileMapInstancer.h"
#include "Lighting/DungeonLightBudgetSubsystem.h"
#include "Rendering/DungeoneerEcoMaterialFactory.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace WaterDetail
{
	inline constexpr float DripGravityUU = 980.0f;
	inline constexpr float HydrologyTickIntervalSec = 1.0f / 30.0f;

	inline float OctantToYawDeg(ECompassOctant Oct) noexcept
	{
		switch (Oct)
		{
		case ECompassOctant::E:  return 0.0f;
		case ECompassOctant::SE: return 45.0f;
		case ECompassOctant::S:  return 90.0f;
		case ECompassOctant::SW: return 135.0f;
		case ECompassOctant::W:  return 180.0f;
		case ECompassOctant::NW: return -135.0f;
		case ECompassOctant::N:  return -90.0f;
		case ECompassOctant::NE: return -45.0f;
		default:                 return 0.0f;
		}
	}
}

UDungeonWaterSubsystem::UDungeonWaterSubsystem()
{
}

void UDungeonWaterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UDungeonLightBudgetSubsystem>();
	Collection.InitializeDependency<UDungeoneerSensoryWorldSubsystem>();
}

void UDungeonWaterSubsystem::Deinitialize()
{
	SurfaceGrid.Reset();
	WaterAudioEmitters.Reset();
	GridWidth = 0;
	GridHeight = 0;
	Super::Deinitialize();
}

float UDungeonWaterSubsystem::ResolveWrappedEcoTime() const noexcept
{
	if (const UWorld* World = GetWorld())
	{
		return EcoColors::WrapEcoTime(World->GetTimeSeconds());
	}
	return EcoColors::WrapEcoTime(PendingEcoTimeFallback);
}

int32 UDungeonWaterSubsystem::WorldToCellIndex(const FVector& WorldLocation) const noexcept
{
	if (GridWidth <= 0 || GridHeight <= 0)
	{
		return INDEX_NONE;
	}
	const int32 GX = FMath::Clamp(FMath::FloorToInt((static_cast<float>(WorldLocation.X) - static_cast<float>(GridOriginXY.X)) / GridCellSizeUU), 0, GridWidth - 1);
	const int32 GY = FMath::Clamp(FMath::FloorToInt((static_cast<float>(WorldLocation.Y) - static_cast<float>(GridOriginXY.Y)) / GridCellSizeUU), 0, GridHeight - 1);
	return GY * GridWidth + GX;
}

EDungeonSurfaceKind UDungeonWaterSubsystem::ResolveSurface(const FVector& WorldLocation) const noexcept
{
	const int32 Idx = WorldToCellIndex(WorldLocation);
	if (SurfaceGrid.IsValidIndex(Idx))
	{
		return static_cast<EDungeonSurfaceKind>(SurfaceGrid[Idx]);
	}
	return EDungeonSurfaceKind::DryStone;
}

void UDungeonWaterSubsystem::SetSurfaceCell(const FVector& WorldLocation, const EDungeonSurfaceKind Kind) noexcept
{
	const int32 Idx = WorldToCellIndex(WorldLocation);
	if (SurfaceGrid.IsValidIndex(Idx))
	{
		SurfaceGrid[Idx] = static_cast<uint8>(Kind);
	}
}

void UDungeonWaterSubsystem::PaintDiscOnGrid(const FVector& Center, const float RadiusUU, const EDungeonSurfaceKind Kind) noexcept
{
	if (GridWidth <= 0 || GridHeight <= 0)
	{
		return;
	}

	const int32 CellRad = FMath::Max(0, FMath::CeilToInt(RadiusUU / GridCellSizeUU));
	const int32 CX = FMath::FloorToInt((static_cast<float>(Center.X) - static_cast<float>(GridOriginXY.X)) / GridCellSizeUU);
	const int32 CY = FMath::FloorToInt((static_cast<float>(Center.Y) - static_cast<float>(GridOriginXY.Y)) / GridCellSizeUU);
	const float RadSq = RadiusUU * RadiusUU;

	for (int32 DY = -CellRad; DY <= CellRad; ++DY)
	{
		const int32 GY = CY + DY;
		if (GY < 0 || GY >= GridHeight)
		{
			continue;
		}
		for (int32 DX = -CellRad; DX <= CellRad; ++DX)
		{
			const int32 GX = CX + DX;
			if (GX < 0 || GX >= GridWidth)
			{
				continue;
			}
			const float WorldCX = static_cast<float>(GridOriginXY.X) + (static_cast<float>(GX) + 0.5f) * GridCellSizeUU;
			const float WorldCY = static_cast<float>(GridOriginXY.Y) + (static_cast<float>(GY) + 0.5f) * GridCellSizeUU;
			const float DistSq = FMath::Square(WorldCX - static_cast<float>(Center.X)) + FMath::Square(WorldCY - static_cast<float>(Center.Y));
			if (DistSq <= RadSq)
			{
				SurfaceGrid[GY * GridWidth + GX] = static_cast<uint8>(Kind);
			}
		}
	}
}

void UDungeonWaterSubsystem::OnFloorBuilt(const FWaterFloorBuildInput& Input)
{
	BoundWaterHISM = Input.WaterHISM;
	BoundWaterFxISM = Input.WaterFxISM;

	WaterAudioEmitters.Reset();
	ActiveDripCount = 0;
	ActiveFootprintCount = 0;
	ActiveCascadeCount = 0;
	ActiveGeyserCount = 0;
	ActiveStreamTileCount = 0;
	FootprintCursor = 0;
	RippleCursor = 0;
	DropletCursor = 0;
	HydrologyAccumSec = 0.0f;
	PendingEcoTimeFallback = 0.0f;
	bAnyFxDirty = false;

	for (int32 i = 0; i < MaxCeilingDrips; ++i)
	{
		Drips[i] = FWaterCeilingDripState();
	}
	for (int32 i = 0; i < MaxWaterFxSlots; ++i)
	{
		FxSlots[i] = FWaterFxSlotState();
	}
	for (int32 i = 0; i < MaxTrackedWalkers; ++i)
	{
		WalkerTracks[i] = FWaterWalkerTrack();
	}

	const FDungeonFloorGraph* Graph = Input.FloorGraph;
	if (!Graph || Graph->Nodes.Num() == 0)
	{
		SurfaceGrid.Reset();
		GridWidth = 0;
		GridHeight = 0;
		return;
	}

	// 1. Compute spatial bounding box covering all rooms cleanly and allocate single O(1) SurfaceGrid
	double MinX = DBL_MAX, MinY = DBL_MAX, MaxX = -DBL_MAX, MaxY = -DBL_MAX;
	for (const FDungeonNode& Node : Graph->Nodes)
	{
		MinX = FMath::Min(MinX, Node.WorldCenter.X - Node.RoomExtents.X - 512.0);
		MinY = FMath::Min(MinY, Node.WorldCenter.Y - Node.RoomExtents.Y - 512.0);
		MaxX = FMath::Max(MaxX, Node.WorldCenter.X + Node.RoomExtents.X + 512.0);
		MaxY = FMath::Max(MaxY, Node.WorldCenter.Y + Node.RoomExtents.Y + 512.0);
	}
	GridOriginXY = FVector2D(MinX, MinY);
	GridWidth = FMath::Clamp(FMath::CeilToInt((MaxX - MinX) / GridCellSizeUU), 8, 512);
	GridHeight = FMath::Clamp(FMath::CeilToInt((MaxY - MinY) / GridCellSizeUU), 8, 512);
	SurfaceGrid.Init(static_cast<uint8>(EDungeonSurfaceKind::DryStone), GridWidth * GridHeight);

	// 2. Bind M_Dungeoneer_EcoInstanced materials + Plane meshes onto HISM_Water and ISM_WaterFX
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UDungeoneerAssetBootstrapSubsystem* Bootstrap = GI ? GI->GetSubsystem<UDungeoneerAssetBootstrapSubsystem>() : nullptr;
	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));

	if (UHierarchicalInstancedStaticMeshComponent* WaterHISM = BoundWaterHISM.Get())
	{
		WaterHISM->ClearInstances();
		WaterHISM->NumCustomDataFloats = EEcoCPD::Count;
		WaterHISM->SetCastShadow(false);
		WaterHISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (PlaneMesh)
		{
			WaterHISM->SetStaticMesh(PlaneMesh);
		}
		if (Bootstrap)
		{
			if (UMaterialInterface* EcoMat = Bootstrap->EnsureEcoInstancedMaterial())
			{
				UMaterialInstanceDynamic* WaterMID = FDungeoneerEcoMaterialFactory::CreateInstancedAtlasMID(
					WaterHISM,
					EcoMat,
					Bootstrap->GetLoadedEcosystemTexture(FName(TEXT("ATLAS_Water_Surfaces_4x4_D"))),
					Bootstrap->GetLoadedEcosystemTexture(FName(TEXT("ATLAS_Water_Surfaces_4x4_N"))),
					Bootstrap->GetLoadedEcosystemTexture(FName(TEXT("ATLAS_Water_Surfaces_4x4_E"))),
					EDungeoneerMaterialLayoutMode::Eco8Float,
					true,
					512.0f,
					4,
					4,
					0.04f,
					1.0f,
					0.28f);
				WaterHISM->SetMaterial(0, WaterMID);
			}
		}
	}

	if (UInstancedStaticMeshComponent* FxISM = BoundWaterFxISM.Get())
	{
		FxISM->ClearInstances();
		FxISM->NumCustomDataFloats = EEcoCPD::Count;
		FxISM->SetCastShadow(false);
		FxISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (PlaneMesh)
		{
			FxISM->SetStaticMesh(PlaneMesh);
		}
		if (Bootstrap)
		{
			if (UMaterialInterface* EcoMat = Bootstrap->EnsureEcoInstancedMaterial())
			{
				UMaterialInstanceDynamic* FxMID = FDungeoneerEcoMaterialFactory::CreateInstancedAtlasMID(
					FxISM,
					EcoMat,
					Bootstrap->GetLoadedEcosystemTexture(FName(TEXT("PROP_Water_And_Fauna_4x4_D"))),
					Bootstrap->GetLoadedEcosystemTexture(FName(TEXT("PROP_Water_And_Fauna_4x4_N"))),
					Bootstrap->GetLoadedEcosystemTexture(FName(TEXT("PROP_Water_And_Fauna_4x4_E"))),
					EDungeoneerMaterialLayoutMode::Eco8Float,
					true,
					512.0f,
					4,
					4,
					0.08f,
					4.0f,
					0.0f);
				FxISM->SetMaterial(0, FxMID);
			}
		}

		// Pre-allocate all 256 hidden instances cleanly at floor build (zero runtime AddInstance allocations)
		const FTransform HiddenTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector);
		for (int32 Slot = 0; Slot < MaxWaterFxSlots; ++Slot)
		{
			const int32 InstIdx = FxISM->AddInstance(HiddenTransform, false);
			if (InstIdx != INDEX_NONE)
			{
				for (int32 c = 0; c < EEcoCPD::Count; ++c)
				{
					FxISM->SetCustomDataValue(InstIdx, c, 0.0f, false);
				}
			}
		}
	}

	// 3. Stamp puddles, Sanctuary rugs, Mirror-Palace obsidian floors, and crystals into SurfaceGrid
	if (Input.PuddleLocations)
	{
		for (const FVector& PuddlePos : *Input.PuddleLocations)
		{
			PaintDiscOnGrid(PuddlePos, Input.TileSizeUU * 0.45f, EDungeonSurfaceKind::ShallowPuddle);
		}
	}
	if (Input.CrystalLocations)
	{
		for (const FVector& CrysPos : *Input.CrystalLocations)
		{
			PaintDiscOnGrid(CrysPos, 110.0f, EDungeonSurfaceKind::CrystalShard);
		}
	}
	for (const FDungeonNode& Node : Graph->Nodes)
	{
		if (Node.RoomType == EDungeonRoomType::RestingGround)
		{
			PaintDiscOnGrid(Node.WorldCenter, 360.0f, EDungeonSurfaceKind::SanctuaryRug);
		}
		else if (Node.RoomType == EDungeonRoomType::MirrorPalaceMaze || Node.BiomeType == EDungeonBiomeType::MirrorPalace)
		{
			PaintDiscOnGrid(Node.WorldCenter, FMath::Min(Node.RoomExtents.X, Node.RoomExtents.Y) * 0.85f, EDungeonSurfaceKind::ObsidianMirror);
		}
	}

	UDungeonLightBudgetSubsystem* LightBudget = World ? World->GetSubsystem<UDungeonLightBudgetSubsystem>() : nullptr;
	FRandomStream FloorRand(Graph->MasterSeed ^ 0x57A7E201);
	const float WrappedTime = ResolveWrappedEcoTime();

	// 4. Carve deterministic Wading Streams across Crypt & SporeFens rooms + populate drifting petals
	int32 PetalIdx = 0;
	for (const FDungeonNode& Node : Graph->Nodes)
	{
		if ((Node.BiomeType != EDungeonBiomeType::Crypt && Node.BiomeType != EDungeonBiomeType::SporeFens) ||
			Node.RoomType == EDungeonRoomType::RestingGround)
		{
			continue;
		}

		const bool bIsEntry = (Node.RoomType == EDungeonRoomType::Entry);
		const bool bHorizontal = bIsEntry ? true : ((Node.RoomSeed & 1) == 0);
		const int32 ChannelLen = bIsEntry ? 3 : 5;
		const float Step = Input.TileSizeUU * 0.92f;
		const FLinearColor StreamHue = (Node.BiomeType == EDungeonBiomeType::SporeFens)
			? EcoPalette::EmeraldMoss
			: EcoPalette::AstralCyan;
		const float FlowOctant = bHorizontal ? 0.0f : 2.0f;
		const FVector StreamOrigin = bIsEntry ? (Node.WorldCenter + FVector(0.0, -256.0, 0.0)) : Node.WorldCenter;

		FVector ChanMin = StreamOrigin;
		FVector ChanMax = StreamOrigin;
		for (int32 s = -(ChannelLen / 2); s <= (ChannelLen / 2); ++s)
		{
			const float Wiggle = FMath::Sin(static_cast<float>(s) * 0.85f) * 48.0f;
			const FVector TilePos = StreamOrigin + (bHorizontal
				? FVector(static_cast<float>(s) * Step, Wiggle, 3.2f)
				: FVector(Wiggle, static_cast<float>(s) * Step, 3.2f));
			if (s == -(ChannelLen / 2)) { ChanMin = TilePos; }
			ChanMax = TilePos;

			PaintDiscOnGrid(TilePos, GridCellSizeUU * 0.85f, EDungeonSurfaceKind::AnkleDeepStream);
			++ActiveStreamTileCount;

			if (UHierarchicalInstancedStaticMeshComponent* WaterHISM = BoundWaterHISM.Get())
			{
				const FVector Scale(Input.TileSizeUU / 100.0f * 0.92f, Input.TileSizeUU / 100.0f * 0.65f, 1.0f);
				const FRotator Rot(0.0f, bHorizontal ? 0.0f : 90.0f, 0.0f);
				const int32 InstIdx = WaterHISM->AddInstance(FTransform(Rot, TilePos, Scale), false);
				if (InstIdx != INDEX_NONE)
				{
					WaterHISM->SetCustomDataValue(InstIdx, EEcoCPD::UVCell, static_cast<float>(FMath::Abs(s) % 4), false);
					WaterHISM->SetCustomDataValue(InstIdx, EEcoCPD::Emissive, 0.14f, false);
					WaterHISM->SetCustomDataValue(InstIdx, EEcoCPD::HueR, StreamHue.R, false);
					WaterHISM->SetCustomDataValue(InstIdx, EEcoCPD::HueG, StreamHue.G, false);
					WaterHISM->SetCustomDataValue(InstIdx, EEcoCPD::HueB, StreamHue.B, false);
					WaterHISM->SetCustomDataValue(InstIdx, EEcoCPD::EventTime, WrappedTime, false);
					WaterHISM->SetCustomDataValue(InstIdx, EEcoCPD::ParamA, FlowOctant, false);
					WaterHISM->SetCustomDataValue(InstIdx, EEcoCPD::KindFlags, EcoColors::PackKindFlags(static_cast<uint8>(EWaterForm::WadingStream), 0), false);
				}
			}

			if (ActiveDripCount < MaxCeilingDrips && (FloorRand.FRand() < 0.35f))
			{
				FWaterCeilingDripState& Drip = Drips[ActiveDripCount];
				Drip.bActive = true;
				Drip.SurfacePos = TilePos + FVector(FloorRand.FRandRange(-45.0f, 45.0f), FloorRand.FRandRange(-45.0f, 45.0f), 1.5f);
				Drip.CeilingHeightUU = FloorRand.FRandRange(220.0f, 290.0f);
				Drip.PeriodSec = FloorRand.FRandRange(1.6f, 4.2f);
				Drip.PhaseSec = FloorRand.FRandRange(0.0f, Drip.PeriodSec);
				Drip.bFalling = false;
				Drip.FallElapsed = 0.0f;
				++ActiveDripCount;
			}
		}

		// Populate drifting bioluminescent petals along stream segment inside PetalPool
		for (int32 p = 0; p < 8 && PetalIdx < PetalPoolCount; ++p)
		{
			const int32 SlotIdx = PetalPoolStart + PetalIdx;
			++PetalIdx;
			const float T = static_cast<float>(p) / 8.0f;
			const FVector StartPos = FMath::Lerp(ChanMin, ChanMax, T) + FVector(FloorRand.FRandRange(-24.0f, 24.0f), FloorRand.FRandRange(-24.0f, 24.0f), 2.5f);
			FxSlots[SlotIdx].StreamMin = ChanMin;
			FxSlots[SlotIdx].StreamMax = ChanMax;
			FxSlots[SlotIdx].DriftDir = bHorizontal ? FVector(1.0f, 0.0f, 0.0f) : FVector(0.0f, 1.0f, 0.0f);
			FxSlots[SlotIdx].DriftSpeed = FloorRand.FRandRange(26.0f, 48.0f);
			SpawnWaterFxInSlot(
				SlotIdx,
				EWaterFxKind::FloatingPetal,
				EcoCells::WaterRippleBase,
				StartPos,
				FRotator(0.0f, FloorRand.FRandRange(0.0f, 360.0f), 0.0f),
				FVector(0.24f, 0.24f, 1.0f),
				StreamHue,
				2.2f,
				0.0f);
		}

		WaterAudioEmitters.Add({ Node.WorldCenter, EWaterForm::WadingStream });
	}

	// Ensure deterministic ceiling drips also sprinkle Crypt/Mirror rooms without full streams
	for (const FDungeonNode& Node : Graph->Nodes)
	{
		if (ActiveDripCount >= MaxCeilingDrips)
		{
			break;
		}
		if (Node.RoomType == EDungeonRoomType::RestingGround)
		{
			continue;
		}
		FWaterCeilingDripState& Drip = Drips[ActiveDripCount];
		Drip.bActive = true;
		Drip.SurfacePos = Node.WorldCenter + FVector(FloorRand.FRandRange(-220.0f, 220.0f), FloorRand.FRandRange(-220.0f, 220.0f), 4.0f);
		Drip.CeilingHeightUU = FloorRand.FRandRange(230.0f, 300.0f);
		Drip.PeriodSec = FloorRand.FRandRange(1.6f, 4.2f);
		Drip.PhaseSec = FloorRand.FRandRange(0.0f, Drip.PeriodSec);
		Drip.bFalling = false;
		Drip.FallElapsed = 0.0f;
		++ActiveDripCount;
	}

	// 5. Place Weeping Wall Cascades along upright camera-facing backdrop walls (South / East),
	// guaranteeing a focal Entry Waterfall pouring onto the Upper-Left Entry stream wall.
	const FRotator CascadeFacingRot = Bootstrap
		? Bootstrap->GetCameraFacingSpritePlaneRotation()
		: FRotator(48.0f, -45.0f, 0.0f);

	int32 CasGeyIdx = 0;
	int32 EntryCascadeWallIdx = INDEX_NONE;
	if (Input.WallEntries)
	{
		const FDungeonNode* EntryNode = nullptr;
		for (const FDungeonNode& Node : Graph->Nodes)
		{
			if (Node.RoomType == EDungeonRoomType::Entry)
			{
				EntryNode = &Node;
				break;
			}
		}
		if (EntryNode)
		{
			double BestEntryDistSq = TNumericLimits<double>::Max();
			for (int32 w = 0; w < Input.WallEntries->Num(); ++w)
			{
				const FDungeoneerWallInstanceEntry& W = (*Input.WallEntries)[w];
				if (W.WallAxis == EDungeoneerWallAxis::South &&
					FMath::Abs(W.WorldCenter.Y - (EntryNode->WorldCenter.Y - EntryNode->RoomExtents.Y)) <= static_cast<double>(Input.TileSizeUU) * 1.25)
				{
					const double D2 = FMath::Square(W.WorldCenter.X - EntryNode->WorldCenter.X);
					if (D2 < BestEntryDistSq)
					{
						BestEntryDistSq = D2;
						EntryCascadeWallIdx = w;
					}
				}
			}
		}

		auto PlaceCascadeOnWall = [&](const FDungeoneerWallInstanceEntry& W, float ScaleMult, float EmissiveVal)
		{
			if (CasGeyIdx >= 16 || ActiveCascadeCount >= 10)
			{
				return;
			}
			const int32 SlotIdx = CascadeGeyserStart + CasGeyIdx;
			++CasGeyIdx;
			++ActiveCascadeCount;

			const FVector WallInward = (W.WallAxis == EDungeoneerWallAxis::South)
				? FVector(0.0, 96.0, -24.0)
				: (W.WallAxis == EDungeoneerWallAxis::East ? FVector(-72.0, 0.0, -16.0) : FVector(0.0, -72.0, -16.0));
			const FVector CascadePos = W.WorldCenter + WallInward;

			SpawnWaterFxInSlot(
				SlotIdx,
				EWaterFxKind::CascadeMist,
				EcoCells::WaterCascadeBase,
				CascadePos,
				CascadeFacingRot,
				FVector(ScaleMult, ScaleMult, 1.0f),
				EcoPalette::AstralCyan,
				EmissiveVal,
				0.0f);

			PaintDiscOnGrid(FVector(CascadePos.X, CascadePos.Y, 0.0), 125.0f, EDungeonSurfaceKind::ShallowPuddle);
			WaterAudioEmitters.Add({ CascadePos, EWaterForm::WeepingCascade });

			if (LightBudget)
			{
				FDungeonLightCandidate C;
				C.WorldPosition = FVector(CascadePos.X, CascadePos.Y, 72.0);
				C.Color = EcoPalette::AstralCyan;
				C.RadiusUU = 160.0f;
				C.BaseIntensity = 240.0f;
				C.RequestedTier = EDungeonLightTier::Tier1_MPCGlow;
				LightBudget->RegisterCandidate(C);
			}
		};

		if (EntryCascadeWallIdx != INDEX_NONE)
		{
			PlaceCascadeOnWall((*Input.WallEntries)[EntryCascadeWallIdx], 2.20f, 0.68f);
		}

		for (int32 w = 0; w < Input.WallEntries->Num() && CasGeyIdx < 16 && ActiveCascadeCount < 10; w += 6)
		{
			if (w == EntryCascadeWallIdx)
			{
				continue;
			}
			const FDungeoneerWallInstanceEntry& W = (*Input.WallEntries)[w];
			if (W.WallAxis != EDungeoneerWallAxis::South &&
				W.WallAxis != EDungeoneerWallAxis::East)
			{
				continue;
			}
			PlaceCascadeOnWall(W, 1.70f, 0.55f);
		}
	}

	// 6. Place Bio & Thermal Geysers in Sanctuary, SporeFens, and EmberForge
	for (const FDungeonNode& Node : Graph->Nodes)
	{
		if (CasGeyIdx >= CascadeGeyserCount)
		{
			break;
		}
		const bool bHasGeyser = (Node.BiomeType == EDungeonBiomeType::SporeFens) ||
			(Node.BiomeType == EDungeonBiomeType::EmberForge) ||
			(Node.RoomType == EDungeonRoomType::RestingGround);
		if (!bHasGeyser)
		{
			continue;
		}

		const int32 SlotIdx = CascadeGeyserStart + CasGeyIdx;
		++CasGeyIdx;
		++ActiveGeyserCount;

		const bool bThermal = (Node.BiomeType == EDungeonBiomeType::EmberForge);
		const FLinearColor GeyserHue = bThermal ? EcoPalette::MagmaOrange : EcoPalette::AstralCyan;
		const FVector GeyserPos = Node.WorldCenter + FVector(
			(Node.RoomType == EDungeonRoomType::RestingGround) ? -220.0 : FloorRand.FRandRange(-200.0f, 200.0f),
			FloorRand.FRandRange(-200.0f, 200.0f),
			4.5);

		SpawnWaterFxInSlot(
			SlotIdx,
			EWaterFxKind::CascadeMist,
			EcoCells::WaterGeyserBase,
			GeyserPos,
			FRotator::ZeroRotator,
			FVector(1.55f, 1.55f, 1.0f),
			GeyserHue,
			4.6f,
			0.0f);

		WaterAudioEmitters.Add({ GeyserPos, EWaterForm::BioThermalGeyser });

		if (LightBudget)
		{
			FDungeonLightCandidate C;
			C.WorldPosition = GeyserPos + FVector(0.0, 0.0, 40.0);
			C.Color = GeyserHue;
			C.RadiusUU = 155.0f;
			C.BaseIntensity = 360.0f;
			C.RequestedTier = EDungeonLightTier::Tier1_MPCGlow;
			LightBudget->RegisterCandidate(C);
		}
	}

	if (UHierarchicalInstancedStaticMeshComponent* WaterHISM = BoundWaterHISM.Get())
	{
		WaterHISM->BuildTreeIfOutdated(false, true);
		WaterHISM->UpdateBounds();
		WaterHISM->MarkRenderStateDirty();
	}
	if (UInstancedStaticMeshComponent* FxISM = BoundWaterFxISM.Get())
	{
		FxISM->UpdateBounds();
		FxISM->MarkRenderStateDirty();
	}
}

int32 UDungeonWaterSubsystem::AcquireRingSlot(const int32 RingStart, const int32 RingCount, int32& InOutCursor) noexcept
{
	const int32 Slot = RingStart + (InOutCursor % FMath::Max(RingCount, 1));
	InOutCursor = (InOutCursor + 1) % FMath::Max(RingCount, 1);
	return Slot;
}

void UDungeonWaterSubsystem::SpawnWaterFxInSlot(
	const int32 SlotIdx,
	const EWaterFxKind Kind,
	const int32 UVCell,
	const FVector& WorldPos,
	const FRotator& Rot,
	const FVector& Scale,
	const FLinearColor& Hue,
	const float Emissive,
	const float LifetimeSec)
{
	if (SlotIdx < 0 || SlotIdx >= MaxWaterFxSlots)
	{
		return;
	}

	FWaterFxSlotState& State = FxSlots[SlotIdx];
	State.WorldPos = WorldPos;
	State.BaseScale = Scale;
	State.Rotation = Rot;
	State.Kind = Kind;
	State.AgeSec = 0.0f;
	State.LifetimeSec = LifetimeSec;
	State.bOccupied = true;

	const float WrappedTime = ResolveWrappedEcoTime();
	if (UInstancedStaticMeshComponent* FxISM = BoundWaterFxISM.Get())
	{
		FxISM->UpdateInstanceTransform(SlotIdx, FTransform(Rot, WorldPos, Scale), true, false, false);
		FxISM->SetCustomDataValue(SlotIdx, EEcoCPD::UVCell, static_cast<float>(UVCell), false);
		FxISM->SetCustomDataValue(SlotIdx, EEcoCPD::Emissive, Emissive, false);
		FxISM->SetCustomDataValue(SlotIdx, EEcoCPD::HueR, Hue.R, false);
		FxISM->SetCustomDataValue(SlotIdx, EEcoCPD::HueG, Hue.G, false);
		FxISM->SetCustomDataValue(SlotIdx, EEcoCPD::HueB, Hue.B, false);
		FxISM->SetCustomDataValue(SlotIdx, EEcoCPD::EventTime, WrappedTime, false);
		FxISM->SetCustomDataValue(SlotIdx, EEcoCPD::ParamA, LifetimeSec, false);
		FxISM->SetCustomDataValue(SlotIdx, EEcoCPD::KindFlags, EcoColors::PackKindFlags(static_cast<uint8>(Kind), 0), false);
		bAnyFxDirty = true;
	}
}

void UDungeonWaterSubsystem::FlushFxSlotToISM(const int32 SlotIdx, const float WrappedEcoTime, const bool bMarkDirty)
{
	(void)WrappedEcoTime;
	if (UInstancedStaticMeshComponent* FxISM = BoundWaterFxISM.Get())
	{
		const FWaterFxSlotState& State = FxSlots[SlotIdx];
		const FVector UseScale = State.bOccupied ? State.BaseScale : FVector::ZeroVector;
		FxISM->UpdateInstanceTransform(SlotIdx, FTransform(State.Rotation, State.WorldPos, UseScale), true, bMarkDirty, false);
		bAnyFxDirty = true;
	}
}

void UDungeonWaterSubsystem::NotifyWalkerStep(
	const AActor* Walker,
	const FVector& WorldLocation,
	const ECompassOctant MoveOctant,
	const bool bIsPlayer)
{
	const EDungeonSurfaceKind Surf = ResolveSurface(WorldLocation);

	// Find or claim walker tracking slot without heap allocation
	int32 TrackIdx = INDEX_NONE;
	int32 FreeIdx = INDEX_NONE;
	for (int32 i = 0; i < MaxTrackedWalkers; ++i)
	{
		if (WalkerTracks[i].Walker.Get() == Walker)
		{
			TrackIdx = i;
			break;
		}
		if (FreeIdx == INDEX_NONE && !WalkerTracks[i].Walker.IsValid())
		{
			FreeIdx = i;
		}
	}
	if (TrackIdx == INDEX_NONE && FreeIdx != INDEX_NONE && Walker)
	{
		TrackIdx = FreeIdx;
		WalkerTracks[TrackIdx].Walker = Walker;
		WalkerTracks[TrackIdx].RemainingWetSteps = 0;
		WalkerTracks[TrackIdx].LastSurface = Surf;
	}

	const FVector GroundPos(WorldLocation.X, WorldLocation.Y, 2.2);

	if (Surf == EDungeonSurfaceKind::ShallowPuddle || Surf == EDungeonSurfaceKind::AnkleDeepStream)
	{
		if (TrackIdx != INDEX_NONE)
		{
			WalkerTracks[TrackIdx].RemainingWetSteps = 6;
			WalkerTracks[TrackIdx].LastSurface = Surf;
		}

		// Spawn 4-frame animated splash/ripple on step inside water
		const int32 SlotIdx = AcquireRingSlot(RippleRingStart, RippleRingCount, RippleCursor);
		const EWaterFxKind FxKind = (Surf == EDungeonSurfaceKind::AnkleDeepStream) ? EWaterFxKind::Splash : EWaterFxKind::Ripple;
		SpawnWaterFxInSlot(
			SlotIdx,
			FxKind,
			EcoCells::WaterRippleBase,
			GroundPos,
			FRotator(0.0f, WaterDetail::OctantToYawDeg(MoveOctant), 0.0f),
			FVector(bIsPlayer ? 0.56f : 0.44f, bIsPlayer ? 0.56f : 0.44f, 1.0f),
			EcoPalette::AstralCyan,
			0.65f,
			0.85f);
	}
	else if (TrackIdx != INDEX_NONE && WalkerTracks[TrackIdx].RemainingWetSteps > 0)
	{
		--WalkerTracks[TrackIdx].RemainingWetSteps;
		WalkerTracks[TrackIdx].LastSurface = Surf;

		const int32 SlotIdx = AcquireRingSlot(FootprintRingStart, FootprintRingCount, FootprintCursor);
		const bool bWasOccupied = FxSlots[SlotIdx].bOccupied && FxSlots[SlotIdx].Kind == EWaterFxKind::Footprint;
		SpawnWaterFxInSlot(
			SlotIdx,
			EWaterFxKind::Footprint,
			EcoCells::WaterRippleBase,
			GroundPos + FVector(0.0, 0.0, -0.3),
			FRotator(0.0f, WaterDetail::OctantToYawDeg(MoveOctant), 0.0f),
			FVector(0.36f, 0.22f, 1.0f),
			EcoPalette::AstralCyan * 0.65f,
			1.25f,
			4.0f);
		if (!bWasOccupied)
		{
			++ActiveFootprintCount;
		}
	}
}

void UDungeonWaterSubsystem::SpawnSporePuffBurst(const FVector& Origin, const int32 Count, const FLinearColor& Color)
{
	const int32 SafeCount = FMath::Clamp(Count, 1, 16);
	for (int32 i = 0; i < SafeCount; ++i)
	{
		const int32 SlotIdx = AcquireRingSlot(RippleRingStart, RippleRingCount, RippleCursor);
		const float Angle = (static_cast<float>(i) / static_cast<float>(SafeCount)) * TWO_PI;
		const float Radial = 18.0f + static_cast<float>((i * 11) % 36);
		const FVector SpawnPos = Origin + FVector(FMath::Cos(Angle) * Radial, FMath::Sin(Angle) * Radial, 28.0f + (i % 3) * 14.0f);
		FxSlots[SlotIdx].DriftDir = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.45f).GetSafeNormal();
		FxSlots[SlotIdx].DriftSpeed = 42.0f + static_cast<float>(i * 4);
		SpawnWaterFxInSlot(
			SlotIdx,
			EWaterFxKind::SporePuff,
			EcoCells::WaterRippleBase + (i & 3),
			SpawnPos,
			FRotator::ZeroRotator,
			FVector(0.28f, 0.28f, 1.0f),
			Color,
			5.5f,
			1.25f);
	}
}

void UDungeonWaterSubsystem::StepHydrologySimulation(const float DeltaSeconds)
{
	PendingEcoTimeFallback += DeltaSeconds;
	const float WrappedTime = ResolveWrappedEcoTime();
	UInstancedStaticMeshComponent* FxISM = BoundWaterFxISM.Get();

	// 1. Update ceiling drips falling under gravity (z = h - 0.5*g*t^2)
	for (int32 i = 0; i < ActiveDripCount; ++i)
	{
		FWaterCeilingDripState& D = Drips[i];
		if (!D.bActive)
		{
			continue;
		}
		if (!D.bFalling)
		{
			D.PhaseSec += DeltaSeconds;
			if (D.PhaseSec >= D.PeriodSec)
			{
				D.PhaseSec -= D.PeriodSec;
				D.bFalling = true;
				D.FallElapsed = 0.0f;
			}
		}
		else
		{
			D.FallElapsed += DeltaSeconds;
			const float Z = D.CeilingHeightUU - 0.5f * WaterDetail::DripGravityUU * D.FallElapsed * D.FallElapsed;
			const int32 DropSlot = DropletPoolStart + (i % DropletPoolCount);
			if (Z <= 4.0f)
			{
				D.bFalling = false;
				FxSlots[DropSlot].bOccupied = false;
				FlushFxSlotToISM(DropSlot, WrappedTime, false);

				// Impact splash ripple + broadcast acoustic drip event
				const int32 RipSlot = AcquireRingSlot(RippleRingStart, RippleRingCount, RippleCursor);
				SpawnWaterFxInSlot(
					RipSlot,
					EWaterFxKind::Ripple,
					EcoCells::WaterRippleBase,
					D.SurfacePos,
					FRotator::ZeroRotator,
					FVector(0.68f, 0.68f, 1.0f),
					EcoPalette::AstralCyan,
					3.4f,
					0.9f);
				OnCeilingDripImpact.Broadcast(D.SurfacePos, D.CeilingHeightUU, EWaterForm::CeilingDrip);
			}
			else if (FxISM)
			{
				FxSlots[DropSlot].bOccupied = true;
				FxSlots[DropSlot].WorldPos = D.SurfacePos + FVector(0.0, 0.0, Z);
				FxSlots[DropSlot].BaseScale = FVector(0.14f, 0.14f, 0.24f);
				FxSlots[DropSlot].Rotation = FRotator(75.0f, 180.0f, 0.0f);
				FlushFxSlotToISM(DropSlot, WrappedTime, false);
			}
		}
	}

	// 2. Advance lifetimes on timed FX slots + drift floating petals/spores
	for (int32 Slot = 0; Slot < CascadeGeyserStart; ++Slot)
	{
		FWaterFxSlotState& S = FxSlots[Slot];
		if (!S.bOccupied)
		{
			continue;
		}
		if (S.LifetimeSec > 0.0f)
		{
			S.AgeSec += DeltaSeconds;
			if (S.AgeSec >= S.LifetimeSec)
			{
				const bool bWasFootprint = (S.Kind == EWaterFxKind::Footprint);
				S.bOccupied = false;
				if (bWasFootprint && ActiveFootprintCount > 0)
				{
					--ActiveFootprintCount;
				}
				FlushFxSlotToISM(Slot, WrappedTime, false);
				continue;
			}
		}
		if (S.DriftSpeed > 0.5f && FxISM)
		{
			S.WorldPos += S.DriftDir * (S.DriftSpeed * DeltaSeconds);
			if (S.Kind == EWaterFxKind::FloatingPetal)
			{
				const double SpanX = FMath::Abs(S.StreamMax.X - S.StreamMin.X) + 64.0;
				const double SpanY = FMath::Abs(S.StreamMax.Y - S.StreamMin.Y) + 64.0;
				if (S.DriftDir.X > 0.5 && S.WorldPos.X > S.StreamMax.X + 32.0)
				{
					S.WorldPos.X -= SpanX;
				}
				if (S.DriftDir.Y > 0.5 && S.WorldPos.Y > S.StreamMax.Y + 32.0)
				{
					S.WorldPos.Y -= SpanY;
				}
			}
			FlushFxSlotToISM(Slot, WrappedTime, false);
		}
	}

	if (bAnyFxDirty && FxISM)
	{
		FxISM->MarkRenderStateDirty();
		bAnyFxDirty = false;
	}
}

void UDungeonWaterSubsystem::SpawnRippleAt(const FVector& Origin, const FLinearColor& Color, const float Scale)
{
	const int32 RipSlot = AcquireRingSlot(RippleRingStart, RippleRingCount, RippleCursor);
	SpawnWaterFxInSlot(
		RipSlot,
		EWaterFxKind::Ripple,
		EcoCells::WaterRippleBase,
		FVector(Origin.X, Origin.Y, 3.6),
		FRotator::ZeroRotator,
		FVector(Scale, Scale, 1.0f),
		Color,
		3.0f,
		0.9f);
	if (UInstancedStaticMeshComponent* FxISM = BoundWaterFxISM.Get())
	{
		FxISM->MarkRenderStateDirty();
		bAnyFxDirty = false;
	}
}

void UDungeonWaterSubsystem::Tick(float DeltaTime)
{
	if (ActiveDripCount == 0 && ActiveCascadeCount == 0 && ActiveGeyserCount == 0 && ActiveStreamTileCount == 0)
	{
		return;
	}

	HydrologyAccumSec += FMath::Min(DeltaTime, 0.1f);
	if (HydrologyAccumSec >= WaterDetail::HydrologyTickIntervalSec)
	{
		const float Step = HydrologyAccumSec;
		HydrologyAccumSec = 0.0f;
		StepHydrologySimulation(Step);
	}
}
