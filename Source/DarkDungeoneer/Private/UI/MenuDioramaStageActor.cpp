#include "UI/MenuDioramaStageActor.h"
#include "AI/ZombieBase.h"
#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dungeon/MirrorMazeActors.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"

AMenuDioramaStageActor::AMenuDioramaStageActor()
{
	PrimaryActorTick.bCanEverTick = true;

	StageRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StageRoot"));
	SetRootComponent(StageRoot);

	DioramaCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("DioramaCamera"));
	DioramaCamera->SetupAttachment(StageRoot);
	DioramaCamera->SetRelativeLocationAndRotation(FVector(-432.0, 552.0, 753.0), FRotator(-42.0, -45.0, 0.0));
	DioramaCamera->FieldOfView = 35.0f;
	DioramaCamera->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
	DioramaCamera->PostProcessSettings.AutoExposureMinBrightness = 0.42f;
	DioramaCamera->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
	DioramaCamera->PostProcessSettings.AutoExposureMaxBrightness = 1.40f;
	DioramaCamera->PostProcessSettings.bOverride_AutoExposureBias = true;
	DioramaCamera->PostProcessSettings.AutoExposureBias = -0.10f;
	DioramaCamera->PostProcessSettings.bOverride_BloomIntensity = true;
	DioramaCamera->PostProcessSettings.BloomIntensity = 0.35f;

	HeroFlashlightSpot = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeroFlashlightSpot"));
	HeroFlashlightSpot->SetupAttachment(StageRoot);
	HeroFlashlightSpot->SetRelativeLocationAndRotation(FVector(-120.0, -40.0, 95.0), FRotator(-8.0, 0.0, 0.0));
	HeroFlashlightSpot->SetIntensityUnits(ELightUnits::Lumens);
	HeroFlashlightSpot->bUseInverseSquaredFalloff = true;
	HeroFlashlightSpot->SetSourceRadius(22.0f);
	HeroFlashlightSpot->SetSoftSourceRadius(32.0f);
	HeroFlashlightSpot->InnerConeAngle = 12.0f;
	HeroFlashlightSpot->OuterConeAngle = 26.0f;
	HeroFlashlightSpot->AttenuationRadius = 920.0f;
	HeroFlashlightSpot->Intensity = 480.0f;
	HeroFlashlightSpot->SetLightColor(FLinearColor(1.0f, 0.74f, 0.34f));
	HeroFlashlightSpot->VolumetricScatteringIntensity = 1.6f;
	HeroFlashlightSpot->CastShadows = false;

	DioramaAmbientFillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("DioramaAmbientFillLight"));
	DioramaAmbientFillLight->SetupAttachment(StageRoot);
	DioramaAmbientFillLight->SetRelativeLocation(FVector(120.0, 0.0, 260.0));
	DioramaAmbientFillLight->SetIntensityUnits(ELightUnits::Lumens);
	DioramaAmbientFillLight->bUseInverseSquaredFalloff = true;
	DioramaAmbientFillLight->SetSourceRadius(32.0f);
	DioramaAmbientFillLight->SetSoftSourceRadius(48.0f);
	DioramaAmbientFillLight->Intensity = 95.0f;
	DioramaAmbientFillLight->AttenuationRadius = 680.0f;
	DioramaAmbientFillLight->SetLightColor(FLinearColor(0.42f, 0.56f, 0.85f));
	DioramaAmbientFillLight->CastShadows = false;

	ShowcaseFloorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShowcaseFloorMesh"));
	ShowcaseFloorMesh->SetupAttachment(StageRoot);
	ShowcaseFloorMesh->SetRelativeLocation(FVector(120.0, 0.0, 0.0));

	ExploderShowcaseProxy = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExploderShowcaseProxy"));
	ExploderShowcaseProxy->SetupAttachment(StageRoot);
	ExploderShowcaseProxy->SetRelativeLocation(FVector(190.0, -145.0, 62.0));

	FastThrowerShowcaseProxy = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FastThrowerShowcaseProxy"));
	FastThrowerShowcaseProxy->SetupAttachment(StageRoot);
	FastThrowerShowcaseProxy->SetRelativeLocation(FVector(255.0, 175.0, 64.0));

	PhantasmMirrorShowcaseProxy = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PhantasmMirrorShowcaseProxy"));
	PhantasmMirrorShowcaseProxy->SetupAttachment(StageRoot);
	PhantasmMirrorShowcaseProxy->SetRelativeLocation(FVector(365.0, 40.0, 74.0));
	PhantasmMirrorShowcaseProxy->SetRenderCustomDepth(true);
	PhantasmMirrorShowcaseProxy->SetCustomDepthStencilValue(MirrorStencilIds::PhantasmMirrorStencil);

	DecoyProjectionProxy = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DecoyProjectionProxy"));
	DecoyProjectionProxy->SetupAttachment(StageRoot);
	DecoyProjectionProxy->SetRelativeLocation(FVector(420.0, -110.0, 64.0));
	DecoyProjectionProxy->SetRenderCustomDepth(true);
	DecoyProjectionProxy->SetCustomDepthStencilValue(MirrorStencilIds::DecoyZombieStencil);
}

void AMenuDioramaStageActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EnsureShowcaseMeshesInitialized();
}

void AMenuDioramaStageActor::EnsureShowcaseMeshesInitialized()
{
	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UMaterialInterface* BaseShapeMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));

	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UDungeoneerAssetBootstrapSubsystem* Bootstrap = GI ? GI->GetSubsystem<UDungeoneerAssetBootstrapSubsystem>() : nullptr;

	auto InitCutoutProxy = [this, BaseShapeMat, Bootstrap](
		UStaticMeshComponent* MeshComp,
		UStaticMesh* MeshAsset,
		const FVector& Scale,
		FName DiffuseKey,
		FName NormalKey,
		const FLinearColor& TintColor,
		float RoughnessVal)
	{
		if (!MeshComp)
		{
			return;
		}
		if (!MeshComp->GetStaticMesh() && MeshAsset)
		{
			MeshComp->SetStaticMesh(MeshAsset);
		}
		MeshComp->SetRelativeScale3D(Scale);
		MeshComp->SetCastShadow(false);

		if (Bootstrap)
		{
			MeshComp->SetRelativeRotation(Bootstrap->GetCameraFacingSpritePlaneRotation());
			if (!bTexturedShowcaseApplied && !DiffuseKey.IsNone())
			{
				if (UMaterialInstanceDynamic* TexturedMID = Bootstrap->CreateDungeonOrRigMaterial(this, DiffuseKey, NormalKey, TintColor, RoughnessVal, false, true))
				{
					MeshComp->SetMaterial(0, TexturedMID);
					return;
				}
			}
		}
		else
		{
			MeshComp->SetRelativeRotation(FRotator(48.0, 135.0, 0.0));
		}

		if (BaseShapeMat && !MeshComp->GetMaterial(0))
		{
			if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseShapeMat, this))
			{
				MID->SetVectorParameterValue(FName(TEXT("Color")), TintColor);
				MID->SetScalarParameterValue(FName(TEXT("Roughness")), RoughnessVal);
				MeshComp->SetMaterial(0, MID);
			}
		}
	};

	InitCutoutProxy(ShowcaseFloorMesh.Get(), PlaneMesh, FVector(18.0, 18.0, 1.0), FName(TEXT("Floor_Tile0_D")), FName(TEXT("Floor_Tile0_N")), FLinearColor(0.18f, 0.21f, 0.28f), 0.72f);
	if (ShowcaseFloorMesh)
	{
		ShowcaseFloorMesh->SetRelativeRotation(FRotator::ZeroRotator);
		ShowcaseFloorMesh->SetVisibility(false);
		ShowcaseFloorMesh->SetHiddenInGame(true);
	}
	InitCutoutProxy(ExploderShowcaseProxy.Get(),       PlaneMesh, FVector(1.55, 1.55, 1.55), FName(TEXT("Exploder_R0_C0_D")),      FName(TEXT("Exploder_R0_C0_N")),      FLinearColor(0.95f, 0.42f, 0.14f), 0.45f);
	InitCutoutProxy(FastThrowerShowcaseProxy.Get(),    PlaneMesh, FVector(1.50, 1.50, 1.50), FName(TEXT("Thrower_R3_C1_D")),       FName(TEXT("Thrower_R3_C1_N")),       FLinearColor(0.28f, 0.85f, 0.96f), 0.40f);
	InitCutoutProxy(PhantasmMirrorShowcaseProxy.Get(), PlaneMesh, FVector(1.45, 1.45, 1.45), FName(TEXT("Mirror_Intact_D")),       FName(TEXT("Mirror_Intact_N")),       FLinearColor(0.55f, 0.90f, 1.00f), 0.14f);
	InitCutoutProxy(DecoyProjectionProxy.Get(),        PlaneMesh, FVector(1.48, 1.48, 1.48), FName(TEXT("MirrorStalker_R0_C0_D")), FName(TEXT("MirrorStalker_R0_C0_N")), FLinearColor(0.32f, 0.88f, 1.00f), 0.35f);

	if (Bootstrap)
	{
		bTexturedShowcaseApplied = true;
	}
}

void AMenuDioramaStageActor::BeginPlay()
{
	Super::BeginPlay();

	EnsureShowcaseMeshesInitialized();

	DeterministicSweepAccumulator = 0.0f;
	SonarPhaseAccumulator = 0.0f;
	EmittedSonarPulseCount = 0;

	if (PhantasmMirrorShowcaseProxy)
	{
		PhantasmMirrorShowcaseProxy->SetRenderCustomDepth(true);
		PhantasmMirrorShowcaseProxy->SetCustomDepthStencilValue(MirrorStencilIds::PhantasmMirrorStencil);
		PhantasmMirrorShowcaseProxy->SetCustomPrimitiveDataFloat(1, 1.0f);
	}

	if (DecoyProjectionProxy)
	{
		DecoyProjectionProxy->SetRenderCustomDepth(true);
		DecoyProjectionProxy->SetCustomDepthStencilValue(MirrorStencilIds::DecoyZombieStencil);
	}
}

void AMenuDioramaStageActor::SetShowcaseModePreview(EDungeoneerMenuAction PreviewAction) noexcept
{
	bAutoSweepOctants = false;
	switch (PreviewAction)
	{
	case EDungeoneerMenuAction::EnterCatacombs:
		SetTargetShowcaseOctant(ECompassOctant::E);
		ChronoGasHazeAlpha = 0.75f;
		break;
	case EDungeoneerMenuAction::MirrorTrial:
		SetTargetShowcaseOctant(ECompassOctant::NE);
		ChronoGasHazeAlpha = 0.45f;
		break;
	case EDungeoneerMenuAction::SensoryLoadout:
		SetTargetShowcaseOctant(ECompassOctant::SE);
		ChronoGasHazeAlpha = 0.95f;
		break;
	case EDungeoneerMenuAction::CoopSplitScreen:
		SetTargetShowcaseOctant(ECompassOctant::N);
		ChronoGasHazeAlpha = 0.65f;
		break;
	case EDungeoneerMenuAction::RestingGround:
		SetTargetShowcaseOctant(ECompassOctant::S);
		ChronoGasHazeAlpha = 0.25f;
		break;
	default:
		bAutoSweepOctants = true;
		break;
	}
}

void AMenuDioramaStageActor::SetTargetShowcaseOctant(ECompassOctant Octant) noexcept
{
	TargetOctant = Octant;
}

void AMenuDioramaStageActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	DeterministicSweepAccumulator += DeltaSeconds;
	SonarPhaseAccumulator += DeltaSeconds;

	if (bAutoSweepOctants)
	{
		const uint8 AutoOctantIndex = static_cast<uint8>(FMath::FloorToInt32(DeterministicSweepAccumulator / 1.8f)) & 7;
		TargetOctant = static_cast<ECompassOctant>(AutoOctantIndex);
	}

	const float TargetYawDeg = DungeoneerMath::GCompassYawDegrees[static_cast<uint8>(TargetOctant) & 7];
	CurrentFlashlightYawDeg = FMath::FixedTurn(CurrentFlashlightYawDeg, TargetYawDeg, FlashlightSweepInterpSpeedDeg * DeltaSeconds);

	if (HeroFlashlightSpot)
	{
		HeroFlashlightSpot->SetRelativeRotation(FRotator(-8.0, static_cast<double>(CurrentFlashlightYawDeg), 0.0));
	}

	// Deterministic Exploder ignition fuse pulse glow via CPD[0]
	ExploderFusePulseAlpha = 0.5f + 0.5f * FMath::Sin(DeterministicSweepAccumulator * 4.2f);
	if (ExploderShowcaseProxy)
	{
		ExploderShowcaseProxy->SetCustomPrimitiveDataFloat(0, ExploderFusePulseAlpha);
		ExploderShowcaseProxy->SetRelativeLocation(FVector(190.0, -145.0, 62.0 + 3.5f * FMath::Sin(DeterministicSweepAccumulator * 3.1f)));
	}
	if (FastThrowerShowcaseProxy)
	{
		FastThrowerShowcaseProxy->SetCustomPrimitiveDataFloat(1, ChronoGasHazeAlpha);
		FastThrowerShowcaseProxy->SetRelativeLocation(FVector(255.0, 175.0, 64.0 + 3.5f * FMath::Cos(DeterministicSweepAccumulator * 2.8f)));
	}
	if (DecoyProjectionProxy)
	{
		DecoyProjectionProxy->SetRelativeLocation(FVector(420.0, -110.0, 64.0 + 4.0f * FMath::Sin(DeterministicSweepAccumulator * 2.3f)));
	}

	// Deterministic phase-accumulated Stencil 105 acoustic sonar ring emission (no FTimerHandle)
	const float Period = FMath::Max(0.5f, SonarPulsePeriodSeconds);
	while (SonarPhaseAccumulator >= Period)
	{
		SonarPhaseAccumulator -= Period;
		++EmittedSonarPulseCount;

		if (UWorld* World = GetWorld())
		{
			if (UDungeoneerSensoryWorldSubsystem* SensoryWorld = World->GetSubsystem<UDungeoneerSensoryWorldSubsystem>())
			{
				SensoryWorld->BroadcastNoiseEvent(GetActorLocation() + FVector(320.0, 170.0, 0.0), 0.9f, 900.0f, true);
			}
		}
	}
}
