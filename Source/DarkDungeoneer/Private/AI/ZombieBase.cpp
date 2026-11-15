#include "AI/ZombieBase.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "Core/DungeoneerMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AZombieBase::AZombieBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
}

void AZombieBase::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;
	PreviousLocation = GetActorLocation();
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;

	if (!bIsMirrorDecoy)
	{
		if (UWorld* World = GetWorld())
		{
			if (UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
			{
				SpatialSys->RegisterZombie(this);
			}
		}
	}
}

void AZombieBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (!bIsMirrorDecoy)
	{
		if (UWorld* World = GetWorld())
		{
			if (UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
			{
				SpatialSys->UnregisterZombie(this);
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AZombieBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsZombieAlive() && !bIsMirrorDecoy)
	{
		AdvanceDeterministicStepAccumulator(DeltaSeconds);
	}
}

void AZombieBase::MoveAlongOctant(ECompassOctant Octant, float Scale) noexcept
{
	CurrentHeadingOctant = Octant;
	const FVector UnitDir = DungeoneerMath::OctantToDirection3D(Octant);
	AddMovementInput(UnitDir, Scale);
}

void AZombieBase::AdvanceDeterministicStepAccumulator(float DeltaSeconds) noexcept
{
	const FVector CurrentLocation = GetActorLocation();
	const double DistSq = FVector::DistSquared2D(PreviousLocation, CurrentLocation);
	PreviousLocation = CurrentLocation;

	if (DistSq <= static_cast<double>(KINDA_SMALL_NUMBER))
	{
		return;
	}

	StepDistanceAccumulator += static_cast<float>(FMath::Sqrt(DistSq));
	while (StepDistanceAccumulator >= StepCycleDistanceUU)
	{
		StepDistanceAccumulator -= StepCycleDistanceUU;
		StepSequenceCounter = (StepSequenceCounter + 1) & 0x3FFFFFFF;

		if (OnZombieStepTriggered.IsBound())
		{
			FDungeoneerStepEvent StepEvt;
			StepEvt.WorldLocation = CurrentLocation;
			StepEvt.StepOctant = CurrentHeadingOctant;
			StepEvt.AcousticLoudness = 1.25f;
			StepEvt.StepSequenceIndex = StepSequenceCounter;
			StepEvt.bIsHostileSource = true;
			OnZombieStepTriggered.Broadcast(StepEvt);
		}

		if (UWorld* World = GetWorld())
		{
			if (UDungeoneerSensoryWorldSubsystem* SensorySS = World->GetSubsystem<UDungeoneerSensoryWorldSubsystem>())
			{
				SensorySS->BroadcastNoiseEvent(CurrentLocation, 1.25f, 900.0f, bIsMirrorDecoy);
			}
		}
	}
}

void AZombieBase::ApplyZombieDamage(float DamageAmount, AActor* DamageInstigator)
{
	if (!HasAuthority() || !IsZombieAlive() || DamageAmount <= 0.0f)
	{
		return;
	}

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);
	if (CurrentHealth <= 0.0f)
	{
		OnZombieDied();
	}
}

void AZombieBase::OnZombieDied()
{
	SetActorEnableCollision(false);
	GetCharacterMovement()->StopMovementImmediately();
}
