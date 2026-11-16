// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Sound/SoundGenerator.h"
#include "Sound/SoundWaveProcedural.h"
#include "DungeoneerPCMSoundWave.generated.h"

/**
 * Runtime procedural PCM sound wave backed by an immutable decoded int16 sample buffer.
 * Spawns lock-free, per-voice ISoundGenerator instances for UE 5.8 AudioMixer playback
 * with ForceInline loading behavior pre-cached on the GameThread.
 */
UCLASS()
class DARKDUNGEONEER_API UDungeoneerPCMSoundWave : public USoundWaveProcedural
{
	GENERATED_BODY()

public:
	explicit UDungeoneerPCMSoundWave(const FObjectInitializer& ObjectInitializer);

	/** Initializes sample buffers and caches ForceInline loading behavior on the GameThread. */
	bool InitializeFromPCMBytes(
		const uint8* InSampleBytes,
		uint32 InSampleByteSize,
		uint32 InSampleRate,
		uint16 InNumChannels,
		uint16 InBitsPerSample,
		bool bInLooping);

	/** Number of decoded samples stored across all channels. */
	int32 GetSampleCount() const noexcept;

	//~ Begin USoundBase / USoundWave Interface
	virtual float GetDuration() const override;
	virtual Audio::EAudioMixerStreamDataFormat::Type GetGeneratedPCMDataFormat() const override;
	virtual ISoundGeneratorPtr CreateSoundGenerator(const FSoundGeneratorInitParams& InParams) override;
	//~ End USoundBase / USoundWave Interface

private:
	TSharedPtr<const TArray<int16>, ESPMode::ThreadSafe> SharedPCM16Samples;
	uint32 SourceSampleRate = 22050U;
	int32 SourceChannels = 1;
	float ActualDurationSeconds = 0.0f;
};
