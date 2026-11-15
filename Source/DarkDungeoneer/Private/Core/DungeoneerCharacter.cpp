#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "Core/DungeoneerGameMode.h"
#include "Core/DungeoneerGameState.h"
#include "Core/DungeoneerMovementComponent.h"
#include "Core/DungeoneerMovementConfig.h"
#include "Rendering/SensoryPerkComponent.h"
#include "Rendering/25DDirectionalRigComponent.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "Audio/DungeoneerTimedAudioComponent.h"
#include "Social/DungeoneerInventoryComponent.h"
#include "Dungeon/DungeonEncounterActors.h"
#include "Dungeon/DungeoneerTileMapInstancer.h"
#include "Dungeon/MirrorLightBounceComponent.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "UI/DungeoneerPlayerHUDWidget.h"
#include "UI/MenuDioramaStageActor.h"
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

	SlashArcMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SlashArcMesh"));
	SlashArcMesh->SetupAttachment(RootComponent);
	SlashArcMesh->SetRelativeLocation(FVector(88.0f, 0.0f, 14.0f));
	SlashArcMesh->SetRelativeScale3D(FVector(1.45, 0.90, 1.0));
	SlashArcMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SlashArcMesh->SetCastShadow(false);
	SlashArcMesh->SetVisibility(false);

	TimedAudioComponent = CreateDefaultSubobject<UDungeoneerTimedAudioComponent>(TEXT("TimedAudioComponent"));
	InventoryComponent = CreateDefaultSubobject<UDungeoneerInventoryComponent>(TEXT("InventoryComponent"));
	MirrorBounceComponent = CreateDefaultSubobject<UMirrorLightBounceComponent>(TEXT("MirrorBounceComponent"));

	DirectionalHeroMIDs.Reserve(16);
}

void ADungeoneerCharacter::EnsureDefaultVisualsInitialized()
{
	if (SlashArcMesh)
	{
		if (!SlashArcMesh->GetStaticMesh())
		{
			if (UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
			{
				SlashArcMesh->SetStaticMesh(PlaneMesh);
			}
		}
		if (!SlashArcMesh->GetMaterial(0))
		{
			if (UMaterialInterface* BaseShapeMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
			{
				if (UMaterialInstanceDynamic* SlashMID = UMaterialInstanceDynamic::Create(BaseShapeMat, this))
				{
					SlashMID->SetVectorParameterValue(FName(TEXT("Color")), FLinearColor(1.0f, 0.72f, 0.22f));
					SlashMID->SetScalarParameterValue(FName(TEXT("Roughness")), 0.15f);
					SlashArcMesh->SetMaterial(0, SlashMID);
				}
			}
		}
		SlashArcMesh->SetVisibility(CombatActionState == EDungeoneerCombatActionState::LanternSlash);
	}

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
	BaseVisualRelativeLocation = FVector(0.0, 0.0, 8.0);
	BaseVisualRelativeScale = FVector(1.65, 1.65, 1.65);
	HeroVisualMesh->SetRelativeLocation(BaseVisualRelativeLocation);
	HeroVisualMesh->SetRelativeScale3D(BaseVisualRelativeScale);
	HeroVisualMesh->SetCastShadow(false);

	if (!bTexturedVisualsApplied)
	{
		if (const UWorld* World = GetWorld())
		{
			if (const UGameInstance* GI = World->GetGameInstance())
			{
				if (UDungeoneerAssetBootstrapSubsystem* Bootstrap = GI->GetSubsystem<UDungeoneerAssetBootstrapSubsystem>())
				{
					BaseVisualRelativeRotation = Bootstrap->GetCameraFacingSpritePlaneRotation();
					HeroVisualMesh->SetRelativeRotation(BaseVisualRelativeRotation);

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
		BaseVisualRelativeRotation = FRotator(60.0f, 180.0f, 0.0f);
		HeroVisualMesh->SetRelativeRotation(BaseVisualRelativeRotation);
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

	if (!IsRunningCommandlet() && GetWorld() && GetWorld()->IsGameWorld() && !GetWorld()->GetAuthGameMode<ADungeoneerGameMode>())
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
	DOREPLIFETIME(ADungeoneerCharacter, KillsCount);
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

		if (AttackAction)
		{
			EnhancedInput->BindAction(AttackAction, ETriggerEvent::Started, this, &ADungeoneerCharacter::HandleAttackInput);
		}

		if (DodgeAction)
		{
			EnhancedInput->BindAction(DodgeAction, ETriggerEvent::Started, this, &ADungeoneerCharacter::HandleDodgeInput);
		}

		if (InteractAction)
		{
			EnhancedInput->BindAction(InteractAction, ETriggerEvent::Started, this, &ADungeoneerCharacter::HandleInteractInput);
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

void ADungeoneerCharacter::HandleAttackInput(const FInputActionValue& Value)
{
	TriggerPrimaryAttack();
}

void ADungeoneerCharacter::HandleDodgeInput(const FInputActionValue& Value)
{
	TriggerDodgeDash();
}

void ADungeoneerCharacter::HandleInteractInput(const FInputActionValue& Value)
{
	TriggerInteract();
}

bool ADungeoneerCharacter::TriggerPrimaryAttack()
{
	if (DownedRespawnRemaining > 0.0f ||
		HitStaggerRemaining > 0.0f ||
		AttackCooldownRemaining > 0.0f ||
		CombatActionState == EDungeoneerCombatActionState::LanternSlash)
	{
		return false;
	}

	const UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement();
	ActiveAttackOctant = ((IsLocallyControlled() || !GetController()) && MoveComp)
		? MoveComp->GetAimOctant()
		: ReplicatedAimOctant;

	CombatActionState = EDungeoneerCombatActionState::LanternSlash;
	AttackSwingElapsed = 0.0f;
	AttackCooldownRemaining = PrimaryAttackCooldown;
	bAttackDamageApplied = false;
	LastAttackHitsCount = 0;

	if (HasAuthority())
	{
		ExecutePrimaryAttackAuthority();
	}
	else
	{
		Server_ExecutePrimaryAttack();
	}

	return true;
}

bool ADungeoneerCharacter::TriggerDodgeDash()
{
	if (DownedRespawnRemaining > 0.0f ||
		HitStaggerRemaining > 0.0f ||
		DodgeCooldownRemaining > 0.0f ||
		DodgeActiveRemaining > 0.0f)
	{
		return false;
	}

	const UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement();
	const ECompassOctant DashOctant = (MoveComp && MoveComp->HasActiveMoveInput())
		? MoveComp->GetMoveOctant()
		: ((IsLocallyControlled() && MoveComp) ? MoveComp->GetAimOctant() : ReplicatedAimOctant);

	DashDirectionXY = UDungeoneerMovementComponent::GetUnitDirectionForOctant(DashOctant).GetSafeNormal2D();
	if (DashDirectionXY.IsNearlyZero())
	{
		DashDirectionXY = FVector(1.0, 0.0, 0.0);
	}

	CombatActionState = EDungeoneerCombatActionState::DodgeDash;
	DodgeActiveRemaining = 0.22f;
	DodgeCooldownRemaining = DodgeCooldown;

	if (HasAuthority())
	{
		ExecuteDodgeDashAuthority();
	}
	else
	{
		Server_ExecuteDodgeDash();
	}

	return true;
}

bool ADungeoneerCharacter::TriggerInteract()
{
	if (DownedRespawnRemaining > 0.0f)
	{
		return false;
	}

	if (HasAuthority())
	{
		ExecuteInteractAuthority();
	}
	else
	{
		Server_ExecuteInteract();
	}
	return true;
}

void ADungeoneerCharacter::RegisterEnemySlain(int32 BonusGold, float BonusBattery) noexcept
{
	if (!HasAuthority())
	{
		return;
	}

	++KillsCount;
	if (InventoryComponent && BonusGold > 0)
	{
		InventoryComponent->AuthorityAdjustGold(BonusGold);
	}
	if (BonusBattery > 0.0f)
	{
		FlashlightBattery = FMath::Clamp(FlashlightBattery + BonusBattery, 0.0f, 100.0f);
	}
}

void ADungeoneerCharacter::ExecutePrimaryAttackAuthority()
{
	UWorld* World = GetWorld();
	if (!World || bAttackDamageApplied)
	{
		return;
	}

	bAttackDamageApplied = true;
	const FVector ForwardXY = UDungeoneerMovementComponent::GetUnitDirectionForOctant(ActiveAttackOctant).GetSafeNormal2D();
	const FVector Origin = GetActorLocation();

	if (UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
	{
		// Wide 130-degree frontal cleave cone (cos(65 deg) ~= 0.4226)
		LastAttackHitsCount = SpatialSys->ApplyMeleeArcDamage(
			Origin,
			ForwardXY,
			PrimaryAttackRangeUU,
			0.42f,
			PrimaryAttackDamage,
			this,
			380.0f);
	}

	if (const ADungeoneerGameMode* GM = World->GetAuthGameMode<ADungeoneerGameMode>())
	{
		if (ADungeoneerTileMapInstancer* Instancer = GM->GetActiveTileMapInstancer())
		{
			Instancer->EvaluateEntityStepOnBreakables(Origin + ForwardXY * 95.0, 1.35f, 120.0f);
		}
	}

	if (UDungeoneerSensoryWorldSubsystem* SensorySub = World->GetSubsystem<UDungeoneerSensoryWorldSubsystem>())
	{
		SensorySub->BroadcastNoiseEvent(Origin, LastAttackHitsCount > 0 ? 0.90f : 0.55f, 650.0f, false);
	}

	OnCombatAttackPerformed.Broadcast(ActiveAttackOctant, LastAttackHitsCount);
}

void ADungeoneerCharacter::ExecuteDodgeDashAuthority()
{
	if (UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement())
	{
		MoveComp->Velocity = FVector(
			DashDirectionXY.X * static_cast<double>(DodgeDashSpeedUU),
			DashDirectionXY.Y * static_cast<double>(DodgeDashSpeedUU),
			MoveComp->Velocity.Z);
	}

	if (UWorld* World = GetWorld())
	{
		if (UDungeoneerSensoryWorldSubsystem* SensorySub = World->GetSubsystem<UDungeoneerSensoryWorldSubsystem>())
		{
			SensorySub->BroadcastNoiseEvent(GetActorLocation(), 0.45f, 450.0f, false);
		}
	}
}

void ADungeoneerCharacter::ExecuteInteractAuthority()
{
	UWorld* World = GetWorld();
	ADungeoneerGameMode* GM = World ? World->GetAuthGameMode<ADungeoneerGameMode>() : nullptr;
	ADungeoneerTileMapInstancer* Instancer = GM ? GM->GetActiveTileMapInstancer() : nullptr;
	if (!World || !GM || !Instancer)
	{
		return;
	}

	const FVector MyLoc = GetActorLocation();
	constexpr double MaxInteractDistSq = 380.0 * 380.0;
	AMajorLootVaultActor* NearestVault = nullptr;
	double BestVaultDistSq = MaxInteractDistSq;
	AVendorActor* NearestVendor = nullptr;
	double BestVendorDistSq = MaxInteractDistSq;

	const int32 EncounterCount = Instancer->GetSpawnedEncounterCount();
	for (int32 i = 0; i < EncounterCount; ++i)
	{
		AActor* Encounter = Instancer->GetSpawnedEncounterAt(i);
		if (!Encounter)
		{
			continue;
		}
		const double DistSq2D = FVector::DistSquared2D(MyLoc, Encounter->GetActorLocation());
		if (DistSq2D > MaxInteractDistSq)
		{
			continue;
		}

		if (AMajorLootVaultActor* Vault = Cast<AMajorLootVaultActor>(Encounter))
		{
			if (DistSq2D < BestVaultDistSq)
			{
				BestVaultDistSq = DistSq2D;
				NearestVault = Vault;
			}
		}
		else if (AVendorActor* Vendor = Cast<AVendorActor>(Encounter))
		{
			if (DistSq2D < BestVendorDistSq)
			{
				BestVendorDistSq = DistSq2D;
				NearestVendor = Vendor;
			}
		}
	}

	if (NearestVault)
	{
		if (!NearestVault->IsVaultUnlocked())
		{
			int32 FloorSeed = 424242 + GM->GetCurrentFloorIndex() * 97;
			if (const ADungeoneerGameState* GS = World->GetGameState<ADungeoneerGameState>())
			{
				FloorSeed = static_cast<int32>(GS->GetReplicatedFloorSeed().MasterSeed) + GM->GetCurrentFloorIndex();
			}
			NearestVault->UnlockVaultWithSeed(FloorSeed);
			if (InventoryComponent)
			{
				InventoryComponent->AuthorityAdjustGold(120);
				const TArray<FVendorCatalogItem>& Loot = NearestVault->GetVaultLootTable();
				if (Loot.Num() > 0)
				{
					InventoryComponent->AuthorityAddItem(Loot[0].ItemGuid, Loot[0].ItemId, 1);
				}
			}
			RestoreAtRestingGround(25.0f, 30.0f);
			RefreshNearbyInteractCandidate();
		}
		else
		{
			GM->AdvanceToNextDungeonFloor();
			NearbyInteractType = EDungeoneerNearbyInteract::None;
		}
		return;
	}

	if (NearestVendor && InventoryComponent)
	{
		const TArray<FVendorCatalogItem>& Catalog = NearestVendor->GetVendorCatalog();
		for (const FVendorCatalogItem& Entry : Catalog)
		{
			if (Entry.StockCount > 0 && InventoryComponent->GetGoldBalance() >= Entry.GoldPrice)
			{
				if (NearestVendor->ConsumeCatalogStock(Entry.ItemGuid))
				{
					InventoryComponent->AuthorityAdjustGold(-Entry.GoldPrice);
					InventoryComponent->AuthorityAddItem(Entry.ItemGuid, Entry.ItemId, 1);

					if (SensoryPerkComponent)
					{
						if (Entry.ItemId == FName(TEXT("Item_Lens_DarkVision")))
						{
							SensoryPerkComponent->UpgradePerk(EDungeoneerSensoryPerk::DarkVision);
						}
						else if (Entry.ItemId == FName(TEXT("Item_Resonator_AcuteHearing")))
						{
							SensoryPerkComponent->UpgradePerk(EDungeoneerSensoryPerk::AcuteHearing);
						}
						else if (Entry.ItemId == FName(TEXT("Item_Vial_ScentTracer")))
						{
							SensoryPerkComponent->UpgradePerk(EDungeoneerSensoryPerk::ScentThermal);
						}
					}
					RestoreAtRestingGround(25.0f, 25.0f);
					break;
				}
			}
		}
	}
}

void ADungeoneerCharacter::Server_ExecutePrimaryAttack_Implementation()
{
	if (CombatActionState != EDungeoneerCombatActionState::LanternSlash)
	{
		const UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement();
		ActiveAttackOctant = MoveComp ? MoveComp->GetAimOctant() : ReplicatedAimOctant;
		CombatActionState = EDungeoneerCombatActionState::LanternSlash;
		AttackSwingElapsed = 0.0f;
		AttackCooldownRemaining = PrimaryAttackCooldown;
		bAttackDamageApplied = false;
	}
	ExecutePrimaryAttackAuthority();
}

void ADungeoneerCharacter::Server_ExecuteDodgeDash_Implementation()
{
	DodgeActiveRemaining = 0.22f;
	DodgeCooldownRemaining = DodgeCooldown;
	CombatActionState = EDungeoneerCombatActionState::DodgeDash;
	ExecuteDodgeDashAuthority();
}

void ADungeoneerCharacter::Server_ExecuteInteract_Implementation()
{
	ExecuteInteractAuthority();
}

void ADungeoneerCharacter::UpdateCombatAndProceduralPose(float DeltaSeconds) noexcept
{
	AttackCooldownRemaining = FMath::Max(0.0f, AttackCooldownRemaining - DeltaSeconds);
	DodgeCooldownRemaining = FMath::Max(0.0f, DodgeCooldownRemaining - DeltaSeconds);
	HitStaggerRemaining = FMath::Max(0.0f, HitStaggerRemaining - DeltaSeconds);

	if (DownedRespawnRemaining > 0.0f)
	{
		DownedRespawnRemaining = FMath::Max(0.0f, DownedRespawnRemaining - DeltaSeconds);
		CombatActionState = EDungeoneerCombatActionState::Downed;
		if (DownedRespawnRemaining <= 0.0f && HasAuthority())
		{
			CurrentHealth = MaxHealth * 0.65f;
			FlashlightBattery = FMath::Max(FlashlightBattery, 50.0f);
			CombatActionState = EDungeoneerCombatActionState::None;
			if (const UWorld* World = GetWorld())
			{
				if (const ADungeoneerGameMode* GM = World->GetAuthGameMode<ADungeoneerGameMode>())
				{
					SetActorLocation(GM->GetEntrySpawnLocation() + FVector(0.0, 0.0, 95.0), false, nullptr, ETeleportType::TeleportPhysics);
				}
			}
		}
	}

	if (CombatActionState == EDungeoneerCombatActionState::LanternSlash)
	{
		constexpr float SlashDuration = 0.22f;
		AttackSwingElapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(AttackSwingElapsed / SlashDuration, 0.0f, 1.0f);
		const FVector ForwardXY = UDungeoneerMovementComponent::GetUnitDirectionForOctant(ActiveAttackOctant).GetSafeNormal2D();
		const float AttackYaw = DungeoneerMath::GCompassYawDegrees[static_cast<uint8>(ActiveAttackOctant) & 7];

		ActiveWalkFrameColumn = (Alpha < 0.42f) ? 1 : 3;

		if (HeroVisualMesh)
		{
			const float LungeDist = FMath::Sin(Alpha * PI) * 24.0f;
			const FVector Offset(ForwardXY.X * LungeDist, ForwardXY.Y * LungeDist, FMath::Sin(Alpha * PI) * 10.0f);
			HeroVisualMesh->SetRelativeLocation(BaseVisualRelativeLocation + Offset);
			HeroVisualMesh->SetRelativeScale3D(BaseVisualRelativeScale * FVector(1.06, 0.94, 1.08));
		}

		if (SlashArcMesh)
		{
			const float SweepYaw = AttackYaw + FMath::Lerp(-55.0f, 55.0f, Alpha);
			const FVector ArcOffset = ForwardXY * (84.0f + 16.0f * Alpha);
			SlashArcMesh->SetRelativeLocation(FVector(ArcOffset.X, ArcOffset.Y, 14.0));
			SlashArcMesh->SetRelativeRotation(FRotator(0.0f, SweepYaw, 0.0f));
			const float ArcScale = FMath::Sin(Alpha * PI) * 1.35f + 0.35f;
			SlashArcMesh->SetRelativeScale3D(FVector(1.45 * ArcScale, 0.92 * ArcScale, 1.0));
			SlashArcMesh->SetVisibility(true);
		}

		if (FlashlightComponent)
		{
			FlashlightComponent->SetIntensity(26000.0f + 12000.0f * (1.0f - Alpha));
		}
		if (HeroLanternFillLight)
		{
			HeroLanternFillLight->SetIntensity(4800.0f + 5600.0f * (1.0f - Alpha));
		}

		if (AttackSwingElapsed >= SlashDuration)
		{
			CombatActionState = EDungeoneerCombatActionState::None;
		}
		return;
	}

	if (SlashArcMesh)
	{
		SlashArcMesh->SetVisibility(false);
	}
	if (FlashlightComponent)
	{
		FlashlightComponent->SetIntensity(26000.0f);
	}
	if (HeroLanternFillLight)
	{
		HeroLanternFillLight->SetIntensity(4800.0f);
	}

	if (CombatActionState == EDungeoneerCombatActionState::DodgeDash)
	{
		DodgeActiveRemaining = FMath::Max(0.0f, DodgeActiveRemaining - DeltaSeconds);
		ActiveWalkFrameColumn = (DodgeActiveRemaining > 0.11f) ? 1 : 3;

		if (UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement())
		{
			MoveComp->Velocity = FVector(
				DashDirectionXY.X * static_cast<double>(DodgeDashSpeedUU),
				DashDirectionXY.Y * static_cast<double>(DodgeDashSpeedUU),
				MoveComp->Velocity.Z);
		}

		if (HeroVisualMesh)
		{
			const float HopZ = (DodgeActiveRemaining > 0.0f) ? 14.0f : 0.0f;
			HeroVisualMesh->SetRelativeLocation(BaseVisualRelativeLocation + FVector(0.0, 0.0, HopZ));
			HeroVisualMesh->SetRelativeScale3D(BaseVisualRelativeScale * FVector(1.12, 0.86, 0.92));
		}

		if (DodgeActiveRemaining <= 0.0f)
		{
			CombatActionState = EDungeoneerCombatActionState::None;
		}
		return;
	}

	if (CombatActionState == EDungeoneerCombatActionState::HitStagger)
	{
		if (HeroVisualMesh)
		{
			const float JitterX = FMath::Sin(HitStaggerRemaining * 90.0f) * 6.0f;
			HeroVisualMesh->SetRelativeLocation(BaseVisualRelativeLocation + FVector(JitterX, 0.0, 0.0));
			HeroVisualMesh->SetRelativeScale3D(BaseVisualRelativeScale * FVector(0.92, 1.08, 0.90));
		}
		if (HitStaggerRemaining <= 0.0f)
		{
			CombatActionState = EDungeoneerCombatActionState::None;
		}
		return;
	}

	if (CombatActionState == EDungeoneerCombatActionState::Downed)
	{
		if (HeroVisualMesh)
		{
			HeroVisualMesh->SetRelativeLocation(BaseVisualRelativeLocation + FVector(0.0, 0.0, -6.0));
			HeroVisualMesh->SetRelativeScale3D(BaseVisualRelativeScale * FVector(1.10, 1.10, 0.35));
		}
		return;
	}

	if (HeroVisualMesh)
	{
		HeroVisualMesh->SetRelativeLocation(BaseVisualRelativeLocation);
		HeroVisualMesh->SetRelativeRotation(BaseVisualRelativeRotation);
		HeroVisualMesh->SetRelativeScale3D(BaseVisualRelativeScale);
	}
}

void ADungeoneerCharacter::RefreshNearbyInteractCandidate() noexcept
{
	NearbyInteractType = EDungeoneerNearbyInteract::None;
	const UWorld* World = GetWorld();
	const ADungeoneerGameMode* GM = World ? World->GetAuthGameMode<ADungeoneerGameMode>() : nullptr;
	const ADungeoneerTileMapInstancer* Instancer = GM ? GM->GetActiveTileMapInstancer() : nullptr;
	if (!Instancer)
	{
		return;
	}

	const FVector MyLoc = GetActorLocation();
	constexpr double MaxInteractDistSq = 380.0 * 380.0;
	const int32 EncounterCount = Instancer->GetSpawnedEncounterCount();
	double BestDistSq = MaxInteractDistSq;

	for (int32 i = 0; i < EncounterCount; ++i)
	{
		const AActor* Encounter = Instancer->GetSpawnedEncounterAt(i);
		if (!Encounter)
		{
			continue;
		}
		const double DistSq2D = FVector::DistSquared2D(MyLoc, Encounter->GetActorLocation());
		if (DistSq2D > BestDistSq)
		{
			continue;
		}

		if (const AMajorLootVaultActor* Vault = Cast<AMajorLootVaultActor>(Encounter))
		{
			BestDistSq = DistSq2D;
			NearbyInteractType = Vault->IsVaultUnlocked()
				? EDungeoneerNearbyInteract::DescendNextFloor
				: EDungeoneerNearbyInteract::UnlockVault;
		}
		else if (Encounter->IsA<AVendorActor>())
		{
			BestDistSq = DistSq2D;
			NearbyInteractType = EDungeoneerNearbyInteract::VendorTrade;
		}
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

	if (DownedRespawnRemaining > 0.0f)
	{
		MoveComp->SetMoveInputVector(FVector2D::ZeroVector);
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

	const ADungeoneerGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADungeoneerGameMode>() : nullptr;
	const bool bMainMenuOpen = GM && GM->GetActiveDioramaStage() && !GM->GetActiveDioramaStage()->IsHidden();

	const bool bAttackKeyDown = !bMainMenuOpen &&
		(PC->IsInputKeyDown(EKeys::LeftMouseButton) || PC->IsInputKeyDown(EKeys::SpaceBar) || PC->IsInputKeyDown(EKeys::J));
	if (bAttackKeyDown && !bPrevAttackKeyDown)
	{
		TriggerPrimaryAttack();
	}
	bPrevAttackKeyDown = bAttackKeyDown;

	const bool bDodgeKeyDown = !bMainMenuOpen &&
		(PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift) || PC->IsInputKeyDown(EKeys::K) || PC->IsInputKeyDown(EKeys::RightMouseButton));
	if (bDodgeKeyDown && !bPrevDodgeKeyDown)
	{
		TriggerDodgeDash();
	}
	bPrevDodgeKeyDown = bDodgeKeyDown;

	const bool bInteractKeyDown = !bMainMenuOpen &&
		(PC->IsInputKeyDown(EKeys::F) || PC->IsInputKeyDown(EKeys::E));
	if (bInteractKeyDown && !bPrevInteractKeyDown)
	{
		TriggerInteract();
	}
	bPrevInteractKeyDown = bInteractKeyDown;
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
	const ECompassOctant ActiveFacingOctant = ((IsLocallyControlled() || !GetController()) && MoveComp)
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

	UpdateCombatAndProceduralPose(DeltaSeconds);

	InteractScanAccumulator += DeltaSeconds;
	if (InteractScanAccumulator >= 0.10f)
	{
		InteractScanAccumulator = 0.0f;
		RefreshNearbyInteractCandidate();
	}

	UpdateHeroDirectionalVisual(ActiveFacingOctant, ActiveWalkFrameColumn);

	if (DirectionalRigComponent)
	{
		DirectionalRigComponent->SetHeadingOctant(ActiveFacingOctant);
		DirectionalRigComponent->SetFrameIndex(ActiveWalkFrameColumn);
		if (MoveComp)
		{
			DirectionalRigComponent->SetAnimationPlaying(bHasMoveInput || CombatActionState != EDungeoneerCombatActionState::None);
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
	const ECompassOctant TargetOctant = ((IsLocallyControlled() || !GetController()) && MoveComp)
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
	if (!HasAuthority() || DamageAmount <= 0.0f || IsInvulnerable())
	{
		return;
	}

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);
	if (CurrentHealth <= 0.0f)
	{
		CombatActionState = EDungeoneerCombatActionState::Downed;
		DownedRespawnRemaining = 1.6f;
		return;
	}

	if (CombatActionState == EDungeoneerCombatActionState::None)
	{
		CombatActionState = EDungeoneerCombatActionState::HitStagger;
	}
	HitStaggerRemaining = 0.16f;

	if (DamageCauser)
	{
		const FVector PushDirXY = (GetActorLocation() - DamageCauser->GetActorLocation()).GetSafeNormal2D();
		if (UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement(); MoveComp && !PushDirXY.IsNearlyZero())
		{
			MoveComp->Velocity += FVector(PushDirXY.X * 180.0, PushDirXY.Y * 180.0, 0.0);
		}
	}
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
