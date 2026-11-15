#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "Core/DungeoneerMovementConfig.h"
#include "Rendering/Dungeoneer25DMaterialBuilder.h"
#include "Audio.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Sound/SoundWave.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace AssetBootstrapPrivate
{
	static bool SaveAssetPackageToContent(UObject* Asset, const TCHAR* LongPackageName)
	{
		if (!Asset)
		{
			return false;
		}

		FString DiskFilename;
		if (!FPackageName::TryConvertLongPackageNameToFilename(LongPackageName, DiskFilename, FPackageName::GetAssetPackageExtension()))
		{
			return false;
		}

		if (FPaths::FileExists(DiskFilename) && IFileManager::Get().FileSize(*DiskFilename) > 0)
		{
			return true;
		}

		IFileManager::Get().MakeDirectory(*FPaths::GetPath(DiskFilename), true);

		if (UPackage* Pkg = Asset->GetOutermost(); Pkg && !Pkg->IsFullyLoaded())
		{
			Pkg->FullyLoad();
		}

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		return UPackage::SavePackage(Asset->GetOutermost(), Asset, *DiskFilename, SaveArgs);
	}
}

void UDungeoneerAssetBootstrapSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void UDungeoneerAssetBootstrapSubsystem::Deinitialize()
{
	LoadedRigTextures.Reset();
	LoadedVoiceStems.Reset();
	SensoryMPC = nullptr;
	MaterialConfigAsset = nullptr;
	MovementConfigAsset = nullptr;
	Super::Deinitialize();
}

int32 UDungeoneerAssetBootstrapSubsystem::ImportAllRigTexturesFromDisk()
{
	const FString RigsRoot = FPaths::ProjectContentDir() / TEXT("Art/Rigs");

	struct FRigDiskSpec
	{
		const TCHAR* KeyPrefix;
		const TCHAR* SubDir;
		const TCHAR* FilePrefix;
	};
	static constexpr FRigDiskSpec RigSpecs[] =
	{
		{ TEXT("Hero"),              TEXT("Hero"),              TEXT("Rig_Hero") },
		{ TEXT("Exploder"),          TEXT("Exploder"),          TEXT("Rig_Exploder") },
		{ TEXT("Thrower"),           TEXT("Thrower"),           TEXT("Rig_Thrower") },
		{ TEXT("DungeoneerHero"),    TEXT("DungeoneerHero"),    TEXT("Rig_DungeoneerHero_Raw") },
		{ TEXT("ZombieExploder"),    TEXT("ZombieExploder"),    TEXT("Rig_ZombieExploder_Raw") },
		{ TEXT("ZombieFastThrower"), TEXT("ZombieFastThrower"), TEXT("Rig_ZombieFastThrower_Raw") }
	};

	struct FRigChannelSpec
	{
		const TCHAR* Suffix;
		bool bIsNormalMap;
		bool bSRGB;
	};
	static constexpr FRigChannelSpec ChannelSpecs[] =
	{
		{ TEXT("D"),   false, true },
		{ TEXT("N"),   true,  false },
		{ TEXT("ORM"), false, false }
	};

	for (const FRigDiskSpec& Rig : RigSpecs)
	{
		for (const FRigChannelSpec& Channel : ChannelSpecs)
		{
			const FString KeyNameStr = FString::Printf(TEXT("%s_%s"), Rig.KeyPrefix, Channel.Suffix);
			const FName Key(*KeyNameStr);
			if (LoadedRigTextures.Contains(Key))
			{
				continue;
			}

			const FString FilePath = RigsRoot / Rig.SubDir / FString::Printf(TEXT("%s_%s.png"), Rig.FilePrefix, Channel.Suffix);
			if (!FPaths::FileExists(FilePath))
			{
				continue;
			}

			if (UTexture2D* ImportedTex = FImageUtils::ImportFileAsTexture2D(FilePath))
			{
				FDungeoneer25DMaterialBuilder::ConfigureTextureFor25D(ImportedTex, Channel.bIsNormalMap, Channel.bSRGB);
				LoadedRigTextures.Add(Key, ImportedTex);
			}
		}
	}

	return LoadedRigTextures.Num();
}

int32 UDungeoneerAssetBootstrapSubsystem::ImportAllVoiceStemsFromDisk()
{
	const FString VoicesRoot = FPaths::ProjectContentDir() / TEXT("Audio/Voices");
	if (!FPaths::DirectoryExists(VoicesRoot))
	{
		return LoadedVoiceStems.Num();
	}

	TArray<FString> WavFiles;
	IFileManager::Get().FindFilesRecursive(WavFiles, *VoicesRoot, TEXT("*.wav"), true, false);
	WavFiles.Sort();

	TArray<uint8> RawWavBytes;
	for (const FString& WavPath : WavFiles)
	{
		const FString BankFolder = FPaths::GetBaseFilename(FPaths::GetPath(WavPath));
		const FString FileStem = FPaths::GetBaseFilename(WavPath);
		const FName Key(*FString::Printf(TEXT("%s/%s"), *BankFolder, *FileStem));
		if (LoadedVoiceStems.Contains(Key))
		{
			continue;
		}

		RawWavBytes.Reset();
		if (!FFileHelper::LoadFileToArray(RawWavBytes, *WavPath) || RawWavBytes.Num() <= 44)
		{
			continue;
		}

		FWaveModInfo WaveInfo;
		if (!WaveInfo.ReadWaveInfo(RawWavBytes.GetData(), RawWavBytes.Num()))
		{
			continue;
		}

		const uint32 SampleRate = WaveInfo.pSamplesPerSec ? *WaveInfo.pSamplesPerSec : 22050U;
		const uint16 Channels = WaveInfo.pChannels ? *WaveInfo.pChannels : 1U;
		const uint16 Bits = WaveInfo.pBitsPerSample ? *WaveInfo.pBitsPerSample : 16U;
		const uint32 ByteRate = FMath::Max(1U, static_cast<uint32>(SampleRate * Channels * FMath::Max<uint16>(1U, Bits / 8U)));

		USoundWave* SoundWave = NewObject<USoundWave>(this);
		if (!SoundWave)
		{
			continue;
		}

		SoundWave->SetSampleRate(SampleRate);
		SoundWave->NumChannels = static_cast<int32>(Channels);
		SoundWave->Duration = static_cast<float>(WaveInfo.SampleDataSize) / static_cast<float>(ByteRate);

		if (WaveInfo.SampleDataSize > 0 && WaveInfo.SampleDataStart)
		{
			SoundWave->RawPCMDataSize = static_cast<int32>(WaveInfo.SampleDataSize);
			SoundWave->RawPCMData = static_cast<uint8*>(FMemory::Malloc(WaveInfo.SampleDataSize));
			FMemory::Memcpy(SoundWave->RawPCMData, WaveInfo.SampleDataStart, WaveInfo.SampleDataSize);
		}

		LoadedVoiceStems.Add(Key, SoundWave);
	}

	return LoadedVoiceStems.Num();
}

UMaterialParameterCollection* UDungeoneerAssetBootstrapSubsystem::EnsureSensoryMPC()
{
	if (!SensoryMPC && FPackageName::DoesPackageExist(TEXT("/Game/Materials/MPC_SensoryState")))
	{
		SensoryMPC = LoadObject<UMaterialParameterCollection>(nullptr, TEXT("/Game/Materials/MPC_SensoryState.MPC_SensoryState"));
	}

	if (!SensoryMPC)
	{
		UPackage* MPCPackage = CreatePackage(TEXT("/Game/Materials/MPC_SensoryState"));
		SensoryMPC = NewObject<UMaterialParameterCollection>(MPCPackage, TEXT("MPC_SensoryState"), RF_Public | RF_Standalone);
	}

	if (SensoryMPC && SensoryMPC->GetScalarParameterByName(FName(TEXT("HearingPulseRadius"))) == nullptr)
	{
		auto AddScalar = [this](const TCHAR* Name, float DefaultVal)
		{
			FCollectionScalarParameter Param;
			Param.ParameterName = FName(Name);
			Param.DefaultValue = DefaultVal;
			SensoryMPC->ScalarParameters.Add(Param);
		};
		auto AddVector = [this](const TCHAR* Name, const FLinearColor& DefaultColor)
		{
			FCollectionVectorParameter Param;
			Param.ParameterName = FName(Name);
			Param.DefaultValue = DefaultColor;
			SensoryMPC->VectorParameters.Add(Param);
		};

		AddScalar(TEXT("HearingPulseRadius"), 0.0f);
		AddScalar(TEXT("DarkVisionRadius"), 450.0f);
		AddScalar(TEXT("DarkVisionFogExtinction"), 1.0f);
		AddScalar(TEXT("AcuteHearingIntensity"), 0.0f);
		AddScalar(TEXT("ScentThermalIntensity"), 0.0f);

		AddVector(TEXT("HearingPulseOrigin"), FLinearColor::Black);
		AddVector(TEXT("PlayerWorldOrigin"), FLinearColor::Black);
	}

	return SensoryMPC.Get();
}

bool UDungeoneerAssetBootstrapSubsystem::SaveCoreDataAssetsToDisk()
{
	UMaterialParameterCollection* MPC = EnsureSensoryMPC();
	if (!MPC)
	{
		return false;
	}

	if (!MaterialConfigAsset && FPackageName::DoesPackageExist(TEXT("/Game/Materials/DA_25DMaterialConfig")))
	{
		MaterialConfigAsset = LoadObject<UDungeoneer25DMaterialConfig>(nullptr, TEXT("/Game/Materials/DA_25DMaterialConfig.DA_25DMaterialConfig"));
	}

	if (!MaterialConfigAsset)
	{
		UPackage* MatCfgPkg = CreatePackage(TEXT("/Game/Materials/DA_25DMaterialConfig"));
		MaterialConfigAsset = NewObject<UDungeoneer25DMaterialConfig>(MatCfgPkg, TEXT("DA_25DMaterialConfig"), RF_Public | RF_Standalone);
	}
	if (MaterialConfigAsset)
	{
		MaterialConfigAsset->SensoryMaterialParameterCollection = MPC;
	}

	if (!MovementConfigAsset && FPackageName::DoesPackageExist(TEXT("/Game/Core/DA_MovementConfig")))
	{
		MovementConfigAsset = LoadObject<UDungeoneerMovementConfig>(nullptr, TEXT("/Game/Core/DA_MovementConfig.DA_MovementConfig"));
	}

	if (!MovementConfigAsset)
	{
		UPackage* MoveCfgPkg = CreatePackage(TEXT("/Game/Core/DA_MovementConfig"));
		MovementConfigAsset = NewObject<UDungeoneerMovementConfig>(MoveCfgPkg, TEXT("DA_MovementConfig"), RF_Public | RF_Standalone);
	}

	const bool bSavedMPC = AssetBootstrapPrivate::SaveAssetPackageToContent(MPC, TEXT("/Game/Materials/MPC_SensoryState"));
	const bool bSavedMatCfg = AssetBootstrapPrivate::SaveAssetPackageToContent(MaterialConfigAsset.Get(), TEXT("/Game/Materials/DA_25DMaterialConfig"));
	const bool bSavedMoveCfg = AssetBootstrapPrivate::SaveAssetPackageToContent(MovementConfigAsset.Get(), TEXT("/Game/Core/DA_MovementConfig"));

	return bSavedMPC && bSavedMatCfg && bSavedMoveCfg;
}

UTexture2D* UDungeoneerAssetBootstrapSubsystem::GetLoadedRigTexture(FName Key) const noexcept
{
	const TObjectPtr<UTexture2D>* Found = LoadedRigTextures.Find(Key);
	return Found ? Found->Get() : nullptr;
}

USoundWave* UDungeoneerAssetBootstrapSubsystem::GetLoadedVoiceStem(FName Key) const noexcept
{
	const TObjectPtr<USoundWave>* Found = LoadedVoiceStems.Find(Key);
	return Found ? Found->Get() : nullptr;
}
