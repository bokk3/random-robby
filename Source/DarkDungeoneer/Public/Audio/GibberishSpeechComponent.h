#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GibberishSpeechComponent.generated.h"

class UAudioComponent;
class USoundBase;
class USoundWave;

/**
 * Dual-Path Dialogue & Procedural Gibberish MetaSound Component (`voice-gibberish-metasound`).
 * - Hero Moments / Key Lore / Major Vendor Greetings -> Plays full offline-generated voice stem.
 * - Minor Moments / Browsing Vendor / Ambient Chatter -> Drives MS_GibberishSynth MetaSound
 *   at CharactersPerSecond using deterministic ASCII hash `CharCode % 24` and punctuation pitch shifts:
 *     '?' -> +3.0 semitones, '.' -> -2.0 semitones.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class DARKDUNGEONEER_API UGibberishSpeechComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGibberishSpeechComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Plays a full offline-generated voice stem for Hero / Major Lore / Key Greeting moments. */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void PlayHeroVoiceStem(USoundWave* VoiceStem);

	/** Starts deterministic gibberish MetaSound playback driven by subtitle string characters. */
	UFUNCTION(BlueprintCallable, Category = "Voice")
	void SpeakProceduralGibberish(const FString& DialogueLine);

	/** Pure deterministic helper mapping a character to grain index [0..23] and semitone pitch offset. */
	static void EvaluateCharacterPhoneme(TCHAR Character, int32& OutGrainIndex, float& OutSemitoneOffset) noexcept;

	UFUNCTION(BlueprintPure, Category = "Voice|State")
	const FString& GetActiveDialogueLine() const noexcept { return ActiveLine; }

	UFUNCTION(BlueprintPure, Category = "Voice|State")
	int32 GetCurrentCharCursor() const noexcept { return CurrentCharCursor; }

	UFUNCTION(BlueprintPure, Category = "Voice|State")
	int32 GetLastSyllableIndex() const noexcept { return LastSyllableIndex; }

	UFUNCTION(BlueprintPure, Category = "Voice|State")
	float GetLastPitchOffsetSemitones() const noexcept { return LastPitchOffsetSemitones; }

	UFUNCTION(BlueprintPure, Category = "Voice|State")
	bool IsPlayingGibberish() const noexcept { return bPlayingGibberish; }

	static constexpr int32 NumSyllableGrains = 24;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voice|MetaSound")
	TObjectPtr<USoundBase> GibberishMetaSoundSource;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Voice|Config", meta = (ClampMin = "1.0"))
	float CharactersPerSecond = 16.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Voice|State")
	int32 LastSyllableIndex = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Voice|State")
	float LastPitchOffsetSemitones = 0.0f;

private:
	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> SpeechAudioComponent;

	FString ActiveLine;
	int32 CurrentCharCursor = 0;
	float CharAccumulator = 0.0f;
	bool bPlayingGibberish = false;
};
