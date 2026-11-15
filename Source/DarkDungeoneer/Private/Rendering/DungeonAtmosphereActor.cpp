/**
 * DungeonAtmosphereActor.cpp
 * 
 * Implementation of volumetric distance fog and GPU-sprite ambient weather presets.
 */

#include "Rendering/DungeonAtmosphereActor.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Core/DungeoneerCharacter.h"
#include "Dungeon/DungeonGeneratorSubsystem.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraSystem.h"

ADungeonAtmosphereActor::ADungeonAtmosphereActor()
{
	PrimaryActorTick.bCanEverTick = true;

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

	AmbientMotesHISM = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("AmbientMotesHISM"));
	AmbientMotesHISM->SetupAttachment(RootComponent);
	AmbientMotesHISM->SetCastShadow(false);
	AmbientMotesHISM->bCastHiddenShadow = false;
	AmbientMotesHISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	AmbientMotesHISM->NumCustomDataFloats = 4;
	AmbientMotesHISM->SetCullDistances(0, 3200);

	// Setup atmospheric defaults for each weather preset
	ColdDraftsConfig.FogDensity = 0.04f;
	ColdDraftsConfig.FogAlbedo = FLinearColor(0.15f, 0.20f, 0.28f, 1.0f);
	ColdDraftsConfig.VolumetricScatteringDistribution = 0.40f;
	ColdDraftsConfig.VolumetricExtinctionScale = 0.9f;

	DrippingWaterConfig.FogDensity = 0.055f;
	DrippingWaterConfig.FogAlbedo = FLinearColor(0.12f, 0.20f, 0.30f, 1.0f);
	DrippingWaterConfig.VolumetricScatteringDistribution = 0.50f;
	DrippingWaterConfig.VolumetricExtinctionScale = 1.05f;

	ToxicSporesConfig.FogDensity = 0.075f;
	ToxicSporesConfig.FogAlbedo = FLinearColor(0.14f, 0.30f, 0.13f, 1.0f);
	ToxicSporesConfig.VolumetricScatteringDistribution = 0.60f;
	ToxicSporesConfig.VolumetricExtinctionScale = 1.35f;

	EmberAshConfig.FogDensity = 0.052f;
	EmberAshConfig.FogAlbedo = FLinearColor(0.32f, 0.15f, 0.09f, 1.0f);
	EmberAshConfig.VolumetricScatteringDistribution = 0.38f;
	EmberAshConfig.VolumetricExtinctionScale = 1.15f;

	MoteBaseOffsets.Reserve(NumAmbientMotes);
	MoteDriftVelocities.Reserve(NumAmbientMotes);
}

void ADungeonAtmosphereActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	InitializeAmbientMotesPool();
	SetWeatherPreset(ActivePreset);
}

void ADungeonAtmosphereActor::BeginPlay()
{
	Super::BeginPlay();
	InitializeAmbientMotesPool();
	SetWeatherPreset(ActivePreset);
}

void ADungeonAtmosphereActor::InitializeAmbientMotesPool()
{
	if (!AmbientMotesHISM || AmbientMotesHISM->GetInstanceCount() >= NumAmbientMotes)
	{
		return;
	}

	if (!AmbientMotesHISM->GetStaticMesh())
	{
		if (UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
		{
			AmbientMotesHISM->SetStaticMesh(PlaneMesh);
		}
	}

	if (!AmbientMotesHISM->GetMaterial(0))
	{
		if (UMaterialInterface* BaseShapeMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseShapeMat, this))
			{
				MID->SetVectorParameterValue(FName(TEXT("Color")), FLinearColor(0.55f, 0.78f, 0.88f));
				MID->SetScalarParameterValue(FName(TEXT("Roughness")), 0.25f);
				AmbientMotesHISM->SetMaterial(0, MID);
			}
		}
	}

	MoteBaseOffsets.Reset();
	MoteDriftVelocities.Reset();
	FRandomStream SeededStream(90210);
	const FRotator FacingRot(55.0f, 180.0f, 0.0f);

	for (int32 i = 0; i < NumAmbientMotes; ++i)
	{
		const FVector Offset(
			SeededStream.FRandRange(-950.0f, 950.0f),
			SeededStream.FRandRange(-950.0f, 950.0f),
			SeededStream.FRandRange(35.0f, 240.0f));
		const FVector Vel(
			SeededStream.FRandRange(-28.0f, 28.0f),
			SeededStream.FRandRange(-28.0f, 28.0f),
			SeededStream.FRandRange(10.0f, 34.0f));
		MoteBaseOffsets.Add(Offset);
		MoteDriftVelocities.Add(Vel);

		const float ScaleXY = SeededStream.FRandRange(0.055f, 0.11f);
		const int32 InstIdx = AmbientMotesHISM->AddInstance(FTransform(FacingRot, Offset, FVector(ScaleXY, ScaleXY, 1.0f)), false);
		if (InstIdx != INDEX_NONE)
		{
			AmbientMotesHISM->SetCustomDataValue(InstIdx, 0, static_cast<float>(i), false);
		}
	}
	AmbientMotesHISM->MarkRenderStateDirty();
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

void ADungeonAtmosphereActor::SetBiomeAtmosphere(const EDungeonBiomeType NewBiome, const bool bInSanctuary)
{
	ActiveBiome = NewBiome;
	bBiomeZoneSanctuary = bInSanctuary;

	if (bInSanctuary)
	{
		ActivePreset = EDungeonWeatherPreset::None;
		TargetFogDensity = 0.022f;
		TargetFogAlbedo = FLinearColor(0.32f, 0.24f, 0.16f, 1.0f);
		TargetScatteringDistribution = 0.30f;
		TargetExtinctionScale = 0.65f;
		bFogBlendActive = true;
		return;
	}

	const FDungeonWeatherConfig* PresetCfg = &ColdDraftsConfig;
	switch (NewBiome)
	{
	case EDungeonBiomeType::Crypt:
		ActivePreset = EDungeonWeatherPreset::DrippingWater;
		PresetCfg = &DrippingWaterConfig;
		break;
	case EDungeonBiomeType::SporeFens:
		ActivePreset = EDungeonWeatherPreset::ToxicSpores;
		PresetCfg = &ToxicSporesConfig;
		break;
	case EDungeonBiomeType::EmberForge:
		ActivePreset = EDungeonWeatherPreset::EmberAsh;
		PresetCfg = &EmberAshConfig;
		break;
	case EDungeonBiomeType::MirrorPalace:
	default:
		ActivePreset = EDungeonWeatherPreset::ColdDrafts;
		PresetCfg = &ColdDraftsConfig;
		break;
	}

	TargetFogDensity = PresetCfg->FogDensity;
	TargetFogAlbedo = PresetCfg->FogAlbedo;
	TargetScatteringDistribution = PresetCfg->VolumetricScatteringDistribution;
	TargetExtinctionScale = PresetCfg->VolumetricExtinctionScale;
	bFogBlendActive = true;
}

void ADungeonAtmosphereActor::ApplyWeatherConfig(const FDungeonWeatherConfig& Config)
{
	TargetFogDensity = Config.FogDensity;
	TargetFogAlbedo = Config.FogAlbedo;
	TargetScatteringDistribution = Config.VolumetricScatteringDistribution;
	TargetExtinctionScale = Config.VolumetricExtinctionScale;

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
	TargetFogDensity = InFogDensity;
	TargetFogAlbedo = InFogAlbedo;
	TargetScatteringDistribution = InScatteringDistribution;
	TargetExtinctionScale = InExtinctionScale;
	bFogBlendActive = false;

	if (FogComponent)
	{
		FogComponent->SetFogDensity(InFogDensity);
		FogComponent->SetVolumetricFogAlbedo(InFogAlbedo.ToFColor(true));
		FogComponent->VolumetricFogScatteringDistribution = InScatteringDistribution;
		FogComponent->VolumetricFogExtinctionScale = InExtinctionScale;
	}
}

void ADungeonAtmosphereActor::UpdateAmbientMotesAroundFocus(const FVector& FocusLocation, const float DeltaSeconds) noexcept
{
	if (!AmbientMotesHISM || MoteBaseOffsets.Num() < NumAmbientMotes)
	{
		return;
	}

	MoteTimeAccumulator += DeltaSeconds;
	const FRotator FacingRot(55.0f, 180.0f, 0.0f);
	const bool bEmberRising = (ActiveBiome == EDungeonBiomeType::EmberForge);
	const bool bWaterFalling = (ActiveBiome == EDungeonBiomeType::Crypt);

	for (int32 i = 0; i < NumAmbientMotes; ++i)
	{
		const FVector& Base = MoteBaseOffsets[i];
		const FVector& Vel = MoteDriftVelocities[i];
		const double VerticalSign = bWaterFalling ? -1.4 : (bEmberRising ? 1.5 : 0.6);
		const double ZCycle = FMath::Fmod(FMath::Abs(Base.Z + Vel.Z * MoteTimeAccumulator * VerticalSign), 240.0) + 25.0;
		const double SwayX = FMath::Sin(MoteTimeAccumulator * 1.4 + static_cast<double>(i) * 0.7) * 32.0;
		const double SwayY = FMath::Cos(MoteTimeAccumulator * 1.1 + static_cast<double>(i) * 0.5) * 32.0;

		const FVector WorldPos(
			FocusLocation.X + Base.X + SwayX,
			FocusLocation.Y + Base.Y + SwayY,
			FocusLocation.Z + ZCycle);
		const float PulseScale = 0.065f + 0.025f * FMath::Sin(MoteTimeAccumulator * 2.5f + static_cast<float>(i));
		AmbientMotesHISM->UpdateInstanceTransform(i, FTransform(FacingRot, WorldPos, FVector(PulseScale, PulseScale, 1.0f)), true, false, true);
	}
	AmbientMotesHISM->MarkRenderStateDirty();
}

void ADungeonAtmosphereActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UWorld* World = GetWorld();
	FVector FocusLocation = GetActorLocation();

	if (World)
	{
		if (const UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
		{
			if (const ADungeoneerCharacter* Player0 = SpatialSys->GetTrackedPlayerAt(0))
			{
				FocusLocation = Player0->GetActorLocation();
			}
		}

		ZonePollAccumulator += DeltaSeconds;
		if (ZonePollAccumulator >= 0.12f)
		{
			ZonePollAccumulator = 0.0f;
			if (const UDungeonGeneratorSubsystem* GenSub = World->GetSubsystem<UDungeonGeneratorSubsystem>())
			{
				const bool bNowSanctuary = GenSub->IsLocationInRestingGround(FocusLocation);
				EDungeonBiomeType RoomBiome = GenSub->GetActiveFloorGraph().DominantBiome;
				for (const FDungeonNode& Node : GenSub->GetActiveFloorGraph().Nodes)
				{
					const double DX = FMath::Abs(FocusLocation.X - Node.WorldCenter.X);
					const double DY = FMath::Abs(FocusLocation.Y - Node.WorldCenter.Y);
					if (DX <= Node.RoomExtents.X && DY <= Node.RoomExtents.Y)
					{
						RoomBiome = Node.BiomeType;
						break;
					}
				}
				if (RoomBiome != ActiveBiome || bNowSanctuary != bBiomeZoneSanctuary)
				{
					SetBiomeAtmosphere(RoomBiome, bNowSanctuary);
				}
			}
		}
	}

	if (bFogBlendActive && FogComponent)
	{
		const float BlendAlpha = FMath::Clamp(DeltaSeconds / 0.6f, 0.0f, 1.0f);
		const float NewDensity = FMath::Lerp(FogComponent->FogDensity, TargetFogDensity, BlendAlpha);
		const FLinearColor CurAlbedo = FLinearColor(FogComponent->VolumetricFogAlbedo);
		const FLinearColor NewAlbedo = FMath::Lerp(CurAlbedo, TargetFogAlbedo, BlendAlpha);
		FogComponent->SetFogDensity(NewDensity);
		FogComponent->SetVolumetricFogAlbedo(NewAlbedo.ToFColor(true));
		FogComponent->VolumetricFogScatteringDistribution = FMath::Lerp(FogComponent->VolumetricFogScatteringDistribution, TargetScatteringDistribution, BlendAlpha);
		FogComponent->VolumetricFogExtinctionScale = FMath::Lerp(FogComponent->VolumetricFogExtinctionScale, TargetExtinctionScale, BlendAlpha);

		if (FMath::IsNearlyEqual(NewDensity, TargetFogDensity, 0.001f))
		{
			bFogBlendActive = false;
		}
	}

	UpdateAmbientMotesAroundFocus(FocusLocation, DeltaSeconds);
}
