#include "AI/ZombieBase.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "Rendering/SensoryPerkComponent.h"
#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "Core/DungeoneerMovementComponent.h"
#include "Core/DungeoneerCharacter.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

AZombieBase::AZombieBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;

	ZombieVisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ZombieVisualMesh"));
	ZombieVisualMesh->SetupAttachment(RootComponent);
	ZombieVisualMesh->SetRelativeLocation(FVector(0.0, 0.0, 8.0));
	ZombieVisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ZombieEmissiveLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ZombieEmissiveLight"));
	ZombieEmissiveLight->SetupAttachment(RootComponent);
	ZombieEmissiveLight->SetRelativeLocation(FVector(0.0, 0.0, 50.0));
	ZombieEmissiveLight->SetIntensity(1100.0f);
	ZombieEmissiveLight->SetAttenuationRadius(360.0f);
	ZombieEmissiveLight->CastShadows = false;

	DirectionalZombieMIDs.Reserve(4);
}

void AZombieBase::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EnsureVisualMeshInitialized();
}

void AZombieBase::EnsureVisualMeshInitialized()
{
	if (!ZombieVisualMesh)
	{
		return;
	}

	if (!ZombieVisualMesh->GetStaticMesh())
	{
		if (UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
		{
			ZombieVisualMesh->SetStaticMesh(PlaneMesh);
		}
	}
	ZombieVisualMesh->SetRelativeLocation(FVector(0.0, 0.0, 8.0));
	ZombieVisualMesh->SetRelativeScale3D(GetArchetypeVisualScale());

	const FLinearColor Tint = GetArchetypeVisualTint();
	if (ZombieEmissiveLight)
	{
		ZombieEmissiveLight->SetLightColor(Tint);
	}

	if (!bTexturedVisualsApplied)
	{
		if (const UWorld* World = GetWorld())
		{
			if (const UGameInstance* GI = World->GetGameInstance())
			{
				if (UDungeoneerAssetBootstrapSubsystem* Bootstrap = GI->GetSubsystem<UDungeoneerAssetBootstrapSubsystem>())
				{
					ZombieVisualMesh->SetRelativeRotation(Bootstrap->GetCameraFacingSpritePlaneRotation());

					const TCHAR* Prefix = GetArchetypeKeyPrefix();
					DirectionalZombieMIDs.Reset();
					for (int32 RowIdx = 0; RowIdx < 4; ++RowIdx)
					{
						const FName DiffKey(*FString::Printf(TEXT("%s_Row%d_D"), Prefix, RowIdx));
						const FName NormKey(*FString::Printf(TEXT("%s_Row%d_N"), Prefix, RowIdx));
						UMaterialInstanceDynamic* Mat = Bootstrap->CreateDungeonOrRigMaterial(
							this,
							DiffKey,
							NormKey,
							Tint,
							0.45f,
							false,
							true);
						DirectionalZombieMIDs.Add(Mat);
					}

					bTexturedVisualsApplied = true;
					ActiveDirectionalRow = -1;
					UpdateZombieDirectionalVisual(CurrentHeadingOctant);
					return;
				}
			}
		}

		ZombieVisualMesh->SetRelativeRotation(FRotator(60.0f, 180.0f, 0.0f));
	}

	if (!ZombieVisualMesh->GetMaterial(0))
	{
		if (UMaterialInterface* BaseShapeMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseShapeMat, this))
			{
				MID->SetVectorParameterValue(FName(TEXT("Color")), Tint);
				MID->SetScalarParameterValue(FName(TEXT("Roughness")), 0.45f);
				ZombieVisualMesh->SetMaterial(0, MID);
			}
		}
	}
}

void AZombieBase::UpdateZombieDirectionalVisual(ECompassOctant HeadingOctant) noexcept
{
	if (!ZombieVisualMesh)
	{
		return;
	}

	const int32 DesiredRow = static_cast<int32>(DungeoneerMath::OctantToScreenDirectionalRow(HeadingOctant));
	if (DesiredRow == ActiveDirectionalRow)
	{
		return;
	}

	if (DirectionalZombieMIDs.IsValidIndex(DesiredRow) && DirectionalZombieMIDs[DesiredRow])
	{
		ZombieVisualMesh->SetMaterial(0, DirectionalZombieMIDs[DesiredRow].Get());
		ActiveDirectionalRow = DesiredRow;
	}
}

void AZombieBase::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;
	PreviousLocation = GetActorLocation();
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;

	EnsureVisualMeshInitialized();

	if (bIsMirrorDecoy)
	{
		SetReplicates(false);
	}
	else if (UWorld* World = GetWorld())
	{
		if (UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
		{
			SpatialSys->RegisterZombie(this);
		}
	}
}

void AZombieBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (!bIsMirrorDecoy)
	{
		if (UWorld* World = GetWorld())
		{
			if (UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
			{
				SpatialSys->UnregisterZombie(this);
			}
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AZombieBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateZombieDirectionalVisual(CurrentHeadingOctant);

	if (IsZombieAlive() && !bIsMirrorDecoy)
	{
		AdvanceDeterministicStepAccumulator(DeltaSeconds);

		if (HasAuthority())
		{
			bool bVisibleToAnyPlayer = false;
			if (const UWorld* World = GetWorld())
			{
				if (const UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
				{
					const int32 PlayerCount = SpatialSys->GetTrackedPlayerCount();
					for (int32 i = 0; i < PlayerCount; ++i)
					{
						if (const ADungeoneerCharacter* Player = SpatialSys->GetTrackedPlayerAt(i))
						{
							if (IsDetectedByViewer(Player, Player->GetActorLocation()))
							{
								bVisibleToAnyPlayer = true;
								break;
							}
						}
					}
				}
			}
			SetNetUpdateFrequency(bVisibleToAnyPlayer ? 15.0f : 2.0f);
		}
	}
}

void AZombieBase::MoveAlongOctant(ECompassOctant Octant, float Scale) noexcept
{
	CurrentHeadingOctant = Octant;
	UpdateZombieDirectionalVisual(Octant);
	const FVector UnitDir = DungeoneerMath::OctantToDirection3D(Octant);
	AddMovementInput(UnitDir, Scale);
}

void AZombieBase::AdvanceDeterministicStepAccumulator(float DeltaSeconds) noexcept
{
	const FVector CurrentLocation = GetActorLocation();
	const double DistSq = FVector::DistSquared2D(PreviousLocation, CurrentLocation);
	PreviousLocation = CurrentLocation;

	if (DistSq <= static_cast<double>(KINDA_SMALL_NUMBER))
	{
		return;
	}

	StepDistanceAccumulator += static_cast<float>(FMath::Sqrt(DistSq));
	while (StepDistanceAccumulator >= StepCycleDistanceUU)
	{
		StepDistanceAccumulator -= StepCycleDistanceUU;
		StepSequenceCounter = (StepSequenceCounter + 1) & 0x3FFFFFFF;

		if (OnZombieStepTriggered.IsBound())
		{
			FDungeoneerStepEvent StepEvt;
			StepEvt.WorldLocation = CurrentLocation;
			StepEvt.StepOctant = CurrentHeadingOctant;
			StepEvt.AcousticLoudness = 1.25f;
			StepEvt.StepSequenceIndex = StepSequenceCounter;
			StepEvt.bIsHostileSource = true;
			OnZombieStepTriggered.Broadcast(StepEvt);
		}

		if (UWorld* World = GetWorld())
		{
			if (UDungeoneerSensoryWorldSubsystem* SensorySS = World->GetSubsystem<UDungeoneerSensoryWorldSubsystem>())
			{
				SensorySS->BroadcastNoiseEvent(CurrentLocation, 1.25f, 900.0f, bIsMirrorDecoy);
			}
		}
	}
}

void AZombieBase::ApplyZombieDamage(float DamageAmount, AActor* DamageInstigator)
{
	if (!HasAuthority() || !IsZombieAlive() || DamageAmount <= 0.0f)
	{
		return;
	}

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);
	if (CurrentHealth <= 0.0f)
	{
		OnZombieDied();
	}
}

void AZombieBase::OnZombieDied()
{
	SetActorEnableCollision(false);
	GetCharacterMovement()->StopMovementImmediately();
}

bool AZombieBase::IsDetectedByViewer(const AActor* ViewerActor, const FVector& ViewerOrigin) const noexcept
{
	const ADungeoneerCharacter* CharacterViewer = Cast<ADungeoneerCharacter>(ViewerActor);
	if (!CharacterViewer)
	{
		if (const APlayerController* PC = Cast<APlayerController>(ViewerActor))
		{
			CharacterViewer = Cast<ADungeoneerCharacter>(PC->GetPawn());
		}
	}

	const FVector ZombieLocation = GetActorLocation();
	if (!CharacterViewer)
	{
		return FVector::DistSquared2D(ZombieLocation, ViewerOrigin) <= FMath::Square(350.0);
	}

	const FVector ViewerLocation = CharacterViewer->GetActorLocation();
	const double DistSq2D = FVector::DistSquared2D(ZombieLocation, ViewerLocation);

	if (const USensoryPerkComponent* Sensory = CharacterViewer->GetSensoryPerkComponent())
	{
		const float DarkVisionRadius = Sensory->GetDarkVisionRadius();
		if (DarkVisionRadius > 0.0f && DistSq2D <= FMath::Square(static_cast<double>(DarkVisionRadius)))
		{
			return true;
		}

		if (const UWorld* World = GetWorld())
		{
			if (const UDungeoneerSensoryWorldSubsystem* SensoryWorld = World->GetSubsystem<UDungeoneerSensoryWorldSubsystem>())
			{
				FDungeoneerNoiseEvent StrongestNoise;
				const float HearingMult = Sensory->GetAcuteHearingRangeMultiplier();
				const float HearingRange = PerceptionHearingRange * HearingMult;
				if (DistSq2D <= FMath::Square(static_cast<double>(HearingRange))
					&& SensoryWorld->QueryStrongestActivePulse(ViewerLocation, HearingMult, StrongestNoise)
					&& StrongestNoise.bActive)
				{
					const double PulseCoverRadius = static_cast<double>(StrongestNoise.CurrentRadius) + 150.0;
					if (FVector::DistSquared2D(StrongestNoise.WorldOrigin, ZombieLocation) <= FMath::Square(PulseCoverRadius))
					{
						return true;
					}
				}
			}
		}
	}

	return CharacterViewer->IsPointLitByFlashlight(ZombieLocation);
}

bool AZombieBase::IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const
{
	// Mirror-Palace zombie decoys are strictly client-side seed-driven illusions; never replicate.
	if (bIsMirrorDecoy)
	{
		return false;
	}

	const AActor* EffectiveViewer = ViewTarget ? ViewTarget : RealViewer;
	if (!EffectiveViewer)
	{
		return false;
	}

	return IsDetectedByViewer(EffectiveViewer, SrcLocation);
}
