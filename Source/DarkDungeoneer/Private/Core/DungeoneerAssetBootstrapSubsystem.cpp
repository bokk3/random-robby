#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "Audio/DungeoneerPCMSoundWave.h"
#include "Core/DungeoneerMovementConfig.h"
#include "Rendering/Dungeoneer25DMaterialBuilder.h"
#include "Rendering/DungeoneerEcoMaterialFactory.h"
#include "Audio.h"
#include "Engine/StaticMesh.h"
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
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "Sound/SoundWave.h"
#include "StaticMeshResources.h"
#include "TextureResource.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace AssetBootstrapPrivate
{
	static UDungeoneerPCMSoundWave* CreateRuntimePCMSoundWaveFromWavFile(
		UObject* Outer,
		const FString& WavPath,
		const bool bLooping,
		TArray<uint8>& ScratchRawWavBytes)
	{
		ScratchRawWavBytes.Reset();
		if (!Outer || !FFileHelper::LoadFileToArray(ScratchRawWavBytes, *WavPath) || ScratchRawWavBytes.Num() <= 44)
		{
			return nullptr;
		}

		FWaveModInfo WaveInfo;
		if (!WaveInfo.ReadWaveInfo(ScratchRawWavBytes.GetData(), ScratchRawWavBytes.Num())
			|| WaveInfo.SampleDataSize == 0
			|| !WaveInfo.SampleDataStart)
		{
			return nullptr;
		}

		const uint32 SampleRate = WaveInfo.pSamplesPerSec ? *WaveInfo.pSamplesPerSec : 22050U;
		const uint16 Channels = WaveInfo.pChannels ? *WaveInfo.pChannels : 1U;
		const uint16 Bits = WaveInfo.pBitsPerSample ? *WaveInfo.pBitsPerSample : 16U;

		UDungeoneerPCMSoundWave* SoundWave = NewObject<UDungeoneerPCMSoundWave>(Outer);
		if (!SoundWave)
		{
			return nullptr;
		}

		if (!SoundWave->InitializeFromPCMBytes(
			WaveInfo.SampleDataStart,
			WaveInfo.SampleDataSize,
			SampleRate,
			Channels,
			Bits,
			bLooping))
		{
			return nullptr;
		}

		return SoundWave;
	}

	static bool SaveAssetPackageToContent(UObject* Asset, const TCHAR* LongPackageName, bool bForceOverwrite = false)
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

		if (!bForceOverwrite && FPaths::FileExists(DiskFilename) && IFileManager::Get().FileSize(*DiskFilename) > 0)
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
	ComputeSpritePlaneOrientation();
	ImportAllRigTexturesFromDisk();
	ImportAllDungeonTexturesFromDisk();
	ExtractDefaultSingleCellTiles();
	ImportAllEcosystemTexturesFromDisk();
	ImportAllFoleyFromDisk();
}

void UDungeoneerAssetBootstrapSubsystem::Deinitialize()
{
	LoadedRigTextures.Reset();
	LoadedDungeonTextures.Reset();
	ExtractedCellTextures.Reset();
	SpriteMaskTextures.Reset();
	LoadedVoiceStems.Reset();
	LoadedEcosystemTextures.Reset();
	LoadedFoleySounds.Reset();
	SensoryMPC = nullptr;
	EcoInstancedMaterial = nullptr;
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
		TEXT("PROP_Breakables_And_Traps_4x4"),
		TEXT("PROP_Doors_And_Level_Transitions_4x4"),
		TEXT("PROP_Resting_Ground_Sanctuary_4x4")
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

void UDungeoneerAssetBootstrapSubsystem::ComputeSpritePlaneOrientation() const noexcept
{
	if (bSpritePlaneOrientationComputed)
	{
		return;
	}

	// Default /Engine/BasicShapes/Plane.Plane basis fallback in case CPU vertex buffers are stripped
	FVector LocalRightU(0.0, 1.0, 0.0);
	FVector LocalUpNegV(1.0, 0.0, 0.0);
	FVector LocalNormal(0.0, 0.0, 1.0);

	if (UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
	{
		if (const FStaticMeshRenderData* RenderData = PlaneMesh->GetRenderData();
			RenderData && RenderData->LODResources.Num() > 0)
		{
			const FStaticMeshLODResources& LOD = RenderData->LODResources[0];
			const uint32 NumVerts = LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices();
			const uint32 NumIndices = static_cast<uint32>(LOD.IndexBuffer.GetNumIndices());
			if (NumVerts >= 3 && NumIndices >= 3)
			{
				const uint32 I0 = LOD.IndexBuffer.GetIndex(0);
				const uint32 I1 = LOD.IndexBuffer.GetIndex(1);
				const uint32 I2 = LOD.IndexBuffer.GetIndex(2);
				if (I0 < NumVerts && I1 < NumVerts && I2 < NumVerts)
				{
					const FVector P0 = FVector(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(I0));
					const FVector P1 = FVector(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(I1));
					const FVector P2 = FVector(LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(I2));
					const FVector2f UV0 = LOD.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(I0, 0);
					const FVector2f UV1 = LOD.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(I1, 0);
					const FVector2f UV2 = LOD.VertexBuffers.StaticMeshVertexBuffer.GetVertexUV(I2, 0);

					const FVector E1 = P1 - P0;
					const FVector E2 = P2 - P0;
					const double dU1 = static_cast<double>(UV1.X - UV0.X);
					const double dV1 = static_cast<double>(UV1.Y - UV0.Y);
					const double dU2 = static_cast<double>(UV2.X - UV0.X);
					const double dV2 = static_cast<double>(UV2.Y - UV0.Y);
					const double DetUV = dU1 * dV2 - dU2 * dV1;

					if (FMath::Abs(DetUV) > 1e-6)
					{
						const FVector TangentU = (E1 * dV2 - E2 * dV1) / DetUV;
						const FVector TangentV = (E2 * dU1 - E1 * dU2) / DetUV;
						LocalRightU = TangentU.GetSafeNormal(UE_SMALL_NUMBER, LocalRightU);
						// Image V=0 is top (head), V=1 is bottom (boots) -> upright sprite vector is -TangentV
						LocalUpNegV = (-TangentV).GetSafeNormal(UE_SMALL_NUMBER, LocalUpNegV);
						const FVector3f TangentZ = LOD.VertexBuffers.StaticMeshVertexBuffer.VertexTangentZ(I0);
						LocalNormal = FVector(TangentZ).GetSafeNormal(UE_SMALL_NUMBER, LocalNormal);
					}
				}
			}
		}
	}

	// Orthonormal right/left-handed local basis (e1 = LocalUpNegV, e2 = LocalNormal x e1, e3 = LocalNormal)
	const FVector E1 = LocalUpNegV.GetSafeNormal(UE_SMALL_NUMBER, FVector(1.0, 0.0, 0.0));
	const FVector Nrm = LocalNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector(0.0, 0.0, 1.0));
	const FVector E2 = FVector::CrossProduct(Nrm, E1).GetSafeNormal(UE_SMALL_NUMBER, FVector(0.0, 1.0, 0.0));

	// Desired world frame aligned with our isometric diorama camera (Pitch = -42 deg, Yaw = -45 deg):
	// Plane normal (-CamForward) points toward camera so 2.5D upright sprites suffer zero skew/foreshortening.
	const FRotationMatrix CamBasis(FRotator(-42.0f, -45.0f, 0.0f));
	const FVector WorldNormal = -CamBasis.GetScaledAxis(EAxis::X).GetSafeNormal();
	const FVector WorldUp = CamBasis.GetScaledAxis(EAxis::Z).GetSafeNormal();
	const FVector WorldRight = CamBasis.GetScaledAxis(EAxis::Y).GetSafeNormal();

	FMatrix SrcMat = FMatrix::Identity;
	SrcMat.SetAxes(&E1, &E2, &Nrm);
	FMatrix DstMat = FMatrix::Identity;
	DstMat.SetAxes(&WorldUp, &WorldRight, &WorldNormal);

	const FMatrix RotMat = SrcMat.GetTransposed() * DstMat;
	CachedSpritePlaneRotation = RotMat.Rotator();

	const FVector TransformedRightU = CachedSpritePlaneRotation.RotateVector(LocalRightU);
	bFlipHorizontalForSpritePlane = (FVector::DotProduct(TransformedRightU, WorldRight) < 0.0);
	bSpritePlaneOrientationComputed = true;
}

FRotator UDungeoneerAssetBootstrapSubsystem::GetCameraFacingSpritePlaneRotation() const noexcept
{
	ComputeSpritePlaneOrientation();
	return CachedSpritePlaneRotation;
}

bool UDungeoneerAssetBootstrapSubsystem::ShouldMirrorSpritePlaneU() const noexcept
{
	ComputeSpritePlaneOrientation();
	return bFlipHorizontalForSpritePlane;
}

void UDungeoneerAssetBootstrapSubsystem::ExtractAllGridCellsFromPng(
	const FString& PngFilePath,
	const FString& CacheKeyPrefix,
	const int32 GridCols,
	const int32 GridRows,
	const bool bIsNormalMap,
	const bool bSRGB,
	const bool bIsCharacterSprite,
	const TCHAR* ChannelSuffix)
{
	if (GridCols <= 0 || GridRows <= 0 || !FPaths::FileExists(PngFilePath))
	{
		return;
	}

	TArray<uint8> CompressedBytes;
	if (!FFileHelper::LoadFileToArray(CompressedBytes, *PngFilePath) || CompressedBytes.Num() == 0)
	{
		return;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(CompressedBytes.GetData(), CompressedBytes.Num()))
	{
		return;
	}

	const int32 FullWidth = ImageWrapper->GetWidth();
	const int32 FullHeight = ImageWrapper->GetHeight();
	if (FullWidth < GridCols || FullHeight < GridRows)
	{
		return;
	}

	TArray64<uint8> RawBGRA;
	if (!ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawBGRA))
	{
		return;
	}

	const int32 CellWidth = FullWidth / GridCols;
	const int32 CellHeight = FullHeight / GridRows;
	const int64 SrcStride = static_cast<int64>(FullWidth) * 4;
	const int64 DestStride = static_cast<int64>(CellWidth) * 4;
	const uint8* SrcBase = RawBGRA.GetData();
	const bool bMirrorHorizontal = bIsCharacterSprite && ShouldMirrorSpritePlaneU();

	for (int32 Row = 0; Row < GridRows; ++Row)
	{
		for (int32 Col = 0; Col < GridCols; ++Col)
		{
			const int32 CellIdx = Row * GridCols + Col;
			const FName CacheKey = bIsCharacterSprite
				? FName(*FString::Printf(TEXT("%s_R%d_C%d_%s"), *CacheKeyPrefix, Row, Col, ChannelSuffix))
				: FName(*FString::Printf(TEXT("%s_Cell%d_%s"), *CacheKeyPrefix, CellIdx, ChannelSuffix));

			if (ExtractedCellTextures.Contains(CacheKey))
			{
				continue;
			}

			UTexture2D* CellTex = UTexture2D::CreateTransient(CellWidth, CellHeight, PF_B8G8R8A8);
			if (!CellTex)
			{
				continue;
			}

			FTexturePlatformData* PlatformData = CellTex->GetPlatformData();
			if (!PlatformData || PlatformData->Mips.Num() == 0)
			{
				continue;
			}

			FTexture2DMipMap& Mip = PlatformData->Mips[0];
			uint8* DestBytes = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
			if (!DestBytes)
			{
				Mip.BulkData.Unlock();
				continue;
			}

			const int32 StartX = Col * CellWidth;
			const int32 StartY = Row * CellHeight;
			for (int32 RowY = 0; RowY < CellHeight; ++RowY)
			{
				const uint8* SrcRow = SrcBase + static_cast<int64>(StartY + RowY) * SrcStride + static_cast<int64>(StartX) * 4;
				uint8* DstRow = DestBytes + static_cast<int64>(RowY) * DestStride;
				if (!bMirrorHorizontal)
				{
					FMemory::Memcpy(DstRow, SrcRow, DestStride);
				}
				else
				{
					for (int32 ColX = 0; ColX < CellWidth; ++ColX)
					{
						const int32 SrcCol = CellWidth - 1 - ColX;
						DstRow[ColX * 4 + 0] = SrcRow[SrcCol * 4 + 0];
						DstRow[ColX * 4 + 1] = SrcRow[SrcCol * 4 + 1];
						DstRow[ColX * 4 + 2] = bIsNormalMap ? static_cast<uint8>(255 - SrcRow[SrcCol * 4 + 2]) : SrcRow[SrcCol * 4 + 2];
						DstRow[ColX * 4 + 3] = SrcRow[SrcCol * 4 + 3];
					}
				}
			}

			Mip.BulkData.Unlock();
			FDungeoneer25DMaterialBuilder::ConfigureTextureFor25D(CellTex, bIsNormalMap, bSRGB);
			ExtractedCellTextures.Add(CacheKey, CellTex);

			if (bIsCharacterSprite && !bIsNormalMap)
			{
				if (UTexture2D* MaskTex = UTexture2D::CreateTransient(CellWidth, CellHeight, PF_B8G8R8A8))
				{
					FTexturePlatformData* MaskPlatformData = MaskTex->GetPlatformData();
					if (MaskPlatformData && MaskPlatformData->Mips.Num() > 0)
					{
						FTexture2DMipMap& MaskMip = MaskPlatformData->Mips[0];
						if (uint8* MaskBytes = static_cast<uint8*>(MaskMip.BulkData.Lock(LOCK_READ_WRITE)))
						{
							for (int32 RowY = 0; RowY < CellHeight; ++RowY)
							{
								const uint8* SrcRow = SrcBase + static_cast<int64>(StartY + RowY) * SrcStride + static_cast<int64>(StartX) * 4;
								uint8* MaskRow = MaskBytes + static_cast<int64>(RowY) * DestStride;
								for (int32 ColX = 0; ColX < CellWidth; ++ColX)
								{
									const int32 SrcCol = bMirrorHorizontal ? (CellWidth - 1 - ColX) : ColX;
									const uint8 AlphaByte = SrcRow[SrcCol * 4 + 3];
									const uint8 MaskVal = (AlphaByte >= 64) ? 255 : 0;
									MaskRow[ColX * 4 + 0] = MaskVal;
									MaskRow[ColX * 4 + 1] = MaskVal;
									MaskRow[ColX * 4 + 2] = MaskVal;
									MaskRow[ColX * 4 + 3] = MaskVal;
								}
							}
							MaskMip.BulkData.Unlock();
							FDungeoneer25DMaterialBuilder::ConfigureTextureFor25D(MaskTex, false, false);
							SpriteMaskTextures.Add(CacheKey, MaskTex);
						}
						else
						{
							MaskMip.BulkData.Unlock();
						}
					}
				}
			}
		}
	}
}

UTexture2D* UDungeoneerAssetBootstrapSubsystem::RegisterProceduralTileFallback(
	const FName CacheKey,
	const FColor& PrimaryColor,
	const FColor& SecondaryColor,
	const bool bIsNormalMap,
	const bool bSRGB,
	const uint8 PatternKind)
{
	if (TObjectPtr<UTexture2D>* Existing = ExtractedCellTextures.Find(CacheKey))
	{
		return Existing->Get();
	}

	constexpr int32 Size = 64;
	UTexture2D* ProcTex = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
	if (!ProcTex)
	{
		return nullptr;
	}

	FTexturePlatformData* PlatformData = ProcTex->GetPlatformData();
	if (!PlatformData || PlatformData->Mips.Num() == 0)
	{
		return nullptr;
	}

	FTexture2DMipMap& Mip = PlatformData->Mips[0];
	uint8* Bytes = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
	if (!Bytes)
	{
		Mip.BulkData.Unlock();
		return nullptr;
	}

	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			bool bUseSecondary = false;
			if (PatternKind == 0) // Checker / masonry bevel
			{
				const bool bBorder = (X < 4 || X >= Size - 4 || Y < 4 || Y >= Size - 4);
				const bool bStep = ((Y / 8) % 2 == 0) && (X > 10 && X < Size - 10);
				bUseSecondary = bBorder || bStep;
			}
			else if (PatternKind == 1) // Brazier / sconce glow
			{
				const int32 DX = X - Size / 2;
				const int32 DY = Y - Size / 2;
				bUseSecondary = (DX * DX + DY * DY) < (18 * 18);
			}
			else // Runic sanctuary / hanging chain silhouette
			{
				const int32 DX = FMath::Abs(X - Size / 2);
				const int32 DY = FMath::Abs(Y - Size / 2);
				bUseSecondary = (DX + DY == 20) || (DX + DY == 19) || ((Y % 12 < 6) && DX < 6);
			}

			const FColor& C = bUseSecondary ? SecondaryColor : PrimaryColor;
			const int32 Idx = (Y * Size + X) * 4;
			Bytes[Idx + 0] = C.B;
			Bytes[Idx + 1] = C.G;
			Bytes[Idx + 2] = C.R;
			Bytes[Idx + 3] = 255;
		}
	}

	Mip.BulkData.Unlock();
	FDungeoneer25DMaterialBuilder::ConfigureTextureFor25D(ProcTex, bIsNormalMap, bSRGB);
	ExtractedCellTextures.Add(CacheKey, ProcTex);
	return ProcTex;
}

int32 UDungeoneerAssetBootstrapSubsystem::ExtractDefaultSingleCellTiles()
{
	ComputeSpritePlaneOrientation();

	const FString DungeonRoot = FPaths::ProjectContentDir() / TEXT("Art/Dungeon");
	const FString RigsRoot = FPaths::ProjectContentDir() / TEXT("Art/Rigs");

	auto AliasCellKey = [this](const TCHAR* AliasKey, const TCHAR* SourceKey)
	{
		if (TObjectPtr<UTexture2D>* Src = ExtractedCellTextures.Find(FName(SourceKey)))
		{
			ExtractedCellTextures.Add(FName(AliasKey), *Src);
		}
		if (TObjectPtr<UTexture2D>* SrcMask = SpriteMaskTextures.Find(FName(SourceKey)))
		{
			SpriteMaskTextures.Add(FName(AliasKey), *SrcMask);
		}
	};

	struct FDungeonExtractDef
	{
		const TCHAR* SubDir;
		const TCHAR* DiffuseFile;
		const TCHAR* NormalFile;
		const TCHAR* Prefix;
	};
	const FDungeonExtractDef DungeonDefs[] =
	{
		{ TEXT("ATLAS_Dungeon_Floors_4x4"),             TEXT("ATLAS_Dungeon_Floors_4x4_D.png"),             TEXT("ATLAS_Dungeon_Floors_4x4_N.png"),             TEXT("Floor") },
		{ TEXT("PROP_Dungeon_Walls_And_Cutaways_4x4"),  TEXT("PROP_Dungeon_Walls_And_Cutaways_4x4_D.png"),  TEXT("PROP_Dungeon_Walls_And_Cutaways_4x4_N.png"),  TEXT("Wall") },
		{ TEXT("PROP_Mirror_Palace_Maze_4x4"),          TEXT("PROP_Mirror_Palace_Maze_4x4_D.png"),          TEXT("PROP_Mirror_Palace_Maze_4x4_N.png"),          TEXT("Mirror") },
		{ TEXT("PROP_Breakables_And_Traps_4x4"),        TEXT("PROP_Breakables_And_Traps_4x4_D.png"),        TEXT("PROP_Breakables_And_Traps_4x4_N.png"),        TEXT("Breakable") },
		{ TEXT("PROP_Doors_And_Level_Transitions_4x4"), TEXT("PROP_Doors_And_Level_Transitions_4x4_D.png"), TEXT("PROP_Doors_And_Level_Transitions_4x4_N.png"), TEXT("Doors") },
		{ TEXT("PROP_Resting_Ground_Sanctuary_4x4"),    TEXT("PROP_Resting_Ground_Sanctuary_4x4_D.png"),    TEXT("PROP_Resting_Ground_Sanctuary_4x4_N.png"),    TEXT("Sanctuary") }
	};

	for (const FDungeonExtractDef& Def : DungeonDefs)
	{
		ExtractAllGridCellsFromPng(DungeonRoot / Def.SubDir / Def.DiffuseFile, Def.Prefix, 4, 4, false, true, false, TEXT("D"));
		ExtractAllGridCellsFromPng(DungeonRoot / Def.SubDir / Def.NormalFile,  Def.Prefix, 4, 4, true, false, false, TEXT("N"));
	}

	// Preserve & expand backward-compatible named aliases across all 16 unlocked cells
	AliasCellKey(TEXT("Floor_Tile0_D"),       TEXT("Floor_Cell0_D"));
	AliasCellKey(TEXT("Floor_Tile0_N"),       TEXT("Floor_Cell0_N"));
	AliasCellKey(TEXT("Floor_Wet0_D"),        TEXT("Floor_Cell4_D"));
	AliasCellKey(TEXT("Floor_Wet0_N"),        TEXT("Floor_Cell4_N"));
	AliasCellKey(TEXT("Floor_Fens0_D"),       TEXT("Floor_Cell8_D"));
	AliasCellKey(TEXT("Floor_Fens0_N"),       TEXT("Floor_Cell8_N"));
	AliasCellKey(TEXT("Floor_Ember0_D"),      TEXT("Floor_Cell12_D"));
	AliasCellKey(TEXT("Floor_Ember0_N"),      TEXT("Floor_Cell12_N"));

	AliasCellKey(TEXT("Wall_Full0_D"),        TEXT("Wall_Cell0_D"));
	AliasCellKey(TEXT("Wall_Full0_N"),        TEXT("Wall_Cell0_N"));
	AliasCellKey(TEXT("Wall_Cutaway_D"),      TEXT("Wall_Cell4_D"));
	AliasCellKey(TEXT("Wall_Diag0_D"),        TEXT("Wall_Cell8_D"));
	AliasCellKey(TEXT("Wall_Diag0_N"),        TEXT("Wall_Cell8_N"));
	AliasCellKey(TEXT("Wall_Chain_D"),        TEXT("Wall_Cell12_D"));
	AliasCellKey(TEXT("Wall_Chain_N"),        TEXT("Wall_Cell12_N"));

	AliasCellKey(TEXT("Mirror_Intact_D"),     TEXT("Mirror_Cell0_D"));
	AliasCellKey(TEXT("Mirror_Intact_N"),     TEXT("Mirror_Cell0_N"));
	AliasCellKey(TEXT("Breakable_Intact_D"),  TEXT("Breakable_Cell0_D"));
	AliasCellKey(TEXT("Breakable_Intact_N"),  TEXT("Breakable_Cell0_N"));
	AliasCellKey(TEXT("Trap_Vent0_D"),        TEXT("Breakable_Cell8_D"));
	AliasCellKey(TEXT("Trap_Vent0_N"),        TEXT("Breakable_Cell8_N"));

	AliasCellKey(TEXT("Doors_Arch_D"),        TEXT("Doors_Cell0_D"));
	AliasCellKey(TEXT("Doors_Arch_N"),        TEXT("Doors_Cell0_N"));
	AliasCellKey(TEXT("Doors_Stairs_D"),      TEXT("Doors_Cell8_D"));
	AliasCellKey(TEXT("Doors_Stairs_N"),      TEXT("Doors_Cell8_N"));
	AliasCellKey(TEXT("Doors_Sconce_D"),      TEXT("Doors_Cell12_D"));
	AliasCellKey(TEXT("Doors_Sconce_N"),      TEXT("Doors_Cell12_N"));
	AliasCellKey(TEXT("Sanctuary_Brazier_D"), TEXT("Sanctuary_Cell0_D"));
	AliasCellKey(TEXT("Sanctuary_Brazier_N"), TEXT("Sanctuary_Cell0_N"));
	AliasCellKey(TEXT("Sanctuary_Rune_D"),    TEXT("Sanctuary_Cell12_D"));
	AliasCellKey(TEXT("Sanctuary_Rune_N"),    TEXT("Sanctuary_Cell12_N"));

	// Safe procedural fallbacks when optional PNG sheets (Doors / Sanctuary) are not yet on disk
	const FColor FlatNorm(128, 128, 255, 255);
	RegisterProceduralTileFallback(FName(TEXT("Doors_Arch_D")),        FColor(46, 52, 68),  FColor(95, 86, 70),   false, true, 0);
	RegisterProceduralTileFallback(FName(TEXT("Doors_Arch_N")),        FlatNorm,           FColor(148, 148, 245), true, false, 0);
	RegisterProceduralTileFallback(FName(TEXT("Doors_Sconce_D")),      FColor(52, 42, 34),  FColor(255, 168, 52),  false, true, 1);
	RegisterProceduralTileFallback(FName(TEXT("Doors_Sconce_N")),      FlatNorm,           FColor(128, 168, 240), true, false, 1);
	RegisterProceduralTileFallback(FName(TEXT("Doors_Stairs_D")),      FColor(38, 42, 54),  FColor(140, 118, 62),  false, true, 0);
	RegisterProceduralTileFallback(FName(TEXT("Doors_Stairs_N")),      FlatNorm,           FColor(108, 148, 235), true, false, 0);
	RegisterProceduralTileFallback(FName(TEXT("Sanctuary_Brazier_D")), FColor(56, 46, 38),  FColor(255, 138, 38),  false, true, 1);
	RegisterProceduralTileFallback(FName(TEXT("Sanctuary_Brazier_N")), FlatNorm,           FColor(148, 168, 240), true, false, 1);
	RegisterProceduralTileFallback(FName(TEXT("Sanctuary_Rune_D")),    FColor(22, 38, 46),  FColor(64, 218, 238),  false, true, 2);
	RegisterProceduralTileFallback(FName(TEXT("Sanctuary_Rune_N")),    FlatNorm,           FColor(128, 168, 245), true, false, 2);
	RegisterProceduralTileFallback(FName(TEXT("Wall_Chain_D")),        FColor(24, 26, 34),  FColor(68, 72, 86),    false, true, 2);
	RegisterProceduralTileFallback(FName(TEXT("Wall_Chain_N")),        FlatNorm,           FColor(148, 148, 240), true, false, 2);

	// Extract all 16 directional walk-cycle cells (4 rows x 4 alternating stride frames) per character/zombie rig cleanly in 1 PNG decode per file
	struct FRigExtractDef
	{
		const TCHAR* SubDir;
		const TCHAR* DiffuseFile;
		const TCHAR* NormalFile;
		const TCHAR* KeyPrefix;
	};
	const FRigExtractDef RigDefs[3] =
	{
		{ TEXT("DungeoneerHero"),    TEXT("Rig_DungeoneerHero_Raw_D.png"),    TEXT("Rig_DungeoneerHero_Raw_N.png"),    TEXT("Hero") },
		{ TEXT("ZombieExploder"),    TEXT("Rig_ZombieExploder_Raw_D.png"),    TEXT("Rig_ZombieExploder_Raw_N.png"),    TEXT("Exploder") },
		{ TEXT("ZombieFastThrower"), TEXT("Rig_ZombieFastThrower_Raw_D.png"), TEXT("Rig_ZombieFastThrower_Raw_N.png"), TEXT("Thrower") }
	};

	for (const FRigExtractDef& Rig : RigDefs)
	{
		ExtractAllGridCellsFromPng(FString(RigsRoot) / Rig.SubDir / Rig.DiffuseFile, Rig.KeyPrefix, 4, 4, false, true, true, TEXT("D"));
		ExtractAllGridCellsFromPng(FString(RigsRoot) / Rig.SubDir / Rig.NormalFile,  Rig.KeyPrefix, 4, 4, true, false, true, TEXT("N"));

		for (int32 Row = 0; Row < 4; ++Row)
		{
			const FString RowSrcD = FString::Printf(TEXT("%s_R%d_C0_D"), Rig.KeyPrefix, Row);
			const FString RowSrcN = FString::Printf(TEXT("%s_R%d_C0_N"), Rig.KeyPrefix, Row);
			const FString RowAliasD = FString::Printf(TEXT("%s_Row%d_D"), Rig.KeyPrefix, Row);
			const FString RowAliasN = FString::Printf(TEXT("%s_Row%d_N"), Rig.KeyPrefix, Row);
			AliasCellKey(*RowAliasD, *RowSrcD);
			AliasCellKey(*RowAliasN, *RowSrcN);

			if (Row == 0)
			{
				const FString Cell0AliasD = FString::Printf(TEXT("%s_Cell0_D"), Rig.KeyPrefix);
				const FString Cell0AliasN = FString::Printf(TEXT("%s_Cell0_N"), Rig.KeyPrefix);
				AliasCellKey(*Cell0AliasD, *RowSrcD);
				AliasCellKey(*Cell0AliasN, *RowSrcN);
			}
		}
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
	const bool bSRGB,
	const bool bIsCharacterSprite)
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
	const bool bMirrorHorizontal = bIsCharacterSprite && ShouldMirrorSpritePlaneU();

	for (int32 RowY = 0; RowY < CellHeight; ++RowY)
	{
		const uint8* SrcRow = SrcBase + static_cast<int64>(StartY + RowY) * SrcStride + static_cast<int64>(StartX) * 4;
		uint8* DstRow = DestBytes + static_cast<int64>(RowY) * DestStride;
		if (!bMirrorHorizontal)
		{
			FMemory::Memcpy(DstRow, SrcRow, DestStride);
		}
		else
		{
			for (int32 ColX = 0; ColX < CellWidth; ++ColX)
			{
				const int32 SrcCol = CellWidth - 1 - ColX;
				DstRow[ColX * 4 + 0] = SrcRow[SrcCol * 4 + 0];
				DstRow[ColX * 4 + 1] = SrcRow[SrcCol * 4 + 1];
				DstRow[ColX * 4 + 2] = bIsNormalMap
					? static_cast<uint8>(255 - SrcRow[SrcCol * 4 + 2])
					: SrcRow[SrcCol * 4 + 2];
				DstRow[ColX * 4 + 3] = SrcRow[SrcCol * 4 + 3];
			}
		}
	}

	Mip.BulkData.Unlock();

	FDungeoneer25DMaterialBuilder::ConfigureTextureFor25D(CellTex, bIsNormalMap, bSRGB);
	ExtractedCellTextures.Add(CacheKey, CellTex);

	if (bIsCharacterSprite && !bIsNormalMap)
	{
		if (UTexture2D* MaskTex = UTexture2D::CreateTransient(CellWidth, CellHeight, PF_B8G8R8A8))
		{
			FTexturePlatformData* MaskPlatformData = MaskTex->GetPlatformData();
			if (MaskPlatformData && MaskPlatformData->Mips.Num() > 0)
			{
				FTexture2DMipMap& MaskMip = MaskPlatformData->Mips[0];
				if (uint8* MaskBytes = static_cast<uint8*>(MaskMip.BulkData.Lock(LOCK_READ_WRITE)))
				{
					for (int32 RowY = 0; RowY < CellHeight; ++RowY)
					{
						const uint8* SrcRow = SrcBase + static_cast<int64>(StartY + RowY) * SrcStride + static_cast<int64>(StartX) * 4;
						uint8* MaskRow = MaskBytes + static_cast<int64>(RowY) * DestStride;
						for (int32 ColX = 0; ColX < CellWidth; ++ColX)
						{
							const int32 SrcCol = bMirrorHorizontal ? (CellWidth - 1 - ColX) : ColX;
							const uint8 AlphaByte = SrcRow[SrcCol * 4 + 3];
							const uint8 MaskVal = (AlphaByte >= 64) ? 255 : 0;
							MaskRow[ColX * 4 + 0] = MaskVal;
							MaskRow[ColX * 4 + 1] = MaskVal;
							MaskRow[ColX * 4 + 2] = MaskVal;
							MaskRow[ColX * 4 + 3] = MaskVal;
						}
					}
					MaskMip.BulkData.Unlock();
					FDungeoneer25DMaterialBuilder::ConfigureTextureFor25D(MaskTex, false, false);
					SpriteMaskTextures.Add(CacheKey, MaskTex);
				}
				else
				{
					MaskMip.BulkData.Unlock();
				}
			}
		}
	}

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
	if (const TObjectPtr<UTexture2D>* Eco = LoadedEcosystemTextures.Find(Key))
	{
		return Eco->Get();
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
	const bool bForHISM,
	const bool bMaskedSprite)
{
	UObject* EffectiveOuter = Outer ? Outer : this;
	UTexture2D* DiffuseTex = ResolveAnyTextureByKey(DiffuseKey);
	UTexture2D* NormalTex = ResolveAnyTextureByKey(NormalKey);

	const FString KeyStr = DiffuseKey.ToString();
	const bool bUseMaskedCutout = bMaskedSprite
		|| KeyStr.StartsWith(TEXT("Hero_"))
		|| KeyStr.StartsWith(TEXT("Exploder_"))
		|| KeyStr.StartsWith(TEXT("Thrower_"));

	if (DiffuseTex)
	{
		if (bUseMaskedCutout && !bForHISM)
		{
			UTexture2D* MaskTex = GetSpriteMaskTexture(DiffuseKey);
			if (!MaskTex)
			{
				MaskTex = DiffuseTex;
			}

			if (UMaterialInterface* MaskedSimpleMat = LoadObject<UMaterialInterface>(
				nullptr, TEXT("/Engine/Engine_MI_Shaders/M_Shader_SimpleMaskedTwoSided.M_Shader_SimpleMaskedTwoSided")))
			{
				if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(MaskedSimpleMat, EffectiveOuter))
				{
					MID->SetTextureParameterValue(FName(TEXT("Diffuse")), DiffuseTex);
					MID->SetTextureParameterValue(FName(TEXT("OpacityMask")), MaskTex);
					if (NormalTex)
					{
						MID->SetTextureParameterValue(FName(TEXT("Normal")), NormalTex);
					}
					if (MID->K2_GetTextureParameterValue(FName(TEXT("Diffuse"))) == DiffuseTex)
					{
						const FLinearColor SubtleTint = FMath::Lerp(FLinearColor::White, FallbackTint, 0.08f);
						MID->SetVectorParameterValue(FName(TEXT("DiffuseColor")), SubtleTint);
						MID->SetScalarParameterValue(FName(TEXT("Roughness")), RoughnessVal);
						return MID;
					}
				}
			}

			if (UMaterialInterface* MaskedPhongMat = LoadObject<UMaterialInterface>(
				nullptr, TEXT("/Engine/Engine_MI_Shaders/M_ES_Phong_Masked_Master_2sided.M_ES_Phong_Masked_Master_2sided")))
			{
				if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(MaskedPhongMat, EffectiveOuter))
				{
					MID->SetTextureParameterValue(FName(TEXT("Diffuse_01")), DiffuseTex);
					MID->SetTextureParameterValue(FName(TEXT("OpacityMask")), MaskTex);
					if (NormalTex)
					{
						MID->SetTextureParameterValue(FName(TEXT("Normal_01")), NormalTex);
					}
					if (MID->K2_GetTextureParameterValue(FName(TEXT("Diffuse_01"))) == DiffuseTex)
					{
						const FLinearColor SubtleTint = FMath::Lerp(FLinearColor::White, FallbackTint, 0.08f);
						MID->SetVectorParameterValue(FName(TEXT("Diffuse_01_Color")), SubtleTint);
						return MID;
					}
				}
			}
		}

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
					const FLinearColor SubtleTint = FMath::Lerp(FLinearColor::White, FallbackTint, bUseMaskedCutout ? 0.08f : 0.25f);
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

		if (UDungeoneerPCMSoundWave* SoundWave = AssetBootstrapPrivate::CreateRuntimePCMSoundWaveFromWavFile(this, WavPath, false, RawWavBytes))
		{
			LoadedVoiceStems.Add(Key, SoundWave);
		}
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
		bSensoryMPCNeedsResave = true;
	}

	if (SensoryMPC && SensoryMPC->GetScalarParameterByName(FName(TEXT("EcoTime"))) == nullptr)
	{
		auto AddEcoScalar = [this](const FName& Name, float DefaultVal)
		{
			FCollectionScalarParameter Param;
			Param.ParameterName = Name;
			Param.DefaultValue = DefaultVal;
			SensoryMPC->ScalarParameters.Add(Param);
		};
		auto AddEcoVector = [this](const FName& Name, const FLinearColor& DefaultColor)
		{
			FCollectionVectorParameter Param;
			Param.ParameterName = Name;
			Param.DefaultValue = DefaultColor;
			SensoryMPC->VectorParameters.Add(Param);
		};

		AddEcoScalar(FName(TEXT("EcoTime")), 0.0f);
		AddEcoScalar(FName(TEXT("EcoGlowCount")), 0.0f);
		for (int32 i = 0; i < 32; ++i)
		{
			AddEcoVector(FName(*FString::Printf(TEXT("EcoGlow%02d"), i)), FLinearColor::Black);
		}
		bSensoryMPCNeedsResave = true;
	}

	if (SensoryMPC && bSensoryMPCNeedsResave)
	{
		if (AssetBootstrapPrivate::SaveAssetPackageToContent(SensoryMPC.Get(), TEXT("/Game/Materials/MPC_SensoryState"), true))
		{
			bSensoryMPCNeedsResave = false;
		}
	}

	return SensoryMPC.Get();
}

UMaterialInterface* UDungeoneerAssetBootstrapSubsystem::EnsureEcoInstancedMaterial()
{
	if (!EcoInstancedMaterial)
	{
		EcoInstancedMaterial = FDungeoneerEcoMaterialFactory::EnsureEcoInstancedMaterial(EnsureSensoryMPC());
	}
	return EcoInstancedMaterial.Get();
}

int32 UDungeoneerAssetBootstrapSubsystem::ImportAllEcosystemTexturesFromDisk()
{
	const FString EcoRoot = FPaths::ProjectContentDir() / TEXT("Art/Ecosystem");
	static constexpr const TCHAR* EcoSets[] =
	{
		TEXT("PROP_Magical_Flora_And_Crystals_4x4"),
		TEXT("PROP_Water_And_Fauna_4x4"),
		TEXT("PROP_Fauna_Critters_Strip_4x1"),
		TEXT("ATLAS_Water_Surfaces_4x4"),
		TEXT("PROP_Water_Cascades_And_Geysers_4x4"),
		TEXT("PROP_Water_FX_4x4"),
		TEXT("PROP_Wall_Bioluminescence_4x4")
	};
	struct FEcoChannel { const TCHAR* Suffix; bool bNormal; bool bSRGB; };
	static constexpr FEcoChannel Channels[] =
	{
		{ TEXT("D"), false, true },
		{ TEXT("N"), true,  false },
		{ TEXT("E"), false, true }
	};

	auto BuildProceduralWaterAtlas = [this](const FName& CacheKey, const bool bEmissiveChannel) -> UTexture2D*
	{
		constexpr int32 Size = 512;
		constexpr int32 CellPx = 128;
		UTexture2D* Tex = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
		if (!Tex || !Tex->GetPlatformData() || Tex->GetPlatformData()->Mips.Num() == 0)
		{
			return nullptr;
		}
		FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
		uint8* Bytes = static_cast<uint8*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
		if (!Bytes)
		{
			Mip.BulkData.Unlock();
			return nullptr;
		}
		for (int32 Y = 0; Y < Size; ++Y)
		{
			const int32 Row = Y / CellPx;
			const int32 LY = Y % CellPx;
			for (int32 X = 0; X < Size; ++X)
			{
				const int32 Col = X / CellPx;
				const int32 LX = X % CellPx;
				const int32 Idx = (Y * Size + X) * 4;
				uint8 R = 0, G = 0, B = 0, A = 0;
				if (Row == 0) // Deep dark indigo subterranean stream with smooth organic cyan current threads & mossy stone banks
				{
					const float FX = static_cast<float>(LX);
					const float FY = static_cast<float>(LY);
					const float Phase = static_cast<float>(Col) * 1.57f;
					const float RippleA = FMath::Sin(FX * 0.22f + FMath::Sin(FY * 0.14f + Phase) * 1.8f);
					const float RippleB = FMath::Sin(FY * 0.19f + FMath::Cos(FX * 0.11f - Phase) * 1.4f);
					const bool bHighlight = (RippleA > 0.90f && RippleB > 0.25f) || (RippleA > 0.96f);
					const bool bMid = (!bHighlight && RippleA > 0.65f);
					const bool bEdge = (LX < 7 || LX >= CellPx - 7);
					if (bEmissiveChannel)
					{
						R = bHighlight ? 4 : 0;
						G = bHighlight ? 28 : (bMid ? 10 : 4);
						B = bHighlight ? 44 : (bMid ? 18 : 9);
					}
					else if (bEdge)
					{
						R = 14;
						G = 28;
						B = 22;
					}
					else
					{
						R = bHighlight ? 20 : (bMid ? 10 : 6);
						G = bHighlight ? 78 : (bMid ? 34 : 16);
						B = bHighlight ? 112 : (bMid ? 52 : 28);
					}
					A = bEdge ? (((LX + LY) % 5 == 0) ? 120 : 165) : (bHighlight ? 230 : 210);
				}
				else if (Row == 1) // Crisp translucent concentric boot/drip ripple rings (no opaque disc fill)
				{
					const float DX = static_cast<float>(LX - CellPx / 2);
					const float DY = static_cast<float>(LY - CellPx / 2);
					const float Dist = FMath::Sqrt(DX * DX + DY * DY);
					const float OuterR = 22.0f + static_cast<float>(Col) * 9.0f;
					const float InnerR = FMath::Max(6.0f, OuterR - 14.0f);
					const bool bOuterRing = (FMath::Abs(Dist - OuterR) < 1.8f);
					const bool bInnerRing = (Col > 0) && (FMath::Abs(Dist - InnerR) < 1.4f);
					if (bOuterRing || bInnerRing)
					{
						if (bEmissiveChannel)
						{
							R = 24;
							G = 128;
							B = 180;
						}
						else
						{
							R = bOuterRing ? 140 : 95;
							G = bOuterRing ? 225 : 190;
							B = bOuterRing ? 255 : 235;
						}
						A = bOuterRing ? 215 : 155;
					}
				}
				else if (Row == 2) // Shallow dark reflective dungeon puddle
				{
					const float DX = static_cast<float>(LX - CellPx / 2);
					const float DY = static_cast<float>(LY - CellPx / 2);
					const float Dist = FMath::Sqrt(DX * DX + DY * DY);
					const bool bInside = Dist < 54.0f;
					const bool bRing = (FMath::Abs(Dist - (24.0f + Col * 7.0f)) < 2.0f);
					if (bInside)
					{
						if (bEmissiveChannel)
						{
							R = bRing ? 14 : 2;
							G = bRing ? 75 : 18;
							B = bRing ? 112 : 34;
						}
						else
						{
							R = bRing ? 48 : 10;
							G = bRing ? 120 : 26;
							B = bRing ? 165 : 46;
						}
						A = bRing ? 195 : 140;
					}
				}
				else // Row 3: Wet bootprints (cols 0..3 => N, NE, E, SE)
				{
					const float AngleRad = static_cast<float>(Col) * 0.785398f;
					const float C = FMath::Cos(AngleRad);
					const float S = FMath::Sin(AngleRad);
					const float CX = static_cast<float>(LX - CellPx / 2);
					const float CY = static_cast<float>(LY - CellPx / 2);
					const float RX = CX * C + CY * S;
					const float RY = -CX * S + CY * C;
					const bool bSole = (FMath::Abs(RX) < 16.0f && FMath::Abs(RY) < 34.0f && (FMath::Abs(RY) > 6.0f || FMath::Abs(RX) < 11.0f));
					if (bSole)
					{
						R = bEmissiveChannel ? 25 : 52;
						G = bEmissiveChannel ? 140 : 145;
						B = bEmissiveChannel ? 185 : 190;
						A = 230;
					}
				}
				Bytes[Idx + 0] = B;
				Bytes[Idx + 1] = G;
				Bytes[Idx + 2] = R;
				Bytes[Idx + 3] = A;
			}
		}
		Mip.BulkData.Unlock();
		FDungeoneer25DMaterialBuilder::ConfigureTextureFor25D(Tex, false, true);
		LoadedEcosystemTextures.Add(CacheKey, Tex);
		return Tex;
	};

	for (const TCHAR* SetName : EcoSets)
	{
		for (const FEcoChannel& Ch : Channels)
		{
			const FName Key(*FString::Printf(TEXT("%s_%s"), SetName, Ch.Suffix));
			if (LoadedEcosystemTextures.Contains(Key))
			{
				continue;
			}
			const FString FilePath = EcoRoot / SetName / FString::Printf(TEXT("%s_%s.png"), SetName, Ch.Suffix);
			if (FPaths::FileExists(FilePath))
			{
				if (UTexture2D* Imported = FImageUtils::ImportFileAsTexture2D(FilePath))
				{
					FDungeoneer25DMaterialBuilder::ConfigureTextureFor25D(Imported, Ch.bNormal, Ch.bSRGB);
					LoadedEcosystemTextures.Add(Key, Imported);
				}
			}
			else if (FCString::Strcmp(SetName, TEXT("ATLAS_Water_Surfaces_4x4")) == 0 &&
				(FCString::Strcmp(Ch.Suffix, TEXT("D")) == 0 || FCString::Strcmp(Ch.Suffix, TEXT("E")) == 0))
			{
				BuildProceduralWaterAtlas(Key, FCString::Strcmp(Ch.Suffix, TEXT("E")) == 0);
			}
			else if (FString(Ch.Suffix) == TEXT("D"))
			{
				// Safe procedural 64x64 fallback for optional future ecosystem sheets
				const FColor Primary = FString(SetName).Contains(TEXT("Water"))
					? FColor(28, 128, 175, 255)
					: FColor(24, 155, 110, 255);
				const FColor Secondary = FColor(16, 48, 64, 255);
				if (UTexture2D* Fallback = RegisterProceduralTileFallback(Key, Primary, Secondary, false, true, 1))
				{
					LoadedEcosystemTextures.Add(Key, Fallback);
				}
			}
		}
	}

	return LoadedEcosystemTextures.Num();
}

int32 UDungeoneerAssetBootstrapSubsystem::ImportAllFoleyFromDisk()
{
	const FString FoleyRoot = FPaths::ProjectContentDir() / TEXT("Audio/Foley");
	if (!FPaths::DirectoryExists(FoleyRoot))
	{
		return LoadedFoleySounds.Num();
	}

	TArray<FString> WavFiles;
	IFileManager::Get().FindFilesRecursive(WavFiles, *FoleyRoot, TEXT("*.wav"), true, false);
	WavFiles.Sort();

	TArray<uint8> RawWavBytes;
	for (const FString& WavPath : WavFiles)
	{
		const FString FileStem = FPaths::GetBaseFilename(WavPath);
		const FName Key(*FileStem);
		if (LoadedFoleySounds.Contains(Key))
		{
			continue;
		}

		const bool bIsLoop = FileStem.Contains(TEXT("Loop_"));
		if (UDungeoneerPCMSoundWave* SoundWave = AssetBootstrapPrivate::CreateRuntimePCMSoundWaveFromWavFile(this, WavPath, bIsLoop, RawWavBytes))
		{
			LoadedFoleySounds.Add(Key, SoundWave);
		}
	}

	return LoadedFoleySounds.Num();
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

	const bool bSavedMPC = AssetBootstrapPrivate::SaveAssetPackageToContent(MPC, TEXT("/Game/Materials/MPC_SensoryState"), bSensoryMPCNeedsResave);
	if (bSavedMPC)
	{
		bSensoryMPCNeedsResave = false;
	}
	EnsureEcoInstancedMaterial();
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

UTexture2D* UDungeoneerAssetBootstrapSubsystem::GetSpriteMaskTexture(FName DiffuseKey) const noexcept
{
	const TObjectPtr<UTexture2D>* Found = SpriteMaskTextures.Find(DiffuseKey);
	return Found ? Found->Get() : nullptr;
}

USoundWave* UDungeoneerAssetBootstrapSubsystem::GetLoadedVoiceStem(FName Key) const noexcept
{
	const TObjectPtr<USoundWave>* Found = LoadedVoiceStems.Find(Key);
	return Found ? Found->Get() : nullptr;
}

UTexture2D* UDungeoneerAssetBootstrapSubsystem::GetLoadedEcosystemTexture(FName Key) const noexcept
{
	const TObjectPtr<UTexture2D>* Found = LoadedEcosystemTextures.Find(Key);
	return Found ? Found->Get() : nullptr;
}

USoundWave* UDungeoneerAssetBootstrapSubsystem::GetLoadedFoley(FName Key) const noexcept
{
	const TObjectPtr<USoundWave>* Found = LoadedFoleySounds.Find(Key);
	return Found ? Found->Get() : nullptr;
}
