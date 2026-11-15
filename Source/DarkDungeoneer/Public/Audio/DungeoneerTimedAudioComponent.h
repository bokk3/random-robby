#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/DungeoneerTypes.h"
#include "Audio/AnimNotify_DungeoneerTimedCue.h"
#include "AI/ZombieFastThrower.h"
#include "DungeoneerTimedAudioComponent.generated.h"

class USoundBase;

/**
 * Deterministic Frame-Accurate & Step-Accumulated Timed Audio Component (`04-audio-asset-compliance.md`).
 * - Never triggers gameplay audio on arbitrary FTimerHandle timers.
 * - Binds directly to:
 *     1. UDungeoneerMovementComponent::OnStepCycleTriggered (player 4-axis step cycle)
 *     2. AZombieBase::OnZombieStepTriggered
 *     3. AZombieExploder::OnExploderFuseTick / OnExploderDetonated
 *     4. AZombieFastThrower::OnThrowPayloadReleased / OnChronoLeechDecaySweep
 *     5. AHazardZoneBase::OnHazardTickCue
 *     6. UAnimNotify_DungeoneerTimedCue
 * - Forwards acoustic loudness events into UDungeoneerSensoryWorldSubsystem for Acute Hearing wave rings.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class DARKDUNGEONEER_API UDungeoneerTimedAudioComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDungeoneerTimedAudioComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Called by UAnimNotify_DungeoneerTimedCue for frame-accurate animation-locked audio. */
	void TriggerAnimNotifyCue(EDungeoneerTimedCueKind CueKind, USoundBase* CueSound, const FVector& WorldLocation, float LoudnessScalar);

protected:
	void HandlePlayerStepEvent(const FDungeoneerStepEvent& StepEvent);
	void HandleZombieStepEvent(const FDungeoneerStepEvent& StepEvent);
	void HandleExploderFuseTick(float FuseProgress, float PitchScalar);
	void HandleExploderDetonation(const FVector& Epicenter);
	void HandleThrowerPayload(EThrowPayload PayloadType, const FVector& TargetLocation);
	void HandleChronoDecaySweep();
	void HandleHazardCue(const FVector& Location);

	void EmitAcousticSensoryRipple(const FVector& WorldOrigin, float Loudness, float MaxRadius, bool bPhaseInverted = false) noexcept;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Cues")
	TObjectPtr<USoundBase> PlayerFootstepSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Cues")
	TObjectPtr<USoundBase> ZombieFootstepSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Cues")
	TObjectPtr<USoundBase> FuseTickSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Cues")
	TObjectPtr<USoundBase> DetonationSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Cues")
	TObjectPtr<USoundBase> ThrowSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Cues")
	TObjectPtr<USoundBase> ChronoDecaySweepSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Audio|Cues")
	TObjectPtr<USoundBase> HazardHissSound;
};
