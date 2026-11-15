#include "AI/ZombieFastThrower.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "AI/HazardZoneActors.h"
#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AZombieFastThrower::AZombieFastThrower()
{
	BaseWalkSpeed = 410.0f;
	MaxHealth = 95.0f;
	GoldKillReward = 35;
	PerceptionHearingRange = 1800.0f;
	CurrentThrowCooldown = BaseThrowCooldown;
}

void AZombieFastThrower::BeginPlay()
{
	Super::BeginPlay();
	CurrentDrainedRatio = 0.0f;
	FrameReceivedDrainedRatio = 0.0f;
	CurrentThrowCooldown = BaseThrowCooldown;
	ThrowCooldownRemaining = 0.8f;
	bWasLeechActive = false;
}

void AZombieFastThrower::UpdateChronoLeechBuff(float InTotalDrainedRatio, float DeltaTime) noexcept
{
	if (InTotalDrainedRatio > 0.0f)
	{
		FrameReceivedDrainedRatio = FMath::Max(FrameReceivedDrainedRatio, InTotalDrainedRatio);
		CurrentDrainedRatio = FrameReceivedDrainedRatio;
		bWasLeechActive = true;
	}
	else if (CurrentDrainedRatio > 0.0f)
	{
		CurrentDrainedRatio = FMath::Max(0.0f, CurrentDrainedRatio - (DeltaTime / FMath::Max(DecayTime, KINDA_SMALL_NUMBER)));
		if (bWasLeechActive && CurrentDrainedRatio <= 0.01f)
		{
			bWasLeechActive = false;
			if (OnChronoLeechDecaySweep.IsBound())
			{
				OnChronoLeechDecaySweep.Broadcast();
			}
		}
	}

	GetCharacterMovement()->MaxWalkSpeed = ComputeLeechWalkSpeed(BaseWalkSpeed, CurrentDrainedRatio, SpeedStealScalar);
	CurrentThrowCooldown = ComputeLeechThrowCooldown(BaseThrowCooldown, CurrentDrainedRatio, CadenceStealScalar);
}

void AZombieFastThrower::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Decay leech buff smoothly when no gas volume pushed a drain ratio this frame
	if (FrameReceivedDrainedRatio <= 0.0f && CurrentDrainedRatio > 0.0f)
	{
		UpdateChronoLeechBuff(0.0f, DeltaSeconds);
	}
	FrameReceivedDrainedRatio = 0.0f;

	if (!IsZombieAlive() || bIsMirrorDecoy)
	{
		return;
	}

	UWorld* World = GetWorld();
	UZombieSpatialSubsystem* SpatialSys = World ? World->GetSubsystem<UZombieSpatialSubsystem>() : nullptr;
	if (!SpatialSys)
	{
		return;
	}

	bool bLit = false;
	ADungeoneerCharacter* TargetPlayer = SpatialSys->FindNearestTargetPlayer(GetActorLocation(), PerceptionHearingRange, bLit);
	if (!TargetPlayer)
	{
		return;
	}

	const FVector TargetLoc = TargetPlayer->GetActorLocation();
	const FVector DeltaToTarget = TargetLoc - GetActorLocation();
	const float Dist = static_cast<float>(DeltaToTarget.Size2D());

	ECompassOctant ToTargetOctant = CurrentHeadingOctant;
	UDungeoneerMovementComponent::QuantizeInputToOctant(FVector2D(DeltaToTarget.X, DeltaToTarget.Y), 1.0f, ToTargetOctant);

	// Strafe along 8 compass octants at OptimalThrowRange
	ECompassOctant MoveOctant = ToTargetOctant;
	if (Dist < OptimalThrowRange * 0.72f)
	{
		// Kite away along opposite octant
		MoveOctant = static_cast<ECompassOctant>((static_cast<uint8>(ToTargetOctant) + 4) & 7);
	}
	else if (Dist <= OptimalThrowRange * 1.15f)
	{
		// Orthogonal compass strafe
		MoveOctant = static_cast<ECompassOctant>((static_cast<uint8>(ToTargetOctant) + (StrafeSign > 0 ? 2 : 6)) & 7);
	}

	MoveAlongOctant(MoveOctant, 1.0f);
	CurrentHeadingOctant = ToTargetOctant;

	ThrowCooldownRemaining -= DeltaSeconds;
	if (ThrowCooldownRemaining <= 0.0f && Dist <= OptimalThrowRange * 1.4f)
	{
		ExecutePooledThrow(TargetLoc);
		ThrowCooldownRemaining = CurrentThrowCooldown;
		if ((ThrowSequenceCount & 1) == 0)
		{
			StrafeSign = -StrafeSign;
		}
	}
}

EThrowPayload AZombieFastThrower::SelectNextPayload(int32 ThrowCount) const noexcept
{
	switch (ThrowCount % 3)
	{
	case 0:
		return EThrowPayload::SloMoGas;
	case 1:
		return EThrowPayload::Fire;
	default:
		return EThrowPayload::Poison;
	}
}

void AZombieFastThrower::ExecutePooledThrow(const FVector& TargetLocation)
{
	const EThrowPayload Payload = SelectNextPayload(ThrowSequenceCount++);
	if (HasAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			if (UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
			{
				EHazardPoolType PoolType = EHazardPoolType::ChronoGas;
				if (Payload == EThrowPayload::Fire)
				{
					PoolType = EHazardPoolType::Fire;
				}
				else if (Payload == EThrowPayload::Poison)
				{
					PoolType = EHazardPoolType::Poison;
				}

				SpatialSys->AcquirePooledHazard(PoolType, TargetLocation, this);
			}
		}
	}

	if (OnThrowPayloadReleased.IsBound())
	{
		OnThrowPayloadReleased.Broadcast(Payload, TargetLocation);
	}
}

void AZombieFastThrower::OnZombieDied()
{
	Super::OnZombieDied();
	if (CurrentDrainedRatio > 0.0f)
	{
		CurrentDrainedRatio = 0.0f;
		if (OnChronoLeechDecaySweep.IsBound())
		{
			OnChronoLeechDecaySweep.Broadcast();
		}
	}
}
