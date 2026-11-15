#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DungeoneerAssetBootstrapSubsystem.generated.h"

class UTexture2D;
class USoundWave;
class UMaterialParameterCollection;
class UDungeoneer25DMaterialConfig;
class UDungeoneerMovementConfig;

/**
 * TASK-903: Runtime & Editor Content Bootstrap Subsystem.
 * Imports generated 2.5D rig PNGs (Art/Rigs) with TEXTUREGROUP_Pixels2D + TF_Nearest,
 * loads phoneme/hero WAV banks (Audio/Voices), synthesizes MPC_SensoryState parameters,
 * and serializes core DataAsset/.uasset packages cleanly to Content/.
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
	int32 GetLoadedVoiceStemCount() const noexcept { return LoadedVoiceStems.Num(); }

private:
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTexture2D>> LoadedRigTextures;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<USoundWave>> LoadedVoiceStems;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> SensoryMPC;

	UPROPERTY(Transient)
	TObjectPtr<UDungeoneer25DMaterialConfig> MaterialConfigAsset;

	UPROPERTY(Transient)
	TObjectPtr<UDungeoneerMovementConfig> MovementConfigAsset;
};
