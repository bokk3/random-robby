#include "AI/HazardZoneActors.h"
#include "AI/ZombieFastThrower.h"
#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Components/PointLightComponent.h"
#include "NiagaraComponent.h"

AHazardZoneBase::AHazardZoneBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	ZoneBounds = CreateDefaultSubobject<USphereComponent>(TEXT("ZoneBounds"));
	SetRootComponent(ZoneBounds);
	ZoneBounds->SetSphereRadius(ZoneRadius);
	ZoneBounds->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	ZoneBounds->OnComponentBeginOverlap.AddDynamic(this, &AHazardZoneBase::HandleSphereBeginOverlap);
	ZoneBounds->OnComponentEndOverlap.AddDynamic(this, &AHazardZoneBase::HandleSphereEndOverlap);

	HazardFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("HazardFX"));
	HazardFX->SetupAttachment(RootComponent);
	HazardFX->bAutoActivate = false;
}

void AHazardZoneBase::ActivateFromPool(const FVector& WorldLocation) noexcept
{
	SetActorLocation(WorldLocation);
	SetActorHiddenInGame(false);
	SetActorEnableCollision(true);
	bHazardActive = true;
	RemainingLifetime = ActiveDuration;
	AudioCueAccumulator = 0.0f;
	OverlappingPlayers.Reset();
	if (HazardFX)
	{
		HazardFX->Activate(true);
	}
}

void AHazardZoneBase::DeactivateToPool() noexcept
{
	bHazardActive = false;
	OnHazardDeactivated();
	OverlappingPlayers.Reset();
	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetActorLocation(FVector(0.0f, 0.0f, -50000.0f));
	if (HazardFX)
	{
		HazardFX->Deactivate();
	}
}

void AHazardZoneBase::HandleSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (!bHazardActive)
	{
		return;
	}

	if (ADungeoneerCharacter* Player = Cast<ADungeoneerCharacter>(OtherActor))
	{
		if (OverlappingPlayers.Num() < 8)
		{
			OverlappingPlayers.AddUnique(Player);
		}
	}
}

void AHazardZoneBase::HandleSphereEndOverlap(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (ADungeoneerCharacter* Player = Cast<ADungeoneerCharacter>(OtherActor))
	{
		if (UDungeoneerMovementComponent* MoveComp = Player->GetDungeoneerMovement())
		{
			MoveComp->ClearChronoSlow();
		}
		OverlappingPlayers.RemoveAll([Player](const TWeakObjectPtr<ADungeoneerCharacter>& Ptr)
		{
			return !Ptr.IsValid() || Ptr.Get() == Player;
		});
	}
}

void AHazardZoneBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bHazardActive)
	{
		return;
	}

	RemainingLifetime -= DeltaSeconds;
	if (RemainingLifetime <= 0.0f)
	{
		DeactivateToPool();
		return;
	}

	// Deterministic fixed-step audio hiss/crackle phase crossing
	AudioCueAccumulator += DeltaSeconds;
	if (AudioCueAccumulator >= 0.5f)
	{
		AudioCueAccumulator -= 0.5f;
		if (OnHazardTickCue.IsBound())
		{
			OnHazardTickCue.Broadcast(GetActorLocation());
		}
	}

	ApplyHazardTick(DeltaSeconds);
}

AHazardZone_Fire::AHazardZone_Fire()
{
	FlickerLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FlickerLight"));
	FlickerLight->SetupAttachment(RootComponent);
	FlickerLight->SetLightColor(FLinearColor(1.0f, 0.42f, 0.08f));
	FlickerLight->SetIntensity(7500.0f);
	FlickerLight->SetAttenuationRadius(680.0f);
	FlickerLight->VolumetricScatteringIntensity = 2.0f;
	FlickerLight->SetVisibility(false);
}

void AHazardZone_Fire::ApplyHazardTick(float DeltaSeconds)
{
	if (FlickerLight)
	{
		FlickerLight->SetVisibility(true);
		FlickerPhase += DeltaSeconds * 16.0f;
		const float Flicker = 6500.0f + 2200.0f * FMath::Sin(FlickerPhase) * FMath::Cos(FlickerPhase * 0.43f);
		FlickerLight->SetIntensity(Flicker);
	}

	if (!HasAuthority())
	{
		return;
	}

	for (const TWeakObjectPtr<ADungeoneerCharacter>& WeakPlayer : OverlappingPlayers)
	{
		if (ADungeoneerCharacter* Player = WeakPlayer.Get())
		{
			Player->ApplyDungeoneerDamage(BurnDamagePerSecond * DeltaSeconds, this);
		}
	}
}

void AHazardZone_Fire::OnHazardDeactivated()
{
	if (FlickerLight)
	{
		FlickerLight->SetVisibility(false);
	}
}

AHazardZone_Poison::AHazardZone_Poison()
{
	ActiveDuration = 9.0f;
}

void AHazardZone_Poison::ApplyHazardTick(float DeltaSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	for (const TWeakObjectPtr<ADungeoneerCharacter>& WeakPlayer : OverlappingPlayers)
	{
		if (ADungeoneerCharacter* Player = WeakPlayer.Get())
		{
			Player->ApplyDungeoneerDamage(PoisonDamagePerSecond * DeltaSeconds, this);
			Player->RestoreAtRestingGround(0.0f, -FlashlightDimmingFactor * DeltaSeconds);
		}
	}
}

AHazardZone_ChronoGas::AHazardZone_ChronoGas()
{
	ActiveDuration = 8.0f;
	for (int32 i = 0; i < PlayerSlowStacks.Num(); ++i)
	{
		PlayerSlowStacks[i] = 0.0f;
	}
}

void AHazardZone_ChronoGas::ApplyHazardTick(float DeltaSeconds)
{
	float TotalDrainedRatio = 0.0f;

	for (int32 i = 0; i < OverlappingPlayers.Num() && i < PlayerSlowStacks.Num(); ++i)
	{
		if (ADungeoneerCharacter* Player = OverlappingPlayers[i].Get())
		{
			PlayerSlowStacks[i] = FMath::Clamp(PlayerSlowStacks[i] + SlowRampPerSecond * DeltaSeconds, 0.0f, MaxSlowRatio);
			if (UDungeoneerMovementComponent* MoveComp = Player->GetDungeoneerMovement())
			{
				MoveComp->SetChronoSlowRatio(PlayerSlowStacks[i]);
			}
			TotalDrainedRatio += PlayerSlowStacks[i];
		}
		else
		{
			PlayerSlowStacks[i] = 0.0f;
		}
	}

	if (AZombieFastThrower* Thrower = OwningThrower.Get())
	{
		if (Thrower->IsZombieAlive())
		{
			Thrower->UpdateChronoLeechBuff(TotalDrainedRatio, DeltaSeconds);
		}
	}
}

void AHazardZone_ChronoGas::OnHazardDeactivated()
{
	for (int32 i = 0; i < PlayerSlowStacks.Num(); ++i)
	{
		PlayerSlowStacks[i] = 0.0f;
	}

	for (const TWeakObjectPtr<ADungeoneerCharacter>& WeakPlayer : OverlappingPlayers)
	{
		if (ADungeoneerCharacter* Player = WeakPlayer.Get())
		{
			if (UDungeoneerMovementComponent* MoveComp = Player->GetDungeoneerMovement())
			{
				MoveComp->ClearChronoSlow();
			}
		}
	}
}
