#include "Audio/GibberishSpeechComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWave.h"
#include "GameFramework/Actor.h"

UGibberishSpeechComponent::UGibberishSpeechComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UGibberishSpeechComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		SpeechAudioComponent = NewObject<UAudioComponent>(Owner, TEXT("SpeechAudioComponent"));
		if (SpeechAudioComponent)
		{
			SpeechAudioComponent->bAutoActivate = false;
			SpeechAudioComponent->RegisterComponent();
			SpeechAudioComponent->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		}
	}
}

void UGibberishSpeechComponent::EvaluateCharacterPhoneme(TCHAR Character, int32& OutGrainIndex, float& OutSemitoneOffset) noexcept
{
	const uint32 CharCode = static_cast<uint32>(Character);
	OutGrainIndex = static_cast<int32>(CharCode % static_cast<uint32>(NumSyllableGrains));

	if (Character == TEXT('?'))
	{
		OutSemitoneOffset = 3.0f;
	}
	else if (Character == TEXT('.'))
	{
		OutSemitoneOffset = -2.0f;
	}
	else if (Character == TEXT('!'))
	{
		OutSemitoneOffset = 1.5f;
	}
	else
	{
		OutSemitoneOffset = 0.0f;
	}
}

void UGibberishSpeechComponent::PlayHeroVoiceStem(USoundWave* VoiceStem)
{
	bPlayingGibberish = false;
	if (!SpeechAudioComponent || !VoiceStem)
	{
		return;
	}

	SpeechAudioComponent->Stop();
	SpeechAudioComponent->SetSound(VoiceStem);
	SpeechAudioComponent->Play();
}

void UGibberishSpeechComponent::SpeakProceduralGibberish(const FString& DialogueLine)
{
	ActiveLine = DialogueLine;
	CurrentCharCursor = 0;
	CharAccumulator = 0.0f;
	bPlayingGibberish = !ActiveLine.IsEmpty();

	if (bPlayingGibberish && SpeechAudioComponent && GibberishMetaSoundSource)
	{
		SpeechAudioComponent->SetSound(GibberishMetaSoundSource);
		if (!SpeechAudioComponent->IsPlaying())
		{
			SpeechAudioComponent->Play();
		}
	}
}

void UGibberishSpeechComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bPlayingGibberish || CurrentCharCursor >= ActiveLine.Len())
	{
		bPlayingGibberish = false;
		return;
	}

	const float StepInterval = 1.0f / FMath::Max(CharactersPerSecond, 1.0f);
	CharAccumulator += DeltaTime;

	while (CharAccumulator >= StepInterval && CurrentCharCursor < ActiveLine.Len())
	{
		CharAccumulator -= StepInterval;
		const TCHAR Ch = ActiveLine[CurrentCharCursor++];

		if (FChar::IsWhitespace(Ch))
		{
			continue;
		}

		EvaluateCharacterPhoneme(Ch, LastSyllableIndex, LastPitchOffsetSemitones);

		if (SpeechAudioComponent)
		{
			// Push parameters directly to MS_GibberishSynth MetaSound Source
			SpeechAudioComponent->SetIntParameter(TEXT("SyllableGrainIndex"), LastSyllableIndex);
			SpeechAudioComponent->SetFloatParameter(TEXT("PitchOffsetSemitones"), LastPitchOffsetSemitones);
			SpeechAudioComponent->SetTriggerParameter(TEXT("TriggerSyllable"));
		}
	}

	if (CurrentCharCursor >= ActiveLine.Len())
	{
		bPlayingGibberish = false;
	}
}
