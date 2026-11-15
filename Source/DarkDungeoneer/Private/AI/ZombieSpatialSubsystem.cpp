#include "AI/ZombieSpatialSubsystem.h"
#include "AI/ZombieBase.h"
#include "AI/ZombieFastThrower.h"
#include "AI/HazardZoneActors.h"
#include "Core/DungeoneerCharacter.h"
#include "Engine/DecalActor.h"
#include "Engine/World.h"

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

	for (int32 i = 0; i < MaxScorchDecals; ++i)
	{
		if (ADecalActor* Decal = World->SpawnActor<ADecalActor>(ADecalActor::StaticClass(), ParkLocation, FRotator(-90.0f, 0.0f, 0.0f), SpawnParams))
		{
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
}

void UZombieSpatialSubsystem::ActivatePooledScorchDecal(const FVector& WorldLocation) noexcept
{
	if (ScorchDecalPool.Num() == 0)
	{
		return;
	}

	ADecalActor* Decal = ScorchDecalPool[DecalPoolCursor % ScorchDecalPool.Num()];
	DecalPoolCursor = (DecalPoolCursor + 1) % ScorchDecalPool.Num();
	if (Decal)
	{
		Decal->SetActorLocation(WorldLocation);
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
			++HitsLanded;
		}
	}

	return HitsLanded;
}
