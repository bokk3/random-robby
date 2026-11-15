#pragma once

/**
 * DungeonAtmosphereActor.h
 * 
 * Manages volumetric distance fog and GPU-sprite ambient weather presets
 * (ColdDrafts, DrippingWater, ToxicSpores, EmberAsh) with depth-buffer collisions.
 * Exposes dynamic preset swapping and sensory fog thinning without runtime actor spawning.
 */

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "NiagaraComponent.h"
#include "DungeonAtmosphereActor.generated.h"

class UNiagaraSystem;

UENUM(BlueprintType)
enum class EDungeonWeatherPreset : uint8
{
	None          = 0 UMETA(DisplayName = "None"),
	ColdDrafts    = 1 UMETA(DisplayName = "Cold Drafts (Dust)"),
	DrippingWater = 2 UMETA(DisplayName = "Dripping Water"),
	ToxicSpores   = 3 UMETA(DisplayName = "Toxic Spores"),
	EmberAsh      = 4 UMETA(DisplayName = "Ember Ash")
};

USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API FDungeonWeatherConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weather")
	TObjectPtr<UNiagaraSystem> ParticleSystem = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weather", meta = (ClampMin = "0.001", ClampMax = "1.0"))
	float FogDensity = 0.05f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weather")
	FLinearColor FogAlbedo = FLinearColor(0.18f, 0.22f, 0.30f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weather", meta = (ClampMin = "-0.9", ClampMax = "0.9"))
	float VolumetricScatteringDistribution = 0.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weather", meta = (ClampMin = "0.01", ClampMax = "10.0"))
	float VolumetricExtinctionScale = 1.0f;
};

UCLASS(Blueprintable)
class DARKDUNGEONEER_API ADungeonAtmosphereActor : public AActor
{
	GENERATED_BODY()

public:
	ADungeonAtmosphereActor();

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;

	/** Switches weather and volumetric fog profile dynamically. */
	UFUNCTION(BlueprintCallable, Category = "Atmosphere")
	void SetWeatherPreset(EDungeonWeatherPreset NewPreset);

	/** Manually tunes volumetric fog parameters (used during sensory reveals or room transitions). */
	UFUNCTION(BlueprintCallable, Category = "Atmosphere")
	void ConfigureVolumetricFog(
		float InFogDensity,
		FLinearColor InFogAlbedo,
		float InScatteringDistribution,
		float InExtinctionScale);

	UFUNCTION(BlueprintPure, Category = "Atmosphere")
	UExponentialHeightFogComponent* GetFogComponent() const noexcept { return FogComponent.Get(); }

	UFUNCTION(BlueprintPure, Category = "Atmosphere")
	UNiagaraComponent* GetWeatherNiagaraComponent() const noexcept { return WeatherNiagaraComponent.Get(); }

	UFUNCTION(BlueprintPure, Category = "Atmosphere")
	EDungeonWeatherPreset GetActivePreset() const noexcept { return ActivePreset; }

protected:
	void ApplyWeatherConfig(const FDungeonWeatherConfig& Config);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UExponentialHeightFogComponent> FogComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNiagaraComponent> WeatherNiagaraComponent;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	EDungeonWeatherPreset ActivePreset = EDungeonWeatherPreset::ColdDrafts;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presets")
	FDungeonWeatherConfig ColdDraftsConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presets")
	FDungeonWeatherConfig DrippingWaterConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presets")
	FDungeonWeatherConfig ToxicSporesConfig;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Presets")
	FDungeonWeatherConfig EmberAshConfig;
};
