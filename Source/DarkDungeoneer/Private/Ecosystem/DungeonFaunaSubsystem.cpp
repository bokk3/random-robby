#include "Ecosystem/DungeonFaunaSubsystem.h"

#include "AI/ZombieBase.h"
#include "AI/ZombieFastThrower.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "Core/DungeoneerCharacter.h"
#include "Dungeon/DungeoneerTileMapInstancer.h"
#include "Dungeon/DungeonGeneratorSubsystem.h"
#include "Ecosystem/DungeonWaterSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Lighting/DungeonLightBudgetSubsystem.h"
#include "Rendering/DungeoneerEcoMaterialFactory.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"

namespace FaunaDetail
{
	inline constexpr float FaunaTickIntervalSec = 1.0f / 30.0f;
	inline constexpr float FreezeDistSqUU = 2000.0f * 2000.0f;
	inline constexpr float MothSprintSpeedSqThreshold = (450.0f * 1.15f) * (450.0f * 1.15f);
	inline constexpr float ThrowerScatterRadiusSq = 250.0f * 250.0f;
	inline constexpr float CritterReactRadiusSq = 180.0f * 180.0f;
	inline constexpr float RoostSpeedSqThreshold = 120.0f * 120.0f;
	inline constexpr float RoostDwellSeconds = 3.0f;
	inline constexpr int32 MaxRoostedMothsPerPlayer = 3;

	template <typename TArrayType, typename TValueType>
	void FillStatic(TArrayType& Arr, const TValueType& Val) noexcept
	{
		for (auto& Elem : Arr)
		{
			Elem = Val;
		}
	}
}

void UDungeonFaunaSubsystem::ResetMetadataArrays() noexcept
{
	SoA.ResetAll();
	FaunaDetail::FillStatic(SegmentHalfSpan, 120.0f);
	FaunaDetail::FillStatic(TangentAxisFlags, static_cast<uint8>(0));
	FaunaDetail::FillStatic(DirtyCustomFlags, static_cast<uint8>(0));
	FaunaDetail::FillStatic(PlayerStillAccumSec, 0.0f);
	FaunaDetail::FillStatic(RoostedMothsForPlayer, static_cast<uint8>(0));
}

UDungeonFaunaSubsystem::UDungeonFaunaSubsystem()
{
	ResetMetadataArrays();
}

void UDungeonFaunaSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ResetMetadataArrays();

	ScratchTransforms.SetNum(MaxCritters);
	const FTransform HiddenTf(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector);
	for (int32 i = 0; i < MaxCritters; ++i)
	{
		ScratchTransforms[i] = HiddenTf;
	}
	ActiveCritterCount = 0;
	BeetlesCount = 0;
}

void UDungeonFaunaSubsystem::Deinitialize()
{
	BoundFaunaISM.Reset();
	SoA.ResetAll();
	ActiveCritterCount = 0;
	Super::Deinitialize();
}

float UDungeonFaunaSubsystem::ResolveWrappedEcoTime() const noexcept
{
	const UWorld* World = GetWorld();
	const double TimeSec = World ? World->GetTimeSeconds() : static_cast<double>(PendingEcoTimeFallback);
	return EcoColors::WrapEcoTime(TimeSec);
}

void UDungeonFaunaSubsystem::EnsureFaunaISMConfigured(UInstancedStaticMeshComponent* FaunaISM)
{
	if (!FaunaISM)
	{
		return;
	}

	FaunaISM->ClearInstances();
	FaunaISM->NumCustomDataFloats = EEcoCPD::Count;
	FaunaISM->SetCastShadow(false);
	FaunaISM->bCastHiddenShadow = false;
	FaunaISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FaunaISM->SetCullDistances(0, 2800);

	if (UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
	{
		FaunaISM->SetStaticMesh(PlaneMesh);
	}

	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	if (UDungeoneerAssetBootstrapSubsystem* Bootstrap = GI ? GI->GetSubsystem<UDungeoneerAssetBootstrapSubsystem>() : nullptr)
	{
		if (UMaterialInterface* EcoMat = Bootstrap->EnsureEcoInstancedMaterial())
		{
			UMaterialInstanceDynamic* FaunaMID = FDungeoneerEcoMaterialFactory::CreateInstancedAtlasMID(
				FaunaISM,
				EcoMat,
				Bootstrap->GetLoadedEcosystemTexture(FName(TEXT("PROP_Fauna_Critters_Strip_4x1_D"))),
				Bootstrap->GetLoadedEcosystemTexture(FName(TEXT("PROP_Fauna_Critters_Strip_4x1_N"))),
				Bootstrap->GetLoadedEcosystemTexture(FName(TEXT("PROP_Fauna_Critters_Strip_4x1_E"))),
				EDungeoneerMaterialLayoutMode::Eco8Float,
				false,
				128.0f,
				4,
				1,
				0.78f,
				1.0f,
				0.0f);
			FaunaISM->SetMaterial(0, FaunaMID);
		}
	}

	const FTransform HiddenTf(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector);
	for (int32 i = 0; i < MaxCritters; ++i)
	{
		ScratchTransforms[i] = HiddenTf;
		const int32 InstIdx = FaunaISM->AddInstance(HiddenTf, false);
		if (InstIdx != INDEX_NONE)
		{
			for (int32 c = 0; c < EEcoCPD::Count; ++c)
			{
				FaunaISM->SetCustomDataValue(InstIdx, c, 0.0f, false);
			}
		}
	}
}

void UDungeonFaunaSubsystem::OnFloorBuilt(const FFaunaFloorBuildInput& Input)
{
	ResetMetadataArrays();
	ActiveCritterCount = 0;
	BeetlesCount = 0;
	SimAccumSec = 0.0f;

	if (ScratchTransforms.Num() != MaxCritters)
	{
		ScratchTransforms.SetNum(MaxCritters);
	}
	const FTransform HiddenTf(FRotator::ZeroRotator, FVector::ZeroVector, FVector::ZeroVector);
	for (int32 i = 0; i < MaxCritters; ++i)
	{
		ScratchTransforms[i] = HiddenTf;
	}

	BoundFaunaISM = Input.FaunaISM;
	EnsureFaunaISMConfigured(Input.FaunaISM);

	const FDungeonFloorGraph* Graph = Input.FloorGraph;
	if (!Graph)
	{
		return;
	}

	UWorld* World = GetWorld();
	UDungeonLightBudgetSubsystem* LightBudget = World ? World->GetSubsystem<UDungeonLightBudgetSubsystem>() : nullptr;
	UDungeonWaterSubsystem* WaterSub = World ? World->GetSubsystem<UDungeonWaterSubsystem>() : nullptr;
	FRandomStream FaunaRand(Graph->MasterSeed ^ 0xFA04A311);

	// 1. Seed Lumen-Moths / Fireflies orbiting sconces, shrines, Entry room, and Prism-Lily patches
	for (const FDungeonNode& Node : Graph->Nodes)
	{
		if (ActiveCritterCount >= 120)
		{
			break;
		}
		const int32 MothsInRoom = (Node.RoomType == EDungeonRoomType::Entry || Node.RoomType == EDungeonRoomType::RestingGround)
			? 6
			: (Node.BiomeType == EDungeonBiomeType::SporeFens ? 5 : 3);
		for (int32 m = 0; m < MothsInRoom && ActiveCritterCount < 120; ++m)
		{
			const float Angle = static_cast<float>(m) * (UE_TWO_PI / static_cast<float>(FMath::Max(MothsInRoom, 1)));
			const float RingR = (Node.RoomType == EDungeonRoomType::Entry) ? 210.0f : FaunaRand.FRandRange(120.0f, 320.0f);
			const FVector Anchor = Node.WorldCenter + FVector(FMath::Cos(Angle) * RingR, FMath::Sin(Angle) * RingR, FaunaRand.FRandRange(72.0f, 132.0f));
			const int32 Idx = SpawnScriptedCritter(EFaunaSpecies::LumenMoth, Anchor, INDEX_NONE);
			if (Idx != INDEX_NONE)
			{
				SoA.Phase[Idx] = FaunaRand.FRandRange(0.0f, UE_TWO_PI);
				SoA.Home[Idx] = FVector3f(
					static_cast<float>(Node.WorldCenter.X + FMath::Cos(Angle) * (RingR * 0.45f)),
					static_cast<float>(Node.WorldCenter.Y + FMath::Sin(Angle) * (RingR * 0.45f)),
					static_cast<float>(Anchor.Z));
			}
		}
	}

	// 2. Seed Lantern-Beetles crawling along 4-axis wall tangents (up to 48, each a moving Tier 1 light node)
	if (Input.WallEntries && Input.WallEntries->Num() > 0)
	{
		const int32 WallStep = FMath::Max(1, Input.WallEntries->Num() / 42);
		for (int32 w = 0; w < Input.WallEntries->Num() && BeetlesCount < 42 && ActiveCritterCount < MaxCritters; w += WallStep)
		{
			const FDungeoneerWallInstanceEntry& Wall = (*Input.WallEntries)[w];
			// Skip foreground North/West cutaway occluders so beetles stay crisp against visible South/East wall plinths
			if (Wall.WallAxis == EDungeoneerWallAxis::North ||
				Wall.WallAxis == EDungeoneerWallAxis::West ||
				Wall.WallAxis == EDungeoneerWallAxis::NorthWest ||
				Wall.WallAxis == EDungeoneerWallAxis::SouthWest)
			{
				continue;
			}

			const bool bHorizontalWall = (Wall.WallAxis == EDungeoneerWallAxis::South);
			const int32 TangentAxis = bHorizontalWall ? 0 : 1; // 0=X tangent, 1=Y tangent
			const double NudgeIn = (Wall.WallAxis == EDungeoneerWallAxis::South) ? 34.0
				: ((Wall.WallAxis == EDungeoneerWallAxis::East) ? -34.0 : 34.0);
			const FVector StartPos(
				Wall.WorldCenter.X + (bHorizontalWall ? 0.0 : NudgeIn),
				Wall.WorldCenter.Y + (bHorizontalWall ? NudgeIn : 0.0),
				14.0);

			int32 MovingLightId = INDEX_NONE;
			if (LightBudget)
			{
				FDungeonLightCandidate C;
				C.WorldPosition = StartPos;
				C.Color = (BeetlesCount & 1) ? EcoPalette::AmberFire : EcoPalette::EmeraldMoss;
				C.RadiusUU = 115.0f;
				C.BaseIntensity = 210.0f;
				C.RequestedTier = EDungeonLightTier::Tier1_MPCGlow;
				C.bMoving = true;
				MovingLightId = LightBudget->RegisterCandidate(C);
			}

			const float HalfLen = static_cast<float>(FMath::Max(Wall.WorldHalfExtents.X, Wall.WorldHalfExtents.Y)) * 0.72f;
			const int32 Idx = SpawnScriptedCritter(EFaunaSpecies::LanternBeetle, StartPos, MovingLightId, HalfLen, TangentAxis);
			if (Idx != INDEX_NONE)
			{
				SoA.Phase[Idx] = FaunaRand.FRandRange(0.0f, UE_TWO_PI);
				++BeetlesCount;
			}
		}
	}

	// 3. Seed Puddle-Newts & Crypt-Crabs strictly on ShallowPuddle / AnkleDeepStream surfaces
	if (Input.PuddleLocations)
	{
		for (int32 p = 0; p < Input.PuddleLocations->Num() && ActiveCritterCount < MaxCritters; ++p)
		{
			const FVector PuddlePos = (*Input.PuddleLocations)[p];
			const EFaunaSpecies AquaticKind = (p & 1) ? EFaunaSpecies::CryptCrab : EFaunaSpecies::PuddleNewt;
			const FVector SpawnSpot(PuddlePos.X, PuddlePos.Y, 6.0);
			if (!WaterSub ||
				WaterSub->ResolveSurface(SpawnSpot) == EDungeonSurfaceKind::ShallowPuddle ||
				WaterSub->ResolveSurface(SpawnSpot) == EDungeonSurfaceKind::AnkleDeepStream)
			{
				SpawnScriptedCritter(AquaticKind, SpawnSpot, INDEX_NONE, 85.0f, p & 1);
			}
		}
	}

	// Also guarantee Entry room gets a couple of visible puddle critters near its stream
	for (const FDungeonNode& Node : Graph->Nodes)
	{
		if (Node.RoomType == EDungeonRoomType::Entry && ActiveCritterCount + 2 <= MaxCritters)
		{
			SpawnScriptedCritter(EFaunaSpecies::PuddleNewt, Node.WorldCenter + FVector(-110.0, -245.0, 6.0), INDEX_NONE, 90.0f, 0);
			SpawnScriptedCritter(EFaunaSpecies::CryptCrab,  Node.WorldCenter + FVector( 125.0, -250.0, 6.0), INDEX_NONE, 90.0f, 1);
			break;
		}
	}

	StepFaunaSimulation(FaunaDetail::FaunaTickIntervalSec);
}

int32 UDungeonFaunaSubsystem::SpawnScriptedCritter(
	const EFaunaSpecies Species,
	const FVector& WorldPos,
	const int32 LightCandidateId,
	const float SegmentHalfLength,
	const int32 TangentAxis)
{
	int32 Slot = INDEX_NONE;
	for (int32 i = 0; i < MaxCritters; ++i)
	{
		if (SoA.bActive[i] == 0)
		{
			Slot = i;
			break;
		}
	}
	if (Slot == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	SoA.bActive[Slot] = 1;
	SoA.Species[Slot] = static_cast<uint8>(Species);
	SoA.Pos[Slot] = FVector3f(static_cast<float>(WorldPos.X), static_cast<float>(WorldPos.Y), static_cast<float>(WorldPos.Z));
	SoA.Home[Slot] = SoA.Pos[Slot];
	SoA.Phase[Slot] = static_cast<float>(Slot) * 0.73f;
	SoA.NoiseCooldown[Slot] = 0.0f;
	SoA.StateTimer[Slot] = 0.0f;
	SoA.EventTime[Slot] = ResolveWrappedEcoTime();
	SoA.Stretch[Slot] = 1.0f;
	SoA.AnchorLight[Slot] = (LightCandidateId >= 0 && LightCandidateId < 65535) ? static_cast<uint16>(LightCandidateId) : MAX_uint16;
	SegmentHalfSpan[Slot] = FMath::Max(SegmentHalfLength, 32.0f);
	TangentAxisFlags[Slot] = static_cast<uint8>(TangentAxis & 3);
	DirtyCustomFlags[Slot] = 1;

	switch (Species)
	{
	case EFaunaSpecies::LumenMoth:
		SoA.State[Slot] = static_cast<uint8>(EFaunaState::Orbit);
		SoA.Vel[Slot] = FVector3f(36.0f, -24.0f, 0.0f);
		SoA.Emissive[Slot] = 1.15f;
		break;
	case EFaunaSpecies::LanternBeetle:
		SoA.State[Slot] = static_cast<uint8>(EFaunaState::Scuttle);
		SoA.Vel[Slot] = (TangentAxis == 0) ? FVector3f(52.0f, 0.0f, 0.0f) : FVector3f(0.0f, 52.0f, 0.0f);
		SoA.Emissive[Slot] = 1.05f;
		break;
	case EFaunaSpecies::PuddleNewt:
	case EFaunaSpecies::CryptCrab:
		SoA.State[Slot] = static_cast<uint8>(EFaunaState::Idle);
		SoA.Vel[Slot] = FVector3f::ZeroVector;
		SoA.Emissive[Slot] = 0.75f;
		break;
	}

	++ActiveCritterCount;
	return Slot;
}

void UDungeonFaunaSubsystem::StepFaunaSimulation(const float DeltaSeconds)
{
	if (ActiveCritterCount <= 0)
	{
		return;
	}

	PendingEcoTimeFallback += DeltaSeconds;
	const float WrappedTime = ResolveWrappedEcoTime();

	UWorld* World = GetWorld();
	UZombieSpatialSubsystem* SpatialSys = World ? World->GetSubsystem<UZombieSpatialSubsystem>() : nullptr;
	UDungeonLightBudgetSubsystem* LightBudget = World ? World->GetSubsystem<UDungeonLightBudgetSubsystem>() : nullptr;
	UDungeonWaterSubsystem* WaterSub = World ? World->GetSubsystem<UDungeonWaterSubsystem>() : nullptr;
	UDungeoneerSensoryWorldSubsystem* SensoryWorld = World ? World->GetSubsystem<UDungeoneerSensoryWorldSubsystem>() : nullptr;

	// Track player positions, stillness timers (roosting), and sprints
	const int32 PlayerCount = SpatialSys ? FMath::Min(SpatialSys->GetTrackedPlayerCount(), MaxTrackedPlayers) : 0;
	for (int32 p = 0; p < PlayerCount; ++p)
	{
		RoostedMothsForPlayer[p] = 0;
		if (const ADungeoneerCharacter* Player = SpatialSys->GetTrackedPlayerAt(p))
		{
			const float SpeedSq = static_cast<float>(Player->GetVelocity().SizeSquared2D());
			if (SpeedSq < FaunaDetail::RoostSpeedSqThreshold)
			{
				PlayerStillAccumSec[p] = FMath::Min(10.0f, PlayerStillAccumSec[p] + DeltaSeconds);
			}
			else
			{
				PlayerStillAccumSec[p] = 0.0f;
			}
		}
		else
		{
			PlayerStillAccumSec[p] = 0.0f;
		}
	}

	const int32 ZombieCount = SpatialSys ? SpatialSys->GetTrackedZombieCount() : 0;
	FRotator SpriteRot(48.0f, -45.0f, 0.0f);
	if (const UGameInstance* GI = World ? World->GetGameInstance() : nullptr)
	{
		if (const UDungeoneerAssetBootstrapSubsystem* Bootstrap = GI->GetSubsystem<UDungeoneerAssetBootstrapSubsystem>())
		{
			SpriteRot = Bootstrap->GetCameraFacingSpritePlaneRotation();
		}
	}

	for (int32 i = 0; i < MaxCritters; ++i)
	{
		if (SoA.bActive[i] == 0)
		{
			continue;
		}

		if (SoA.NoiseCooldown[i] > 0.0f)
		{
			SoA.NoiseCooldown[i] = FMath::Max(0.0f, SoA.NoiseCooldown[i] - DeltaSeconds);
		}
		if (SoA.StateTimer[i] > 0.0f)
		{
			SoA.StateTimer[i] = FMath::Max(0.0f, SoA.StateTimer[i] - DeltaSeconds);
		}
		SoA.Phase[i] += DeltaSeconds * 7.5f;

		// Check distance freeze (> 2000 UU from every tracked player)
		const FVector CritterPos(SoA.Pos[i]);
		if (PlayerCount > 0)
		{
			double MinPlayerDistSq = TNumericLimits<double>::Max();
			for (int32 p = 0; p < PlayerCount; ++p)
			{
				if (const ADungeoneerCharacter* Player = SpatialSys->GetTrackedPlayerAt(p))
				{
					MinPlayerDistSq = FMath::Min(MinPlayerDistSq, FVector::DistSquared2D(CritterPos, Player->GetActorLocation()));
				}
			}
			if (MinPlayerDistSq > static_cast<double>(FaunaDetail::FreezeDistSqUU))
			{
				ScratchTransforms[i] = FTransform(SpriteRot, CritterPos, FVector(0.40, 0.40, 1.0));
				continue;
			}
		}

		const EFaunaSpecies Spec = static_cast<EFaunaSpecies>(SoA.Species[i]);
		if (Spec == EFaunaSpecies::LumenMoth)
		{
			// 1. Check sprint or nearby Fast Thrower scatter trigger
			bool bScattered = false;
			FVector ThreatPos = FVector::ZeroVector;
			for (int32 p = 0; p < PlayerCount; ++p)
			{
				if (const ADungeoneerCharacter* Player = SpatialSys->GetTrackedPlayerAt(p))
				{
					const double DistSq = FVector::DistSquared2D(CritterPos, Player->GetActorLocation());
					const float SpeedSq = static_cast<float>(Player->GetVelocity().SizeSquared2D());
					const bool bSprinting = (SpeedSq > FaunaDetail::MothSprintSpeedSqThreshold) ||
						(Player->GetCombatActionState() == EDungeoneerCombatActionState::DodgeDash);
					if (bSprinting && DistSq < 340.0 * 340.0)
					{
						bScattered = true;
						ThreatPos = Player->GetActorLocation();
						break;
					}
				}
			}
			if (!bScattered && SpatialSys)
			{
				for (int32 z = 0; z < ZombieCount; ++z)
				{
					if (const AZombieFastThrower* Thrower = Cast<AZombieFastThrower>(SpatialSys->GetTrackedZombieAt(z)))
					{
						if (FVector::DistSquared2D(CritterPos, Thrower->GetActorLocation()) <= static_cast<double>(FaunaDetail::ThrowerScatterRadiusSq))
						{
							bScattered = true;
							ThreatPos = Thrower->GetActorLocation();
							break;
						}
					}
				}
			}

			if (bScattered)
			{
				FVector3f Away = SoA.Pos[i] - FVector3f(static_cast<float>(ThreatPos.X), static_cast<float>(ThreatPos.Y), SoA.Pos[i].Z);
				if (Away.SizeSquared2D() < 1.0f)
				{
					Away = FVector3f(1.0f, 0.25f, 0.0f);
				}
				Away.Normalize();
				SoA.Vel[i] = Away * 290.0f + FVector3f(0.0f, 0.0f, 38.0f);
				SoA.State[i] = static_cast<uint8>(EFaunaState::Flee);
				SoA.StateTimer[i] = 2.2f;
				SoA.Emissive[i] = 4.6f;
				SoA.Stretch[i] = 1.65f;
				SoA.EventTime[i] = WrappedTime;
				DirtyCustomFlags[i] = 1;
			}
			else if (SoA.State[i] == static_cast<uint8>(EFaunaState::Flee))
			{
				SoA.Pos[i] += SoA.Vel[i] * DeltaSeconds;
				SoA.Vel[i] *= FMath::Clamp(1.0f - DeltaSeconds * 1.6f, 0.0f, 1.0f);
				if (SoA.StateTimer[i] <= 0.0f)
				{
					SoA.State[i] = static_cast<uint8>(EFaunaState::Orbit);
					SoA.Emissive[i] = 3.2f;
					SoA.Stretch[i] = 1.0f;
					DirtyCustomFlags[i] = 1;
				}
			}
			else
			{
				// Check lantern roost eligibility (<120 UU/s for 3s, up to 3 nearest moths)
				int32 RoostPlayerIdx = INDEX_NONE;
				FVector RoostTarget = FVector::ZeroVector;
				for (int32 p = 0; p < PlayerCount; ++p)
				{
					if (PlayerStillAccumSec[p] >= FaunaDetail::RoostDwellSeconds &&
						RoostedMothsForPlayer[p] < FaunaDetail::MaxRoostedMothsPerPlayer)
					{
						if (const ADungeoneerCharacter* Player = SpatialSys->GetTrackedPlayerAt(p))
						{
							if (FVector::DistSquared2D(CritterPos, Player->GetActorLocation()) < 520.0 * 520.0)
							{
								RoostPlayerIdx = p;
								RoostTarget = Player->GetActorLocation() + FVector(
									FMath::Cos(SoA.Phase[i] * 0.35f) * 42.0,
									FMath::Sin(SoA.Phase[i] * 0.35f) * 42.0,
									114.0);
								break;
							}
						}
					}
				}

				if (RoostPlayerIdx != INDEX_NONE)
				{
					++RoostedMothsForPlayer[RoostPlayerIdx];
					SoA.State[i] = static_cast<uint8>(EFaunaState::Roost);
					SoA.Pos[i] = FVector3f(FMath::VInterpTo(FVector(SoA.Pos[i]), RoostTarget, DeltaSeconds, 4.5f));
				}
				else
				{
					SoA.State[i] = static_cast<uint8>(EFaunaState::Orbit);
					const FVector3f Center = SoA.Home[i];
					FVector3f Radial = SoA.Pos[i] - Center;
					Radial.Z = 0.0f;
					const float RLen = FMath::Max(Radial.Size2D(), 1.0f);
					const FVector3f DirR = Radial / RLen;
					const FVector3f Tangent(-DirR.Y, DirR.X, 0.0f);

					// Cohesion pull toward target orbit ring (88 UU) + swirl + slight neighbor separation
					FVector3f Sep = FVector3f::ZeroVector;
					for (int32 k = FMath::Max(0, i - 6); k < FMath::Min(MaxCritters, i + 6); ++k)
					{
						if (k != i && SoA.bActive[k] && SoA.Species[k] == SoA.Species[i])
						{
							const FVector3f Diff = SoA.Pos[i] - SoA.Pos[k];
							const float DSq = Diff.SizeSquared();
							if (DSq > 0.1f && DSq < 36.0f * 36.0f)
							{
								Sep += Diff.GetSafeNormal() * 65.0f;
							}
						}
					}

					const float RadialPull = (88.0f - RLen) * 1.8f;
					const float VerticalTargetZ = Center.Z + FMath::Sin(SoA.Phase[i] * 0.6f) * 18.0f;
					SoA.Vel[i] = Tangent * 78.0f + DirR * RadialPull + Sep + FVector3f(0.0f, 0.0f, (VerticalTargetZ - SoA.Pos[i].Z) * 3.0f);
					SoA.Pos[i] += SoA.Vel[i] * DeltaSeconds;
				}
			}

			// Procedural wing-flap shimmer in Y scale until 4x4 animated moth sheet replaces strip
			const float WingFlap = 0.25f + 0.06f * FMath::Sin(SoA.Phase[i] * 2.4f);
			const float StretchMul = (SoA.State[i] == static_cast<uint8>(EFaunaState::Flee)) ? 1.35f : 1.0f;
			ScratchTransforms[i] = FTransform(SpriteRot, FVector(SoA.Pos[i]), FVector(WingFlap * StretchMul, 0.26, 1.0));
		}
		else if (Spec == EFaunaSpecies::LanternBeetle)
		{
			SoA.State[i] = static_cast<uint8>(EFaunaState::Scuttle);
			const FVector3f DiffFromHome = SoA.Pos[i] - SoA.Home[i];
			const float HalfSpan = SegmentHalfSpan[i];
			const bool bAxisX = (TangentAxisFlags[i] == 0);
			if (bAxisX)
			{
				if (DiffFromHome.X > HalfSpan && SoA.Vel[i].X > 0.0f) { SoA.Vel[i].X = -FMath::Abs(SoA.Vel[i].X); }
				else if (DiffFromHome.X < -HalfSpan && SoA.Vel[i].X < 0.0f) { SoA.Vel[i].X = FMath::Abs(SoA.Vel[i].X); }
			}
			else
			{
				if (DiffFromHome.Y > HalfSpan && SoA.Vel[i].Y > 0.0f) { SoA.Vel[i].Y = -FMath::Abs(SoA.Vel[i].Y); }
				else if (DiffFromHome.Y < -HalfSpan && SoA.Vel[i].Y < 0.0f) { SoA.Vel[i].Y = FMath::Abs(SoA.Vel[i].Y); }
			}
			SoA.Pos[i] += SoA.Vel[i] * DeltaSeconds;
			if (LightBudget && SoA.AnchorLight[i] != MAX_uint16)
			{
				LightBudget->UpdateMovingCandidate(static_cast<int32>(SoA.AnchorLight[i]), FVector(SoA.Pos[i]));
			}
			const float Bob = 0.26f + 0.03f * FMath::Sin(SoA.Phase[i] * 3.0f);
			ScratchTransforms[i] = FTransform(SpriteRot, FVector(SoA.Pos[i]), FVector(Bob, 0.26, 1.0));
		}
		else // PuddleNewt or CryptCrab
		{
			bool bNearThreat = false;
			FVector ThreatOrigin = FVector::ZeroVector;
			for (int32 p = 0; p < PlayerCount; ++p)
			{
				if (const ADungeoneerCharacter* Player = SpatialSys->GetTrackedPlayerAt(p))
				{
					if (FVector::DistSquared2D(CritterPos, Player->GetActorLocation()) <= static_cast<double>(FaunaDetail::CritterReactRadiusSq))
					{
						bNearThreat = true;
						ThreatOrigin = Player->GetActorLocation();
						break;
					}
				}
			}
			if (!bNearThreat && SpatialSys)
			{
				for (int32 z = 0; z < ZombieCount; ++z)
				{
					if (const AZombieBase* Z = SpatialSys->GetTrackedZombieAt(z))
					{
						if (FVector::DistSquared2D(CritterPos, Z->GetActorLocation()) <= static_cast<double>(FaunaDetail::CritterReactRadiusSq))
						{
							bNearThreat = true;
							ThreatOrigin = Z->GetActorLocation();
							break;
						}
					}
				}
			}

			if (bNearThreat && SoA.NoiseCooldown[i] <= 0.0f)
			{
				FVector3f Away = SoA.Pos[i] - FVector3f(static_cast<float>(ThreatOrigin.X), static_cast<float>(ThreatOrigin.Y), SoA.Pos[i].Z);
				if (Away.SizeSquared2D() < 1.0f)
				{
					Away = FVector3f(0.8f, 0.6f, 0.0f);
				}
				Away.Normalize();
				SoA.Vel[i] = Away * 185.0f;
				SoA.State[i] = static_cast<uint8>(EFaunaState::Dart);
				SoA.StateTimer[i] = 0.75f;
				SoA.NoiseCooldown[i] = 4.0f; // 4s per-critter cooldown
				SoA.EventTime[i] = WrappedTime;
				DirtyCustomFlags[i] = 1;

				if (WaterSub)
				{
					WaterSub->SpawnRippleAt(FVector(SoA.Pos[i]), EcoPalette::AstralCyan, 0.75f);
				}
				if (SensoryWorld)
				{
					// Low loudness 0.25 decoy ring: well below the 1.55 zombie lure threshold
					SensoryWorld->BroadcastNoiseEvent(FVector(SoA.Pos[i]), 0.25f, 320.0f, false);
				}
			}
			else if (SoA.State[i] == static_cast<uint8>(EFaunaState::Dart))
			{
				SoA.Pos[i] += SoA.Vel[i] * DeltaSeconds;
				SoA.Vel[i] *= FMath::Clamp(1.0f - DeltaSeconds * 3.0f, 0.0f, 1.0f);
				if (SoA.StateTimer[i] <= 0.0f)
				{
					SoA.State[i] = static_cast<uint8>(EFaunaState::Idle);
				}
			}
			else
			{
				// Stay near home puddle cell
				const FVector3f ToHome = SoA.Home[i] - SoA.Pos[i];
				if (ToHome.SizeSquared2D() > 18.0f * 18.0f)
				{
					SoA.Pos[i] += ToHome.GetSafeNormal() * (32.0f * DeltaSeconds);
				}
			}

			ScratchTransforms[i] = FTransform(SpriteRot, FVector(SoA.Pos[i]), FVector(0.36, 0.36, 1.0));
		}
	}

	if (UInstancedStaticMeshComponent* FaunaISM = BoundFaunaISM.Get())
	{
		FaunaISM->BatchUpdateInstancesTransforms(0, ScratchTransforms, /*bWorldSpace=*/true, /*bMarkRenderStateDirty=*/true, /*bTeleport=*/true);

		int32 UpdatesRemaining = EEcoCPD::MaxCPDUpdatesPerFrame;
		bool bAnyCustomDirty = false;
		for (int32 i = 0; i < MaxCritters && UpdatesRemaining > 0; ++i)
		{
			if (DirtyCustomFlags[i])
			{
				DirtyCustomFlags[i] = 0;
				const int32 CellIdx = EcoCells::FaunaStrip[FMath::Clamp<int32>(SoA.Species[i], 0, 3)];
				const FLinearColor Tint =
					(SoA.Species[i] == static_cast<uint8>(EFaunaSpecies::LumenMoth)) ? EcoPalette::GoldenRune :
					(SoA.Species[i] == static_cast<uint8>(EFaunaSpecies::LanternBeetle)) ? EcoPalette::EmeraldMoss :
					(SoA.Species[i] == static_cast<uint8>(EFaunaSpecies::PuddleNewt)) ? EcoPalette::AstralCyan :
					EcoPalette::MagentaBloom;
				FaunaISM->SetCustomDataValue(i, EEcoCPD::UVCell, static_cast<float>(CellIdx), false);
				FaunaISM->SetCustomDataValue(i, EEcoCPD::Emissive, SoA.Emissive[i], false);
				FaunaISM->SetCustomDataValue(i, EEcoCPD::HueR, Tint.R, false);
				FaunaISM->SetCustomDataValue(i, EEcoCPD::HueG, Tint.G, false);
				FaunaISM->SetCustomDataValue(i, EEcoCPD::HueB, Tint.B, false);
				FaunaISM->SetCustomDataValue(i, EEcoCPD::EventTime, SoA.EventTime[i], false);
				FaunaISM->SetCustomDataValue(i, EEcoCPD::ParamA, SoA.Stretch[i], false);
				FaunaISM->SetCustomDataValue(i, EEcoCPD::KindFlags, EcoColors::PackKindFlags(SoA.Species[i], SoA.State[i]), false);
				--UpdatesRemaining;
				bAnyCustomDirty = true;
			}
		}
		if (bAnyCustomDirty)
		{
			FaunaISM->MarkRenderStateDirty();
		}
	}
}

void UDungeonFaunaSubsystem::Tick(const float DeltaTime)
{
	if (ActiveCritterCount <= 0)
	{
		return;
	}

	SimAccumSec += FMath::Min(DeltaTime, 0.1f);
	if (SimAccumSec >= FaunaDetail::FaunaTickIntervalSec)
	{
		const float Step = FaunaDetail::FaunaTickIntervalSec;
		SimAccumSec -= Step;
		if (SimAccumSec > FaunaDetail::FaunaTickIntervalSec)
		{
			SimAccumSec = 0.0f;
		}
		StepFaunaSimulation(Step);
	}
}

bool UDungeonFaunaSubsystem::FindNearestAudibleBioEmitter(
	const FVector& ListenerPos,
	FVector& OutWorldPos,
	EFaunaSpecies& OutSpecies) const noexcept
{
	double BestDistSq = TNumericLimits<double>::Max();
	bool bFound = false;
	for (int32 i = 0; i < MaxCritters; ++i)
	{
		if (SoA.bActive[i] == 0)
		{
			continue;
		}
		const EFaunaSpecies Spec = static_cast<EFaunaSpecies>(SoA.Species[i]);
		if (Spec != EFaunaSpecies::LumenMoth && Spec != EFaunaSpecies::LanternBeetle)
		{
			continue;
		}
		const FVector CandidatePos(SoA.Pos[i]);
		const double DistSq = FVector::DistSquared2D(ListenerPos, CandidatePos);
		if (DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			OutWorldPos = CandidatePos;
			OutSpecies = Spec;
			bFound = true;
		}
	}
	return bFound;
}
