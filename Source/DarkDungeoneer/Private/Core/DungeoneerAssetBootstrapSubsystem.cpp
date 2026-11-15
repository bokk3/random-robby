#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "Core/DungeoneerMovementConfig.h"
#include "Rendering/Dungeoneer25DMaterialBuilder.h"
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
	ComputeSpritePlaneOrientation();
	ImportAllRigTexturesFromDisk();
	ImportAllDungeonTexturesFromDisk();
	ExtractDefaultSingleCellTiles();
}

void UDungeoneerAssetBootstrapSubsystem::Deinitialize()
{
	LoadedRigTextures.Reset();
	LoadedDungeonTextures.Reset();
	ExtractedCellTextures.Reset();
	SpriteMaskTextures.Reset();
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

	// Desired world frame for 45 deg 2.5D billboard tilt toward the -60 deg camera looking along +X:
	// - Sprite head (V=0) above boots (V=1): WorldUp = (0.7071068, 0.0, 0.7071068)
	// - Plane front (+Z) tilted toward camera:   WorldNormal = (-0.7071068, 0.0, 0.7071068)
	// - Cross(WorldNormal, WorldUp) = (0, +1, 0) = Screen Right (+Y)
	const FVector WorldUp = FVector(0.7071067811865475, 0.0, 0.7071067811865475).GetSafeNormal();
	const FVector WorldNormal = FVector(-0.7071067811865475, 0.0, 0.7071067811865475).GetSafeNormal();
	const FVector WorldRight = FVector::CrossProduct(WorldNormal, WorldUp).GetSafeNormal();

	FMatrix SrcMat = FMatrix::Identity;
	SrcMat.SetAxes(&E1, &E2, &Nrm);
	FMatrix DstMat = FMatrix::Identity;
	DstMat.SetAxes(&WorldUp, &WorldRight, &WorldNormal);

	const FMatrix RotMat = SrcMat.GetTransposed() * DstMat;
	CachedSpritePlaneRotation = RotMat.Rotator();

	const FVector TransformedRightU = CachedSpritePlaneRotation.RotateVector(LocalRightU);
	bFlipHorizontalForSpritePlane = (FVector::DotProduct(TransformedRightU, FVector(0.0, 1.0, 0.0)) < 0.0);
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

int32 UDungeoneerAssetBootstrapSubsystem::ExtractDefaultSingleCellTiles()
{
	ComputeSpritePlaneOrientation();

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
		bool bIsCharacterSprite;
	};

	const FCellExtractSpec Specs[] =
	{
		// Dungeon Environment Cells (4x4 atlases)
		{ *DungeonRoot, TEXT("ATLAS_Dungeon_Floors_4x4"),            TEXT("ATLAS_Dungeon_Floors_4x4_D.png"),            TEXT("Floor_Tile0_D"),       0, false, true,  false },
		{ *DungeonRoot, TEXT("ATLAS_Dungeon_Floors_4x4"),            TEXT("ATLAS_Dungeon_Floors_4x4_N.png"),            TEXT("Floor_Tile0_N"),       0, true,  false, false },
		{ *DungeonRoot, TEXT("ATLAS_Dungeon_Floors_4x4"),            TEXT("ATLAS_Dungeon_Floors_4x4_D.png"),            TEXT("Floor_Wet0_D"),        4, false, true,  false },
		{ *DungeonRoot, TEXT("PROP_Dungeon_Walls_And_Cutaways_4x4"), TEXT("PROP_Dungeon_Walls_And_Cutaways_4x4_D.png"), TEXT("Wall_Full0_D"),        0, false, true,  false },
		{ *DungeonRoot, TEXT("PROP_Dungeon_Walls_And_Cutaways_4x4"), TEXT("PROP_Dungeon_Walls_And_Cutaways_4x4_N.png"), TEXT("Wall_Full0_N"),        0, true,  false, false },
		{ *DungeonRoot, TEXT("PROP_Dungeon_Walls_And_Cutaways_4x4"), TEXT("PROP_Dungeon_Walls_And_Cutaways_4x4_D.png"), TEXT("Wall_Cutaway_D"),      4, false, true,  false },
		{ *DungeonRoot, TEXT("PROP_Mirror_Palace_Maze_4x4"),         TEXT("PROP_Mirror_Palace_Maze_4x4_D.png"),         TEXT("Mirror_Intact_D"),     0, false, true,  false },
		{ *DungeonRoot, TEXT("PROP_Mirror_Palace_Maze_4x4"),         TEXT("PROP_Mirror_Palace_Maze_4x4_N.png"),         TEXT("Mirror_Intact_N"),     0, true,  false, false },
		{ *DungeonRoot, TEXT("PROP_Breakables_And_Traps_4x4"),       TEXT("PROP_Breakables_And_Traps_4x4_D.png"),       TEXT("Breakable_Intact_D"),  0, false, true,  false },
		{ *DungeonRoot, TEXT("PROP_Breakables_And_Traps_4x4"),       TEXT("PROP_Breakables_And_Traps_4x4_N.png"),       TEXT("Breakable_Intact_N"),  0, true,  false, false },
		// Hero 4-Directional Sprite Cells (Row0=Front/Cell0, Row1=Back/Cell4, Row2=Right/Cell8, Row3=Left/Cell12)
		{ *RigsRoot,    TEXT("DungeoneerHero"),                      TEXT("Rig_DungeoneerHero_Raw_D.png"),              TEXT("Hero_Row0_D"),         0,  false, true,  true },
		{ *RigsRoot,    TEXT("DungeoneerHero"),                      TEXT("Rig_DungeoneerHero_Raw_N.png"),              TEXT("Hero_Row0_N"),         0,  true,  false, true },
		{ *RigsRoot,    TEXT("DungeoneerHero"),                      TEXT("Rig_DungeoneerHero_Raw_D.png"),              TEXT("Hero_Row1_D"),         4,  false, true,  true },
		{ *RigsRoot,    TEXT("DungeoneerHero"),                      TEXT("Rig_DungeoneerHero_Raw_N.png"),              TEXT("Hero_Row1_N"),         4,  true,  false, true },
		{ *RigsRoot,    TEXT("DungeoneerHero"),                      TEXT("Rig_DungeoneerHero_Raw_D.png"),              TEXT("Hero_Row2_D"),         8,  false, true,  true },
		{ *RigsRoot,    TEXT("DungeoneerHero"),                      TEXT("Rig_DungeoneerHero_Raw_N.png"),              TEXT("Hero_Row2_N"),         8,  true,  false, true },
		{ *RigsRoot,    TEXT("DungeoneerHero"),                      TEXT("Rig_DungeoneerHero_Raw_D.png"),              TEXT("Hero_Row3_D"),         12, false, true,  true },
		{ *RigsRoot,    TEXT("DungeoneerHero"),                      TEXT("Rig_DungeoneerHero_Raw_N.png"),              TEXT("Hero_Row3_N"),         12, true,  false, true },
		// Zombie Exploder 4-Directional Sprite Cells
		{ *RigsRoot,    TEXT("ZombieExploder"),                      TEXT("Rig_ZombieExploder_Raw_D.png"),              TEXT("Exploder_Row0_D"),     0,  false, true,  true },
		{ *RigsRoot,    TEXT("ZombieExploder"),                      TEXT("Rig_ZombieExploder_Raw_N.png"),              TEXT("Exploder_Row0_N"),     0,  true,  false, true },
		{ *RigsRoot,    TEXT("ZombieExploder"),                      TEXT("Rig_ZombieExploder_Raw_D.png"),              TEXT("Exploder_Row1_D"),     4,  false, true,  true },
		{ *RigsRoot,    TEXT("ZombieExploder"),                      TEXT("Rig_ZombieExploder_Raw_N.png"),              TEXT("Exploder_Row1_N"),     4,  true,  false, true },
		{ *RigsRoot,    TEXT("ZombieExploder"),                      TEXT("Rig_ZombieExploder_Raw_D.png"),              TEXT("Exploder_Row2_D"),     8,  false, true,  true },
		{ *RigsRoot,    TEXT("ZombieExploder"),                      TEXT("Rig_ZombieExploder_Raw_N.png"),              TEXT("Exploder_Row2_N"),     8,  true,  false, true },
		{ *RigsRoot,    TEXT("ZombieExploder"),                      TEXT("Rig_ZombieExploder_Raw_D.png"),              TEXT("Exploder_Row3_D"),     12, false, true,  true },
		{ *RigsRoot,    TEXT("ZombieExploder"),                      TEXT("Rig_ZombieExploder_Raw_N.png"),              TEXT("Exploder_Row3_N"),     12, true,  false, true },
		// Zombie Fast Thrower 4-Directional Sprite Cells
		{ *RigsRoot,    TEXT("ZombieFastThrower"),                   TEXT("Rig_ZombieFastThrower_Raw_D.png"),           TEXT("Thrower_Row0_D"),      0,  false, true,  true },
		{ *RigsRoot,    TEXT("ZombieFastThrower"),                   TEXT("Rig_ZombieFastThrower_Raw_N.png"),           TEXT("Thrower_Row0_N"),      0,  true,  false, true },
		{ *RigsRoot,    TEXT("ZombieFastThrower"),                   TEXT("Rig_ZombieFastThrower_Raw_D.png"),           TEXT("Thrower_Row1_D"),      4,  false, true,  true },
		{ *RigsRoot,    TEXT("ZombieFastThrower"),                   TEXT("Rig_ZombieFastThrower_Raw_N.png"),           TEXT("Thrower_Row1_N"),      4,  true,  false, true },
		{ *RigsRoot,    TEXT("ZombieFastThrower"),                   TEXT("Rig_ZombieFastThrower_Raw_D.png"),           TEXT("Thrower_Row2_D"),      8,  false, true,  true },
		{ *RigsRoot,    TEXT("ZombieFastThrower"),                   TEXT("Rig_ZombieFastThrower_Raw_N.png"),           TEXT("Thrower_Row2_N"),      8,  true,  false, true },
		{ *RigsRoot,    TEXT("ZombieFastThrower"),                   TEXT("Rig_ZombieFastThrower_Raw_D.png"),           TEXT("Thrower_Row3_D"),      12, false, true,  true },
		{ *RigsRoot,    TEXT("ZombieFastThrower"),                   TEXT("Rig_ZombieFastThrower_Raw_N.png"),           TEXT("Thrower_Row3_N"),      12, true,  false, true }
	};

	for (const FCellExtractSpec& Spec : Specs)
	{
		const FString FullPath = FString(Spec.BaseDir) / Spec.SubDir / Spec.FileName;
		ExtractAtlasCellTexture(
			FullPath,
			FName(Spec.CacheKey),
			Spec.CellIndex,
			4,
			4,
			Spec.bIsNormalMap,
			Spec.bSRGB,
			Spec.bIsCharacterSprite);
	}

	// Backward-compatible Cell0 aliases pointing to Row0 (zero duplicate allocation)
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
	AliasCellKey(TEXT("Hero_Cell0_D"),     TEXT("Hero_Row0_D"));
	AliasCellKey(TEXT("Hero_Cell0_N"),     TEXT("Hero_Row0_N"));
	AliasCellKey(TEXT("Exploder_Cell0_D"), TEXT("Exploder_Row0_D"));
	AliasCellKey(TEXT("Exploder_Cell0_N"), TEXT("Exploder_Row0_N"));
	AliasCellKey(TEXT("Thrower_Cell0_D"),  TEXT("Thrower_Row0_D"));
	AliasCellKey(TEXT("Thrower_Cell0_N"),  TEXT("Thrower_Row0_N"));

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
