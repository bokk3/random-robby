#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DungeoneerAssetBootstrapSubsystem.generated.h"

class UTexture2D;
class USoundWave;
class UMaterialInstanceDynamic;
class UMaterialParameterCollection;
class UDungeoneer25DMaterialConfig;
class UDungeoneerMovementConfig;

/**
 * TASK-903 & TASK-907: Runtime & Editor Content Bootstrap Subsystem.
 * Imports generated 2.5D rig PNGs (Art/Rigs) and dungeon atlas PNGs (Art/Dungeon) with
 * TEXTUREGROUP_Pixels2D + TF_Nearest, extracts single-cell 2.5D tiles for pre-compiled engine
 * materials, loads phoneme/hero WAV banks (Audio/Voices), synthesizes MPC_SensoryState
 * parameters, and serializes core DataAsset/.uasset packages cleanly to Content/.
 */
UCLASS()
class DARKDUNGEONEER_API UDungeoneerAssetBootstrapSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Imports all 18 generated rig PNG sheets (_D, _N, _ORM across 6 rig directories) from Content/Art/Rigs. */
	UFUNCTION(BlueprintCallable, Category = "Assets|Bootstrap")
	int32 ImportAllRigTexturesFromDisk();

	/** Imports all generated dungeon atlas/prop PNG sheets from Content/Art/Dungeon. */
	UFUNCTION(BlueprintCallable, Category = "Assets|Bootstrap")
	int32 ImportAllDungeonTexturesFromDisk();

	/** Crops single-cell textures from 4x4 dungeon/rig sheets so standard Basic/Engine materials tile cleanly. */
	UFUNCTION(BlueprintCallable, Category = "Assets|Bootstrap")
	int32 ExtractDefaultSingleCellTiles();

	/** Decompresses a PNG sheet and extracts a single (Col, Row) cell into a TF_Nearest transient UTexture2D. */
	UTexture2D* ExtractAtlasCellTexture(
		const FString& PngFilePath,
		FName CacheKey,
		int32 CellIndex,
		int32 GridCols = 4,
		int32 GridRows = 4,
		bool bIsNormalMap = false,
		bool bSRGB = true,
		bool bIsCharacterSprite = false);

	/**
	 * Builds a textured UMaterialInstanceDynamic using pre-compiled Engine materials (zero shader-compile hitch),
	 * automatically falling back to tinted BasicShapeMaterial if a texture or engine shader is unavailable.
	 */
	UMaterialInstanceDynamic* CreateDungeonOrRigMaterial(
		UObject* Outer,
		FName DiffuseKey,
		FName NormalKey,
		const FLinearColor& FallbackTint,
		float RoughnessVal = 0.75f,
		bool bForHISM = false,
		bool bMaskedSprite = false);

	/** Exact 3D rotation for /Engine/BasicShapes/Plane.Plane so sprite V:1->0 points up-screen and front faces the -60 deg camera. */
	UFUNCTION(BlueprintPure, Category = "Assets|Bootstrap")
	FRotator GetCameraFacingSpritePlaneRotation() const noexcept;

	/** True when /Engine/BasicShapes/Plane.Plane's U axis maps opposite to Screen Right (+Y), requiring horizontal mirror on character cell extraction. */
	UFUNCTION(BlueprintPure, Category = "Assets|Bootstrap")
	bool ShouldMirrorSpritePlaneU() const noexcept;

	/** Imports all WAV voice stems & 24-syllable gibberish banks from Content/Audio/Voices. */
	UFUNCTION(BlueprintCallable, Category = "Assets|Bootstrap")
	int32 ImportAllVoiceStemsFromDisk();

	/** Ensures MPC_SensoryState exists and registers all sensory scalar/vector parameters. */
	UFUNCTION(BlueprintCallable, Category = "Assets|Bootstrap")
	UMaterialParameterCollection* EnsureSensoryMPC();

	/** Creates and saves MPC_SensoryState.uasset, DA_25DMaterialConfig.uasset, and DA_MovementConfig.uasset to Content/. */
	UFUNCTION(BlueprintCallable, Category = "Assets|Bootstrap")
	bool SaveCoreDataAssetsToDisk();

	UFUNCTION(BlueprintPure, Category = "Assets|Bootstrap")
	UTexture2D* GetLoadedRigTexture(FName Key) const noexcept;

	UFUNCTION(BlueprintPure, Category = "Assets|Bootstrap")
	UTexture2D* GetLoadedDungeonTexture(FName Key) const noexcept;

	UFUNCTION(BlueprintPure, Category = "Assets|Bootstrap")
	UTexture2D* GetExtractedCellTexture(FName Key) const noexcept;

	UFUNCTION(BlueprintPure, Category = "Assets|Bootstrap")
	USoundWave* GetLoadedVoiceStem(FName Key) const noexcept;

	UFUNCTION(BlueprintPure, Category = "Assets|Bootstrap")
	UMaterialParameterCollection* GetSensoryMPC() const noexcept { return SensoryMPC.Get(); }

	UFUNCTION(BlueprintPure, Category = "Assets|Bootstrap")
	UDungeoneer25DMaterialConfig* GetMaterialConfigAsset() const noexcept { return MaterialConfigAsset.Get(); }

	UFUNCTION(BlueprintPure, Category = "Assets|Bootstrap")
	UDungeoneerMovementConfig* GetMovementConfigAsset() const noexcept { return MovementConfigAsset.Get(); }

	UFUNCTION(BlueprintPure, Category = "Assets|Bootstrap")
	int32 GetLoadedRigTextureCount() const noexcept { return LoadedRigTextures.Num(); }

	UFUNCTION(BlueprintPure, Category = "Assets|Bootstrap")
	int32 GetLoadedDungeonTextureCount() const noexcept { return LoadedDungeonTextures.Num(); }

	UFUNCTION(BlueprintPure, Category = "Assets|Bootstrap")
	int32 GetExtractedCellTextureCount() const noexcept { return ExtractedCellTextures.Num(); }

	UFUNCTION(BlueprintPure, Category = "Assets|Bootstrap")
	int32 GetLoadedVoiceStemCount() const noexcept { return LoadedVoiceStems.Num(); }

private:
	UTexture2D* ResolveAnyTextureByKey(FName Key) const noexcept;
	void ComputeSpritePlaneOrientation() const noexcept;

	mutable FRotator CachedSpritePlaneRotation = FRotator(60.0f, 180.0f, 0.0f);
	mutable bool bFlipHorizontalForSpritePlane = false;
	mutable bool bSpritePlaneOrientationComputed = false;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> LoadedRigTextures;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> LoadedDungeonTextures;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> ExtractedCellTextures;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USoundWave>> LoadedVoiceStems;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> SensoryMPC;

	UPROPERTY(Transient)
	TObjectPtr<UDungeoneer25DMaterialConfig> MaterialConfigAsset;

	UPROPERTY(Transient)
	TObjectPtr<UDungeoneerMovementConfig> MovementConfigAsset;
};
