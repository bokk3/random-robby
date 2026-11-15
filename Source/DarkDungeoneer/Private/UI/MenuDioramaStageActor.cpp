#include "UI/MenuDioramaStageActor.h"
#include "AI/ZombieBase.h"
#include "Camera/CameraComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dungeon/MirrorMazeActors.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "Engine/World.h"

AMenuDioramaStageActor::AMenuDioramaStageActor()
{
	PrimaryActorTick.bCanEverTick = true;

	StageRoot = CreateDefaultSubobject<USceneComponent>(TEXT("StageRoot"));
	SetRootComponent(StageRoot);

	DioramaCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("DioramaCamera"));
	DioramaCamera->SetupAttachment(StageRoot);
	DioramaCamera->SetRelativeLocationAndRotation(FVector(-620.0, 0.0, 980.0), FRotator(-60.0, 0.0, 0.0));
	DioramaCamera->FieldOfView = 45.0f;

	HeroFlashlightSpot = CreateDefaultSubobject<USpotLightComponent>(TEXT("HeroFlashlightSpot"));
	HeroFlashlightSpot->SetupAttachment(StageRoot);
	HeroFlashlightSpot->SetRelativeLocationAndRotation(FVector(-120.0, -40.0, 95.0), FRotator(-8.0, 0.0, 0.0));
	HeroFlashlightSpot->InnerConeAngle = 14.0f;
	HeroFlashlightSpot->OuterConeAngle = 26.0f;
	HeroFlashlightSpot->AttenuationRadius = 1150.0f;
	HeroFlashlightSpot->Intensity = 4800.0f;

	ShowcaseFloorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShowcaseFloorMesh"));
	ShowcaseFloorMesh->SetupAttachment(StageRoot);

	ExploderShowcaseProxy = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ExploderShowcaseProxy"));
	ExploderShowcaseProxy->SetupAttachment(StageRoot);
	ExploderShowcaseProxy->SetRelativeLocation(FVector(210.0, -140.0, 0.0));

	FastThrowerShowcaseProxy = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FastThrowerShowcaseProxy"));
	FastThrowerShowcaseProxy->SetupAttachment(StageRoot);
	FastThrowerShowcaseProxy->SetRelativeLocation(FVector(320.0, 170.0, 0.0));

	PhantasmMirrorShowcaseProxy = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PhantasmMirrorShowcaseProxy"));
	PhantasmMirrorShowcaseProxy->SetupAttachment(StageRoot);
	PhantasmMirrorShowcaseProxy->SetRelativeLocation(FVector(460.0, 30.0, 0.0));
	PhantasmMirrorShowcaseProxy->SetRenderCustomDepth(true);
	PhantasmMirrorShowcaseProxy->SetCustomDepthStencilValue(MirrorStencilIds::PhantasmMirrorStencil);

	DecoyProjectionProxy = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DecoyProjectionProxy"));
	DecoyProjectionProxy->SetupAttachment(PhantasmMirrorShowcaseProxy);
	DecoyProjectionProxy->SetRelativeLocation(FVector(80.0, 0.0, 0.0));
	DecoyProjectionProxy->SetRenderCustomDepth(true);
	DecoyProjectionProxy->SetCustomDepthStencilValue(MirrorStencilIds::DecoyZombieStencil);
}

void AMenuDioramaStageActor::BeginPlay()
{
	Super::BeginPlay();

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
	}
	if (FastThrowerShowcaseProxy)
	{
		FastThrowerShowcaseProxy->SetCustomPrimitiveDataFloat(1, ChronoGasHazeAlpha);
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
