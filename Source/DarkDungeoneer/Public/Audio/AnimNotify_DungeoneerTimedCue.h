#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "AnimNotify_DungeoneerTimedCue.generated.h"

class USoundBase;

UENUM(BlueprintType)
enum class EDungeoneerTimedCueKind : uint8
{
	Footstep       = 0,
	ExploderFuse   = 1,
	HazardGasHiss  = 2,
	ProjectileThrow = 3
};

/**
 * Deterministic frame-accurate AnimNotify for gameplay audio cues (`04-audio-asset-compliance.md`).
 * Triggers audio strictly on animation keyframe sync rather than arbitrary timers.
 */
UCLASS(meta = (DisplayName = "Dungeoneer Timed Audio Cue"))
class DARKDUNGEONEER_API UAnimNotify_DungeoneerTimedCue : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference) override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	EDungeoneerTimedCueKind CueKind = EDungeoneerTimedCueKind::Footstep;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<USoundBase> CueSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "0.1"))
	float LoudnessScalar = 1.0f;
};
