#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "Core/DungeoneerMovementConfig.h"
#include "Rendering/Dungeoneer25DMaterialBuilder.h"
#include "Audio.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "ImageUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Sound/SoundWave.h"
#include "TextureResource.h"
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

	// Auto-import generated 2.5D rig and dungeon textures and crop single-cell tiles on startup
	ImportAllRigTexturesFromDisk();
	ImportAllDungeonTexturesFromDisk();
	ExtractDefaultSingleCellTiles();
}

void UDungeoneerAssetBootstrapSubsystem::Deinitialize()
{
	LoadedRigTextures.Reset();
	LoadedDungeonTextures.Reset();
	ExtractedCellTextures.Reset();
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

int32 UDungeoneerAssetBootstrapSubsystem::ImportAllDungeonTexturesFromDisk()
{
	const FString DungeonRoot = FPaths::ProjectContentDir() / TEXT("Art/Dungeon");
	if (!FPaths::DirectoryExists(DungeonRoot))
	{
		return LoadedDungeonTextures.Num();
	}

	static constexpr const TCHAR* DungeonSets[] =
	{
		TEXT("ATLAS_Dungeon_Floors_4x4"),
		TEXT("PROP_Dungeon_Walls_And_Cutaways_4x4"),
		TEXT("PROP_Mirror_Palace_Maze_4x4"),
		TEXT("PROP_Breakables_And_Traps_4x4")
	};

	struct FDungeonChannelSpec
	{
		const TCHAR* Suffix;
		bool bIsNormalMap;
		bool bSRGB;
	};
	static constexpr FDungeonChannelSpec ChannelSpecs[] =
	{
		{ TEXT("D"),   false, true },
		{ TEXT("N"),   true,  false },
		{ TEXT("H"),   false, false },
		{ TEXT("R"),   false, false },
		{ TEXT("ORM"), false, false },
		{ TEXT("E"),   false, true }
	};

	for (const TCHAR* SetName : DungeonSets)
	{
		for (const FDungeonChannelSpec& Channel : ChannelSpecs)
		{
			const FName Key(*FString::Printf(TEXT("%s_%s"), SetName, Channel.Suffix));
			if (LoadedDungeonTextures.Contains(Key))
			{
				continue;
			}

			const FString FilePath = DungeonRoot / SetName / FString::Printf(TEXT("%s_%s.png"), SetName, Channel.Suffix);
			if (!FPaths::FileExists(FilePath))
			{
				continue;
			}

			if (UTexture2D* ImportedTex = FImageUtils::ImportFileAsTexture2D(FilePath))
			{
				FDungeoneer25DMaterialBuilder::ConfigureTextureFor25D(ImportedTex, Channel.bIsNormalMap, Channel.bSRGB);
				LoadedDungeonTextures.Add(Key, ImportedTex);
			}
		}
	}

	return LoadedDungeonTextures.Num();
}

int32 UDungeoneerAssetBootstrapSubsystem::ExtractDefaultSingleCellTiles()
{
	const FString DungeonRoot = FPaths::ProjectContentDir() / TEXT("Art/Dungeon");
	const FString RigsRoot = FPaths::ProjectContentDir() / TEXT("Art/Rigs");

	struct FCellExtractSpec
	{
		const TCHAR* BaseDir;
		const TCHAR* SubDir;
		const TCHAR* FileName;
		const TCHAR* CacheKey;
		int32 CellIndex;
		bool bIsNormalMap;
		bool bSRGB;
	};

	const FCellExtractSpec Specs[] =
	{
		// Dungeon Environment Cells (4x4 atlases)
		{ *DungeonRoot, TEXT("ATLAS_Dungeon_Floors_4x4"),            TEXT("ATLAS_Dungeon_Floors_4x4_D.png"),            TEXT("Floor_Tile0_D"),       0, false, true },
		{ *DungeonRoot, TEXT("ATLAS_Dungeon_Floors_4x4"),            TEXT("ATLAS_Dungeon_Floors_4x4_N.png"),            TEXT("Floor_Tile0_N"),       0, true,  false },
		{ *DungeonRoot, TEXT("ATLAS_Dungeon_Floors_4x4"),            TEXT("ATLAS_Dungeon_Floors_4x4_D.png"),            TEXT("Floor_Wet0_D"),        4, false, true },
		{ *DungeonRoot, TEXT("PROP_Dungeon_Walls_And_Cutaways_4x4"), TEXT("PROP_Dungeon_Walls_And_Cutaways_4x4_D.png"), TEXT("Wall_Full0_D"),        0, false, true },
		{ *DungeonRoot, TEXT("PROP_Dungeon_Walls_And_Cutaways_4x4"), TEXT("PROP_Dungeon_Walls_And_Cutaways_4x4_N.png"), TEXT("Wall_Full0_N"),        0, true,  false },
		{ *DungeonRoot, TEXT("PROP_Dungeon_Walls_And_Cutaways_4x4"), TEXT("PROP_Dungeon_Walls_And_Cutaways_4x4_D.png"), TEXT("Wall_Cutaway_D"),      4, false, true },
		{ *DungeonRoot, TEXT("PROP_Mirror_Palace_Maze_4x4"),         TEXT("PROP_Mirror_Palace_Maze_4x4_D.png"),         TEXT("Mirror_Intact_D"),     0, false, true },
		{ *DungeonRoot, TEXT("PROP_Mirror_Palace_Maze_4x4"),         TEXT("PROP_Mirror_Palace_Maze_4x4_N.png"),         TEXT("Mirror_Intact_N"),     0, true,  false },
		{ *DungeonRoot, TEXT("PROP_Breakables_And_Traps_4x4"),       TEXT("PROP_Breakables_And_Traps_4x4_D.png"),       TEXT("Breakable_Intact_D"),  0, false, true },
		{ *DungeonRoot, TEXT("PROP_Breakables_And_Traps_4x4"),       TEXT("PROP_Breakables_And_Traps_4x4_N.png"),       TEXT("Breakable_Intact_N"),  0, true,  false },
		// Character & Zombie Sprite Cells (Cell 0 = South Idle/Step0)
		{ *RigsRoot,    TEXT("DungeoneerHero"),                      TEXT("Rig_DungeoneerHero_Raw_D.png"),              TEXT("Hero_Cell0_D"),        0, false, true },
		{ *RigsRoot,    TEXT("DungeoneerHero"),                      TEXT("Rig_DungeoneerHero_Raw_N.png"),              TEXT("Hero_Cell0_N"),        0, true,  false },
		{ *RigsRoot,    TEXT("ZombieExploder"),                      TEXT("Rig_ZombieExploder_Raw_D.png"),              TEXT("Exploder_Cell0_D"),    0, false, true },
		{ *RigsRoot,    TEXT("ZombieExploder"),                      TEXT("Rig_ZombieExploder_Raw_N.png"),              TEXT("Exploder_Cell0_N"),    0, true,  false },
		{ *RigsRoot,    TEXT("ZombieFastThrower"),                   TEXT("Rig_ZombieFastThrower_Raw_D.png"),           TEXT("Thrower_Cell0_D"),     0, false, true },
		{ *RigsRoot,    TEXT("ZombieFastThrower"),                   TEXT("Rig_ZombieFastThrower_Raw_N.png"),           TEXT("Thrower_Cell0_N"),     0, true,  false }
	};

	for (const FCellExtractSpec& Spec : Specs)
	{
		const FString FullPath = FString(Spec.BaseDir) / Spec.SubDir / Spec.FileName;
		ExtractAtlasCellTexture(FullPath, FName(Spec.CacheKey), Spec.CellIndex, 4, 4, Spec.bIsNormalMap, Spec.bSRGB);
	}

	return ExtractedCellTextures.Num();
}

UTexture2D* UDungeoneerAssetBootstrapSubsystem::ExtractAtlasCellTexture(
	const FString& PngFilePath,
	const FName CacheKey,
	const int32 CellIndex,
	const int32 GridCols,
	const int32 GridRows,
	const bool bIsNormalMap,
	const bool bSRGB)
{
	if (TObjectPtr<UTexture2D>* Existing = ExtractedCellTextures.Find(CacheKey))
	{
		return Existing->Get();
	}

	if (GridCols <= 0 || GridRows <= 0 || !FPaths::FileExists(PngFilePath))
	{
		return nullptr;
	}

	TArray<uint8> CompressedBytes;
	if (!FFileHelper::LoadFileToArray(CompressedBytes, *PngFilePath) || CompressedBytes.Num() == 0)
	{
		return nullptr;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(CompressedBytes.GetData(), CompressedBytes.Num()))
	{
		return nullptr;
	}

	const int32 FullWidth = ImageWrapper->GetWidth();
	const int32 FullHeight = ImageWrapper->GetHeight();
	if (FullWidth < GridCols || FullHeight < GridRows)
	{
		return nullptr;
	}

	TArray64<uint8> RawBGRA;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawBGRA))
	{
		return nullptr;
	}

	const int32 CellWidth = FullWidth / GridCols;
	const int32 CellHeight = FullHeight / GridRows;
	const int32 TotalCells = GridCols * GridRows;
	const int32 ClampedCell = FMath::Clamp(CellIndex, 0, TotalCells - 1);
	const int32 CellCol = ClampedCell % GridCols;
	const int32 CellRow = ClampedCell / GridCols;
	const int32 StartX = CellCol * CellWidth;
	const int32 StartY = CellRow * CellHeight;

	UTexture2D* CellTex = UTexture2D::CreateTransient(CellWidth, CellHeight, PF_B8G8R8A8);
	if (!CellTex)
	{
		return nullptr;
	}

	FTexturePlatformData* PlatformData = CellTex->GetPlatformData();
	if (!PlatformData || PlatformData->Mips.Num() == 0)
	{
		return nullptr;
	}

	FTexture2DMipMap& Mip = PlatformData->Mips[0];
	uint8* DestBytes = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
	if (!DestBytes)
	{
		Mip.BulkData.Unlock();
		return nullptr;
	}

	const int64 SrcStride = static_cast<int64>(FullWidth) * 4;
	const int64 DestStride = static_cast<int64>(CellWidth) * 4;
	const uint8* SrcBase = RawBGRA.GetData();

	for (int32 RowY = 0; RowY < CellHeight; ++RowY)
	{
		const uint8* SrcRow = SrcBase + static_cast<int64>(StartY + RowY) * SrcStride + static_cast<int64>(StartX) * 4;
		uint8* DstRow = DestBytes + static_cast<int64>(RowY) * DestStride;
		FMemory::Memcpy(DstRow, SrcRow, DestStride);
	}

	Mip.BulkData.Unlock();

	FDungeoneer25DMaterialBuilder::ConfigureTextureFor25D(CellTex, bIsNormalMap, bSRGB);
	ExtractedCellTextures.Add(CacheKey, CellTex);
	return CellTex;
}

UTexture2D* UDungeoneerAssetBootstrapSubsystem::ResolveAnyTextureByKey(const FName Key) const noexcept
{
	if (Key.IsNone())
	{
		return nullptr;
	}
	if (const TObjectPtr<UTexture2D>* Cell = ExtractedCellTextures.Find(Key))
	{
		return Cell->Get();
	}
	if (const TObjectPtr<UTexture2D>* Dungeon = LoadedDungeonTextures.Find(Key))
	{
		return Dungeon->Get();
	}
	if (const TObjectPtr<UTexture2D>* Rig = LoadedRigTextures.Find(Key))
	{
		return Rig->Get();
	}
	return nullptr;
}

UMaterialInstanceDynamic* UDungeoneerAssetBootstrapSubsystem::CreateDungeonOrRigMaterial(
	UObject* Outer,
	const FName DiffuseKey,
	const FName NormalKey,
	const FLinearColor& FallbackTint,
	const float RoughnessVal,
	const bool bForHISM)
{
	UObject* EffectiveOuter = Outer ? Outer : this;
	UTexture2D* DiffuseTex = ResolveAnyTextureByKey(DiffuseKey);
	UTexture2D* NormalTex = ResolveAnyTextureByKey(NormalKey);

	if (DiffuseTex)
	{
		const TCHAR* EngineMatPath = bForHISM
			? TEXT("/Engine/EngineDebugMaterials/VertexPaint_2Tex_Color.VertexPaint_2Tex_Color")
			: TEXT("/Engine/Engine_MI_Shaders/M_Shader_Simple.M_Shader_Simple");

		if (UMaterialInterface* EngineMat = LoadObject<UMaterialInterface>(nullptr, EngineMatPath))
		{
			if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(EngineMat, EffectiveOuter))
			{
				MID->SetTextureParameterValue(FName(TEXT("Diffuse")), DiffuseTex);
				if (bForHISM)
				{
					MID->SetTextureParameterValue(FName(TEXT("Diffuse2")), DiffuseTex);
				}

				if (NormalTex)
				{
					MID->SetTextureParameterValue(FName(TEXT("Normal")), NormalTex);
					if (bForHISM)
					{
						MID->SetTextureParameterValue(FName(TEXT("Normal2")), NormalTex);
					}
				}

				// Verify the pre-compiled engine shader actually exposes "Diffuse"
				if (MID->K2_GetTextureParameterValue(FName(TEXT("Diffuse"))) == DiffuseTex)
				{
					const FLinearColor SubtleTint = FMath::Lerp(FLinearColor::White, FallbackTint, 0.25f);
					MID->SetVectorParameterValue(FName(TEXT("DiffuseColor")), SubtleTint);
					MID->SetScalarParameterValue(FName(TEXT("Roughness")), RoughnessVal);
					MID->SetScalarParameterValue(FName(TEXT("UsePinkDebug")), 0.0f);
					return MID;
				}
			}
		}
	}

	if (UMaterialInterface* BaseShapeMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
	{
		if (UMaterialInstanceDynamic* FallbackMID = UMaterialInstanceDynamic::Create(BaseShapeMat, EffectiveOuter))
		{
			FallbackMID->SetVectorParameterValue(FName(TEXT("Color")), FallbackTint);
			FallbackMID->SetScalarParameterValue(FName(TEXT("Roughness")), RoughnessVal);
			return FallbackMID;
		}
	}

	return nullptr;
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

UTexture2D* UDungeoneerAssetBootstrapSubsystem::GetLoadedDungeonTexture(FName Key) const noexcept
{
	const TObjectPtr<UTexture2D>* Found = LoadedDungeonTextures.Find(Key);
	return Found ? Found->Get() : nullptr;
}

UTexture2D* UDungeoneerAssetBootstrapSubsystem::GetExtractedCellTexture(FName Key) const noexcept
{
	const TObjectPtr<UTexture2D>* Found = ExtractedCellTextures.Find(Key);
	return Found ? Found->Get() : nullptr;
}

USoundWave* UDungeoneerAssetBootstrapSubsystem::GetLoadedVoiceStem(FName Key) const noexcept
{
	const TObjectPtr<USoundWave>* Found = LoadedVoiceStems.Find(Key);
	return Found ? Found->Get() : nullptr;
}
