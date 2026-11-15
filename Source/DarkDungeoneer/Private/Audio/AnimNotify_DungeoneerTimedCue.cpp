#include "Audio/AnimNotify_DungeoneerTimedCue.h"
#include "Audio/DungeoneerTimedAudioComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Kismet/GameplayStatics.h"

void UAnimNotify_DungeoneerTimedCue::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	if (UDungeoneerTimedAudioComponent* TimedAudio = OwnerActor->FindComponentByClass<UDungeoneerTimedAudioComponent>())
	{
		TimedAudio->TriggerAnimNotifyCue(CueKind, CueSound, OwnerActor->GetActorLocation(), LoudnessScalar);
	}
	else if (CueSound)
	{
		UGameplayStatics::PlaySoundAtLocation(MeshComp, CueSound, OwnerActor->GetActorLocation(), LoudnessScalar);
	}
}
