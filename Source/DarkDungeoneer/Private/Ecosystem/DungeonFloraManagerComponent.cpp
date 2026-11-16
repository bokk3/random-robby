#include "Ecosystem/DungeonFloraManagerComponent.h"
#include "AI/ZombieBase.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "Core/DungeoneerCharacter.h"
#include "Dungeon/DungeoneerTileMapInstancer.h"
#include "Dungeon/MirrorLightBounceComponent.h"
#include "Ecosystem/DungeonWaterSubsystem.h"
#include "Lighting/DungeonLightBudgetSubsystem.h"
#include "Rendering/DungeoneerEcoMaterialFactory.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace FloraDetail
{
	inline constexpr float FloraTickIntervalSec = 1.0f / 30.0f;
}

UDungeonFloraManagerComponent::UDungeonFloraManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	FloraHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("HISM_MagicalFlora"));
	if (FloraHISM)
	{
		FloraHISM->NumCustomDataFloats = EEcoCPD::Count;
		FloraHISM->SetCastShadow(false);
		FloraHISM->bCastHiddenShadow = false;
		FloraHISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		FloraHISM->SetCullDistances(0, 3800);
	}
}

float UDungeonFloraManagerComponent::PointToSegmentDistSq(const FVector& Pt, const FVector& SegA, const FVector& SegB) noexcept
{
	const FVector AB = SegB - SegA;
	const double LenSq = AB.SizeSquared();
	if (LenSq <= 1e-4)
	{
		return static_cast<float>(FVector::DistSquared(Pt, SegA));
	}
	const double T = FMath::Clamp(FVector::DotProduct(Pt - SegA, AB) / LenSq, 0.0, 1.0);
	const FVector Closest = SegA + AB * T;
	return static_cast<float>(FVector::DistSquared(Pt, Closest));
}

void UDungeonFloraManagerComponent::EnsureFloraHISMVisuals()
{
	if (!FloraHISM)
	{
		return;
	}

	if (AActor* Owner = GetOwner())
	{
		if (USceneComponent* Root = Owner->GetRootComponent())
		{
			if (FloraHISM->GetAttachParent() != Root)
			{
				FloraHISM->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);
			}
			if (!FloraHISM->IsRegistered())
			{
				FloraHISM->RegisterComponent();
			}
		}
	}

	FloraHISM->NumCustomDataFloats = EEcoCPD::Count;
	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh && !FloraHISM->GetStaticMesh())
	{
		FloraHISM->SetStaticMesh(PlaneMesh);
	}

	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	if (UDungeoneerAssetBootstrapSubsystem* Bootstrap = GI ? GI->GetSubsystem<UDungeoneerAssetBootstrapSubsystem>() : nullptr)
	{
		if (UMaterialInterface* EcoMat = Bootstrap->EnsureEcoInstancedMaterial())
		{
			UMaterialInstanceDynamic* FloraMID = FDungeoneerEcoMaterialFactory::CreateInstancedAtlasMID(
				FloraHISM,
				EcoMat,
				Bootstrap->GetLoadedEcosystemTexture(FName(TEXT("PROP_Magical_Flora_And_Crystals_4x4_D"))),
				Bootstrap->GetLoadedEcosystemTexture(FName(TEXT("PROP_Magical_Flora_And_Crystals_4x4_N"))),
				Bootstrap->GetLoadedEcosystemTexture(FName(TEXT("PROP_Magical_Flora_And_Crystals_4x4_E"))),
				EDungeoneerMaterialLayoutMode::Eco8Float,
				true,
				512.0f,
				4,
				4,
				0.35f,
				1.0f,
				0.0f);
			FloraHISM->SetMaterial(0, FloraMID);
		}
	}
}

void UDungeonFloraManagerComponent::GatherCrystalPositions(TArray<FVector>& OutPositions) const
{
	OutPositions.Reset();
	for (int32 i = 0; i < Kinds.Num(); ++i)
	{
		if (Kinds[i] == EFloraKind::EchoCrystal)
		{
			OutPositions.Add(WorldPositions[i]);
		}
	}
}

bool UDungeonFloraManagerComponent::IsPositionNearAnyFlora(const FVector& WorldPos, const float RadiusUU) const noexcept
{
	const double RadSq = static_cast<double>(RadiusUU) * static_cast<double>(RadiusUU);
	for (int32 i = 0; i < WorldPositions.Num(); ++i)
	{
		if (FVector::DistSquared2D(WorldPos, WorldPositions[i]) <= RadSq)
		{
			return true;
		}
	}
	return false;
}

bool UDungeonFloraManagerComponent::FindNearestEchoCrystal(const FVector& QueryPos, FVector& OutCrystalPos) const noexcept
{
	double BestDistSq = TNumericLimits<double>::Max();
	bool bFound = false;
	for (int32 i = 0; i < Kinds.Num(); ++i)
	{
		if (Kinds[i] == EFloraKind::EchoCrystal)
		{
			const double DistSq = FVector::DistSquared2D(QueryPos, WorldPositions[i]);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				OutCrystalPos = WorldPositions[i];
				bFound = true;
			}
		}
	}
	return bFound;
}

int32 UDungeonFloraManagerComponent::AddFloraInstance(
	const EFloraKind Kind,
	const FVector& WorldPos,
	const FLinearColor& Hue,
	const float Scale)
{
	const UWorld* World = GetWorld();
	UDungeonLightBudgetSubsystem* LightBudget = World ? World->GetSubsystem<UDungeonLightBudgetSubsystem>() : nullptr;
	UDungeonWaterSubsystem* WaterSub = World ? World->GetSubsystem<UDungeonWaterSubsystem>() : nullptr;

	const int32 FloraIdx = Kinds.Num();
	int32 InitialCell = EcoCells::PrismLilyBloom[0];
	float InitialEmissive = 0.2f;
	FRotator SpawnRot = FRotator(48.0f, -45.0f, 0.0f);
	FVector SpawnScale(0.75f * Scale, 0.75f * Scale, 1.0f);

	if (const UGameInstance* GI = World ? World->GetGameInstance() : nullptr)
	{
		if (const UDungeoneerAssetBootstrapSubsystem* Bootstrap = GI->GetSubsystem<UDungeoneerAssetBootstrapSubsystem>())
		{
			SpawnRot = Bootstrap->GetCameraFacingSpritePlaneRotation();
		}
	}

	int32 LightCandId = INDEX_NONE;
	switch (Kind)
	{
	case EFloraKind::PrismLily:
		InitialCell = EcoCells::PrismLilyBloom[0];
		InitialEmissive = 0.2f;
		if (LightBudget)
		{
			FDungeonLightCandidate C;
			C.WorldPosition = WorldPos + FVector(0.0, 0.0, 35.0);
			C.Color = Hue;
			C.RadiusUU = 300.0f;
			C.BaseIntensity = 520.0f;
			C.RequestedTier = EDungeonLightTier::Tier2_Pooled;
			C.bEnabled = false; // dormant until flashlight or mirror bounce blooms it
			LightCandId = LightBudget->RegisterCandidate(C);
		}
		break;

	case EFloraKind::SporeCap:
		InitialCell = EcoCells::SporeCapVariants[FloraIdx & 3];
		InitialEmissive = 1.6f;
		if (LightBudget)
		{
			FDungeonLightCandidate C;
			C.WorldPosition = WorldPos + FVector(0.0, 0.0, 28.0);
			C.Color = Hue;
			C.RadiusUU = 155.0f;
			C.BaseIntensity = 280.0f;
			C.RequestedTier = EDungeonLightTier::Tier1_MPCGlow;
			LightCandId = LightBudget->RegisterCandidate(C);
		}
		break;

	case EFloraKind::EchoCrystal:
		InitialCell = EcoCells::EchoCrystalCharge[0];
		InitialEmissive = 2.4f;
		if (LightBudget)
		{
			FDungeonLightCandidate C;
			C.WorldPosition = WorldPos + FVector(0.0, 0.0, 42.0);
			C.Color = Hue;
			C.RadiusUU = 185.0f;
			C.BaseIntensity = 360.0f;
			C.RequestedTier = EDungeonLightTier::Tier1_MPCGlow;
			LightCandId = LightBudget->RegisterCandidate(C);
		}
		if (WaterSub)
		{
			WaterSub->SetSurfaceCell(WorldPos, EDungeonSurfaceKind::CrystalShard);
		}
		break;

	case EFloraKind::PulseMoss:
		InitialCell = EcoCells::PulseMossStage[FloraIdx & 3];
		InitialEmissive = 1.15f;
		SpawnRot = FRotator::ZeroRotator; // Ground carpet lies flat on stone flags
		SpawnScale = FVector(1.05f * Scale, 1.05f * Scale, 1.0f);
		if (WaterSub)
		{
			WaterSub->SetSurfaceCell(WorldPos, EDungeonSurfaceKind::SquishyMoss);
		}
		break;
	}

	int32 InstIdx = INDEX_NONE;
	if (FloraHISM)
	{
		const FVector FinalPos = (Kind == EFloraKind::PulseMoss) ? FVector(WorldPos.X, WorldPos.Y, 1.6) : WorldPos + FVector(0.0, 0.0, 32.0);
		InstIdx = FloraHISM->AddInstance(FTransform(SpawnRot, FinalPos, SpawnScale), true);
		if (InstIdx != INDEX_NONE)
		{
			FloraHISM->SetCustomDataValue(InstIdx, EEcoCPD::UVCell, static_cast<float>(InitialCell), false);
			FloraHISM->SetCustomDataValue(InstIdx, EEcoCPD::Emissive, InitialEmissive, false);
			FloraHISM->SetCustomDataValue(InstIdx, EEcoCPD::HueR, Hue.R, false);
			FloraHISM->SetCustomDataValue(InstIdx, EEcoCPD::HueG, Hue.G, false);
			FloraHISM->SetCustomDataValue(InstIdx, EEcoCPD::HueB, Hue.B, false);
			FloraHISM->SetCustomDataValue(InstIdx, EEcoCPD::EventTime, 0.0f, false);
			FloraHISM->SetCustomDataValue(InstIdx, EEcoCPD::ParamA, 0.0f, false);
			FloraHISM->SetCustomDataValue(InstIdx, EEcoCPD::KindFlags, EcoColors::PackKindFlags(static_cast<uint8>(Kind), 0), false);
		}
	}

	Kinds.Add(Kind);
	WorldPositions.Add(WorldPos);
	Hues.Add(Hue);
	States.Add(0);
	StateTimers.Add(0.0f);
	Emissives.Add(InitialEmissive);
	EventTimes.Add(0.0f);
	UVCells.Add(InitialCell);
	LightCandidateIds.Add(LightCandId);
	HISMInstanceIndices.Add(InstIdx);

	return FloraIdx;
}

void UDungeonFloraManagerComponent::RebuildPulseMossCSRGraph(const float MaxEdgeDistanceUU)
{
	MossIndices.Reset();
	FloraToDenseMoss.Init(INDEX_NONE, Kinds.Num());

	for (int32 i = 0; i < Kinds.Num(); ++i)
	{
		if (Kinds[i] == EFloraKind::PulseMoss)
		{
			const int32 DenseIdx = MossIndices.Add(i);
			FloraToDenseMoss[i] = DenseIdx;
		}
	}

	const int32 M = MossIndices.Num();
	MossOffsets.Init(0, M + 1);
	MossAdj.Reset();
	MossVisitedEpoch.Init(0, M);

	if (M == 0)
	{
		return;
	}

	const double MaxDistSq = FMath::Square(static_cast<double>(MaxEdgeDistanceUU));
	for (int32 u = 0; u < M; ++u)
	{
		MossOffsets[u] = MossAdj.Num();
		const FVector& PosU = WorldPositions[MossIndices[u]];
		for (int32 v = 0; v < M; ++v)
		{
			if (u == v)
			{
				continue;
			}
			if (FVector::DistSquared2D(PosU, WorldPositions[MossIndices[v]]) <= MaxDistSq)
			{
				MossAdj.Add(v);
			}
		}
	}
	MossOffsets[M] = MossAdj.Num();
}

void UDungeonFloraManagerComponent::PopulateForFloor(
	const FDungeonFloorGraph& FloorGraph,
	const TArray<FDungeoneerMirrorInstanceEntry>* MirrorEntries,
	const float TileSizeUU)
{
	if (FloraHISM)
	{
		FloraHISM->ClearInstances();
	}
	Kinds.Reset();
	WorldPositions.Reset();
	Hues.Reset();
	States.Reset();
	StateTimers.Reset();
	Emissives.Reset();
	EventTimes.Reset();
	UVCells.Reset();
	LightCandidateIds.Reset();
	HISMInstanceIndices.Reset();
	MossIndices.Reset();
	FloraToDenseMoss.Reset();
	MossOffsets.Reset();
	MossAdj.Reset();
	MossVisitedEpoch.Reset();
	LastSeenNoiseSerial = 0;
	SimAccumSec = 0.0f;

	EnsureFloraHISMVisuals();

	const int32 EstCount = FMath::Max(16, FloorGraph.Nodes.Num() * 14);
	Kinds.Reserve(EstCount);
	WorldPositions.Reserve(EstCount);
	Hues.Reserve(EstCount);
	States.Reserve(EstCount);
	StateTimers.Reserve(EstCount);
	Emissives.Reserve(EstCount);
	EventTimes.Reserve(EstCount);
	UVCells.Reserve(EstCount);
	LightCandidateIds.Reserve(EstCount);
	HISMInstanceIndices.Reserve(EstCount);

	for (const FDungeonNode& Node : FloorGraph.Nodes)
	{
		FRandomStream Rand(Node.RoomSeed ^ 0x464C4F52);
		const float HalfSpan = FMath::Min(Node.RoomExtents.X, Node.RoomExtents.Y) * 0.55f;

		if (Node.RoomType == EDungeonRoomType::RestingGround)
		{
			// Sacred protective Pulse-Moss ring around Sanctuary perimeter
			for (int32 r = 0; r < 8; ++r)
			{
				const float Ang = (static_cast<float>(r) / 8.0f) * TWO_PI;
				const FVector MossPos = Node.WorldCenter + FVector(FMath::Cos(Ang) * 290.0f, FMath::Sin(Ang) * 290.0f, 0.0f);
				AddFloraInstance(EFloraKind::PulseMoss, MossPos, EcoPalette::EmeraldMoss, 1.0f);
			}
			continue;
		}

		if (Node.BiomeType == EDungeonBiomeType::MirrorPalace || Node.RoomType == EDungeonRoomType::MirrorPalaceMaze)
		{
			// Paired Prism-Lilies + Echo-Crystals positioned to combo with Mirror-Palace beam reflections
			for (int32 i = 0; i < 3; ++i)
			{
				const FVector Offset(Rand.FRandRange(-HalfSpan, HalfSpan), Rand.FRandRange(-HalfSpan, HalfSpan), 0.0);
				AddFloraInstance(EFloraKind::PrismLily, Node.WorldCenter + Offset, EcoPalette::MagentaBloom, 0.9f);
			}
			for (int32 i = 0; i < 3; ++i)
			{
				const FVector Offset(Rand.FRandRange(-HalfSpan, HalfSpan), Rand.FRandRange(-HalfSpan, HalfSpan), 0.0);
				AddFloraInstance(EFloraKind::EchoCrystal, Node.WorldCenter + Offset, EcoPalette::AstralCyan, 0.95f);
			}
		}
		else if (Node.BiomeType == EDungeonBiomeType::SporeFens)
		{
			// Connected Pulse-Moss clusters (chains for CSR BFS ripples) + Spore-Caps + Prism-Lilies
			const FVector ClusterOrigin = Node.WorldCenter + FVector(Rand.FRandRange(-200.0f, 200.0f), Rand.FRandRange(-200.0f, 200.0f), 0.0);
			for (int32 m = 0; m < 6; ++m)
			{
				const FVector MossPos = ClusterOrigin + FVector(static_cast<float>(m % 3) * 140.0f, static_cast<float>(m / 3) * 140.0f, 0.0);
				AddFloraInstance(EFloraKind::PulseMoss, MossPos, EcoPalette::EmeraldMoss, 1.15f);
			}
			for (int32 s = 0; s < 4; ++s)
			{
				const FVector Offset(Rand.FRandRange(-HalfSpan, HalfSpan), Rand.FRandRange(-HalfSpan, HalfSpan), 0.0);
				AddFloraInstance(EFloraKind::SporeCap, Node.WorldCenter + Offset, EcoPalette::EmeraldMoss, 0.85f);
			}
			AddFloraInstance(EFloraKind::PrismLily, Node.WorldCenter + FVector(-HalfSpan * 0.6f, HalfSpan * 0.5f, 0.0), EcoPalette::MagentaBloom, 0.9f);
		}
		else if (Node.BiomeType == EDungeonBiomeType::EmberForge)
		{
			// Golden/Magma Prism-Lilies + resonant Echo-Crystals
			for (int32 i = 0; i < 3; ++i)
			{
				const FVector Offset(Rand.FRandRange(-HalfSpan, HalfSpan), Rand.FRandRange(-HalfSpan, HalfSpan), 0.0);
				AddFloraInstance(EFloraKind::PrismLily, Node.WorldCenter + Offset, EcoPalette::GoldenRune, 0.95f);
			}
			for (int32 i = 0; i < 2; ++i)
			{
				const FVector Offset(Rand.FRandRange(-HalfSpan, HalfSpan), Rand.FRandRange(-HalfSpan, HalfSpan), 0.0);
				AddFloraInstance(EFloraKind::EchoCrystal, Node.WorldCenter + Offset, EcoPalette::MagmaOrange, 0.95f);
			}
		}
		else // Sunken Crypt
		{
			if (Node.RoomType == EDungeonRoomType::Entry)
			{
				// Mirror REF_Living_Dungeon_Target diorama framing around the Entry stream:
				// blooming magenta Prism-Lily banks, cyan Echo-Crystal clusters near the waterfall & foreground, and emerald Pulse-Moss
				const UWorld* CurWorld = GetWorld();
				UDungeonLightBudgetSubsystem* LB = CurWorld ? CurWorld->GetSubsystem<UDungeonLightBudgetSubsystem>() : nullptr;
				auto SpawnPreBloomedLily = [&](const FVector& Pos, const FLinearColor& LilyHue, float LilyScale, int32 CellIdx)
				{
					const int32 FloraIdx = AddFloraInstance(EFloraKind::PrismLily, Pos, LilyHue, LilyScale);
					if (Kinds.IsValidIndex(FloraIdx) && FloraHISM)
					{
						States[FloraIdx] = 3;
						StateTimers[FloraIdx] = 9999.0f;
						Emissives[FloraIdx] = 2.6f;
						UVCells[FloraIdx] = CellIdx;
						const int32 InstIdx = HISMInstanceIndices[FloraIdx];
						if (InstIdx != INDEX_NONE)
						{
							FloraHISM->SetCustomDataValue(InstIdx, EEcoCPD::UVCell, static_cast<float>(CellIdx), false);
							FloraHISM->SetCustomDataValue(InstIdx, EEcoCPD::Emissive, 2.6f, false);
						}
						if (LB)
						{
							FDungeonLightCandidate C;
							C.WorldPosition = Pos + FVector(0.0, 0.0, 28.0);
							C.Color = LilyHue;
							C.RadiusUU = 165.0f;
							C.BaseIntensity = 340.0f;
							C.RequestedTier = EDungeonLightTier::Tier1_MPCGlow;
							LB->RegisterCandidate(C);
						}
					}
				};

				// Upper-Left stream bank magenta lily bed + cyan waterfall crystal
				SpawnPreBloomedLily(Node.WorldCenter + FVector( -70.0, -355.0, 0.0), EcoPalette::MagentaBloom, 0.88f, 3);
				SpawnPreBloomedLily(Node.WorldCenter + FVector(  25.0, -380.0, 0.0), EcoPalette::MagentaBloom, 0.72f, 2);
				SpawnPreBloomedLily(Node.WorldCenter + FVector( 105.0, -345.0, 0.0), EcoPalette::MagentaBloom, 0.76f, 3);
				AddFloraInstance(EFloraKind::EchoCrystal, Node.WorldCenter + FVector(-205.0, -345.0, 0.0), EcoPalette::AstralCyan, 1.05f);

				// Foreground & SE bank clusters
				SpawnPreBloomedLily(Node.WorldCenter + FVector( 220.0, -140.0, 0.0), EcoPalette::MagentaBloom, 0.85f, 3);
				SpawnPreBloomedLily(Node.WorldCenter + FVector( 285.0,  -85.0, 0.0), EcoPalette::MagentaBloom, 0.70f, 2);
				AddFloraInstance(EFloraKind::PrismLily,   Node.WorldCenter + FVector(-220.0,  220.0, 0.0), EcoPalette::MagentaBloom, 0.88f);
				AddFloraInstance(EFloraKind::EchoCrystal, Node.WorldCenter + FVector(-140.0,  280.0, 0.0), EcoPalette::AstralCyan,   1.00f);
				AddFloraInstance(EFloraKind::SporeCap,    Node.WorldCenter + FVector(-310.0, -120.0, 0.0), EcoPalette::EmeraldMoss,  0.92f);
				AddFloraInstance(EFloraKind::EchoCrystal, Node.WorldCenter + FVector(-340.0,  110.0, 0.0), EcoPalette::EmeraldMoss,  0.90f);
				for (int32 m = 0; m < 4; ++m)
				{
					AddFloraInstance(EFloraKind::PulseMoss, Node.WorldCenter + FVector(-150.0 + m * 125.0, -245.0, 0.0), EcoPalette::EmeraldMoss, 1.05f);
				}
			}
			else
			{
				// Crypt chambers get Echo-Crystals, Prism-Lilies, and damp Pulse-Moss corners
				for (int32 i = 0; i < 2; ++i)
				{
					const FVector Offset(Rand.FRandRange(-HalfSpan, HalfSpan), Rand.FRandRange(-HalfSpan, HalfSpan), 0.0);
					AddFloraInstance(EFloraKind::PrismLily, Node.WorldCenter + Offset, EcoPalette::MagentaBloom, 0.85f);
				}
				for (int32 i = 0; i < 2; ++i)
				{
					const FVector Offset(Rand.FRandRange(-HalfSpan, HalfSpan), Rand.FRandRange(-HalfSpan, HalfSpan), 0.0);
					AddFloraInstance(EFloraKind::EchoCrystal, Node.WorldCenter + Offset, EcoPalette::AstralCyan, 0.9f);
				}
				const FVector CornerMoss = Node.WorldCenter + FVector(HalfSpan * 0.7f, -HalfSpan * 0.7f, 0.0);
				for (int32 m = 0; m < 4; ++m)
				{
					AddFloraInstance(EFloraKind::PulseMoss, CornerMoss + FVector((m % 2) * 145.0f, (m / 2) * 145.0f, 0.0), EcoPalette::EmeraldMoss, 0.95f);
				}
			}
		}
	}

	// Extra Mirror-adjacent Prism-Lilies so bouncing a beam guarantees an immediate bloom chain
	if (MirrorEntries)
	{
		for (int32 m = 0; m < MirrorEntries->Num() && m < 8; m += 2)
		{
			const FDungeoneerMirrorInstanceEntry& Mirror = (*MirrorEntries)[m];
			const FVector FrontSpot = FVector(Mirror.WorldCenter.X, Mirror.WorldCenter.Y, 0.0) + Mirror.SurfaceNormal * (TileSizeUU * 0.55f);
			AddFloraInstance(EFloraKind::PrismLily, FrontSpot, EcoPalette::MagentaBloom, 0.85f);
		}
	}

	RebuildPulseMossCSRGraph(260.0f);

	if (FloraHISM)
	{
		FloraHISM->MarkRenderStateDirty();
	}
}

void UDungeonFloraManagerComponent::PushCustomDataIfBudget(const int32 FloraIdx, int32& InOutWritesRemaining, bool& bOutAnyDirty)
{
	if (!FloraHISM || InOutWritesRemaining <= 0)
	{
		return;
	}
	const int32 InstIdx = HISMInstanceIndices.IsValidIndex(FloraIdx) ? HISMInstanceIndices[FloraIdx] : INDEX_NONE;
	if (InstIdx == INDEX_NONE)
	{
		return;
	}

	FloraHISM->SetCustomDataValue(InstIdx, EEcoCPD::UVCell, static_cast<float>(UVCells[FloraIdx]), false);
	FloraHISM->SetCustomDataValue(InstIdx, EEcoCPD::Emissive, Emissives[FloraIdx], false);
	FloraHISM->SetCustomDataValue(InstIdx, EEcoCPD::EventTime, EventTimes[FloraIdx], false);
	FloraHISM->SetCustomDataValue(InstIdx, EEcoCPD::KindFlags, EcoColors::PackKindFlags(static_cast<uint8>(Kinds[FloraIdx]), States[FloraIdx]), false);
	--InOutWritesRemaining;
	bOutAnyDirty = true;
}

void UDungeonFloraManagerComponent::IgnitePrismLily(const int32 FloraIndex, const float LeaseSeconds)
{
	if (!Kinds.IsValidIndex(FloraIndex) || Kinds[FloraIndex] != EFloraKind::PrismLily)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const double NowSec = World ? World->GetTimeSeconds() : PendingEcoTimeFallback;
	const float WrappedNow = EcoColors::WrapEcoTime(NowSec);

	const bool bStateChanged = (States[FloraIndex] == 0);
	States[FloraIndex] = 1;
	StateTimers[FloraIndex] = 4.0f; // 4s persistence before cooling back toward dormant
	Emissives[FloraIndex] = 12.0f;
	UVCells[FloraIndex] = EcoCells::PrismLilyBloom[3];
	EventTimes[FloraIndex] = WrappedNow;

	if (World)
	{
		if (UDungeonLightBudgetSubsystem* LightBudget = World->GetSubsystem<UDungeonLightBudgetSubsystem>())
		{
			LightBudget->RequestTier2Lease(LightCandidateIds[FloraIndex], LeaseSeconds);
		}
	}

	if (bStateChanged && FloraHISM && HISMInstanceIndices[FloraIndex] != INDEX_NONE)
	{
		const int32 InstIdx = HISMInstanceIndices[FloraIndex];
		FloraHISM->SetCustomDataValue(InstIdx, EEcoCPD::UVCell, static_cast<float>(UVCells[FloraIndex]), false);
		FloraHISM->SetCustomDataValue(InstIdx, EEcoCPD::Emissive, Emissives[FloraIndex], false);
		FloraHISM->SetCustomDataValue(InstIdx, EEcoCPD::EventTime, EventTimes[FloraIndex], false);
		FloraHISM->MarkRenderStateDirty();
	}
}

int32 UDungeonFloraManagerComponent::TriggerPulseMossWaveNear(const FVector& WorldPos, const float TriggerRadiusUU)
{
	if (MossIndices.Num() == 0)
	{
		return 0;
	}

	const double RadSq = FMath::Square(static_cast<double>(TriggerRadiusUU));
	int32 StartDenseMoss = INDEX_NONE;
	double BestDistSq = RadSq;

	for (int32 m = 0; m < MossIndices.Num(); ++m)
	{
		const int32 FloraIdx = MossIndices[m];
		const double DistSq = FVector::DistSquared2D(WorldPos, WorldPositions[FloraIdx]);
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			StartDenseMoss = m;
		}
	}
	if (StartDenseMoss == INDEX_NONE)
	{
		return 0;
	}

	const UWorld* World = GetWorld();
	const double NowSec = World ? World->GetTimeSeconds() : PendingEcoTimeFallback;
	++CurrentBfsEpoch;
	if (CurrentBfsEpoch == 0)
	{
		CurrentBfsEpoch = 1;
		MossVisitedEpoch.Init(0, MossIndices.Num());
	}

	int32 QueueHead = 0;
	int32 QueueTail = 0;
	BfsQueue[QueueTail] = StartDenseMoss;
	BfsHops[QueueTail] = 0;
	++QueueTail;
	MossVisitedEpoch[StartDenseMoss] = CurrentBfsEpoch;

	int32 WritesLeft = EEcoCPD::MaxCPDUpdatesPerFrame;
	bool bDirty = false;
	int32 PropagatedCount = 0;

	while (QueueHead < QueueTail)
	{
		const int32 CurDense = BfsQueue[QueueHead];
		const uint8 Hop = BfsHops[QueueHead];
		++QueueHead;

		const int32 FloraIdx = MossIndices[CurDense];
		const float WavePhaseTime = EcoColors::WrapEcoTime(NowSec + static_cast<double>(Hop) * 0.08);
		States[FloraIdx] = 1;
		StateTimers[FloraIdx] = 2.2f;
		Emissives[FloraIdx] = 6.5f;
		EventTimes[FloraIdx] = WavePhaseTime;
		UVCells[FloraIdx] = EcoCells::PulseMossStage[Hop & 3];
		PushCustomDataIfBudget(FloraIdx, WritesLeft, bDirty);
		++PropagatedCount;

		if (MossOffsets.IsValidIndex(CurDense + 1))
		{
			const int32 EdgeBegin = MossOffsets[CurDense];
			const int32 EdgeEnd = MossOffsets[CurDense + 1];
			for (int32 e = EdgeBegin; e < EdgeEnd && QueueTail < 64; ++e)
			{
				const int32 NextDense = MossAdj[e];
				if (MossVisitedEpoch[NextDense] != CurrentBfsEpoch)
				{
					MossVisitedEpoch[NextDense] = CurrentBfsEpoch;
					BfsQueue[QueueTail] = NextDense;
					BfsHops[QueueTail] = static_cast<uint8>(FMath::Min<int32>(Hop + 1, 255));
					++QueueTail;
				}
			}
		}
	}

	if (bDirty && FloraHISM)
	{
		FloraHISM->MarkRenderStateDirty();
	}
	return PropagatedCount;
}

bool UDungeonFloraManagerComponent::TriggerSporeCapNear(const FVector& WorldPos, const float TriggerRadiusUU)
{
	const UWorld* World = GetWorld();
	const double RadSq = FMath::Square(static_cast<double>(TriggerRadiusUU));
	UDungeonWaterSubsystem* WaterSub = World ? World->GetSubsystem<UDungeonWaterSubsystem>() : nullptr;
	UDungeonLightBudgetSubsystem* LightBudget = World ? World->GetSubsystem<UDungeonLightBudgetSubsystem>() : nullptr;

	int32 WritesLeft = EEcoCPD::MaxCPDUpdatesPerFrame;
	bool bDirty = false;
	bool bTriggeredAny = false;

	for (int32 i = 0; i < Kinds.Num(); ++i)
	{
		if (Kinds[i] != EFloraKind::SporeCap || StateTimers[i] > 0.0f)
		{
			continue;
		}
		if (FVector::DistSquared2D(WorldPos, WorldPositions[i]) <= RadSq)
		{
			States[i] = 1;
			StateTimers[i] = 6.0f; // 6s cooldown per cap
			Emissives[i] = 6.8f;
			UVCells[i] = EcoCells::SporeCapVariants[3];
			EventTimes[i] = EcoColors::WrapEcoTime(World ? World->GetTimeSeconds() : PendingEcoTimeFallback);
			PushCustomDataIfBudget(i, WritesLeft, bDirty);

			if (WaterSub)
			{
				WaterSub->SpawnSporePuffBurst(WorldPositions[i], 12, Hues[i]);
			}
			if (LightBudget)
			{
				LightBudget->TriggerTransientTier1Glow(WorldPositions[i] + FVector(0.0, 0.0, 28.0), Hues[i], 130.0f, 1.2f);
			}
			bTriggeredAny = true;
		}
	}

	if (bDirty && FloraHISM)
	{
		FloraHISM->MarkRenderStateDirty();
	}
	return bTriggeredAny;
}

int32 UDungeonFloraManagerComponent::EvaluateEchoCrystalsAgainstNoise()
{
	const UWorld* World = GetWorld();
	const UDungeoneerSensoryWorldSubsystem* SensoryWorld = World ? World->GetSubsystem<UDungeoneerSensoryWorldSubsystem>() : nullptr;
	if (!SensoryWorld)
	{
		return 0;
	}

	const uint64 LatestSerial = SensoryWorld->GetNoiseEventSerial();
	if (LatestSerial == LastSeenNoiseSerial)
	{
		return 0;
	}
	LastSeenNoiseSerial = LatestSerial;

	const TStaticArray<FDungeoneerNoiseEvent, UDungeoneerSensoryWorldSubsystem::MaxNoiseEvents>& NoiseRing = SensoryWorld->GetNoiseRingBuffer();
	const float WrappedNow = EcoColors::WrapEcoTime(World->GetTimeSeconds());

	int32 FlashedCount = 0;
	int32 WritesLeft = EEcoCPD::MaxCPDUpdatesPerFrame;
	bool bDirty = false;

	for (int32 i = 0; i < Kinds.Num(); ++i)
	{
		if (Kinds[i] != EFloraKind::EchoCrystal)
		{
			continue;
		}

		bool bCovered = false;
		for (int32 n = 0; n < UDungeoneerSensoryWorldSubsystem::MaxNoiseEvents; ++n)
		{
			const FDungeoneerNoiseEvent& Ev = NoiseRing[n];
			if (!Ev.bActive)
			{
				continue;
			}
			const double MaxR = static_cast<double>(Ev.MaxPulseRadius) + 80.0;
			if (FVector::DistSquared2D(WorldPositions[i], Ev.WorldOrigin) <= (MaxR * MaxR))
			{
				bCovered = true;
				break;
			}
		}

		if (bCovered)
		{
			States[i] = 1;
			StateTimers[i] = 2.0f;
			Emissives[i] = 9.0f;
			UVCells[i] = EcoCells::EchoCrystalCharge[3];
			EventTimes[i] = WrappedNow;
			PushCustomDataIfBudget(i, WritesLeft, bDirty);
			OnEchoCrystalChime.Broadcast(WorldPositions[i], i & 7);
			++FlashedCount;
			// Crucial invariant: crystals NEVER emit noise events into SensoryWorld (no feedback loop).
		}
	}

	if (bDirty && FloraHISM)
	{
		FloraHISM->MarkRenderStateDirty();
	}
	return FlashedCount;
}

void UDungeonFloraManagerComponent::StepFloraSimulation(const float DeltaSeconds)
{
	PendingEcoTimeFallback += DeltaSeconds;
	const UWorld* World = GetWorld();
	const UZombieSpatialSubsystem* SpatialSys = World ? World->GetSubsystem<UZombieSpatialSubsystem>() : nullptr;

	// 1. Test Prism-Lilies against player flashlights & Mirror-bounce beams
	if (SpatialSys)
	{
		const int32 PlayerCount = SpatialSys->GetTrackedPlayerCount();
		for (int32 p = 0; p < PlayerCount; ++p)
		{
			const ADungeoneerCharacter* Player = SpatialSys->GetTrackedPlayerAt(p);
			if (!Player)
			{
				continue;
			}
			const FVector PlayerLoc = Player->GetActorLocation();
			TriggerPulseMossWaveNear(PlayerLoc, 95.0f);
			TriggerSporeCapNear(PlayerLoc, 120.0f);

			const UMirrorLightBounceComponent* BounceComp = Player->GetMirrorBounceComponent();
			const FMirrorLightBounceResult* BounceRes = BounceComp ? &BounceComp->GetLastBounceResult() : nullptr;
			const FVector SegA = BounceRes ? BounceRes->HitLocation : FVector::ZeroVector;
			const FVector SegB = BounceRes ? (BounceRes->HitLocation + BounceRes->ReflectedDirection * BounceRes->BouncedRangeUU) : FVector::ZeroVector;

			for (int32 i = 0; i < Kinds.Num(); ++i)
			{
				if (Kinds[i] != EFloraKind::PrismLily)
				{
					continue;
				}
				const FVector& Pos = WorldPositions[i];
				const bool bHitByCone = Player->IsPointLitByFlashlight(Pos);
				const bool bHitByBounce = (BounceRes && BounceRes->bDidBounce && PointToSegmentDistSq(Pos + FVector(0.0, 0.0, 32.0), SegA, SegB) <= 3600.0f);
				if (bHitByCone || bHitByBounce)
				{
					IgnitePrismLily(i, 15.0f);
				}
			}
		}

		const int32 ZombieCount = SpatialSys->GetTrackedZombieCount();
		for (int32 z = 0; z < ZombieCount; ++z)
		{
			if (const AZombieBase* Zombie = SpatialSys->GetTrackedZombieAt(z))
			{
				if (Zombie->IsZombieAlive())
				{
					TriggerSporeCapNear(Zombie->GetActorLocation(), 110.0f);
				}
			}
		}
	}

	// 2. React Echo-Crystals to new acoustic waves
	EvaluateEchoCrystalsAgainstNoise();

	// 3. Smoothly cool down timed glow states cleanly within per-frame CPD budget
	int32 WritesLeft = EEcoCPD::MaxCPDUpdatesPerFrame;
	bool bDirty = false;
	for (int32 i = 0; i < Kinds.Num(); ++i)
	{
		if (StateTimers[i] <= 0.0f)
		{
			continue;
		}
		StateTimers[i] = FMath::Max(0.0f, StateTimers[i] - DeltaSeconds);
		if (StateTimers[i] <= 0.0f)
		{
			States[i] = 0;
			switch (Kinds[i])
			{
			case EFloraKind::PrismLily:
				Emissives[i] = 0.2f;
				UVCells[i] = EcoCells::PrismLilyBloom[0];
				break;
			case EFloraKind::SporeCap:
				Emissives[i] = 1.6f;
				UVCells[i] = EcoCells::SporeCapVariants[i & 3];
				break;
			case EFloraKind::EchoCrystal:
				Emissives[i] = 2.4f;
				UVCells[i] = EcoCells::EchoCrystalCharge[0];
				break;
			case EFloraKind::PulseMoss:
				Emissives[i] = 1.15f;
				UVCells[i] = EcoCells::PulseMossStage[i & 3];
				break;
			}
			PushCustomDataIfBudget(i, WritesLeft, bDirty);
		}
	}

	if (bDirty && FloraHISM)
	{
		FloraHISM->MarkRenderStateDirty();
	}
}

void UDungeonFloraManagerComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (Kinds.Num() == 0)
	{
		return;
	}

	SimAccumSec += FMath::Min(DeltaTime, 0.1f);
	if (SimAccumSec >= FloraDetail::FloraTickIntervalSec)
	{
		const float Step = SimAccumSec;
		SimAccumSec = 0.0f;
		StepFloraSimulation(Step);
	}
}
