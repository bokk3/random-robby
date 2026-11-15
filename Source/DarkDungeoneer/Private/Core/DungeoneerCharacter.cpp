#include "Core/DungeoneerCharacter.h"
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
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
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
	HeroLanternFillLight->SetRelativeLocation(FVector(0.0f, 0.0f, 95.0f));
	HeroLanternFillLight->SetIntensity(4800.0f);
	HeroLanternFillLight->SetAttenuationRadius(1100.0f);
	HeroLanternFillLight->SetLightColor(FLinearColor(1.0f, 0.84f, 0.55f));
	HeroLanternFillLight->CastShadows = false;

	SensoryPerkComponent = CreateDefaultSubobject<USensoryPerkComponent>(TEXT("SensoryPerkComponent"));

	DirectionalRigComponent = CreateDefaultSubobject<U25DDirectionalRigComponent>(TEXT("DirectionalRigComponent"));
	DirectionalRigComponent->SetupAttachment(RootComponent);

	HeroVisualMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeroVisualMesh"));
	HeroVisualMesh->SetupAttachment(RootComponent);
	HeroVisualMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -5.0f));
	HeroVisualMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TimedAudioComponent = CreateDefaultSubobject<UDungeoneerTimedAudioComponent>(TEXT("TimedAudioComponent"));
	InventoryComponent = CreateDefaultSubobject<UDungeoneerInventoryComponent>(TEXT("InventoryComponent"));
	MirrorBounceComponent = CreateDefaultSubobject<UMirrorLightBounceComponent>(TEXT("MirrorBounceComponent"));
}

void ADungeoneerCharacter::EnsureDefaultVisualsInitialized()
{
	if (!HeroVisualMesh)
	{
		return;
	}

	if (!HeroVisualMesh->GetStaticMesh())
	{
		if (UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
		{
			HeroVisualMesh->SetStaticMesh(CylinderMesh);
			HeroVisualMesh->SetRelativeScale3D(FVector(0.85, 0.85, 1.65));
		}
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

	if (DirectionalRigComponent)
	{
		DirectionalRigComponent->RegisterSocketPrimitive(E25DRigSocket::Torso, HeroVisualMesh.Get());
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

void ADungeoneerCharacter::PollDirectKeyboardAndMouseFallback() noexcept
{
	APlayerController* PC = GetController<APlayerController>();
	UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement();
	if (!PC || !MoveComp)
	{
		return;
	}

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
	if (PC->GetMousePosition(MouseX, MouseY))
	{
		PC->GetViewportSize(ViewW, ViewH);
		if (ViewW > 0 && ViewH > 0)
		{
			const float CenterX = static_cast<float>(ViewW) * 0.5f;
			const float CenterY = static_cast<float>(ViewH) * 0.5f;
			const FVector2D AimVec(static_cast<double>(CenterY - MouseY), static_cast<double>(MouseX - CenterX));
			MoveComp->SetAimInputVector(AimVec);
		}
	}
	else if (MoveComp->HasActiveMoveInput())
	{
		MoveComp->SetAimInputVector(MoveVec);
	}
}

void ADungeoneerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsLocallyControlled() && !DefaultMappingContext)
	{
		PollDirectKeyboardAndMouseFallback();
	}

	SyncFlashlightTransform(DeltaSeconds);

	if (DirectionalRigComponent)
	{
		DirectionalRigComponent->SetHeadingOctant(ReplicatedAimOctant);
		if (const UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement())
		{
			DirectionalRigComponent->SetAnimationPlaying(MoveComp->HasActiveMoveInput());
		}
	}

	if (IsLocallyControlled())
	{
		if (const UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement())
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
