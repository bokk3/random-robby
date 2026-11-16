// Copyright Epic Games, Inc. All Rights Reserved.

#include "Audio/DungeoneerPCMSoundWave.h"
#include "HAL/PlatformAtomics.h"

namespace DungeoneerPCMAudioPrivate
{
	class FDungeoneerPCMBufferGenerator final : public ISoundGenerator
	{
	public:
		FDungeoneerPCMBufferGenerator(
			TSharedPtr<const TArray<int16>, ESPMode::ThreadSafe> InSamples,
			uint32 InSourceSampleRate,
			int32 InSourceChannels,
			float InOutputSampleRate,
			int32 InOutputChannels,
			bool bInLooping)
			: Samples(MoveTemp(InSamples))
			, SourceChannels(FMath::Max(1, InSourceChannels))
			, OutputChannels(FMath::Max(1, InOutputChannels))
			, bLooping(bInLooping)
		{
			const double SafeOutRate = FMath::Max(1.0, static_cast<double>(InOutputSampleRate));
			FrameStep = static_cast<double>(FMath::Max(1U, InSourceSampleRate)) / SafeOutRate;
			SourceFrameCount = Samples.IsValid() ? (Samples->Num() / SourceChannels) : 0;
			if (SourceFrameCount <= 0)
			{
				bFinished = true;
			}
		}

		virtual int32 GetNumChannels() const override
		{
			return OutputChannels;
		}

		virtual int32 GetDesiredNumSamplesToRenderPerCallback() const override
		{
			return 1024 * OutputChannels;
		}

		virtual bool IsFinished() const override
		{
			return bFinished;
		}

		virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override
		{
			if (!OutAudio || NumSamples <= 0)
			{
				return 0;
			}

			if (!Samples.IsValid() || SourceFrameCount <= 0 || bFinished)
			{
				FMemory::Memzero(OutAudio, static_cast<SIZE_T>(NumSamples) * sizeof(float));
				bFinished = true;
				return NumSamples;
			}

			const int16* RawSamples = Samples->GetData();
			const int32 OutFrames = NumSamples / OutputChannels;
			static constexpr float Int16ToFloat = 1.0f / 32768.0f;

			for (int32 FrameIdx = 0; FrameIdx < OutFrames; ++FrameIdx)
			{
				if (!bLooping && FrameCursor >= static_cast<double>(SourceFrameCount))
				{
					const int32 StartSample = FrameIdx * OutputChannels;
					const int32 RemainingSamples = NumSamples - StartSample;
					if (RemainingSamples > 0)
					{
						FMemory::Memzero(OutAudio + StartSample, static_cast<SIZE_T>(RemainingSamples) * sizeof(float));
					}
					bFinished = true;
					return NumSamples;
				}

				int32 Frame0 = static_cast<int32>(FrameCursor);
				const float Frac = static_cast<float>(FrameCursor - static_cast<double>(Frame0));
				int32 Frame1 = Frame0 + 1;

				if (bLooping)
				{
					Frame0 %= SourceFrameCount;
					Frame1 %= SourceFrameCount;
				}
				else
				{
					Frame0 = FMath::Clamp(Frame0, 0, SourceFrameCount - 1);
					Frame1 = FMath::Clamp(Frame1, 0, SourceFrameCount - 1);
				}

				for (int32 Ch = 0; Ch < OutputChannels; ++Ch)
				{
					const int32 SrcCh = Ch % SourceChannels;
					const float S0 = static_cast<float>(RawSamples[Frame0 * SourceChannels + SrcCh]) * Int16ToFloat;
					const float S1 = static_cast<float>(RawSamples[Frame1 * SourceChannels + SrcCh]) * Int16ToFloat;
					OutAudio[FrameIdx * OutputChannels + Ch] = FMath::Lerp(S0, S1, Frac);
				}

				FrameCursor += FrameStep;
				if (bLooping && FrameCursor >= static_cast<double>(SourceFrameCount))
				{
					FrameCursor = FMath::Fmod(FrameCursor, static_cast<double>(SourceFrameCount));
				}
			}

			return OutFrames * OutputChannels;
		}

	private:
		TSharedPtr<const TArray<int16>, ESPMode::ThreadSafe> Samples;
		int32 SourceChannels = 1;
		int32 OutputChannels = 1;
		int32 SourceFrameCount = 0;
		double FrameStep = 1.0;
		double FrameCursor = 0.0;
		bool bLooping = false;
		bool bFinished = false;
	};
}

UDungeoneerPCMSoundWave::UDungeoneerPCMSoundWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bProcedural = true;
	bCanProcessAsync = true;
	DecompressionType = DTYPE_Procedural;
}

bool UDungeoneerPCMSoundWave::InitializeFromPCMBytes(
	const uint8* InSampleBytes,
	const uint32 InSampleByteSize,
	const uint32 InSampleRate,
	const uint16 InNumChannels,
	const uint16 InBitsPerSample,
	const bool bInLooping)
{
	check(IsInGameThread());

	if (!InSampleBytes || InSampleByteSize == 0)
	{
		return false;
	}

	SourceSampleRate = FMath::Max(1U, InSampleRate);
	SourceChannels = FMath::Max(1, static_cast<int32>(InNumChannels));

	TSharedRef<TArray<int16>, ESPMode::ThreadSafe> Decoded = MakeShared<TArray<int16>, ESPMode::ThreadSafe>();
	if (InBitsPerSample == 8)
	{
		const int32 NumSamples = static_cast<int32>(InSampleByteSize);
		Decoded->SetNumUninitialized(NumSamples);
		int16* Dst = Decoded->GetData();
		for (int32 i = 0; i < NumSamples; ++i)
		{
			Dst[i] = static_cast<int16>((static_cast<int32>(InSampleBytes[i]) - 128) << 8);
		}
	}
	else
	{
		const int32 NumSamples = static_cast<int32>(InSampleByteSize / sizeof(int16));
		if (NumSamples <= 0)
		{
			return false;
		}
		Decoded->SetNumUninitialized(NumSamples);
		FMemory::Memcpy(Decoded->GetData(), InSampleBytes, static_cast<SIZE_T>(NumSamples) * sizeof(int16));
	}

	SharedPCM16Samples = Decoded;

	const int32 FrameCount = Decoded->Num() / SourceChannels;
	ActualDurationSeconds = static_cast<float>(FrameCount) / static_cast<float>(SourceSampleRate);

	bProcedural = true;
	bCanProcessAsync = true;
	bLooping = bInLooping;
	DecompressionType = DTYPE_Procedural;
	VirtualizationMode = bInLooping ? EVirtualizationMode::PlayWhenSilent : EVirtualizationMode::Disabled;
	NumChannels = SourceChannels;
	SetSampleRate(SourceSampleRate);
	SetImportedSampleRate(SourceSampleRate);
	Duration = bInLooping ? INDEFINITELY_LOOPING_DURATION : ActualDurationSeconds;

	// Populate WorkingSoundWaveData->LoadingBehavior on the GameThread (RetainOnLoad <= all cache CVars)
	// so AudioMixer never hits SoundWave.cpp:5070 CacheInheritedLoadingBehavior() on an audio thread.
	OverrideLoadingBehavior(ESoundWaveLoadingBehavior::RetainOnLoad);
	const ESoundWaveLoadingBehavior ResolvedBehavior = GetLoadingBehavior(true);
	check(ResolvedBehavior != ESoundWaveLoadingBehavior::Uninitialized
		&& ResolvedBehavior != ESoundWaveLoadingBehavior::Inherited);

	return FrameCount > 0;
}

int32 UDungeoneerPCMSoundWave::GetSampleCount() const noexcept
{
	return SharedPCM16Samples.IsValid() ? SharedPCM16Samples->Num() : 0;
}

float UDungeoneerPCMSoundWave::GetDuration() const
{
	return bLooping ? INDEFINITELY_LOOPING_DURATION : ActualDurationSeconds;
}

Audio::EAudioMixerStreamDataFormat::Type UDungeoneerPCMSoundWave::GetGeneratedPCMDataFormat() const
{
	return Audio::EAudioMixerStreamDataFormat::Float;
}

ISoundGeneratorPtr UDungeoneerPCMSoundWave::CreateSoundGenerator(const FSoundGeneratorInitParams& InParams)
{
	if (!SharedPCM16Samples.IsValid() || SharedPCM16Samples->IsEmpty())
	{
		return nullptr;
	}

	const float OutRate = (InParams.SampleRate > 0.0f) ? InParams.SampleRate : static_cast<float>(SourceSampleRate);
	const int32 OutCh = (InParams.NumChannels > 0) ? InParams.NumChannels : SourceChannels;
	return MakeShared<DungeoneerPCMAudioPrivate::FDungeoneerPCMBufferGenerator, ESPMode::ThreadSafe>(
		SharedPCM16Samples,
		SourceSampleRate,
		SourceChannels,
		OutRate,
		OutCh,
		bLooping != 0);
}
