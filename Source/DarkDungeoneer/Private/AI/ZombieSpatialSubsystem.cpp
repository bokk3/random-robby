#include "AI/ZombieSpatialSubsystem.h"
#include "AI/BossGuardianActor.h"
#include "AI/ZombieBase.h"
#include "AI/ZombieExploder.h"
#include "AI/ZombieFastThrower.h"
#include "AI/HazardZoneActors.h"
#include "Components/DecalComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "Core/DungeoneerCharacter.h"
#include "Engine/DecalActor.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

void UZombieSpatialSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	NumTrackedPlayers = 0;
	NumTrackedZombies = 0;
	SpatialGrid = MakeUnique<FSpatialGridBuffer>();
	for (int32 i = 0; i < SpatialGrid->Cells.Num(); ++i)
	{
		SpatialGrid->Cells[i].Reset();
	}
}

void UZombieSpatialSubsystem::SetActiveBossGuardian(ABossGuardianActor* InBoss) noexcept
{
	ActiveBossGuardian = InBoss;
}

ABossGuardianActor* UZombieSpatialSubsystem::GetActiveBossGuardian() const noexcept
{
	return ActiveBossGuardian.Get();
}

void UZombieSpatialSubsystem::Deinitialize()
{
	SpatialGrid.Reset();
	FirePool.Reset();
	PoisonPool.Reset();
	ChronoGasPool.Reset();
	ScorchDecalPool.Reset();
	bPoolsPreallocated = false;
	Super::Deinitialize();
}

void UZombieSpatialSubsystem::RegisterPlayer(ADungeoneerCharacter* Player)
{
	if (!Player)
	{
		return;
	}

	for (int32 i = 0; i < NumTrackedPlayers; ++i)
	{
		if (TrackedPlayers[i].Get() == Player)
		{
			return;
		}
	}

	if (NumTrackedPlayers < MaxTrackedPlayers)
	{
		TrackedPlayers[NumTrackedPlayers++] = Player;
	}
}

void UZombieSpatialSubsystem::UnregisterPlayer(ADungeoneerCharacter* Player)
{
	for (int32 i = 0; i < NumTrackedPlayers; ++i)
	{
		if (TrackedPlayers[i].Get() == Player || !TrackedPlayers[i].IsValid())
		{
			TrackedPlayers[i] = TrackedPlayers[NumTrackedPlayers - 1];
			TrackedPlayers[NumTrackedPlayers - 1] = nullptr;
			--NumTrackedPlayers;
			--i;
		}
	}
}

int32 UZombieSpatialSubsystem::GetAliveOtherPlayerCount(const ADungeoneerCharacter* ExcludePlayer) const noexcept
{
	int32 AliveCount = 0;
	for (int32 i = 0; i < NumTrackedPlayers; ++i)
	{
		const ADungeoneerCharacter* Candidate = TrackedPlayers[i].Get();
		if (Candidate && Candidate != ExcludePlayer && !Candidate->IsDowned())
		{
			++AliveCount;
		}
	}
	return AliveCount;
}

ADungeoneerCharacter* UZombieSpatialSubsystem::FindNearestDownedAlly(const ADungeoneerCharacter* QueryPlayer, float MaxRangeUU) const noexcept
{
	if (!QueryPlayer || MaxRangeUU <= 0.0f)
	{
		return nullptr;
	}

	const FVector QueryOrigin = QueryPlayer->GetActorLocation();
	double BestDistSq = static_cast<double>(MaxRangeUU) * static_cast<double>(MaxRangeUU);
	ADungeoneerCharacter* BestAlly = nullptr;

	for (int32 i = 0; i < NumTrackedPlayers; ++i)
	{
		ADungeoneerCharacter* Candidate = TrackedPlayers[i].Get();
		if (Candidate && Candidate != QueryPlayer && Candidate->IsDowned())
		{
			const double DistSq = FVector::DistSquared2D(QueryOrigin, Candidate->GetActorLocation());
			if (DistSq <= BestDistSq)
			{
				BestDistSq = DistSq;
				BestAlly = Candidate;
			}
		}
	}
	return BestAlly;
}

ADungeoneerCharacter* UZombieSpatialSubsystem::FindNearestLivingAlly(const ADungeoneerCharacter* QueryPlayer, float MaxRangeUU) const noexcept
{
	if (!QueryPlayer || MaxRangeUU <= 0.0f)
	{
		return nullptr;
	}

	const FVector QueryOrigin = QueryPlayer->GetActorLocation();
	double BestDistSq = static_cast<double>(MaxRangeUU) * static_cast<double>(MaxRangeUU);
	ADungeoneerCharacter* BestAlly = nullptr;

	for (int32 i = 0; i < NumTrackedPlayers; ++i)
	{
		ADungeoneerCharacter* Candidate = TrackedPlayers[i].Get();
		if (Candidate && Candidate != QueryPlayer && !Candidate->IsDowned())
		{
			const double DistSq = FVector::DistSquared2D(QueryOrigin, Candidate->GetActorLocation());
			if (DistSq <= BestDistSq)
			{
				BestDistSq = DistSq;
				BestAlly = Candidate;
			}
		}
	}
	return BestAlly;
}

void UZombieSpatialSubsystem::RegisterZombie(AZombieBase* Zombie)
{
	if (!Zombie)
	{
		return;
	}

	for (int32 i = 0; i < NumTrackedZombies; ++i)
	{
		if (TrackedZombies[i].Get() == Zombie)
		{
			return;
		}
	}

	if (NumTrackedZombies < MaxTrackedZombies)
	{
		TrackedZombies[NumTrackedZombies++] = Zombie;
	}
}

void UZombieSpatialSubsystem::UnregisterZombie(AZombieBase* Zombie)
{
	for (int32 i = 0; i < NumTrackedZombies; ++i)
	{
		if (TrackedZombies[i].Get() == Zombie || !TrackedZombies[i].IsValid())
		{
			TrackedZombies[i] = TrackedZombies[NumTrackedZombies - 1];
			TrackedZombies[NumTrackedZombies - 1] = nullptr;
			--NumTrackedZombies;
			--i;
		}
	}
}

int32 UZombieSpatialSubsystem::ComputeCellIndex(const FVector& Location) noexcept
{
	const int32 GX = FMath::FloorToInt(static_cast<float>(Location.X) / CellSizeUU) & (GridDimension - 1);
	const int32 GY = FMath::FloorToInt(static_cast<float>(Location.Y) / CellSizeUU) & (GridDimension - 1);
	return GY * GridDimension + GX;
}

void UZombieSpatialSubsystem::RefreshSpatialGrid() noexcept
{
	if (!SpatialGrid)
	{
		return;
	}

	for (int32 i = 0; i < SpatialGrid->Cells.Num(); ++i)
	{
		SpatialGrid->Cells[i].Reset();
	}

	for (int32 i = 0; i < NumTrackedPlayers; ++i)
	{
		if (ADungeoneerCharacter* Player = TrackedPlayers[i].Get())
		{
			const FVector Loc = Player->GetActorLocation();
			const int32 CellIdx = ComputeCellIndex(Loc);
			FZombieSpatialCell& Cell = SpatialGrid->Cells[CellIdx];
			if (Cell.Count < FZombieSpatialCell::MaxEntitiesPerCell)
			{
				FSpatialEntityRecord& Rec = Cell.Records[Cell.Count++];
				Rec.Actor = Player;
				Rec.CachedLocation = Loc;
				Rec.bIsPlayer = true;
				Rec.AcousticVisibility = Player->GetVelocity().Size2D() > 10.0 ? 1.0f : 0.25f;
			}
		}
	}
}

ADungeoneerCharacter* UZombieSpatialSubsystem::FindNearestTargetPlayer(
	const FVector& QueryOrigin,
	float MaxHearingRange,
	bool& bOutLitByFlashlight) const noexcept
{
	bOutLitByFlashlight = false;
	ADungeoneerCharacter* BestPlayer = nullptr;
	double BestScoreDistSq = static_cast<double>(MaxHearingRange) * static_cast<double>(MaxHearingRange);

	for (int32 i = 0; i < NumTrackedPlayers; ++i)
	{
		ADungeoneerCharacter* Player = TrackedPlayers[i].Get();
		if (!Player || Player->GetCurrentHealth() <= 0.0f)
		{
			continue;
		}

		const FVector PlayerLoc = Player->GetActorLocation();
		const double DistSq = FVector::DistSquared2D(QueryOrigin, PlayerLoc);
		const bool bLit = Player->IsPointLitByFlashlight(QueryOrigin);

		// Lit zombies attract aggro at slightly extended effective priority
		const double EffectiveDistSq = bLit ? DistSq * 0.65 : DistSq;
		if (EffectiveDistSq < BestScoreDistSq)
		{
			BestScoreDistSq = EffectiveDistSq;
			BestPlayer = Player;
			bOutLitByFlashlight = bLit;
		}
	}

	return BestPlayer;
}

void UZombieSpatialSubsystem::PreallocateHazardPools(int32 PerTypeCapacity)
{
	if (bPoolsPreallocated)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	const int32 Count = FMath::Clamp(PerTypeCapacity, 4, PoolPerHazardType);
	FirePool.Reserve(Count);
	PoisonPool.Reserve(Count);
	ChronoGasPool.Reserve(Count);
	ScorchDecalPool.Reserve(MaxScorchDecals);

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FVector ParkLocation(0.0f, 0.0f, -50000.0f);

	for (int32 i = 0; i < Count; ++i)
	{
		if (AHazardZone_Fire* Fire = World->SpawnActor<AHazardZone_Fire>(AHazardZone_Fire::StaticClass(), ParkLocation, FRotator::ZeroRotator, SpawnParams))
		{
			Fire->DeactivateToPool();
			FirePool.Add(Fire);
		}
		if (AHazardZone_Poison* Poison = World->SpawnActor<AHazardZone_Poison>(AHazardZone_Poison::StaticClass(), ParkLocation, FRotator::ZeroRotator, SpawnParams))
		{
			Poison->DeactivateToPool();
			PoisonPool.Add(Poison);
		}
		if (AHazardZone_ChronoGas* Chrono = World->SpawnActor<AHazardZone_ChronoGas>(AHazardZone_ChronoGas::StaticClass(), ParkLocation, FRotator::ZeroRotator, SpawnParams))
		{
			Chrono->DeactivateToPool();
			ChronoGasPool.Add(Chrono);
		}
	}

	UStaticMesh* ScorchPlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UDungeoneerAssetBootstrapSubsystem* Bootstrap = nullptr;
	if (const UGameInstance* GI = World->GetGameInstance())
	{
		Bootstrap = GI->GetSubsystem<UDungeoneerAssetBootstrapSubsystem>();
	}

	for (int32 i = 0; i < MaxScorchDecals; ++i)
	{
		if (ADecalActor* Decal = World->SpawnActor<ADecalActor>(ADecalActor::StaticClass(), ParkLocation, FRotator::ZeroRotator, SpawnParams))
		{
			// Disable UE's unassigned DefaultDeferredDecalMaterial (which projects pale streaked artifacts onto sprites)
			// and render a crisp masked 2.5D charcoal scorch crater quad on the floor plane instead.
			if (UDecalComponent* DecalComp = Decal->GetDecal())
			{
				DecalComp->SetVisibility(false);
				DecalComp->SetHiddenInGame(true);
			}
			if (ScorchPlaneMesh)
			{
				UStaticMeshComponent* ScorchMesh = NewObject<UStaticMeshComponent>(Decal, *FString::Printf(TEXT("ScorchQuad_%d"), i));
				ScorchMesh->SetStaticMesh(ScorchPlaneMesh);
				ScorchMesh->SetupAttachment(Decal->GetRootComponent());
				ScorchMesh->RegisterComponent();
				ScorchMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				ScorchMesh->SetCastShadow(false);
				ScorchMesh->SetReceivesDecals(false);
				ScorchMesh->SetRelativeLocation(FVector::ZeroVector);
				ScorchMesh->SetRelativeRotation(FRotator::ZeroRotator);
				ScorchMesh->SetRelativeScale3D(FVector(1.85f, 1.85f, 1.0f));
				if (Bootstrap)
				{
					UMaterialInstanceDynamic* ScorchMID = Bootstrap->CreateDungeonOrRigMaterial(
						Decal,
						FName(TEXT("ScorchCrater_D")),
						FName(TEXT("ScorchCrater_N")),
						FLinearColor(0.32f, 0.28f, 0.26f),
						0.94f,
						false,
						true);
					if (ScorchMID)
					{
						ScorchMesh->SetMaterial(0, ScorchMID);
					}
				}
			}
			Decal->SetActorHiddenInGame(true);
			ScorchDecalPool.Add(Decal);
		}
	}

	bPoolsPreallocated = true;
}

AHazardZoneBase* UZombieSpatialSubsystem::AcquirePooledHazard(EHazardPoolType Type, const FVector& WorldLocation, AZombieFastThrower* OwningThrower) noexcept
{
	if (!bPoolsPreallocated)
	{
		PreallocateHazardPools(PoolPerHazardType);
	}

	AHazardZoneBase* Selected = nullptr;
	switch (Type)
	{
	case EHazardPoolType::Fire:
		if (FirePool.Num() > 0)
		{
			Selected = FirePool[FirePoolCursor % FirePool.Num()];
			FirePoolCursor = (FirePoolCursor + 1) % FirePool.Num();
		}
		break;
	case EHazardPoolType::Poison:
		if (PoisonPool.Num() > 0)
		{
			Selected = PoisonPool[PoisonPoolCursor % PoisonPool.Num()];
			PoisonPoolCursor = (PoisonPoolCursor + 1) % PoisonPool.Num();
		}
		break;
	case EHazardPoolType::ChronoGas:
	default:
		if (ChronoGasPool.Num() > 0)
		{
			AHazardZone_ChronoGas* Chrono = ChronoGasPool[ChronoPoolCursor % ChronoGasPool.Num()];
			ChronoPoolCursor = (ChronoPoolCursor + 1) % ChronoGasPool.Num();
			if (Chrono)
			{
				Chrono->SetOwningThrower(OwningThrower);
			}
			Selected = Chrono;
		}
		break;
	}

	if (Selected)
	{
		Selected->ActivateFromPool(WorldLocation);
	}

	return Selected;
}

void UZombieSpatialSubsystem::ApplyRadialDetonationDamage(const FVector& Epicenter, float Radius, float MaxDamage, AActor* InstigatorActor) noexcept
{
	const float RadiusSq = Radius * Radius;
	for (int32 i = 0; i < NumTrackedPlayers; ++i)
	{
		if (ADungeoneerCharacter* Player = TrackedPlayers[i].Get())
		{
			const double DistSq = FVector::DistSquared(Epicenter, Player->GetActorLocation());
			if (DistSq <= static_cast<double>(RadiusSq))
			{
				const float Dist = static_cast<float>(FMath::Sqrt(DistSq));
				const float Falloff = FMath::Clamp(1.0f - (Dist / FMath::Max(Radius, 1.0f)), 0.15f, 1.0f);
				Player->ApplyDungeoneerDamage(MaxDamage * Falloff, InstigatorActor);
			}
		}
	}

	// Chain-detonation reaction: accelerate nearby Exploder fuses cleanly (zero allocation)
	for (int32 i = 0; i < NumTrackedZombies; ++i)
	{
		AZombieBase* Zombie = TrackedZombies[i].Get();
		if (!Zombie || !Zombie->IsZombieAlive() || Zombie == InstigatorActor)
		{
			continue;
		}
		if (FVector::DistSquared2D(Epicenter, Zombie->GetActorLocation()) <= static_cast<double>(RadiusSq))
		{
			if (AZombieExploder* Exploder = Cast<AZombieExploder>(Zombie))
			{
				Exploder->IgniteOrAccelerateFuse(1.50f);
			}
			else if (InstigatorActor && !InstigatorActor->IsA<AZombieBase>())
			{
				Zombie->ApplyZombieDamage(MaxDamage * 0.45f, InstigatorActor);
			}
		}
	}
}

void UZombieSpatialSubsystem::ActivatePooledScorchDecal(const FVector& WorldLocation) noexcept
{
	if (!bPoolsPreallocated)
	{
		PreallocateHazardPools(PoolPerHazardType);
	}
	if (ScorchDecalPool.Num() == 0)
	{
		return;
	}

	ADecalActor* Decal = ScorchDecalPool[DecalPoolCursor % ScorchDecalPool.Num()];
	DecalPoolCursor = (DecalPoolCursor + 1) % ScorchDecalPool.Num();
	if (Decal)
	{
		const double FloorZ = (WorldLocation.Z > 40.0) ? (WorldLocation.Z - 93.0) : FMath::Max(2.0, WorldLocation.Z - 5.0);
		Decal->SetActorLocation(FVector(WorldLocation.X, WorldLocation.Y, FloorZ));
		Decal->SetActorHiddenInGame(false);
	}
}

int32 UZombieSpatialSubsystem::ApplyMeleeArcDamage(
	const FVector& Origin,
	const FVector& ForwardDirXY,
	const float RangeUU,
	const float MinCosHalfAngle,
	const float DamageAmount,
	AActor* InstigatorActor,
	const float KnockbackImpulseUU) noexcept
{
	const FVector SafeFwd = ForwardDirXY.GetSafeNormal2D();
	const double RangeSq = static_cast<double>(RangeUU) * static_cast<double>(RangeUU);
	int32 HitsLanded = 0;

	for (int32 i = 0; i < NumTrackedZombies; ++i)
	{
		AZombieBase* Zombie = TrackedZombies[i].Get();
		if (!Zombie || !Zombie->IsZombieAlive() || Zombie == InstigatorActor)
		{
			continue;
		}

		const FVector Delta = Zombie->GetActorLocation() - Origin;
		const double DistSq2D = Delta.SizeSquared2D();
		if (DistSq2D > RangeSq || FMath::Abs(Delta.Z) > 220.0)
		{
			continue;
		}

		const FVector DirXY = (DistSq2D > 1.0) ? Delta.GetSafeNormal2D() : SafeFwd;
		const float Dot = static_cast<float>(FVector::DotProduct(SafeFwd, DirXY));
		if (Dot >= MinCosHalfAngle || DistSq2D <= (72.0 * 72.0))
		{
			Zombie->TriggerHitFeedback(DirXY, KnockbackImpulseUU);
			Zombie->ApplyZombieDamage(DamageAmount, InstigatorActor);
			if (ADungeoneerCharacter* Attacker = Cast<ADungeoneerCharacter>(InstigatorActor))
			{
				const bool bCrit = (DamageAmount >= 70.0f);
				Attacker->PushFloatingCombatPopup(
					Zombie->GetActorLocation() + FVector(0.0, 0.0, 82.0),
					FString::Printf(TEXT("-%d%s"), FMath::RoundToInt(DamageAmount), bCrit ? TEXT("!") : TEXT("")),
					bCrit ? EDungeoneerFloatingTextKind::CritDamage : EDungeoneerFloatingTextKind::Damage);
			}
			++HitsLanded;
		}
	}

	return HitsLanded;
}
