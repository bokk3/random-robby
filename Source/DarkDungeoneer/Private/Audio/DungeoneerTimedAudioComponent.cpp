#include "Audio/DungeoneerTimedAudioComponent.h"
#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerMovementComponent.h"
#include "AI/ZombieBase.h"
#include "AI/ZombieExploder.h"
#include "AI/HazardZoneActors.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UDungeoneerTimedAudioComponent::UDungeoneerTimedAudioComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDungeoneerTimedAudioComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (ADungeoneerCharacter* Player = Cast<ADungeoneerCharacter>(Owner))
	{
		if (UDungeoneerMovementComponent* MoveComp = Player->GetDungeoneerMovement())
		{
			MoveComp->OnStepCycleTriggered.AddUObject(this, &UDungeoneerTimedAudioComponent::HandlePlayerStepEvent);
		}
	}
	else if (AZombieBase* Zombie = Cast<AZombieBase>(Owner))
	{
		Zombie->OnZombieStepTriggered.AddUObject(this, &UDungeoneerTimedAudioComponent::HandleZombieStepEvent);

		if (AZombieExploder* Exploder = Cast<AZombieExploder>(Zombie))
		{
			Exploder->OnExploderFuseTick.AddUObject(this, &UDungeoneerTimedAudioComponent::HandleExploderFuseTick);
			Exploder->OnExploderDetonated.AddUObject(this, &UDungeoneerTimedAudioComponent::HandleExploderDetonation);
		}
		else if (AZombieFastThrower* Thrower = Cast<AZombieFastThrower>(Zombie))
		{
			Thrower->OnThrowPayloadReleased.AddUObject(this, &UDungeoneerTimedAudioComponent::HandleThrowerPayload);
			Thrower->OnChronoLeechDecaySweep.AddUObject(this, &UDungeoneerTimedAudioComponent::HandleChronoDecaySweep);
		}
	}
	else if (AHazardZoneBase* Hazard = Cast<AHazardZoneBase>(Owner))
	{
		Hazard->OnHazardTickCue.AddUObject(this, &UDungeoneerTimedAudioComponent::HandleHazardCue);
	}
}

void UDungeoneerTimedAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (AActor* Owner = GetOwner())
	{
		if (ADungeoneerCharacter* Player = Cast<ADungeoneerCharacter>(Owner))
		{
			if (UDungeoneerMovementComponent* MoveComp = Player->GetDungeoneerMovement())
			{
				MoveComp->OnStepCycleTriggered.RemoveAll(this);
			}
		}
		else if (AZombieBase* Zombie = Cast<AZombieBase>(Owner))
		{
			Zombie->OnZombieStepTriggered.RemoveAll(this);
			if (AZombieExploder* Exploder = Cast<AZombieExploder>(Zombie))
			{
				Exploder->OnExploderFuseTick.RemoveAll(this);
				Exploder->OnExploderDetonated.RemoveAll(this);
			}
			else if (AZombieFastThrower* Thrower = Cast<AZombieFastThrower>(Zombie))
			{
				Thrower->OnThrowPayloadReleased.RemoveAll(this);
				Thrower->OnChronoLeechDecaySweep.RemoveAll(this);
			}
		}
		else if (AHazardZoneBase* Hazard = Cast<AHazardZoneBase>(Owner))
		{
			Hazard->OnHazardTickCue.RemoveAll(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void UDungeoneerTimedAudioComponent::TriggerAnimNotifyCue(
	EDungeoneerTimedCueKind CueKind,
	USoundBase* CueSound,
	const FVector& WorldLocation,
	float LoudnessScalar)
{
	if (CueSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, CueSound, WorldLocation, LoudnessScalar);
	}

	const float Radius = (CueKind == EDungeoneerTimedCueKind::ExploderFuse) ? 1400.0f : 850.0f;
	EmitAcousticSensoryRipple(WorldLocation, LoudnessScalar, Radius);
}

void UDungeoneerTimedAudioComponent::HandlePlayerStepEvent(const FDungeoneerStepEvent& StepEvent)
{
	if (PlayerFootstepSound)
	{
		const float PitchJitter = 0.96f + 0.08f * static_cast<float>(StepEvent.StepSequenceIndex & 1);
		UGameplayStatics::PlaySoundAtLocation(this, PlayerFootstepSound, StepEvent.WorldLocation, StepEvent.AcousticLoudness, PitchJitter);
	}

	EmitAcousticSensoryRipple(StepEvent.WorldLocation, StepEvent.AcousticLoudness * 0.6f, 600.0f);
}

void UDungeoneerTimedAudioComponent::HandleZombieStepEvent(const FDungeoneerStepEvent& StepEvent)
{
	if (ZombieFootstepSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ZombieFootstepSound, StepEvent.WorldLocation, StepEvent.AcousticLoudness);
	}

	EmitAcousticSensoryRipple(StepEvent.WorldLocation, StepEvent.AcousticLoudness, 1100.0f);
}

void UDungeoneerTimedAudioComponent::HandleExploderFuseTick(float FuseProgress, float PitchScalar)
{
	const FVector Loc = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	if (FuseTickSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FuseTickSound, Loc, 1.0f + FuseProgress * 0.5f, PitchScalar);
	}

	EmitAcousticSensoryRipple(Loc, 1.35f, 1300.0f);
}

void UDungeoneerTimedAudioComponent::HandleExploderDetonation(const FVector& Epicenter)
{
	if (DetonationSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DetonationSound, Epicenter, 1.6f);
	}

	EmitAcousticSensoryRipple(Epicenter, 2.2f, 2000.0f);
}

void UDungeoneerTimedAudioComponent::HandleThrowerPayload(EThrowPayload PayloadType, const FVector& TargetLocation)
{
	const FVector Origin = GetOwner() ? GetOwner()->GetActorLocation() : TargetLocation;
	if (ThrowSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ThrowSound, Origin, 1.0f);
	}

	EmitAcousticSensoryRipple(Origin, 1.1f, 950.0f);
}

void UDungeoneerTimedAudioComponent::HandleChronoDecaySweep()
{
	if (ChronoDecaySweepSound && GetOwner())
	{
		UGameplayStatics::PlaySoundAtLocation(this, ChronoDecaySweepSound, GetOwner()->GetActorLocation(), 1.0f, 0.8f);
	}
}

void UDungeoneerTimedAudioComponent::HandleHazardCue(const FVector& Location)
{
	if (HazardHissSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, HazardHissSound, Location, 0.75f);
	}
}

void UDungeoneerTimedAudioComponent::EmitAcousticSensoryRipple(
	const FVector& WorldOrigin,
	float Loudness,
	float MaxRadius,
	bool bPhaseInverted) noexcept
{
	if (UWorld* World = GetWorld())
	{
		if (UDungeoneerSensoryWorldSubsystem* SensorySys = World->GetSubsystem<UDungeoneerSensoryWorldSubsystem>())
		{
			SensorySys->BroadcastNoiseEvent(WorldOrigin, Loudness, MaxRadius, bPhaseInverted);
		}
	}
}
