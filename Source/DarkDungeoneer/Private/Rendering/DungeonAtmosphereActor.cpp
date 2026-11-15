/**
 * DungeonAtmosphereActor.cpp
 * 
 * Implementation of volumetric distance fog and GPU-sprite ambient weather presets.
 */

#include "Rendering/DungeonAtmosphereActor.h"
#include "NiagaraSystem.h"

ADungeonAtmosphereActor::ADungeonAtmosphereActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;

	FogComponent = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("FogComponent"));
	FogComponent->SetupAttachment(RootComponent);
	FogComponent->bEnableVolumetricFog = true;
	FogComponent->VolumetricFogScatteringDistribution = 0.45f;
	FogComponent->VolumetricFogExtinctionScale = 1.0f;
	FogComponent->SetFogDensity(0.05f);
	FogComponent->SetVolumetricFogAlbedo(FColor(45, 55, 75));

	WeatherNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("WeatherNiagaraComponent"));
	WeatherNiagaraComponent->SetupAttachment(RootComponent);
	WeatherNiagaraComponent->SetAutoActivate(true);

	// Setup atmospheric defaults for each weather preset
	ColdDraftsConfig.FogDensity = 0.04f;
	ColdDraftsConfig.FogAlbedo = FLinearColor(0.15f, 0.20f, 0.28f, 1.0f);
	ColdDraftsConfig.VolumetricScatteringDistribution = 0.40f;
	ColdDraftsConfig.VolumetricExtinctionScale = 0.9f;

	DrippingWaterConfig.FogDensity = 0.06f;
	DrippingWaterConfig.FogAlbedo = FLinearColor(0.12f, 0.18f, 0.25f, 1.0f);
	DrippingWaterConfig.VolumetricScatteringDistribution = 0.50f;
	DrippingWaterConfig.VolumetricExtinctionScale = 1.1f;

	ToxicSporesConfig.FogDensity = 0.08f;
	ToxicSporesConfig.FogAlbedo = FLinearColor(0.14f, 0.28f, 0.12f, 1.0f);
	ToxicSporesConfig.VolumetricScatteringDistribution = 0.60f;
	ToxicSporesConfig.VolumetricExtinctionScale = 1.4f;

	EmberAshConfig.FogDensity = 0.05f;
	EmberAshConfig.FogAlbedo = FLinearColor(0.28f, 0.15f, 0.10f, 1.0f);
	EmberAshConfig.VolumetricScatteringDistribution = 0.35f;
	EmberAshConfig.VolumetricExtinctionScale = 1.2f;
}

void ADungeonAtmosphereActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	SetWeatherPreset(ActivePreset);
}

void ADungeonAtmosphereActor::BeginPlay()
{
	Super::BeginPlay();
	SetWeatherPreset(ActivePreset);
}

void ADungeonAtmosphereActor::SetWeatherPreset(EDungeonWeatherPreset NewPreset)
{
	ActivePreset = NewPreset;

	switch (NewPreset)
	{
	case EDungeonWeatherPreset::ColdDrafts:
		ApplyWeatherConfig(ColdDraftsConfig);
		break;
	case EDungeonWeatherPreset::DrippingWater:
		ApplyWeatherConfig(DrippingWaterConfig);
		break;
	case EDungeonWeatherPreset::ToxicSpores:
		ApplyWeatherConfig(ToxicSporesConfig);
		break;
	case EDungeonWeatherPreset::EmberAsh:
		ApplyWeatherConfig(EmberAshConfig);
		break;
	case EDungeonWeatherPreset::None:
	default:
		if (WeatherNiagaraComponent)
		{
			WeatherNiagaraComponent->Deactivate();
		}
		break;
	}
}

void ADungeonAtmosphereActor::ApplyWeatherConfig(const FDungeonWeatherConfig& Config)
{
	if (FogComponent)
	{
		FogComponent->SetFogDensity(Config.FogDensity);
		FogComponent->SetVolumetricFogAlbedo(Config.FogAlbedo.ToFColor(true));
		FogComponent->VolumetricFogScatteringDistribution = Config.VolumetricScatteringDistribution;
		FogComponent->VolumetricFogExtinctionScale = Config.VolumetricExtinctionScale;
	}

	if (WeatherNiagaraComponent)
	{
		if (Config.ParticleSystem)
		{
			WeatherNiagaraComponent->SetAsset(Config.ParticleSystem);
			// Enforce GPU particle depth-buffer collision flag
			WeatherNiagaraComponent->SetVariableBool(FName(TEXT("EnableDepthCollision")), true);
			WeatherNiagaraComponent->Activate(true);
		}
		else
		{
			WeatherNiagaraComponent->Deactivate();
		}
	}
}

void ADungeonAtmosphereActor::ConfigureVolumetricFog(
	float InFogDensity,
	FLinearColor InFogAlbedo,
	float InScatteringDistribution,
	float InExtinctionScale)
{
	if (FogComponent)
	{
		FogComponent->SetFogDensity(InFogDensity);
		FogComponent->SetVolumetricFogAlbedo(InFogAlbedo.ToFColor(true));
		FogComponent->VolumetricFogScatteringDistribution = InScatteringDistribution;
		FogComponent->VolumetricFogExtinctionScale = InExtinctionScale;
	}
}
