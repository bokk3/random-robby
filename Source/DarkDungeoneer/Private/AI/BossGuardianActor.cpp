#include "AI/BossGuardianActor.h"
#include "AI/ZombieExploder.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerGameState.h"
#include "Core/DungeoneerMovementComponent.h"
#include "Dungeon/DungeonEncounterActors.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "Social/DungeoneerInventoryComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

ABossGuardianActor::ABossGuardianActor()
{
	MaxHealth = 680.0f;
	BaseWalkSpeed = 165.0f;
	PerceptionHearingRange = 2200.0f;
	StepCycleDistanceUU = 135.0f;
	GoldKillReward = 250;
	BatteryKillReward = 35.0f;
	GetCharacterMovement()->Mass = 600.0f;
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
}

void ABossGuardianActor::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;
	CurrentBossPhase = 1;
	AttackCycleAccumulator = 0.0f;
	SlamTelegraphRemaining = 0.0f;
	PatternIndex = 0;
	bPhase2AddsWoken = false;

	if (UWorld* World = GetWorld())
	{
		if (UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
		{
			SpatialSys->SetActiveBossGuardian(this);
		}
	}

	PreallocateDormantAdds();
}

void ABossGuardianActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
		{
			if (SpatialSys->GetActiveBossGuardian() == this)
			{
				SpatialSys->SetActiveBossGuardian(nullptr);
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

void ABossGuardianActor::SetLinkedVault(AMajorLootVaultActor* InVault) noexcept
{
	LinkedVault = InVault;
	if (InVault && IsZombieAlive())
	{
		InVault->SetLockedByBossGuardian(true);
	}
}

void ABossGuardianActor::PreallocateDormantAdds()
{
	if (!HasAuthority())
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector SpawnBase = GetActorLocation();

	for (int32 i = 0; i < 2; ++i)
	{
		if (AZombieExploder* Add = World->SpawnActor<AZombieExploder>(AZombieExploder::StaticClass(), SpawnBase + FVector(0.0, 0.0, -2500.0), FRotator::ZeroRotator, Params))
		{
			Add->SetActorHiddenInGame(true);
			Add->SetActorEnableCollision(false);
			Add->SetActorTickEnabled(false);
			DormantAdds[i] = Add;
		}
	}
}

void ABossGuardianActor::WakePhase2DormantAdds()
{
	if (!HasAuthority() || bPhase2AddsWoken)
	{
		return;
	}
	bPhase2AddsWoken = true;

	const FVector BossLoc = GetActorLocation();
	const FVector Offsets[2] = { FVector(-210.0, 160.0, 5.0), FVector(210.0, -160.0, 5.0) };
	for (int32 i = 0; i < 2; ++i)
	{
		if (AZombieExploder* Add = DormantAdds[i].Get())
		{
			Add->SetActorLocation(BossLoc + Offsets[i]);
			Add->SetActorHiddenInGame(false);
			Add->SetActorEnableCollision(true);
			Add->SetActorTickEnabled(true);
		}
	}
}

void ABossGuardianActor::ExecuteRadialBossSlam()
{
	UWorld* World = GetWorld();
	if (!World || !IsZombieAlive())
	{
		return;
	}

	const FVector BossLoc = GetActorLocation();
	if (UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
	{
		SpatialSys->ApplyRadialDetonationDamage(BossLoc, SlamRadiusUU, SlamDamage, this);
		SpatialSys->ActivatePooledScorchDecal(BossLoc);

		const EHazardPoolType HazardKind = (PatternIndex & 1) ? EHazardPoolType::ChronoGas : EHazardPoolType::Fire;
		const FVector TargetOffset = DungeoneerMath::OctantToDirection3D(CurrentHeadingOctant) * 190.0;
		SpatialSys->AcquirePooledHazard(HazardKind, BossLoc + TargetOffset, nullptr);
	}

	if (UDungeoneerSensoryWorldSubsystem* Sensory = World->GetSubsystem<UDungeoneerSensoryWorldSubsystem>())
	{
		// Screech / slam shockwave awakens nearby Echo Crystals cleanly
		Sensory->BroadcastNoiseEvent(BossLoc, 2.15f, 1600.0f, false);
	}

	++PatternIndex;
}

void ABossGuardianActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsZombieAlive() || bIsMirrorDecoy)
	{
		return;
	}

	if (CurrentBossPhase == 1 && CurrentHealth <= MaxHealth * 0.55f)
	{
		CurrentBossPhase = 2;
		WakePhase2DormantAdds();
	}

	UWorld* World = GetWorld();
	UZombieSpatialSubsystem* SpatialSys = World ? World->GetSubsystem<UZombieSpatialSubsystem>() : nullptr;
	if (!SpatialSys)
	{
		return;
	}

	bool bLit = false;
	const FVector SelfLoc = GetActorLocation();
	if (ADungeoneerCharacter* TargetPlayer = SpatialSys->FindNearestTargetPlayer(SelfLoc, PerceptionHearingRange, bLit))
	{
		const FVector ToTarget = TargetPlayer->GetActorLocation() - SelfLoc;
		if (SlamTelegraphRemaining <= 0.0f && ToTarget.SizeSquared2D() > FMath::Square(150.0))
		{
			ECompassOctant DesiredOctant = CurrentHeadingOctant;
			UDungeoneerMovementComponent::QuantizeInputToOctant(FVector2D(ToTarget.X, ToTarget.Y), 1.0f, DesiredOctant);
			MoveAlongOctant(DesiredOctant, CurrentBossPhase >= 2 ? 1.15f : 0.90f);
		}

		const float CyclePeriod = (CurrentBossPhase >= 2) ? 2.25f : 3.35f;
		AttackCycleAccumulator += DeltaSeconds;
		if (AttackCycleAccumulator >= CyclePeriod && SlamTelegraphRemaining <= 0.0f)
		{
			AttackCycleAccumulator = 0.0f;
			SlamTelegraphRemaining = (CurrentBossPhase >= 2) ? 0.55f : 0.75f;
		}
	}

	if (SlamTelegraphRemaining > 0.0f)
	{
		SlamTelegraphRemaining = FMath::Max(0.0f, SlamTelegraphRemaining - DeltaSeconds);
		if (ZombieEmissiveLight)
		{
			ZombieEmissiveLight->SetIntensity(280.0f);
		}
		if (SlamTelegraphRemaining <= 0.0f)
		{
			ExecuteRadialBossSlam();
		}
	}
	else if (ZombieEmissiveLight && HitFlashRemaining <= 0.0f)
	{
		ZombieEmissiveLight->SetIntensity(140.0f);
	}
}

void ABossGuardianActor::OnZombieDied()
{
	Super::OnZombieDied();

	if (AMajorLootVaultActor* Vault = LinkedVault.Get())
	{
		Vault->SetLockedByBossGuardian(false);
	}

	if (UWorld* World = GetWorld())
	{
		if (ADungeoneerGameState* GS = World->GetGameState<ADungeoneerGameState>())
		{
			GS->AuthorityRecordKillInLedger(0x08);
			GS->AuthorityRecordRelicCollected();
		}
		if (UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
		{
			if (SpatialSys->GetActiveBossGuardian() == this)
			{
				SpatialSys->SetActiveBossGuardian(nullptr);
			}
			const int32 PlayerCount = SpatialSys->GetTrackedPlayerCount();
			for (int32 i = 0; i < PlayerCount; ++i)
			{
				if (ADungeoneerCharacter* Player = SpatialSys->GetTrackedPlayerAt(i))
				{
					if (UDungeoneerInventoryComponent* Inv = Player->GetInventoryComponent())
					{
						Inv->AuthorityGrantRelic(EDungeoneerRelicId::SolarFlashlightCore);
					}
					Player->PushFloatingCombatPopup(
						GetActorLocation() + FVector(0.0, 0.0, 120.0),
						TEXT("COLOSSUS SLAIN! +SOLAR CORE"),
						EDungeoneerFloatingTextKind::Relic);
				}
			}
		}
	}
}
