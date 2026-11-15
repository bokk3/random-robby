#include "AI/ZombieExploder.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/MeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AZombieExploder::AZombieExploder()
{
	BaseWalkSpeed = 185.0f;
	MaxHealth = 220.0f;
	PerceptionHearingRange = 1600.0f;
	GetCharacterMovement()->Mass = 250.0f;
}

void AZombieExploder::BeginPlay()
{
	Super::BeginPlay();
	bFuseIgnited = false;
	bDetonated = false;
	FuseElapsed = 0.0f;
	NextTickPhase = 0.12f;
	EmissivePulseScalar = 0.0f;
}

float AZombieExploder::GetFuseProgressNormalized() const noexcept
{
	return FMath::Clamp(FuseElapsed / FMath::Max(FuseDuration, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
}

void AZombieExploder::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsZombieAlive() || bDetonated || bIsMirrorDecoy)
	{
		return;
	}

	UWorld* World = GetWorld();
	UZombieSpatialSubsystem* SpatialSys = World ? World->GetSubsystem<UZombieSpatialSubsystem>() : nullptr;
	if (!SpatialSys)
	{
		return;
	}

	if (!bFuseIgnited)
	{
		bool bLit = false;
		if (ADungeoneerCharacter* TargetPlayer = SpatialSys->FindNearestTargetPlayer(GetActorLocation(), PerceptionHearingRange, bLit))
		{
			const FVector ToTarget = TargetPlayer->GetActorLocation() - GetActorLocation();
			const float DistSq = static_cast<float>(ToTarget.SizeSquared2D());
			ECompassOctant DesiredOctant = CurrentHeadingOctant;
			UDungeoneerMovementComponent::QuantizeInputToOctant(FVector2D(ToTarget.X, ToTarget.Y), 1.0f, DesiredOctant);
			MoveAlongOctant(DesiredOctant, 1.0f);

			if (DistSq <= IgnitionRadius * IgnitionRadius)
			{
				IgniteFuse();
			}
		}
		else if (const UDungeoneerSensoryWorldSubsystem* SensorySub = World->GetSubsystem<UDungeoneerSensoryWorldSubsystem>())
		{
			FDungeoneerNoiseEvent HeardNoise;
			if (SensorySub->QueryStrongestActivePulse(GetActorLocation(), 1.25f, HeardNoise) && HeardNoise.Loudness >= 1.55f)
			{
				const FVector ToNoise = HeardNoise.WorldOrigin - GetActorLocation();
				if (ToNoise.SizeSquared2D() > (90.0 * 90.0))
				{
					ECompassOctant LureOctant = CurrentHeadingOctant;
					UDungeoneerMovementComponent::QuantizeInputToOctant(FVector2D(ToNoise.X, ToNoise.Y), 1.0f, LureOctant);
					MoveAlongOctant(LureOctant, 0.90f);
				}
			}
		}
	}
	else
	{
		FuseElapsed += DeltaSeconds;
		const float Progress = GetFuseProgressNormalized();

		// Accelerate pulse frequency deterministically with fuse progress
		const float PulseFreq = FMath::Lerp(4.0f, 22.0f, Progress * Progress);
		EmissivePulseScalar = 0.5f + 0.5f * FMath::Sin(FuseElapsed * PulseFreq * PI);

		// Push emissive belly-crack glow via Custom Primitive Data (zero MID allocation)
		const float PulseCPD = EmissivePulseScalar * (1.0f + Progress * 4.0f);
		if (UMeshComponent* BodyMesh = GetMesh())
		{
			BodyMesh->SetCustomPrimitiveDataFloat(2, PulseCPD);
		}
		if (ZombieVisualMesh)
		{
			ZombieVisualMesh->SetCustomPrimitiveDataFloat(2, PulseCPD);
		}

		// Deterministic accelerating tick phase crossings (shrinking step interval as fuse nears 1.0)
		if (Progress >= NextTickPhase)
		{
			const float Remaining = FMath::Max(0.08f, 1.0f - Progress);
			NextTickPhase += FMath::Clamp(Remaining * 0.28f, 0.045f, 0.22f);
			const float PitchScalar = FMath::Lerp(1.0f, 2.25f, Progress);
			if (OnExploderFuseTick.IsBound())
			{
				OnExploderFuseTick.Broadcast(Progress, PitchScalar);
			}
		}

		if (FuseElapsed >= FuseDuration)
		{
			ExecuteDetonation();
		}
	}
}

void AZombieExploder::IgniteFuse() noexcept
{
	bFuseIgnited = true;
	FuseElapsed = 0.0f;
	NextTickPhase = 0.12f;
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed * 0.55f;
}

void AZombieExploder::IgniteOrAccelerateFuse(const float FuseBoostSeconds) noexcept
{
	if (!IsZombieAlive() || bDetonated)
	{
		return;
	}

	const float ClampedBoost = FMath::Max(0.0f, FuseBoostSeconds);
	if (!bFuseIgnited)
	{
		IgniteFuse();
		FuseElapsed = FMath::Min(FuseDuration * 0.85f, ClampedBoost * 0.65f);
	}
	else
	{
		FuseElapsed = FMath::Min(FuseDuration, FuseElapsed + ClampedBoost);
	}
}

void AZombieExploder::ExecuteDetonation()
{
	if (bDetonated)
	{
		return;
	}
	bDetonated = true;

	const FVector Epicenter = GetActorLocation();
	if (HasAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			if (UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
			{
				SpatialSys->ApplyRadialDetonationDamage(Epicenter, DetonationRadius, MaxDetonationDamage, this);
				SpatialSys->ActivatePooledScorchDecal(Epicenter - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
			}
		}
	}

	if (OnExploderDetonated.IsBound())
	{
		OnExploderDetonated.Broadcast(Epicenter);
	}

	ApplyZombieDamage(MaxHealth * 10.0f, this);
	SetActorHiddenInGame(true);
}
