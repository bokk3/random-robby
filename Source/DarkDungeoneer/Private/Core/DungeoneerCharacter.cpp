#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "Core/DungeoneerMovementComponent.h"
#include "Core/DungeoneerMovementConfig.h"
#include "Rendering/SensoryPerkComponent.h"
#include "Rendering/25DDirectionalRigComponent.h"
#include "Audio/DungeoneerTimedAudioComponent.h"
#include "Social/DungeoneerInventoryComponent.h"
#include "Dungeon/MirrorLightBounceComponent.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "UI/DungeoneerPlayerHUDWidget.h"
#include "Blueprint/UserWidget.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Net/UnrealNetwork.h"

ADungeoneerCharacter::ADungeoneerCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UDungeoneerMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->TargetArmLength = 1100.0f;
	CameraBoom->SetRelativeRotation(FRotator(-60.0f, 0.0f, 0.0f));
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.0f;

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCamera->bUsePawnControlRotation = false;
	TopDownCamera->FieldOfView = 42.0f; // Orthographic-feel perspective foreshortening
	TopDownCamera->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
	TopDownCamera->PostProcessSettings.AutoExposureMinBrightness = 0.25f;
	TopDownCamera->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
	TopDownCamera->PostProcessSettings.AutoExposureMaxBrightness = 2.0f;

	FlashlightComponent = CreateDefaultSubobject<USpotLightComponent>(TEXT("FlashlightComponent"));
	FlashlightComponent->SetupAttachment(RootComponent);
	FlashlightComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 55.0f));
	FlashlightComponent->SetIntensity(26000.0f);
	FlashlightComponent->SetLightColor(FLinearColor(1.0f, 0.94f, 0.82f));
	FlashlightComponent->SetInnerConeAngle(12.0f);
	FlashlightComponent->SetOuterConeAngle(28.0f);
	FlashlightComponent->SetAttenuationRadius(2200.0f);
	FlashlightComponent->bUseInverseSquaredFalloff = true;
	FlashlightComponent->VolumetricScatteringIntensity = 2.5f;
	FlashlightComponent->CastShadows = true;

	HeroLanternFillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("HeroLanternFillLight"));
	HeroLanternFillLight->SetupAttachment(RootComponent);
	HeroLanternFillLight->SetRelativeLocation(FVector(-65.0f, 0.0f, 110.0f));
	HeroLanternFillLight->SetIntensity(4800.0f);
	HeroLanternFillLight->SetAttenuationRadius(1100.0f);
	HeroLanternFillLight->SetLightColor(FLinearColor(1.0f, 0.84f, 0.55f));
	HeroLanternFillLight->CastShadows = false;

	SensoryPerkComponent = CreateDefaultSubobject<USensoryPerkComponent>(TEXT("SensoryPerkComponent"));

	DirectionalRigComponent = CreateDefaultSubobject<U25DDirectionalRigComponent>(TEXT("DirectionalRigComponent"));
	DirectionalRigComponent->SetupAttachment(RootComponent);

	HeroVisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeroVisualMesh"));
	HeroVisualMesh->SetupAttachment(RootComponent);
	HeroVisualMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 8.0f));
	HeroVisualMesh->SetRelativeScale3D(FVector(1.65, 1.65, 1.65));
	HeroVisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeroVisualMesh->SetCastShadow(false);

	TimedAudioComponent = CreateDefaultSubobject<UDungeoneerTimedAudioComponent>(TEXT("TimedAudioComponent"));
	InventoryComponent = CreateDefaultSubobject<UDungeoneerInventoryComponent>(TEXT("InventoryComponent"));
	MirrorBounceComponent = CreateDefaultSubobject<UMirrorLightBounceComponent>(TEXT("MirrorBounceComponent"));

	DirectionalHeroMIDs.Reserve(16);
}

void ADungeoneerCharacter::EnsureDefaultVisualsInitialized()
{
	if (!HeroVisualMesh)
	{
		return;
	}

	if (!HeroVisualMesh->GetStaticMesh())
	{
		if (UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
		{
			HeroVisualMesh->SetStaticMesh(PlaneMesh);
		}
	}
	HeroVisualMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 8.0f));
	HeroVisualMesh->SetRelativeScale3D(FVector(1.65, 1.65, 1.65));
	HeroVisualMesh->SetCastShadow(false);

	if (!bTexturedVisualsApplied)
	{
		if (const UWorld* World = GetWorld())
		{
			if (const UGameInstance* GI = World->GetGameInstance())
			{
				if (UDungeoneerAssetBootstrapSubsystem* Bootstrap = GI->GetSubsystem<UDungeoneerAssetBootstrapSubsystem>())
				{
					HeroVisualMesh->SetRelativeRotation(Bootstrap->GetCameraFacingSpritePlaneRotation());

					DirectionalHeroMIDs.Reset();
					for (int32 RowIdx = 0; RowIdx < 4; ++RowIdx)
					{
						for (int32 ColIdx = 0; ColIdx < 4; ++ColIdx)
						{
							const FName DiffKey(*FString::Printf(TEXT("Hero_R%d_C%d_D"), RowIdx, ColIdx));
							const FName NormKey(*FString::Printf(TEXT("Hero_R%d_C%d_N"), RowIdx, ColIdx));
							UMaterialInstanceDynamic* CellMID = Bootstrap->CreateDungeonOrRigMaterial(
								this,
								DiffKey,
								NormKey,
								FLinearColor::White,
								0.45f,
								false,
								true);
							DirectionalHeroMIDs.Add(CellMID);
						}
					}

					bTexturedVisualsApplied = true;
					ActiveDirectionalRow = -1;
					ActiveWalkFrameColumn = 0;
					ActiveDirectionalCellIndex = -1;
					const UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement();
					const ECompassOctant InitialFacing = MoveComp ? MoveComp->GetAimOctant() : ReplicatedAimOctant;
					UpdateHeroDirectionalVisual(InitialFacing, ActiveWalkFrameColumn);
					return;
				}
			}
		}

		// Bare test-world fallback without a GameInstance: orient upright plane facing the -60 deg camera
		HeroVisualMesh->SetRelativeRotation(FRotator(60.0f, 180.0f, 0.0f));
	}

	if (!HeroVisualMesh->GetMaterial(0))
	{
		if (UMaterialInterface* BaseShapeMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			if (UMaterialInstanceDynamic* HeroMID = UMaterialInstanceDynamic::Create(BaseShapeMat, this))
			{
				HeroMID->SetVectorParameterValue(FName(TEXT("Color")), FLinearColor(0.95f, 0.78f, 0.28f));
				HeroMID->SetScalarParameterValue(FName(TEXT("Roughness")), 0.45f);
				HeroVisualMesh->SetMaterial(0, HeroMID);
			}
		}
	}
}

void ADungeoneerCharacter::AdvanceWalkFlipbookByDistance(float DeltaDistanceUU, bool bIsMoving) noexcept
{
	const float ClampedDist = FMath::Max(DeltaDistanceUU, 0.0f);
	if (bIsMoving || ClampedDist > 0.25f)
	{
		WalkDistanceAccumulatorUU += ClampedDist;
		const float SafeStride = FMath::Max(WalkCycleStrideUU, 20.0f);
		ActiveWalkFrameColumn = FMath::FloorToInt((WalkDistanceAccumulatorUU / SafeStride) * 4.0f) & 3;
	}
	else
	{
		// Lock to Column 0 (Contact / Idle Pose A) when standing still
		ActiveWalkFrameColumn = 0;
	}
}

void ADungeoneerCharacter::UpdateHeroDirectionalVisual(ECompassOctant FacingOctant, int32 FrameColumn) noexcept
{
	if (!HeroVisualMesh)
	{
		return;
	}

	const int32 DesiredRow = static_cast<int32>(DungeoneerMath::OctantToScreenDirectionalRow(FacingOctant));
	const int32 DesiredCol = FMath::Clamp((FrameColumn >= 0) ? FrameColumn : ActiveWalkFrameColumn, 0, 3);
	const int32 DesiredCell = DesiredRow * 4 + DesiredCol;

	if (DesiredCell == ActiveDirectionalCellIndex)
	{
		return;
	}

	if (DirectionalHeroMIDs.IsValidIndex(DesiredCell) && DirectionalHeroMIDs[DesiredCell])
	{
		HeroVisualMesh->SetMaterial(0, DirectionalHeroMIDs[DesiredCell].Get());
		ActiveDirectionalRow = DesiredRow;
		ActiveWalkFrameColumn = DesiredCol;
		ActiveDirectionalCellIndex = DesiredCell;
	}
}

void ADungeoneerCharacter::BeginPlay()
{
	Super::BeginPlay();

	EnsureDefaultVisualsInitialized();

	if (const UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement())
	{
		if (const UDungeoneerMovementConfig* Config = MoveComp->GetMovementConfig())
		{
			FlashlightComponent->SetInnerConeAngle(Config->FlashlightInnerConeAngleDeg);
			FlashlightComponent->SetOuterConeAngle(Config->FlashlightOuterConeAngleDeg);
			FlashlightComponent->SetAttenuationRadius(Config->FlashlightAttenuationRadius);
			FlashlightComponent->VolumetricScatteringIntensity = Config->FlashlightVolumetricScattering;
		}
	}

	if (UWorld* World = GetWorld())
	{
		if (UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
		{
			SpatialSys->RegisterPlayer(this);
		}
	}

	BindLocalInputContext();

	if (!IsRunningCommandlet() && GetWorld() && GetWorld()->IsGameWorld())
	{
		if (ULocalPlayer* LP = GetOwningLocalPlayer())
		{
			if (APlayerController* OwningPC = LP->GetPlayerController(GetWorld()))
			{
				if (UDungeoneerPlayerHUDWidget* PlayerHUD = CreateWidget<UDungeoneerPlayerHUDWidget>(OwningPC, UDungeoneerPlayerHUDWidget::StaticClass()))
				{
					PlayerHUD->RefreshHUDFromCharacter(this);
					PlayerHUD->AddToPlayerScreen(5);
				}
			}
		}
	}
}

void ADungeoneerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
		{
			SpatialSys->UnregisterPlayer(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void ADungeoneerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	BindLocalInputContext();
}

void ADungeoneerCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();
	BindLocalInputContext();
}

void ADungeoneerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADungeoneerCharacter, CurrentHealth);
	DOREPLIFETIME(ADungeoneerCharacter, FlashlightBattery);
	DOREPLIFETIME_CONDITION(ADungeoneerCharacter, ReplicatedMoveOctant, COND_SkipOwner);
	DOREPLIFETIME_CONDITION(ADungeoneerCharacter, ReplicatedAimOctant, COND_SkipOwner);
}

UDungeoneerMovementComponent* ADungeoneerCharacter::GetDungeoneerMovement() const noexcept
{
	return Cast<UDungeoneerMovementComponent>(GetCharacterMovement());
}

ULocalPlayer* ADungeoneerCharacter::GetOwningLocalPlayer() const noexcept
{
	const APlayerController* PC = GetController<APlayerController>();
	return PC ? PC->GetLocalPlayer() : nullptr;
}

void ADungeoneerCharacter::BindLocalInputContext()
{
	if (!DefaultMappingContext)
	{
		return;
	}

	if (ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			InputSubsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void ADungeoneerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (MoveAction)
		{
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADungeoneerCharacter::HandleMoveInput);
			EnhancedInput->BindAction(MoveAction, ETriggerEvent::Completed, this, &ADungeoneerCharacter::HandleMoveCompleted);
		}

		if (AimAction)
		{
			EnhancedInput->BindAction(AimAction, ETriggerEvent::Triggered, this, &ADungeoneerCharacter::HandleAimInput);
		}
	}
}

void ADungeoneerCharacter::HandleMoveInput(const FInputActionValue& Value)
{
	const FVector2D MoveVec = Value.Get<FVector2D>();
	if (UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement())
	{
		MoveComp->SetMoveInputVector(MoveVec);
		const FVector MoveDir = UDungeoneerMovementComponent::GetUnitDirectionForOctant(MoveComp->GetMoveOctant());
		if (MoveComp->HasActiveMoveInput())
		{
			AddMovementInput(MoveDir, 1.0f);
		}
	}
}

void ADungeoneerCharacter::HandleMoveCompleted(const FInputActionValue& Value)
{
	if (UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement())
	{
		MoveComp->SetMoveInputVector(FVector2D::ZeroVector);
	}
}

void ADungeoneerCharacter::HandleAimInput(const FInputActionValue& Value)
{
	const FVector2D AimVec = Value.Get<FVector2D>();
	if (UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement())
	{
		MoveComp->SetAimInputVector(AimVec);
	}
}

void ADungeoneerCharacter::PollDirectKeyboardAndMouseFallback(float DeltaSeconds) noexcept
{
	APlayerController* PC = GetController<APlayerController>();
	UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement();
	if (!PC || !MoveComp)
	{
		return;
	}

	TimeSinceMouseMoved = FMath::Min(TimeSinceMouseMoved + DeltaSeconds, 10.0f);

	double MoveX = 0.0;
	double MoveY = 0.0;
	if (PC->IsInputKeyDown(EKeys::W) || PC->IsInputKeyDown(EKeys::Up))    { MoveX += 1.0; }
	if (PC->IsInputKeyDown(EKeys::S) || PC->IsInputKeyDown(EKeys::Down))  { MoveX -= 1.0; }
	if (PC->IsInputKeyDown(EKeys::D) || PC->IsInputKeyDown(EKeys::Right)) { MoveY += 1.0; }
	if (PC->IsInputKeyDown(EKeys::A) || PC->IsInputKeyDown(EKeys::Left))  { MoveY -= 1.0; }

	const FVector2D MoveVec(MoveX, MoveY);
	MoveComp->SetMoveInputVector(MoveVec);
	if (MoveComp->HasActiveMoveInput())
	{
		const FVector MoveDir = UDungeoneerMovementComponent::GetUnitDirectionForOctant(MoveComp->GetMoveOctant());
		AddMovementInput(MoveDir, 1.0f);
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	int32 ViewW = 0;
	int32 ViewH = 0;
	bool bMouseActivelyAiming = false;
	if (PC->GetMousePosition(MouseX, MouseY))
	{
		const FVector2D CurrentMouse(static_cast<double>(MouseX), static_cast<double>(MouseY));
		if (!bHasMouseSample)
		{
			LastMouseSample = CurrentMouse;
			bHasMouseSample = true;
		}
		else if (FVector2D::DistSquared(CurrentMouse, LastMouseSample) > 4.0)
		{
			LastMouseSample = CurrentMouse;
			TimeSinceMouseMoved = 0.0f;
		}

		if (TimeSinceMouseMoved <= 0.45f)
		{
			PC->GetViewportSize(ViewW, ViewH);
			if (ViewW > 0 && ViewH > 0)
			{
				const float CenterX = static_cast<float>(ViewW) * 0.5f;
				const float CenterY = static_cast<float>(ViewH) * 0.5f;
				const FVector2D AimVec(static_cast<double>(CenterY - MouseY), static_cast<double>(MouseX - CenterX));
				MoveComp->SetAimInputVector(AimVec);
				bMouseActivelyAiming = true;
			}
		}
	}

	if (!bMouseActivelyAiming && MoveComp->HasActiveMoveInput())
	{
		MoveComp->SetAimInputVector(MoveVec);
	}
}

void ADungeoneerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsLocallyControlled() && !DefaultMappingContext)
	{
		PollDirectKeyboardAndMouseFallback(DeltaSeconds);
	}

	SyncFlashlightTransform(DeltaSeconds);

	const UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement();
	const ECompassOctant ActiveFacingOctant = (IsLocallyControlled() && MoveComp)
		? MoveComp->GetAimOctant()
		: ReplicatedAimOctant;

	const FVector CurrentLoc = GetActorLocation();
	float DeltaMoveDistUU = 0.0f;
	if (bHasPreviousWalkLocation)
	{
		DeltaMoveDistUU = static_cast<float>(FVector::Dist2D(PreviousWalkLocation, CurrentLoc));
	}
	PreviousWalkLocation = CurrentLoc;
	bHasPreviousWalkLocation = true;

	const bool bHasMoveInput = MoveComp && MoveComp->HasActiveMoveInput();
	if (bHasMoveInput && DeltaMoveDistUU <= 0.1f && MoveComp)
	{
		DeltaMoveDistUU = static_cast<float>(MoveComp->Velocity.Size2D()) * FMath::Max(DeltaSeconds, 0.0f);
	}
	AdvanceWalkFlipbookByDistance(DeltaMoveDistUU, bHasMoveInput);

	UpdateHeroDirectionalVisual(ActiveFacingOctant, ActiveWalkFrameColumn);

	if (DirectionalRigComponent)
	{
		DirectionalRigComponent->SetHeadingOctant(ActiveFacingOctant);
		DirectionalRigComponent->SetFrameIndex(ActiveWalkFrameColumn);
		if (MoveComp)
		{
			DirectionalRigComponent->SetAnimationPlaying(bHasMoveInput);
		}
	}

	if (IsLocallyControlled() && MoveComp)
	{
		const ECompassOctant LocalMove = MoveComp->GetMoveOctant();
		const ECompassOctant LocalAim = MoveComp->GetAimOctant();
		OctantSyncAccumulator += DeltaSeconds;

		if ((LocalMove != ReplicatedMoveOctant || LocalAim != ReplicatedAimOctant) && OctantSyncAccumulator >= 0.05f)
		{
			ReplicatedMoveOctant = LocalMove;
			ReplicatedAimOctant = LocalAim;
			OctantSyncAccumulator = 0.0f;
			if (!HasAuthority())
			{
				Server_UpdateOctants(LocalMove, LocalAim);
			}
		}
	}
}

void ADungeoneerCharacter::SyncFlashlightTransform(float DeltaSeconds) noexcept
{
	if (!FlashlightComponent)
	{
		return;
	}

	const UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement();
	const ECompassOctant TargetOctant = IsLocallyControlled() && MoveComp
		? MoveComp->GetAimOctant()
		: ReplicatedAimOctant;

	const float TargetYawDeg = DungeoneerMath::GCompassYawDegrees[static_cast<uint8>(TargetOctant) & 7];
	const float InterpSpeed = (MoveComp && MoveComp->GetMovementConfig())
		? MoveComp->GetMovementConfig()->FlashlightInterpSpeed
		: 18.0f;

	CurrentFlashlightYawDeg = FMath::FixedTurn(CurrentFlashlightYawDeg, TargetYawDeg, InterpSpeed * DeltaSeconds * 30.0f);
	FlashlightComponent->SetWorldRotation(FRotator(-5.0f, CurrentFlashlightYawDeg, 0.0f));
}

void ADungeoneerCharacter::Server_UpdateOctants_Implementation(ECompassOctant NewMoveOctant, ECompassOctant NewAimOctant)
{
	ReplicatedMoveOctant = NewMoveOctant;
	ReplicatedAimOctant = NewAimOctant;
}

void ADungeoneerCharacter::ApplyDungeoneerDamage(float DamageAmount, AActor* DamageCauser)
{
	if (!HasAuthority() || DamageAmount <= 0.0f)
	{
		return;
	}

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);
}

void ADungeoneerCharacter::RestoreAtRestingGround(float HealthDelta, float BatteryDelta)
{
	if (!HasAuthority())
	{
		return;
	}

	CurrentHealth = FMath::Clamp(CurrentHealth + HealthDelta, 0.0f, MaxHealth);
	FlashlightBattery = FMath::Clamp(FlashlightBattery + BatteryDelta, 0.0f, 100.0f);
}

bool ADungeoneerCharacter::IsPointLitByFlashlight(const FVector& TargetWorldLocation) const noexcept
{
	if (!FlashlightComponent || FlashlightBattery <= 0.0f)
	{
		return false;
	}

	const FVector LightOrigin = FlashlightComponent->GetComponentLocation();
	const FVector ToTarget = TargetWorldLocation - LightOrigin;
	const float DistSq = static_cast<float>(ToTarget.SizeSquared2D());
	const float MaxRange = FlashlightComponent->AttenuationRadius;
	if (DistSq > MaxRange * MaxRange || DistSq <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const FVector ForwardXY = FlashlightComponent->GetForwardVector().GetSafeNormal2D();
	const FVector DirXY = ToTarget.GetSafeNormal2D();
	const float CosTheta = static_cast<float>(FVector::DotProduct(ForwardXY, DirXY));
	const float ConeCosThreshold = FMath::Cos(FMath::DegreesToRadians(FlashlightComponent->OuterConeAngle));
	return CosTheta >= ConeCosThreshold;
}
