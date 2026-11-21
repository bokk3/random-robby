#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "Core/DungeoneerGameMode.h"
#include "Core/DungeoneerGameState.h"
#include "Core/DungeoneerMovementComponent.h"
#include "Core/DungeoneerMovementConfig.h"
#include "Core/DungeoneerSaveGame.h"
#include "Rendering/SensoryPerkComponent.h"
#include "Rendering/25DDirectionalRigComponent.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "Audio/DungeonAcousticSubsystem.h"
#include "Audio/DungeoneerTimedAudioComponent.h"
#include "Social/DungeoneerInventoryComponent.h"
#include "Social/TradeManagerComponent.h"
#include "Dungeon/DungeonEncounterActors.h"
#include "Dungeon/DungeoneerTileMapInstancer.h"
#include "Dungeon/MirrorLightBounceComponent.h"
#include "Ecosystem/DungeonEcosystemTypes.h"
#include "Ecosystem/DungeonFloraManagerComponent.h"
#include "Ecosystem/DungeonWaterSubsystem.h"
#include "AI/ZombieBase.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Combat/DungeoneerSkillComponent.h"
#include "UI/DungeoneerPlayerHUDWidget.h"
#include "UI/MenuDioramaStageActor.h"
#include "Rendering/DungeoneerFxPoolSubsystem.h"
#include "Rendering/DungeoneerSpriteMaterialFactory.h"
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
	CameraBoom->TargetArmLength = 1320.0f;
	CameraBoom->SetRelativeRotation(FRotator(-42.0f, -45.0f, 0.0f));
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 12.0f;

	TopDownCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCamera->bUsePawnControlRotation = false;
	TopDownCamera->FieldOfView = 35.0f; // Crisp 2.5D isometric diorama framing
	TopDownCamera->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
	TopDownCamera->PostProcessSettings.AutoExposureMinBrightness = 0.42f;
	TopDownCamera->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
	TopDownCamera->PostProcessSettings.AutoExposureMaxBrightness = 1.40f;
	TopDownCamera->PostProcessSettings.bOverride_AutoExposureBias = true;
	TopDownCamera->PostProcessSettings.AutoExposureBias = -0.10f;
	TopDownCamera->PostProcessSettings.bOverride_BloomIntensity = true;
	TopDownCamera->PostProcessSettings.BloomIntensity = 0.35f;

	FlashlightComponent = CreateDefaultSubobject<USpotLightComponent>(TEXT("FlashlightComponent"));
	FlashlightComponent->SetupAttachment(RootComponent);
	FlashlightComponent->SetRelativeLocation(FVector(30.0f, 30.0f, 32.0f));
	FlashlightComponent->SetIntensityUnits(ELightUnits::Lumens);
	FlashlightComponent->SetSourceRadius(22.0f);
	FlashlightComponent->SetSoftSourceRadius(32.0f);
	FlashlightComponent->SetIntensity(650.0f);
	FlashlightComponent->SetLightColor(FLinearColor(1.0f, 0.74f, 0.34f));
	FlashlightComponent->SetInnerConeAngle(8.0f);
	FlashlightComponent->SetOuterConeAngle(22.0f);
	FlashlightComponent->SetAttenuationRadius(920.0f);
	FlashlightComponent->bUseInverseSquaredFalloff = true;
	FlashlightComponent->VolumetricScatteringIntensity = 1.85f;
	FlashlightComponent->CastShadows = true;

	HeroLanternFillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("HeroLanternFillLight"));
	HeroLanternFillLight->SetupAttachment(RootComponent);
	HeroLanternFillLight->SetRelativeLocation(FVector(-35.0f, 35.0f, 75.0f));
	HeroLanternFillLight->SetUseInverseSquaredFalloff(true);
	HeroLanternFillLight->SetIntensityUnits(ELightUnits::Lumens);
	HeroLanternFillLight->SetSourceRadius(32.0f);
	HeroLanternFillLight->SetSoftSourceRadius(48.0f);
	HeroLanternFillLight->SetIntensity(120.0f);
	HeroLanternFillLight->SetAttenuationRadius(680.0f);
	HeroLanternFillLight->SetLightColor(FLinearColor(1.0f, 0.78f, 0.45f));
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
	TradeManagerComponent = CreateDefaultSubobject<UTradeManagerComponent>(TEXT("TradeManagerComponent"));
	MirrorBounceComponent = CreateDefaultSubobject<UMirrorLightBounceComponent>(TEXT("MirrorBounceComponent"));
	SkillComponent = CreateDefaultSubobject<UDungeoneerSkillComponent>(TEXT("SkillComponent"));

	// Effect quads live in world space so they can stay behind when the hero moves.
	auto MakeWorldSpaceQuad = [this](const TCHAR* Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* Quad = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Quad->SetupAttachment(RootComponent);
		Quad->SetUsingAbsoluteLocation(true);
		Quad->SetUsingAbsoluteRotation(true);
		Quad->SetUsingAbsoluteScale(true);
		Quad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Quad->SetCastShadow(false);
		Quad->SetVisibility(false);
		return Quad;
	};
	SkillBeamMesh = MakeWorldSpaceQuad(TEXT("SkillBeamMesh"));
	AfterimageMesh = MakeWorldSpaceQuad(TEXT("AfterimageMesh"));
	DecoyMesh = MakeWorldSpaceQuad(TEXT("DecoyMesh"));

	for (bool& bDown : PrevButtonDown)
	{
		bDown = false;
	}

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
		// The crescent texture comes from the asset bootstrap; bare test worlds without a GameInstance get a flat tint.
		const UGameInstance* SlashGI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		UDungeoneerAssetBootstrapSubsystem* SlashBootstrap = SlashGI ? SlashGI->GetSubsystem<UDungeoneerAssetBootstrapSubsystem>() : nullptr;
		if (!bSlashArcTextured && SlashBootstrap)
		{
			// Additive and unlit, so the swing glows in the dark instead of reading as a dark smear.
			if (UMaterialInstanceDynamic* ArcMID = SlashBootstrap->CreateFxMaterial(
				this, FName(TEXT("SlashArc_D")), FLinearColor(1.0f, 0.78f, 0.36f), 2.6f))
			{
				SlashArcMesh->SetMaterial(0, ArcMID);
				bSlashArcTextured = true;
				// The skill beam (hook chain, flask arc) is an additive streak too.
				if (SkillBeamMesh)
				{
					if (UMaterialInstanceDynamic* BeamFx = SlashBootstrap->CreateFxMaterial(this, FName(TEXT("FxSpark_D")), FLinearColor::White, 2.2f))
					{
						SkillBeamMesh->SetMaterial(0, BeamFx);
					}
				}
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

	UStaticMesh* QuadMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	for (UStaticMeshComponent* Quad : { SkillBeamMesh.Get(), AfterimageMesh.Get(), DecoyMesh.Get() })
	{
		if (Quad && !Quad->GetStaticMesh() && QuadMesh)
		{
			Quad->SetStaticMesh(QuadMesh);
		}
	}
	if (SkillBeamMesh && !SkillBeamMesh->GetMaterial(0))
	{
		if (UMaterialInterface* BaseShapeMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
		{
			if (UMaterialInstanceDynamic* BeamMID = UMaterialInstanceDynamic::Create(BaseShapeMat, this))
			{
				BeamMID->SetScalarParameterValue(FName(TEXT("Roughness")), 0.2f);
				SkillBeamMesh->SetMaterial(0, BeamMID);
			}
		}
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

					// One material per cell: 16 for the walk sheet, then 16 per move sheet (nullptr where a sheet is missing).
					static const TCHAR* const SheetPrefixes[static_cast<int32>(EHeroPoseSheet::Count)] =
						{ TEXT("Hero"), TEXT("HeroCombo"), TEXT("HeroHeavy"), TEXT("HeroEvade"), TEXT("HeroSkills"), TEXT("HeroHurtIdle") };
					DirectionalHeroMIDs.Reset();
					DirectionalHeroMIDs.SetNum(16 * static_cast<int32>(EHeroPoseSheet::Count));
					for (int32 SheetIdx = 0; SheetIdx < static_cast<int32>(EHeroPoseSheet::Count); ++SheetIdx)
					{
						for (int32 RowIdx = 0; RowIdx < 4; ++RowIdx)
						{
							for (int32 ColIdx = 0; ColIdx < 4; ++ColIdx)
							{
								const FName DiffKey(*FString::Printf(TEXT("%s_R%d_C%d_D"), SheetPrefixes[SheetIdx], RowIdx, ColIdx));
								const FName NormKey(*FString::Printf(TEXT("%s_R%d_C%d_N"), SheetPrefixes[SheetIdx], RowIdx, ColIdx));
								if (SheetIdx > 0 && !Bootstrap->GetExtractedCellTexture(DiffKey))
								{
									continue; // Move sheet not on disk: leave the slot empty so the walk frame is used.
								}
								DirectionalHeroMIDs[SheetIdx * 16 + RowIdx * 4 + ColIdx] = Bootstrap->CreateDungeonOrRigMaterial(
									this, DiffKey, NormKey, FLinearColor::White, 0.45f, false, true);
							}
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
	const int32 PrevColumn = ActiveWalkFrameColumn;
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

	if (ActiveWalkFrameColumn != PrevColumn)
	{
		if (ActiveWalkFrameColumn == 1 || ActiveWalkFrameColumn == 3)
		{
			++WalkStepSequenceCount;
			if (const UWorld* World = GetWorld())
			{
				if (UDungeonWaterSubsystem* WaterSub = World->GetSubsystem<UDungeonWaterSubsystem>())
				{
					const UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement();
					const ECompassOctant MoveOct = MoveComp ? MoveComp->GetMoveOctant() : ReplicatedMoveOctant;
					WaterSub->NotifyWalkerStep(this, GetActorLocation(), MoveOct, true);
				}
			}
		}
		OnWalkColumnChanged.Broadcast(ActiveWalkFrameColumn);
	}
}

void ADungeoneerCharacter::UpdateHeroDirectionalVisual(ECompassOctant FacingOctant, int32 FrameColumn) noexcept
{
	if (!HeroVisualMesh)
	{
		return;
	}

	const int32 DesiredRow = static_cast<int32>(DungeoneerMath::OctantToScreenDirectionalRowForCameraYaw(FacingOctant, -45.0f));
	const int32 WalkCol = FMath::Clamp((FrameColumn >= 0) ? FrameColumn : ActiveWalkFrameColumn, 0, 3);

	// A move sheet frame if this action has one and the sheet is loaded, otherwise the walk frame.
	int32 DesiredCell = DesiredRow * 4 + WalkCol;
	if (ActivePoseSheet != EHeroPoseSheet::Walk)
	{
		const int32 PoseCell = static_cast<int32>(ActivePoseSheet) * 16 + DesiredRow * 4 + FMath::Clamp(ActivePoseColumn, 0, 3);
		if (DirectionalHeroMIDs.IsValidIndex(PoseCell) && DirectionalHeroMIDs[PoseCell])
		{
			DesiredCell = PoseCell;
		}
	}

	if (DesiredCell == ActiveDirectionalCellIndex)
	{
		return;
	}

	if (DirectionalHeroMIDs.IsValidIndex(DesiredCell) && DirectionalHeroMIDs[DesiredCell])
	{
		HeroVisualMesh->SetMaterial(0, DirectionalHeroMIDs[DesiredCell].Get());
		ActiveDirectionalRow = DesiredRow;
		if (DesiredCell < 16)
		{
			ActiveWalkFrameColumn = WalkCol;
		}
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
		if (UDungeonAcousticSubsystem* AcousticSys = World->GetSubsystem<UDungeonAcousticSubsystem>())
		{
			AcousticSys->RegisterWalker(this);
		}
	}

	BindLocalInputContext();

	if (!IsRunningCommandlet())
	{
		if (const UDungeoneerSaveGame* Ledger = UDungeoneerSaveGame::LoadOrCreateMetaLedger())
		{
			ApplyLanternTrimTint(Ledger->EquippedLanternTrimIndex);
		}
	}

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
	const FVector2D RawMoveVec = Value.Get<FVector2D>();
	const FVector2D MoveVec = DungeoneerMath::RotateScreenInputToWorldForCameraYaw(RawMoveVec, -45.0f);
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
	const FVector2D RawAimVec = Value.Get<FVector2D>();
	const FVector2D AimVec = DungeoneerMath::RotateScreenInputToWorldForCameraYaw(RawAimVec, -45.0f);
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
	if (DownedRespawnRemaining > 0.0f || HitStaggerRemaining > 0.0f)
	{
		return false;
	}

	// Attacking out of a roll turns into a piercing lunge.
	if (CombatActionState == EDungeoneerCombatActionState::DodgeDash || DashStrikeWindowRemaining > 0.0f)
	{
		return TriggerDashStrike();
	}

	// Pressed mid-swing or during recovery: remember it and chain as soon as the swing allows.
	const bool bBusyButBufferable = CombatActionState == EDungeoneerCombatActionState::LanternSlash
		|| CombatActionState == EDungeoneerCombatActionState::Parry
		|| CombatActionState == EDungeoneerCombatActionState::SkillCast
		|| (CombatActionState == EDungeoneerCombatActionState::None && AttackCooldownRemaining > 0.0f);
	if (bBusyButBufferable)
	{
		AttackBufferRemaining = 0.30f;
		return true;
	}
	if (CombatActionState != EDungeoneerCombatActionState::None)
	{
		return false;
	}
	AttackBufferRemaining = 0.0f;

	ActiveAttackOctant = GetEffectiveAimOctant();

	ComboStepIndex = (ComboResetRemaining > 0.0f) ? (ComboStepIndex + 1) % ComboLength : 0;
	ComboResetRemaining = 0.65f;

	const bool bFinisher = (ComboStepIndex == ComboLength - 1);
	CombatActionState = EDungeoneerCombatActionState::LanternSlash;
	{
		static const FName SwingCues[3] = { FName(TEXT("Combat_Swing_0")), FName(TEXT("Combat_Swing_1")), FName(TEXT("Combat_Swing_2")) };
		UDungeonAcousticSubsystem::PlayCombatCueAt(GetWorld(), SwingCues[ComboStepIndex % 3], GetActorLocation(), bFinisher ? 1.0f : 0.75f, bFinisher ? 0.02f : 0.08f);
	}
	AttackSwingElapsed = 0.0f;
	AttackCooldownRemaining = bFinisher ? (PrimaryAttackCooldown * 1.25f) : (PrimaryAttackCooldown * 0.78f);
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

bool ADungeoneerCharacter::TriggerChargedFlare()
{
	if (DownedRespawnRemaining > 0.0f ||
		HitStaggerRemaining > 0.0f ||
		FlareFlashRemaining > 0.0f ||
		FlashlightBattery < 25.0f)
	{
		return false;
	}

	FlashlightBattery = FMath::Max(0.0f, FlashlightBattery - 25.0f);
	FlareFlashRemaining = 0.40f;
	CombatActionState = EDungeoneerCombatActionState::ChargedFlare;
	AddCameraTrauma(0.3f);
	if (UDungeoneerFxPoolSubsystem* Fx = GetWorld() ? GetWorld()->GetSubsystem<UDungeoneerFxPoolSubsystem>() : nullptr)
	{
		Fx->SpawnGroundRing(GetActorLocation() - FVector(0.0, 0.0, 85.0), FLinearColor(1.0f, 0.9f, 0.55f), 60.0f, 420.0f, 0.45f);
		Fx->SpawnBurst(GetActorLocation() + FVector(0.0, 0.0, 60.0), FLinearColor(1.0f, 0.95f, 0.75f), 12, 420.0f, 0.3f);
	}

	if (UWorld* World = GetWorld())
	{
		const FVector Origin = GetActorLocation();
		const FVector ForwardXY = UDungeoneerMovementComponent::GetUnitDirectionForOctant(ActiveAttackOctant).GetSafeNormal2D();
		const float SolarMult = (InventoryComponent && InventoryComponent->HasRelic(EDungeoneerRelicId::SolarFlashlightCore)) ? 1.25f : 1.0f;

		if (UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
		{
			SpatialSys->ApplyMeleeArcDamage(Origin, ForwardXY, 420.0f, -1.0f, 45.0f * SolarMult, this, 480.0f, 1.2f);
		}

		if (const ADungeoneerGameMode* GM = World->GetAuthGameMode<ADungeoneerGameMode>())
		{
			if (ADungeoneerTileMapInstancer* Instancer = GM->GetActiveTileMapInstancer())
			{
				Instancer->EvaluateEntityStepOnBreakables(Origin, 2.0f, 380.0f);
				if (UDungeonFloraManagerComponent* Flora = Instancer->GetFloraManagerComponent())
				{
					Flora->IgnitePrismLiliesInRadius(Origin, 680.0f, 15.0f);
				}
			}
		}

		if (UDungeoneerSensoryWorldSubsystem* SensorySub = World->GetSubsystem<UDungeoneerSensoryWorldSubsystem>())
		{
			SensorySub->BroadcastNoiseEvent(Origin, 2.0f, 1100.0f, false);
		}

		PushFloatingCombatPopup(Origin + FVector(0.0, 0.0, 95.0), TEXT("SOLAR FLARE!"), EDungeoneerFloatingTextKind::Relic);
		TriggerCombatCameraImpulse(-ForwardXY, 20.0f, 0.035f);
	}

	return true;
}

bool ADungeoneerCharacter::TriggerQuickConsumable(EDungeoneerConsumableId ConsumableId)
{
	if (!InventoryComponent || DownedRespawnRemaining > 0.0f)
	{
		return false;
	}

	if (!InventoryComponent->AuthorityConsumeQuickItem(ConsumableId))
	{
		return false;
	}

	const FVector MyLoc = GetActorLocation();
	if (ConsumableId == EDungeoneerConsumableId::LanternOilPhial)
	{
		FlashlightBattery = FMath::Clamp(FlashlightBattery + 45.0f, 0.0f, 100.0f);
		PushFloatingCombatPopup(MyLoc + FVector(0.0, 0.0, 90.0), TEXT("+45% OIL"), EDungeoneerFloatingTextKind::Gold);
		return true;
	}
	if (ConsumableId == EDungeoneerConsumableId::AntidoteDraught)
	{
		CurrentHealth = FMath::Clamp(CurrentHealth + 35.0f, 0.0f, MaxHealth);
		if (UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement())
		{
			MoveComp->SetChronoSlowRatio(0.0f);
		}
		PushFloatingCombatPopup(MyLoc + FVector(0.0, 0.0, 90.0), TEXT("CLEANSED +35"), EDungeoneerFloatingTextKind::Relic);
		return true;
	}
	return false;
}

void ADungeoneerCharacter::PushFloatingCombatPopup(const FVector& WorldLoc, const FString& Text, EDungeoneerFloatingTextKind Kind) noexcept
{
	FDungeoneerFloatingPopup& Slot = FloatingPopups[FloatingPopupCursor % MaxFloatingPopups];
	FloatingPopupCursor = (FloatingPopupCursor + 1) % MaxFloatingPopups;
	Slot.WorldLocation = WorldLoc;
	Slot.Text = Text;
	Slot.Kind = Kind;
	Slot.AgeSeconds = 0.0f;
	Slot.LifetimeSeconds = 0.90f;
	Slot.bActive = true;
}

void ADungeoneerCharacter::TriggerCombatCameraImpulse(const FVector& ImpulseDirXY, const float StrengthUU, const float HitStopSec) noexcept
{
	const FVector Dir = ImpulseDirXY.GetSafeNormal2D();
	CameraImpulseVelocity += FVector(Dir.X * StrengthUU * 12.0, Dir.Y * StrengthUU * 12.0, -StrengthUU * 4.0);
	HitStopRemaining = FMath::Max(HitStopRemaining, HitStopSec);
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
	if (UDungeoneerFxPoolSubsystem* Fx = GetWorld() ? GetWorld()->GetSubsystem<UDungeoneerFxPoolSubsystem>() : nullptr)
	{
		Fx->SpawnDust(GetActorLocation() - FVector(0.0, 0.0, 85.0));
	}
	bPerfectDodgeUsedThisDash = false;
	AttackBufferRemaining = 0.0f;
	bAttackHeld = false;

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

void ADungeoneerCharacter::RegisterEnemySlain(int32 BonusGold, float BonusBattery, uint8 BestiaryMask) noexcept
{
	if (!HasAuthority())
	{
		return;
	}

	++KillsCount;
	if (const UWorld* World = GetWorld())
	{
		if (ADungeoneerGameState* GS = World->GetGameState<ADungeoneerGameState>())
		{
			GS->AuthorityRecordKillInLedger(BestiaryMask);
		}
	}
	if (InventoryComponent && BonusGold > 0)
	{
		InventoryComponent->AuthorityAdjustGold(BonusGold);
		PushFloatingCombatPopup(
			GetActorLocation() + FVector(0.0, 0.0, 86.0),
			FString::Printf(TEXT("+%dg"), BonusGold),
			EDungeoneerFloatingTextKind::Gold);
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

	// Lamplighter's Round: three quick cuts and a heavy finisher that stuns.
	static constexpr float ComboDamageMult[ComboLength] = { 1.0f, 1.1f, 1.25f, 1.8f };
	static constexpr float ComboKnockback[ComboLength]  = { 300.0f, 340.0f, 380.0f, 620.0f };
	static constexpr float ComboStun[ComboLength]       = { 0.0f, 0.0f, 0.0f, 0.5f };
	const int32 Step = FMath::Clamp(ComboStepIndex, 0, ComboLength - 1);
	const bool bFinisher = (Step == ComboLength - 1);

	const float EffectiveDamage = ComputeMeleeDamage(PrimaryAttackDamage * ComboDamageMult[Step], Origin);

	if (UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
	{
		// 130-degree frontal cone (cos 65 deg ~= 0.42).
		LastAttackHitsCount = SpatialSys->ApplyMeleeArcDamage(
			Origin,
			ForwardXY,
			PrimaryAttackRangeUU + (bFinisher ? 40.0f : 0.0f),
			0.42f,
			EffectiveDamage,
			this,
			ComboKnockback[Step],
			ComboStun[Step]);
	}

	if (LastAttackHitsCount > 0)
	{
		TriggerCombatCameraImpulse(ForwardXY, bFinisher ? 24.0f : 12.0f, bFinisher ? 0.06f : 0.035f);
		if (InventoryComponent && InventoryComponent->HasRelic(EDungeoneerRelicId::VampireWick))
		{
			CurrentHealth = FMath::Clamp(CurrentHealth + 6.0f * static_cast<float>(LastAttackHitsCount), 0.0f, MaxHealth);
		}
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
		const bool bEchoBlade = InventoryComponent && InventoryComponent->HasRelic(EDungeoneerRelicId::EchoTunedBlade);
		const float NoiseLoudness = bEchoBlade ? 1.85f : (LastAttackHitsCount > 0 ? 0.90f : 0.55f);
		const float NoiseRadius = bEchoBlade ? 1350.0f : 650.0f;
		SensorySub->BroadcastNoiseEvent(Origin, NoiseLoudness, NoiseRadius, false);
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
	if (!World)
	{
		return;
	}

	if (UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
	{
		if (ADungeoneerCharacter* DownedAlly = SpatialSys->FindNearestDownedAlly(this, 260.0f))
		{
			if (DownedAlly->ReviveByAlly(this))
			{
				RefreshNearbyInteractCandidate();
				return;
			}
		}
	}

	ADungeoneerTileMapInstancer* Instancer = GM ? GM->GetActiveTileMapInstancer() : nullptr;
	if (!GM || !Instancer)
	{
		return;
	}

	const FVector MyLoc = GetActorLocation();
	constexpr double MaxInteractDistSq = 380.0 * 380.0;
	AMajorLootVaultActor* NearestVault = nullptr;
	double BestVaultDistSq = MaxInteractDistSq;
	AVendorActor* NearestVendor = nullptr;
	double BestVendorDistSq = MaxInteractDistSq;
	ARestingGroundActor* NearestSanctuary = nullptr;
	double BestSanctuaryDistSq = MaxInteractDistSq;
	ADungeoneerLootPickupActor* NearestPickup = nullptr;
	double BestPickupDistSq = MaxInteractDistSq;

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

		if (ADungeoneerLootPickupActor* Pickup = Cast<ADungeoneerLootPickupActor>(Encounter))
		{
			if (!Pickup->IsCollected() && DistSq2D < BestPickupDistSq)
			{
				BestPickupDistSq = DistSq2D;
				NearestPickup = Pickup;
			}
		}
		else if (AMajorLootVaultActor* Vault = Cast<AMajorLootVaultActor>(Encounter))
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
		else if (ARestingGroundActor* Sanctuary = Cast<ARestingGroundActor>(Encounter))
		{
			if (DistSq2D < BestSanctuaryDistSq)
			{
				BestSanctuaryDistSq = DistSq2D;
				NearestSanctuary = Sanctuary;
			}
		}
	}

	if (NearestPickup && NearestPickup->CollectByPlayer(this))
	{
		RefreshNearbyInteractCandidate();
		return;
	}

	if (NearestVault)
	{
		if (NearestVault->IsLockedByBossGuardian())
		{
			PushFloatingCombatPopup(MyLoc + FVector(0.0, 0.0, 90.0), TEXT("VAULT SEALED BY COLOSSUS"), EDungeoneerFloatingTextKind::Status);
			return;
		}
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
			PushFloatingCombatPopup(MyLoc + FVector(0.0, 0.0, 96.0), TEXT("VAULT OPENED +120g"), EDungeoneerFloatingTextKind::Gold);
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
					PushFloatingCombatPopup(MyLoc + FVector(0.0, 0.0, 90.0), TEXT("SUPPLY PURCHASED"), EDungeoneerFloatingTextKind::Relic);
					RestoreAtRestingGround(25.0f, 25.0f);
					break;
				}
			}
		}
		return;
	}

	if (NearestSanctuary && NearestSanctuary->TriggerSensoryAltarUpgrade(this, 90))
	{
		RestoreAtRestingGround(30.0f, 35.0f);
		RefreshNearbyInteractCandidate();
	}
}

bool ADungeoneerCharacter::ReviveByAlly(ADungeoneerCharacter* Rescuer)
{
	if (!HasAuthority() || !Rescuer || Rescuer == this || Rescuer->IsDowned() || !IsDowned())
	{
		return false;
	}

	CurrentHealth = MaxHealth * 0.50f;
	FlashlightBattery = FMath::Max(FlashlightBattery, 30.0f);
	DownedRespawnRemaining = 0.0f;
	bWasDownedWithAliveAllies = false;
	CombatActionState = EDungeoneerCombatActionState::None;

	if (UWorld* World = GetWorld())
	{
		if (ADungeoneerGameState* GS = World->GetGameState<ADungeoneerGameState>())
		{
			GS->AuthorityRecordRevivePerformed();
		}
	}

	PushFloatingCombatPopup(GetActorLocation() + FVector(0.0, 0.0, 96.0), TEXT("REVIVED BY ALLY!"), EDungeoneerFloatingTextKind::Status);
	Rescuer->PushFloatingCombatPopup(Rescuer->GetActorLocation() + FVector(0.0, 0.0, 96.0), TEXT("+1 ALLY REVIVE"), EDungeoneerFloatingTextKind::Relic);
	return true;
}

bool ADungeoneerCharacter::TriggerAllyTradeWithNearest()
{
	if (!HasAuthority() || IsDowned() || !InventoryComponent || !TradeManagerComponent)
	{
		return false;
	}

	UWorld* World = GetWorld();
	UZombieSpatialSubsystem* SpatialSys = World ? World->GetSubsystem<UZombieSpatialSubsystem>() : nullptr;
	ADungeoneerCharacter* Partner = SpatialSys ? SpatialSys->FindNearestLivingAlly(this, 340.0f) : nullptr;
	if (!Partner)
	{
		return false;
	}

	UDungeoneerInventoryComponent* PartnerInv = Partner->GetInventoryComponent();
	if (!PartnerInv || PartnerInv == InventoryComponent)
	{
		return false;
	}

	const int32 ShareGold = FMath::Min(25, InventoryComponent->GetGoldBalance());
	FTradeOfferPayload MyOffer;
	MyOffer.OfferedGold = ShareGold;
	const TArray<FDungeoneerInventoryItem>& MyItems = InventoryComponent->GetItems();
	for (const FDungeoneerInventoryItem& Item : MyItems)
	{
		if (!Item.bLockedForTrade && Item.ItemGuid.IsValid())
		{
			InventoryComponent->AuthoritySetItemTradeLock(Item.ItemGuid, true);
			MyOffer.OfferedItemGuids.Add(Item.ItemGuid);
			break;
		}
	}

	FTradeOfferPayload PartnerOffer;
	if (PartnerInv->GetConsumableCount(EDungeoneerConsumableId::LanternOilPhial) > 0)
	{
		PartnerInv->AuthorityConsumeQuickItem(EDungeoneerConsumableId::LanternOilPhial);
		InventoryComponent->AuthorityAddConsumable(EDungeoneerConsumableId::LanternOilPhial, 1);
	}

	if (ShareGold <= 0 && MyOffer.OfferedItemGuids.Num() == 0)
	{
		PushFloatingCombatPopup(GetActorLocation() + FVector(0.0, 0.0, 92.0), TEXT("NO SUPPLIES TO SWAP"), EDungeoneerFloatingTextKind::Status);
		return false;
	}

	FTradeTransaction Tx;
	Tx.TransactionId = FGuid::NewGuid();
	Tx.bIsVendorTrade = false;
	Tx.InitiatorInventory = InventoryComponent;
	Tx.CounterpartyInventory = PartnerInv;
	Tx.InitiatorOffer = MyOffer;
	Tx.CounterpartyOffer = PartnerOffer;
	Tx.bInitiatorConfirmed = true;
	Tx.bCounterpartyConfirmed = true;
	Tx.Phase = ETradePhaseState::OfferLocked;

	const bool bCommitted = TradeManagerComponent->AuthorityExecuteTwoPhaseCommit(Tx);
	if (bCommitted)
	{
		PushFloatingCombatPopup(GetActorLocation() + FVector(0.0, 0.0, 94.0), TEXT("SUPPLIES SHARED"), EDungeoneerFloatingTextKind::Gold);
		Partner->PushFloatingCombatPopup(Partner->GetActorLocation() + FVector(0.0, 0.0, 94.0), TEXT("RECEIVED CO-OP SUPPLY"), EDungeoneerFloatingTextKind::Gold);
	}
	else
	{
		for (const FGuid& Id : MyOffer.OfferedItemGuids)
		{
			InventoryComponent->AuthoritySetItemTradeLock(Id, false);
		}
	}
	return bCommitted;
}

void ADungeoneerCharacter::ApplyLanternTrimTint(int32 TrimIndex) noexcept
{
	EquippedLanternTrimIndex = FMath::Clamp(TrimIndex, 0, UDungeoneerSaveGame::LanternTrimCount - 1);
	const FLinearColor TrimTint = UDungeoneerSaveGame::GetLanternTrimTintColor(EquippedLanternTrimIndex);

	if (FlashlightComponent)
	{
		const FLinearColor SpotTint(
			FMath::Clamp(1.0f * TrimTint.R, 0.45f, 1.0f),
			FMath::Clamp(0.84f * TrimTint.G, 0.45f, 1.0f),
			FMath::Clamp(0.56f * TrimTint.B, 0.28f, 1.0f));
		FlashlightComponent->SetLightColor(SpotTint);
	}
	if (HeroLanternFillLight)
	{
		const FLinearColor FillTint(
			FMath::Clamp(1.0f * TrimTint.R, 0.45f, 1.0f),
			FMath::Clamp(0.86f * TrimTint.G, 0.45f, 1.0f),
			FMath::Clamp(0.62f * TrimTint.B, 0.30f, 1.0f));
		HeroLanternFillLight->SetLightColor(FillTint);
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
	bPerfectDodgeUsedThisDash = false;
	ExecuteDodgeDashAuthority();
}

void ADungeoneerCharacter::Server_ExecuteInteract_Implementation()
{
	ExecuteInteractAuthority();
}

// ---------------------------------------------------------------------------------------------
// Hack-and-slash moves: heavy smash, dash strike, parry, perfect dodge, empowered strikes.
// ---------------------------------------------------------------------------------------------

namespace HeroMoveTuning
{
	constexpr float HeavyMinChargeSec = 0.35f;
	constexpr float HeavyFullChargeSec = 1.0f;
	constexpr float HeavyAutoReleaseSec = 1.4f;
	constexpr float HeavySmashPoseSec = 0.34f;
	constexpr float DashStrikePoseSec = 0.20f;
	constexpr float DashStrikeLungeSec = 0.15f;
	constexpr float DashStrikeSpeedUU = 1050.0f;
	constexpr float DashStrikeWindowSec = 0.18f;
	constexpr float ParryActiveSec = 0.22f;
	constexpr float ParryTotalSec = 0.42f;
	constexpr float ParryCooldownSec = 0.65f;
	constexpr float PerfectDodgeSlowRadius = 650.0f;
}

ECompassOctant ADungeoneerCharacter::GetEffectiveAimOctant() const noexcept
{
	const UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement();
	return ((IsLocallyControlled() || !GetController()) && MoveComp) ? MoveComp->GetAimOctant() : ReplicatedAimOctant;
}

FVector ADungeoneerCharacter::GetAimForwardXY() const noexcept
{
	const FVector Fwd = UDungeoneerMovementComponent::GetUnitDirectionForOctant(GetEffectiveAimOctant()).GetSafeNormal2D();
	return Fwd.IsNearlyZero() ? FVector(1.0, 0.0, 0.0) : Fwd;
}

void ADungeoneerCharacter::FaceTowards(const FVector& WorldLocation) noexcept
{
	const FVector Dir = (WorldLocation - GetActorLocation()).GetSafeNormal2D();
	if (UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement(); MoveComp && !Dir.IsNearlyZero())
	{
		MoveComp->SetAimInputVector(FVector2D(Dir.X, Dir.Y));
		ReplicatedAimOctant = MoveComp->GetAimOctant();
	}
}

bool ADungeoneerCharacter::CanStartAction() const noexcept
{
	return DownedRespawnRemaining <= 0.0f
		&& HitStaggerRemaining <= 0.0f
		&& (CombatActionState == EDungeoneerCombatActionState::None || CombatActionState == EDungeoneerCombatActionState::LanternSlash);
}

bool ADungeoneerCharacter::SpendLanternOil(const float Amount) noexcept
{
	if (Amount <= 0.0f)
	{
		return true;
	}
	if (FlashlightBattery < Amount)
	{
		return false;
	}
	FlashlightBattery -= Amount;
	return true;
}

void ADungeoneerCharacter::GrantEmpoweredStrike(const float Multiplier, const float WindowSec, const TCHAR* Label) noexcept
{
	EmpoweredMultiplier = IsEmpowered() ? FMath::Max(EmpoweredMultiplier, Multiplier) : Multiplier;
	EmpoweredRemaining = FMath::Max(EmpoweredRemaining, WindowSec);
	if (Label)
	{
		PushFloatingCombatPopup(GetActorLocation() + FVector(0.0, 0.0, 104.0), Label, EDungeoneerFloatingTextKind::Status);
	}
}

void ADungeoneerCharacter::GrantInvulnerability(const float Seconds) noexcept
{
	InvulnRemaining = FMath::Max(InvulnRemaining, Seconds);
}

void ADungeoneerCharacter::BeginSkillPose(const EDungeoneerCombatActionState PoseState, const float DurationSec) noexcept
{
	CombatActionState = PoseState;
	SkillPoseDuration = FMath::Max(DurationSec, 0.01f);
	SkillPoseRemaining = SkillPoseDuration;
	AttackBufferRemaining = 0.0f;
}

float ADungeoneerCharacter::GetHeavyChargeAlpha() const noexcept
{
	if (CombatActionState != EDungeoneerCombatActionState::HeavyCharge)
	{
		return 0.0f;
	}
	return FMath::Clamp(HeavyChargeElapsed / HeroMoveTuning::HeavyFullChargeSec, 0.0f, 1.0f);
}

float ADungeoneerCharacter::ComputeMeleeDamage(const float BaseDamage, const FVector& Origin)
{
	float Damage = BaseDamage;
	if (InventoryComponent)
	{
		if (InventoryComponent->HasRelic(EDungeoneerRelicId::SolarFlashlightCore))
		{
			Damage *= 1.20f;
		}
		if (InventoryComponent->HasRelic(EDungeoneerRelicId::SunGlassLens))
		{
			const UWorld* World = GetWorld();
			const ADungeoneerGameMode* GM = World ? World->GetAuthGameMode<ADungeoneerGameMode>() : nullptr;
			const ADungeoneerTileMapInstancer* Instancer = GM ? GM->GetActiveTileMapInstancer() : nullptr;
			const UDungeonFloraManagerComponent* Flora = Instancer ? Instancer->GetFloraManagerComponent() : nullptr;
			if (Flora && Flora->IsPointInBloomRadius(Origin, 280.0f))
			{
				Damage *= 1.35f;
			}
		}
	}
	if (IsEmpowered())
	{
		Damage *= EmpoweredMultiplier;
		EmpoweredRemaining = 0.0f;
		EmpoweredMultiplier = 1.0f;
	}
	return Damage;
}

bool ADungeoneerCharacter::TriggerSkillSlot(const int32 SlotIndex)
{
	return SkillComponent && SkillComponent->TryActivateSlot(SlotIndex);
}

bool ADungeoneerCharacter::BeginHeavyCharge()
{
	if (!CanStartAction() || CombatActionState != EDungeoneerCombatActionState::None || AttackCooldownRemaining > 0.0f)
	{
		return false;
	}
	CombatActionState = EDungeoneerCombatActionState::HeavyCharge;
	HeavyChargeElapsed = 0.0f;
	ActiveAttackOctant = GetEffectiveAimOctant();
	return true;
}

bool ADungeoneerCharacter::ReleaseHeavyCharge()
{
	bAttackHeld = false;
	if (CombatActionState != EDungeoneerCombatActionState::HeavyCharge)
	{
		return false;
	}
	if (HeavyChargeElapsed < HeroMoveTuning::HeavyMinChargeSec)
	{
		CombatActionState = EDungeoneerCombatActionState::None;
		return false;
	}
	const float Alpha = FMath::Clamp(
		(HeavyChargeElapsed - HeroMoveTuning::HeavyMinChargeSec) / (HeroMoveTuning::HeavyFullChargeSec - HeroMoveTuning::HeavyMinChargeSec),
		0.0f, 1.0f);
	StartHeavySmash(Alpha);
	return true;
}

void ADungeoneerCharacter::StartHeavySmash(const float ChargeAlpha)
{
	ActiveAttackOctant = GetEffectiveAimOctant();
	CombatActionState = EDungeoneerCombatActionState::HeavySmash;
	HeavySmashElapsed = 0.0f;
	HeavySmashAlpha = ChargeAlpha;
	AttackCooldownRemaining = 0.55f;
	ComboResetRemaining = 0.0f;
	ComboStepIndex = 0;

	if (HasAuthority())
	{
		ExecuteHeavySmashAuthority(ChargeAlpha);
	}
	else
	{
		Server_ExecuteHeavySmash(ChargeAlpha);
	}
}

void ADungeoneerCharacter::ExecuteHeavySmashAuthority(const float ChargeAlpha)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const float Alpha = FMath::Clamp(ChargeAlpha, 0.0f, 1.0f);
	const FVector Fwd = UDungeoneerMovementComponent::GetUnitDirectionForOctant(ActiveAttackOctant).GetSafeNormal2D();
	const FVector Origin = GetActorLocation();
	const float Damage = ComputeMeleeDamage(PrimaryAttackDamage * FMath::Lerp(1.8f, 2.6f, Alpha), Origin);

	LastAttackHitsCount = 0;
	if (UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
	{
		LastAttackHitsCount = SpatialSys->ApplyMeleeArcDamage(
			Origin + Fwd * 40.0, Fwd, 210.0f + 60.0f * Alpha, 0.2f, Damage, this, 700.0f, 0.8f + 0.6f * Alpha);
	}
	if (const ADungeoneerGameMode* GM = World->GetAuthGameMode<ADungeoneerGameMode>())
	{
		if (ADungeoneerTileMapInstancer* Instancer = GM->GetActiveTileMapInstancer())
		{
			Instancer->EvaluateEntityStepOnBreakables(Origin + Fwd * 110.0, 2.0f, 200.0f);
		}
	}
	if (UDungeoneerSensoryWorldSubsystem* SensorySub = World->GetSubsystem<UDungeoneerSensoryWorldSubsystem>())
	{
		SensorySub->BroadcastNoiseEvent(Origin, 1.6f, 1000.0f, false);
	}
	if (LastAttackHitsCount > 0 && InventoryComponent && InventoryComponent->HasRelic(EDungeoneerRelicId::VampireWick))
	{
		CurrentHealth = FMath::Clamp(CurrentHealth + 6.0f * static_cast<float>(LastAttackHitsCount), 0.0f, MaxHealth);
	}
	TriggerCombatCameraImpulse(Fwd, 26.0f, LastAttackHitsCount > 0 ? 0.07f : 0.02f);
	AddCameraTrauma(0.3f + 0.25f * Alpha);
	UDungeonAcousticSubsystem::PlayCombatCueAt(World, FName(TEXT("Combat_Heavy_0")), Origin + Fwd * 110.0, 1.0f, 0.04f);
	if (UDungeoneerFxPoolSubsystem* Fx = World->GetSubsystem<UDungeoneerFxPoolSubsystem>())
	{
		const FVector Impact = Origin + Fwd * 110.0 - FVector(0.0, 0.0, 85.0);
		Fx->SpawnGroundRing(Impact, FLinearColor(1.0f, 0.72f, 0.28f), 40.0f, 210.0f + 60.0f * Alpha, 0.35f);
		Fx->SpawnDust(Impact + FVector(0.0, 0.0, 85.0));
	}
	OnCombatAttackPerformed.Broadcast(ActiveAttackOctant, LastAttackHitsCount);
}

void ADungeoneerCharacter::Server_ExecuteHeavySmash_Implementation(const float ChargeAlpha)
{
	ActiveAttackOctant = ReplicatedAimOctant;
	CombatActionState = EDungeoneerCombatActionState::HeavySmash;
	HeavySmashElapsed = 0.0f;
	HeavySmashAlpha = FMath::Clamp(ChargeAlpha, 0.0f, 1.0f);
	ExecuteHeavySmashAuthority(HeavySmashAlpha);
}

bool ADungeoneerCharacter::TriggerDashStrike()
{
	if (DownedRespawnRemaining > 0.0f)
	{
		return false;
	}
	ActiveAttackOctant = GetEffectiveAimOctant();
	DashDirectionXY = GetAimForwardXY();
	DodgeActiveRemaining = 0.0f;
	DashStrikeWindowRemaining = 0.0f;
	CombatActionState = EDungeoneerCombatActionState::DashStrike;
	DashStrikeElapsed = 0.0f;
	AttackCooldownRemaining = 0.35f;
	AttackBufferRemaining = 0.0f;
	ShowAfterimage(GetActorLocation(), 0.25f);

	if (HasAuthority())
	{
		ExecuteDashStrikeAuthority();
	}
	else
	{
		Server_ExecuteDashStrike(FVector_NetQuantizeNormal(DashDirectionXY));
	}
	return true;
}

void ADungeoneerCharacter::ExecuteDashStrikeAuthority()
{
	UWorld* World = GetWorld();
	UZombieSpatialSubsystem* SpatialSys = World ? World->GetSubsystem<UZombieSpatialSubsystem>() : nullptr;
	if (!SpatialSys)
	{
		return;
	}
	const FVector Origin = GetActorLocation();
	const float Damage = ComputeMeleeDamage(PrimaryAttackDamage * 1.5f, Origin);
	// Narrow, long cone: everything along the lunge path gets pierced.
	LastAttackHitsCount = SpatialSys->ApplyMeleeArcDamage(Origin, DashDirectionXY, 320.0f, 0.86f, Damage, this, 420.0f, 0.25f);
	if (LastAttackHitsCount > 0)
	{
		TriggerCombatCameraImpulse(DashDirectionXY, 16.0f, 0.04f);
	}
	OnCombatAttackPerformed.Broadcast(ActiveAttackOctant, LastAttackHitsCount);
}

void ADungeoneerCharacter::Server_ExecuteDashStrike_Implementation(FVector_NetQuantizeNormal Direction)
{
	const FVector Dir = FVector(Direction).GetSafeNormal2D();
	DashDirectionXY = Dir.IsNearlyZero() ? GetAimForwardXY() : Dir;
	ActiveAttackOctant = ReplicatedAimOctant;
	DodgeActiveRemaining = 0.0f;
	CombatActionState = EDungeoneerCombatActionState::DashStrike;
	DashStrikeElapsed = 0.0f;
	ExecuteDashStrikeAuthority();
}

bool ADungeoneerCharacter::TriggerParry()
{
	if (!CanStartAction() || ParryCooldownRemaining > 0.0f)
	{
		return false;
	}
	ActiveAttackOctant = GetEffectiveAimOctant();
	CombatActionState = EDungeoneerCombatActionState::Parry;
	ParryActiveRemaining = HeroMoveTuning::ParryActiveSec;
	ParryRecoveryRemaining = HeroMoveTuning::ParryTotalSec;
	ParryCooldownRemaining = HeroMoveTuning::ParryCooldownSec;
	if (!HasAuthority())
	{
		Server_TriggerParry();
	}
	return true;
}

void ADungeoneerCharacter::Server_TriggerParry_Implementation()
{
	if (ParryCooldownRemaining > 0.0f && ParryActiveRemaining <= 0.0f)
	{
		return;
	}
	ActiveAttackOctant = ReplicatedAimOctant;
	CombatActionState = EDungeoneerCombatActionState::Parry;
	ParryActiveRemaining = HeroMoveTuning::ParryActiveSec;
	ParryRecoveryRemaining = HeroMoveTuning::ParryTotalSec;
	ParryCooldownRemaining = HeroMoveTuning::ParryCooldownSec;
}

bool ADungeoneerCharacter::TryDeflectThrownFlask(AActor* Thrower)
{
	if (ParryActiveRemaining <= 0.0f || IsDowned())
	{
		return false;
	}
	++ParryCount;
	ParryCooldownRemaining = 0.0f;
	GrantEmpoweredStrike(2.0f, 1.5f, TEXT("DEFLECTED!"));
	if (Thrower)
	{
		const FVector Dir = (Thrower->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		ShowSkillBeam(GetActorLocation(), Thrower->GetActorLocation(), 0.18f, FLinearColor(1.0f, 0.85f, 0.4f));
		TriggerCombatCameraImpulse(Dir, 14.0f, 0.05f);
	}
	return true;
}

void ADungeoneerCharacter::HandleSuccessfulParry(AZombieBase* Attacker)
{
	++ParryCount;
	ParryCooldownRemaining = 0.0f;
	const FVector Dir = (Attacker->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	Attacker->TriggerHitFeedback(Dir, 520.0f);
	Attacker->ApplyStun(1.4f);
	Attacker->ApplyZombieDamage(15.0f, this);
	GrantEmpoweredStrike(2.0f, 1.5f, TEXT("PARRY!"));
	TriggerCombatCameraImpulse(Dir, 18.0f, 0.08f);
	AddCameraTrauma(0.25f);
	Attacker->ApplyHitStop(0.12f);
	UDungeonAcousticSubsystem::PlayCombatCueAt(GetWorld(), FName(TEXT("Combat_Parry_0")), GetActorLocation(), 1.0f, 0.03f);
	if (UDungeoneerFxPoolSubsystem* Fx = GetWorld() ? GetWorld()->GetSubsystem<UDungeoneerFxPoolSubsystem>() : nullptr)
	{
		Fx->SpawnBurst(GetActorLocation() + Dir * 60.0 + FVector(0.0, 0.0, 50.0), FLinearColor(1.0f, 0.95f, 0.7f), 10, 380.0f, 0.3f);
	}
}

void ADungeoneerCharacter::HandlePerfectDodge()
{
	bPerfectDodgeUsedThisDash = true;
	++PerfectDodgeCount;
	DodgeCooldownRemaining = 0.0f;
	GrantEmpoweredStrike(2.0f, 1.5f, TEXT("PERFECT DODGE"));
	ShowAfterimage(GetActorLocation(), 0.4f);
	UDungeonAcousticSubsystem::PlayCombatCueAt(GetWorld(), FName(TEXT("Combat_PerfectDodge_0")), GetActorLocation(), 0.9f, 0.0f);
	if (UDungeoneerFxPoolSubsystem* Fx = GetWorld() ? GetWorld()->GetSubsystem<UDungeoneerFxPoolSubsystem>() : nullptr)
	{
		Fx->SpawnGroundRing(GetActorLocation() - FVector(0.0, 0.0, 85.0), FLinearColor(0.4f, 0.75f, 1.0f), 60.0f, HeroMoveTuning::PerfectDodgeSlowRadius, 0.5f);
	}
	DungeoneerCombat::ForEachZombieInRadius(GetWorld(), GetActorLocation(), HeroMoveTuning::PerfectDodgeSlowRadius, [](AZombieBase& Zombie, double)
	{
		Zombie.ApplyTimeSlow(0.3f, 1.0f);
	});
}

UMaterialInterface* ADungeoneerCharacter::GetCurrentHeroMaterial() const noexcept
{
	return HeroVisualMesh ? HeroVisualMesh->GetMaterial(0) : nullptr;
}

void ADungeoneerCharacter::ShowSkillBeam(const FVector& Start, const FVector& End, const float DurationSec, const FLinearColor& Color) noexcept
{
	if (!SkillBeamMesh)
	{
		return;
	}
	const FVector Delta = End - Start;
	const double Length = FMath::Max(Delta.Size2D(), 1.0);
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(Delta.Y), static_cast<float>(Delta.X)));
	SkillBeamMesh->SetWorldLocationAndRotation((Start + End) * 0.5 + FVector(0.0, 0.0, 30.0), FRotator(0.0f, Yaw, 0.0f));
	SkillBeamMesh->SetWorldScale3D(FVector(Length / 100.0, 0.14, 1.0));
	if (UMaterialInstanceDynamic* BeamMID = Cast<UMaterialInstanceDynamic>(SkillBeamMesh->GetMaterial(0)))
	{
		BeamMID->SetVectorParameterValue(FName(TEXT("Color")), Color);
	}
	SkillBeamMesh->SetVisibility(true);
	BeamRemaining = DurationSec;
}

void ADungeoneerCharacter::ShowAfterimage(const FVector& Location, const float DurationSec) noexcept
{
	if (!AfterimageMesh || !HeroVisualMesh)
	{
		return;
	}
	AfterimageMesh->SetMaterial(0, GetCurrentHeroMaterial());
	AfterimageMesh->SetWorldLocationAndRotation(
		Location + (HeroVisualMesh->GetComponentLocation() - GetActorLocation()),
		HeroVisualMesh->GetComponentRotation());
	AfterimageBaseScale = HeroVisualMesh->GetComponentScale();
	AfterimageMesh->SetWorldScale3D(AfterimageBaseScale);
	AfterimageMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashAmount, 0.45f);
	AfterimageMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashR, 0.45f);
	AfterimageMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashG, 0.7f);
	AfterimageMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashB, 1.0f);
	AfterimageMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FadeOut, 0.25f);
	AfterimageMesh->SetVisibility(true);
	AfterimageDuration = FMath::Max(DurationSec, 0.01f);
	AfterimageRemaining = AfterimageDuration;
}

void ADungeoneerCharacter::ShowDecoy(const FVector& Location, const bool bVisible) noexcept
{
	if (!DecoyMesh || !HeroVisualMesh)
	{
		return;
	}
	if (bVisible)
	{
		DecoyMesh->SetMaterial(0, GetCurrentHeroMaterial());
		DecoyMesh->SetWorldLocationAndRotation(
			Location + (HeroVisualMesh->GetComponentLocation() - GetActorLocation()),
			HeroVisualMesh->GetComponentRotation());
		DecoyMesh->SetWorldScale3D(HeroVisualMesh->GetComponentScale());
		// A reflection, not a person: silvery and a little see-through.
		DecoyMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashAmount, 0.35f);
		DecoyMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashR, 0.6f);
		DecoyMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashG, 0.8f);
		DecoyMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashB, 1.0f);
		DecoyMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FadeOut, 0.3f);
	}
	DecoyMesh->SetVisibility(bVisible);
}

void ADungeoneerCharacter::UpdateEffectQuads(const float DeltaSeconds) noexcept
{
	CameraTrauma = FMath::Max(0.0f, CameraTrauma - DeltaSeconds * 1.8f);
	if (HurtFlashRemaining > 0.0f)
	{
		HurtFlashRemaining = FMath::Max(0.0f, HurtFlashRemaining - DeltaSeconds);
		if (HeroVisualMesh)
		{
			// 60% so the hero stays readable under repeated hits; the screen-edge wash carries the rest.
			HeroVisualMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashAmount, 0.6f * GetHurtFlashAlpha());
		}
	}
	if (BeamRemaining > 0.0f)
	{
		BeamRemaining = FMath::Max(0.0f, BeamRemaining - DeltaSeconds);
		if (BeamRemaining <= 0.0f && SkillBeamMesh)
		{
			SkillBeamMesh->SetVisibility(false);
		}
	}
	if (AfterimageRemaining > 0.0f && AfterimageMesh)
	{
		AfterimageRemaining = FMath::Max(0.0f, AfterimageRemaining - DeltaSeconds);
		const float Alpha = AfterimageRemaining / AfterimageDuration;
		AfterimageMesh->SetWorldScale3D(AfterimageBaseScale * (0.55f + 0.45f * Alpha));
		AfterimageMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FadeOut, 1.0f - 0.75f * Alpha);
		if (AfterimageRemaining <= 0.0f)
		{
			AfterimageMesh->SetVisibility(false);
		}
	}
}

/**
 * Poses for the moves added on top of the original slash/flare/dodge set.
 * Returns true if it handled the current state (the caller then skips the older pose code).
 */
bool ADungeoneerCharacter::UpdateNewMovePoses(const float DeltaSeconds) noexcept
{
	const float EffectiveDt = (HitStopRemaining > 0.0f) ? (DeltaSeconds * 0.12f) : DeltaSeconds;
	const FVector Fwd = UDungeoneerMovementComponent::GetUnitDirectionForOctant(ActiveAttackOctant).GetSafeNormal2D();
	const float AttackYaw = DungeoneerMath::GCompassYawDegrees[static_cast<uint8>(ActiveAttackOctant) & 7];

	switch (CombatActionState)
	{
	case EDungeoneerCombatActionState::HeavyCharge:
	{
		HeavyChargeElapsed += DeltaSeconds;
		const float Alpha = GetHeavyChargeAlpha();
		const bool bReady = HeavyChargeElapsed >= HeroMoveTuning::HeavyMinChargeSec;
		ActiveWalkFrameColumn = 0;
		if (HeroVisualMesh)
		{
			// Crouch and coil: shorter and wider, with a shiver once fully charged.
			const float Shiver = (Alpha >= 1.0f) ? FMath::Sin(HeavyChargeElapsed * 70.0f) * 2.5f : 0.0f;
			HeroVisualMesh->SetRelativeLocation(BaseVisualRelativeLocation + FVector(-Fwd.X * 10.0 * Alpha + Shiver, -Fwd.Y * 10.0 * Alpha, 0.0));
			HeroVisualMesh->SetRelativeScale3D(BaseVisualRelativeScale * FVector(1.0 - 0.12 * Alpha, 1.0 + 0.10 * Alpha, 1.0));
		}
		if (HeroLanternFillLight)
		{
			HeroLanternFillLight->SetIntensity(120.0f + (bReady ? 520.0f : 180.0f) * Alpha);
		}
		if (SlashArcMesh)
		{
			SlashArcMesh->SetVisibility(false);
		}
		if (HeavyChargeElapsed >= HeroMoveTuning::HeavyAutoReleaseSec)
		{
			ReleaseHeavyCharge();
		}
		return true;
	}

	case EDungeoneerCombatActionState::HeavySmash:
	{
		HeavySmashElapsed += EffectiveDt;
		const float T = FMath::Clamp(HeavySmashElapsed / HeroMoveTuning::HeavySmashPoseSec, 0.0f, 1.0f);
		ActiveWalkFrameColumn = (T < 0.3f) ? 1 : 3;
		if (HeroVisualMesh)
		{
			// Rise (stretch tall) then slam (squash wide) and lunge into it.
			const double Tall = (T < 0.3f) ? 1.0 + 0.16 * (T / 0.3f) : 1.0 - 0.2 * FMath::Sin((T - 0.3f) / 0.7f * PI);
			const double Wide = (T < 0.3f) ? 1.0 - 0.06 * (T / 0.3f) : 1.0 + 0.18 * FMath::Sin((T - 0.3f) / 0.7f * PI);
			const double Lunge = FMath::Sin(T * PI) * 34.0;
			HeroVisualMesh->SetRelativeLocation(BaseVisualRelativeLocation + FVector(Fwd.X * Lunge, Fwd.Y * Lunge, (T < 0.3f) ? 12.0 * T / 0.3f : 0.0));
			HeroVisualMesh->SetRelativeScale3D(BaseVisualRelativeScale * FVector(Tall, Wide, 1.0));
		}
		if (SlashArcMesh)
		{
			const float Burst = (T < 0.3f) ? 0.0f : FMath::Sin((T - 0.3f) / 0.7f * PI);
			const float Size = (1.4f + 0.8f * HeavySmashAlpha) * Burst + 0.1f;
			const FVector ArcPos = Fwd * 120.0;
			SlashArcMesh->SetRelativeLocation(FVector(ArcPos.X, ArcPos.Y, 6.0));
			SlashArcMesh->SetRelativeRotation(FRotator(0.0f, AttackYaw + 90.0f, 0.0f));
			SlashArcMesh->SetRelativeScale3D(FVector(1.8 * Size, 1.3 * Size, 1.0));
			SlashArcMesh->SetVisibility(T >= 0.3f);
		}
		if (HeroLanternFillLight)
		{
			HeroLanternFillLight->SetIntensity(120.0f + 700.0f * (1.0f - T));
		}
		if (HeavySmashElapsed >= HeroMoveTuning::HeavySmashPoseSec)
		{
			CombatActionState = EDungeoneerCombatActionState::None;
		}
		return true;
	}

	case EDungeoneerCombatActionState::DashStrike:
	{
		DashStrikeElapsed += EffectiveDt;
		const float T = FMath::Clamp(DashStrikeElapsed / HeroMoveTuning::DashStrikePoseSec, 0.0f, 1.0f);
		ActiveWalkFrameColumn = 1;
		if (DashStrikeElapsed < HeroMoveTuning::DashStrikeLungeSec)
		{
			if (UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement())
			{
				MoveComp->Velocity = FVector(
					DashDirectionXY.X * static_cast<double>(HeroMoveTuning::DashStrikeSpeedUU),
					DashDirectionXY.Y * static_cast<double>(HeroMoveTuning::DashStrikeSpeedUU),
					MoveComp->Velocity.Z);
			}
		}
		if (HeroVisualMesh)
		{
			HeroVisualMesh->SetRelativeLocation(BaseVisualRelativeLocation + FVector(DashDirectionXY.X * 18.0, DashDirectionXY.Y * 18.0, 4.0));
			HeroVisualMesh->SetRelativeScale3D(BaseVisualRelativeScale * FVector(0.92, 1.14, 1.0));
		}
		if (SlashArcMesh)
		{
			const float DashYaw = FMath::RadiansToDegrees(FMath::Atan2(static_cast<float>(DashDirectionXY.Y), static_cast<float>(DashDirectionXY.X)));
			const FVector ArcPos = DashDirectionXY * 120.0;
			SlashArcMesh->SetRelativeLocation(FVector(ArcPos.X, ArcPos.Y, 14.0));
			SlashArcMesh->SetRelativeRotation(FRotator(0.0f, DashYaw, 0.0f));
			SlashArcMesh->SetRelativeScale3D(FVector(2.6 * (1.0 - 0.5 * T), 0.3, 1.0));
			SlashArcMesh->SetVisibility(true);
		}
		if (DashStrikeElapsed >= HeroMoveTuning::DashStrikePoseSec)
		{
			CombatActionState = EDungeoneerCombatActionState::None;
		}
		return true;
	}

	case EDungeoneerCombatActionState::Parry:
	{
		const bool bActive = ParryActiveRemaining > 0.0f;
		ActiveWalkFrameColumn = 0;
		if (HeroVisualMesh)
		{
			// Brace: lean back from the aim direction and narrow.
			HeroVisualMesh->SetRelativeLocation(BaseVisualRelativeLocation + FVector(-Fwd.X * 8.0, -Fwd.Y * 8.0, 0.0));
			HeroVisualMesh->SetRelativeScale3D(BaseVisualRelativeScale * FVector(1.04, 0.94, 1.0));
		}
		if (SlashArcMesh)
		{
			const FVector ArcPos = Fwd * 70.0;
			SlashArcMesh->SetRelativeLocation(FVector(ArcPos.X, ArcPos.Y, 14.0));
			SlashArcMesh->SetRelativeRotation(FRotator(0.0f, AttackYaw + 90.0f, 0.0f));
			SlashArcMesh->SetRelativeScale3D(FVector(1.2, 0.35, 1.0));
			SlashArcMesh->SetVisibility(bActive);
		}
		if (HeroLanternFillLight)
		{
			HeroLanternFillLight->SetIntensity(bActive ? 640.0f : 120.0f);
		}
		if (ParryRecoveryRemaining <= 0.0f)
		{
			CombatActionState = EDungeoneerCombatActionState::None;
		}
		return true;
	}

	case EDungeoneerCombatActionState::SkillCast:
	{
		SkillPoseRemaining = FMath::Max(0.0f, SkillPoseRemaining - DeltaSeconds);
		const float T = 1.0f - SkillPoseRemaining / SkillPoseDuration;
		ActiveWalkFrameColumn = 1;
		if (HeroVisualMesh)
		{
			const double Pop = FMath::Sin(T * PI);
			HeroVisualMesh->SetRelativeLocation(BaseVisualRelativeLocation + FVector(Fwd.X * 14.0 * Pop, Fwd.Y * 14.0 * Pop, 6.0 * Pop));
			HeroVisualMesh->SetRelativeScale3D(BaseVisualRelativeScale * FVector(1.0 + 0.08 * Pop, 1.0 - 0.04 * Pop, 1.0));
		}
		if (SlashArcMesh)
		{
			SlashArcMesh->SetVisibility(false);
		}
		if (SkillPoseRemaining <= 0.0f)
		{
			CombatActionState = EDungeoneerCombatActionState::None;
		}
		return true;
	}

	case EDungeoneerCombatActionState::Whirl:
	{
		SkillPoseRemaining = FMath::Max(0.0f, SkillPoseRemaining - DeltaSeconds);
		const float Elapsed = SkillPoseDuration - SkillPoseRemaining;
		ActiveWalkFrameColumn = 2;
		if (HeroVisualMesh)
		{
			HeroVisualMesh->SetRelativeLocation(BaseVisualRelativeLocation + FVector(0.0, 0.0, 4.0 + 3.0 * FMath::Sin(Elapsed * 30.0f)));
			HeroVisualMesh->SetRelativeScale3D(BaseVisualRelativeScale * FVector(0.97, 1.05, 1.0));
		}
		if (SlashArcMesh)
		{
			// The lantern orbits the hero on its chain.
			const float OrbitYaw = Elapsed * 1080.0f;
			const float OrbitRad = FMath::DegreesToRadians(OrbitYaw);
			SlashArcMesh->SetRelativeLocation(FVector(FMath::Cos(OrbitRad) * 110.0, FMath::Sin(OrbitRad) * 110.0, 14.0));
			SlashArcMesh->SetRelativeRotation(FRotator(0.0f, OrbitYaw + 90.0f, 0.0f));
			SlashArcMesh->SetRelativeScale3D(FVector(1.6, 0.6, 1.0));
			SlashArcMesh->SetVisibility(true);
		}
		if (HeroLanternFillLight)
		{
			HeroLanternFillLight->SetIntensity(420.0f);
		}
		if (SkillPoseRemaining <= 0.0f)
		{
			CombatActionState = EDungeoneerCombatActionState::None;
		}
		return true;
	}

	default:
		return false;
	}
}

void ADungeoneerCharacter::UpdateCombatAndProceduralPose(float DeltaSeconds) noexcept
{
	AttackCooldownRemaining = FMath::Max(0.0f, AttackCooldownRemaining - DeltaSeconds);
	DodgeCooldownRemaining = FMath::Max(0.0f, DodgeCooldownRemaining - DeltaSeconds);
	HitStaggerRemaining = FMath::Max(0.0f, HitStaggerRemaining - DeltaSeconds);
	ComboResetRemaining = FMath::Max(0.0f, ComboResetRemaining - DeltaSeconds);
	FlareFlashRemaining = FMath::Max(0.0f, FlareFlashRemaining - DeltaSeconds);
	HitStopRemaining = FMath::Max(0.0f, HitStopRemaining - DeltaSeconds);
	AttackBufferRemaining = FMath::Max(0.0f, AttackBufferRemaining - DeltaSeconds);
	DashStrikeWindowRemaining = FMath::Max(0.0f, DashStrikeWindowRemaining - DeltaSeconds);
	ParryActiveRemaining = FMath::Max(0.0f, ParryActiveRemaining - DeltaSeconds);
	ParryRecoveryRemaining = FMath::Max(0.0f, ParryRecoveryRemaining - DeltaSeconds);
	ParryCooldownRemaining = FMath::Max(0.0f, ParryCooldownRemaining - DeltaSeconds);
	EmpoweredRemaining = FMath::Max(0.0f, EmpoweredRemaining - DeltaSeconds);
	InvulnRemaining = FMath::Max(0.0f, InvulnRemaining - DeltaSeconds);
	UpdateEffectQuads(DeltaSeconds);

	// Chain a buffered press, or start charging a heavy smash if attack is still held.
	if (CombatActionState == EDungeoneerCombatActionState::None && DownedRespawnRemaining <= 0.0f && AttackCooldownRemaining <= 0.0f)
	{
		if (AttackBufferRemaining > 0.0f)
		{
			AttackBufferRemaining = 0.0f;
			TriggerPrimaryAttack();
		}
		else if (bAttackHeld)
		{
			BeginHeavyCharge();
		}
	}

	if (UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement())
	{
		float SpeedScalar = 1.0f;
		switch (CombatActionState)
		{
		case EDungeoneerCombatActionState::HeavyCharge: SpeedScalar = 0.45f; break;
		case EDungeoneerCombatActionState::HeavySmash:  SpeedScalar = 0.2f; break;
		case EDungeoneerCombatActionState::Parry:       SpeedScalar = 0.3f; break;
		case EDungeoneerCombatActionState::SkillCast:   SpeedScalar = 0.6f; break;
		case EDungeoneerCombatActionState::Whirl:       SpeedScalar = 0.8f; break;
		default: break;
		}
		MoveComp->SetActionSpeedScalar(SpeedScalar);
	}

	// Advance pre-allocated floating combat popups with zero allocations
	for (int32 p = 0; p < MaxFloatingPopups; ++p)
	{
		if (FloatingPopups[p].bActive)
		{
			FloatingPopups[p].AgeSeconds += DeltaSeconds;
			FloatingPopups[p].WorldLocation.Z += static_cast<double>(DeltaSeconds * 42.0f);
			if (FloatingPopups[p].AgeSeconds >= FloatingPopups[p].LifetimeSeconds)
			{
				FloatingPopups[p].bActive = false;
			}
		}
	}

	// Deterministic critically-damped camera micro-impulse spring
	if (CameraBoom)
	{
		const double Stiffness = 220.0;
		const double Damping = 26.0;
		const FVector SpringAccel = (-CameraImpulseOffset * Stiffness) - (CameraImpulseVelocity * Damping);
		CameraImpulseVelocity += SpringAccel * static_cast<double>(DeltaSeconds);
		CameraImpulseOffset += CameraImpulseVelocity * static_cast<double>(DeltaSeconds);
		if (CameraImpulseOffset.SizeSquared() < 0.01 && CameraImpulseVelocity.SizeSquared() < 0.1)
		{
			CameraImpulseOffset = FVector::ZeroVector;
			CameraImpulseVelocity = FVector::ZeroVector;
		}
		CameraBoom->SocketOffset = BaseCameraSocketOffset + CameraImpulseOffset;
	}

	if (DownedRespawnRemaining > 0.0f)
	{
		DownedRespawnRemaining = FMath::Max(0.0f, DownedRespawnRemaining - DeltaSeconds);
		CombatActionState = EDungeoneerCombatActionState::Downed;
		if (DownedRespawnRemaining <= 0.0f && HasAuthority())
		{
			const UWorld* World = GetWorld();
			const UZombieSpatialSubsystem* SpatialSys = World ? World->GetSubsystem<UZombieSpatialSubsystem>() : nullptr;
			const int32 AliveAllies = SpatialSys ? SpatialSys->GetAliveOtherPlayerCount(this) : 0;
			ADungeoneerGameMode* GM = World ? World->GetAuthGameMode<ADungeoneerGameMode>() : nullptr;

			if (!IsRunningCommandlet() && GM && !GM->IsActiveRunMainMenu() && AliveAllies == 0)
			{
				bWasDownedWithAliveAllies = false;
				GM->ShowRunSummaryOverlay(false);
			}
			else
			{
				CurrentHealth = MaxHealth * 0.65f;
				FlashlightBattery = FMath::Max(FlashlightBattery, 50.0f);
				CombatActionState = EDungeoneerCombatActionState::None;
				bWasDownedWithAliveAllies = false;
				// Get back up beside the partner so both stay on the shared screen; the entry room is only
				// a fallback when nobody else is standing.
				const ADungeoneerCharacter* Partner = SpatialSys ? SpatialSys->FindNearestLivingAlly(this, 1.0e6f) : nullptr;
				if (Partner)
				{
					const FVector Beside = Partner->GetActorLocation() + FVector(92.0, 92.0, 0.0); // screen-right of the partner
					SetActorLocation(Beside, true, nullptr, ETeleportType::TeleportPhysics);
				}
				else if (GM)
				{
					SetActorLocation(GM->GetEntrySpawnLocation() + FVector(0.0, 0.0, 95.0), false, nullptr, ETeleportType::TeleportPhysics);
				}
			}
		}
	}

	if (CombatActionState == EDungeoneerCombatActionState::ChargedFlare)
	{
		if (HeroLanternFillLight)
		{
			HeroLanternFillLight->SetIntensity(920.0f * (FlareFlashRemaining / 0.40f) + 120.0f);
		}
		if (FlareFlashRemaining <= 0.0f)
		{
			CombatActionState = EDungeoneerCombatActionState::None;
		}
		return;
	}

	if (UpdateNewMovePoses(DeltaSeconds))
	{
		return;
	}

	if (CombatActionState == EDungeoneerCombatActionState::LanternSlash)
	{
		const float SlashDuration = (ComboStepIndex == ComboLength - 1) ? 0.28f : 0.22f;
		const float EffectiveDt = (HitStopRemaining > 0.0f) ? (DeltaSeconds * 0.12f) : DeltaSeconds;
		AttackSwingElapsed += EffectiveDt;
		const float Alpha = FMath::Clamp(AttackSwingElapsed / SlashDuration, 0.0f, 1.0f);
		const FVector ForwardXY = UDungeoneerMovementComponent::GetUnitDirectionForOctant(ActiveAttackOctant).GetSafeNormal2D();
		const float AttackYaw = DungeoneerMath::GCompassYawDegrees[static_cast<uint8>(ActiveAttackOctant) & 7];

		ActiveWalkFrameColumn = (Alpha < 0.42f) ? 1 : 3;

		if (HeroVisualMesh)
		{
			const float LungeAmp = (ComboStepIndex == ComboLength - 1) ? 42.0f : 24.0f;
			const float LungeDist = FMath::Sin(Alpha * PI) * LungeAmp;
			const FVector Offset(ForwardXY.X * LungeDist, ForwardXY.Y * LungeDist, FMath::Sin(Alpha * PI) * 10.0f);
			HeroVisualMesh->SetRelativeLocation(BaseVisualRelativeLocation + Offset);
			HeroVisualMesh->SetRelativeScale3D(BaseVisualRelativeScale * FVector(1.06, 0.94, 1.08));
		}

		if (SlashArcMesh)
		{
			// Alternate swing direction each hit; the finisher sweeps wider.
			const float Sweep = (ComboStepIndex == ComboLength - 1) ? 75.0f : 55.0f;
			const float StartOffset = (ComboStepIndex % 2 == 1) ? Sweep : -Sweep;
			const float EndOffset = -StartOffset;
			const float SweepYaw = AttackYaw + FMath::Lerp(StartOffset, EndOffset, Alpha);
			const FVector ArcOffset = ForwardXY * ((ComboStepIndex == ComboLength - 1 ? 108.0f : 84.0f) + 16.0f * Alpha);
			SlashArcMesh->SetRelativeLocation(FVector(ArcOffset.X, ArcOffset.Y, 14.0));
			SlashArcMesh->SetRelativeRotation(FRotator(0.0f, SweepYaw, 0.0f));
			const float ArcScale = FMath::Sin(Alpha * PI) * (ComboStepIndex == ComboLength - 1 ? 1.7f : 1.35f) + 0.35f;
			SlashArcMesh->SetRelativeScale3D(FVector(1.45 * ArcScale, 0.92 * ArcScale, 1.0));
			SlashArcMesh->SetVisibility(true);
		}

		if (FlashlightComponent)
		{
			FlashlightComponent->SetIntensity(650.0f + 250.0f * (1.0f - Alpha));
		}
		if (HeroLanternFillLight)
		{
			HeroLanternFillLight->SetIntensity(120.0f + 55.0f * (1.0f - Alpha));
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
		FlashlightComponent->SetIntensity(650.0f);
	}
	if (HeroLanternFillLight)
	{
		HeroLanternFillLight->SetIntensity(120.0f);
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
			DashStrikeWindowRemaining = HeroMoveTuning::DashStrikeWindowSec;
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
		const UDungeoneerMovementComponent* MoveComp = GetDungeoneerMovement();
		const double WadingBootDrop = (MoveComp && MoveComp->IsWadingInStream())
			? -static_cast<double>(EcoCells::WadingBootDropUU)
			: 0.0;
		HeroVisualMesh->SetRelativeLocation(BaseVisualRelativeLocation + FVector(0.0, 0.0, WadingBootDrop));
		HeroVisualMesh->SetRelativeRotation(BaseVisualRelativeRotation);
		HeroVisualMesh->SetRelativeScale3D(BaseVisualRelativeScale);
	}
}

void ADungeoneerCharacter::RefreshNearbyInteractCandidate() noexcept
{
	NearbyInteractType = EDungeoneerNearbyInteract::None;
	const UWorld* World = GetWorld();
	const UZombieSpatialSubsystem* SpatialSys = World ? World->GetSubsystem<UZombieSpatialSubsystem>() : nullptr;
	if (SpatialSys && SpatialSys->FindNearestDownedAlly(this, 260.0f))
	{
		NearbyInteractType = EDungeoneerNearbyInteract::ReviveAlly;
		return;
	}

	const ADungeoneerGameMode* GM = World ? World->GetAuthGameMode<ADungeoneerGameMode>() : nullptr;
	const ADungeoneerTileMapInstancer* Instancer = GM ? GM->GetActiveTileMapInstancer() : nullptr;
	if (!Instancer)
	{
		if (SpatialSys && SpatialSys->FindNearestLivingAlly(this, 340.0f))
		{
			NearbyInteractType = EDungeoneerNearbyInteract::AllyTrade;
		}
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

		if (const ADungeoneerLootPickupActor* Pickup = Cast<ADungeoneerLootPickupActor>(Encounter))
		{
			if (!Pickup->IsCollected())
			{
				BestDistSq = DistSq2D;
				NearbyInteractType = EDungeoneerNearbyInteract::CollectPickup;
			}
		}
		else if (const AMajorLootVaultActor* Vault = Cast<AMajorLootVaultActor>(Encounter))
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
		else if (Encounter->IsA<ARestingGroundActor>())
		{
			BestDistSq = DistSq2D;
			NearbyInteractType = EDungeoneerNearbyInteract::SensoryAltar;
		}
	}

	if (NearbyInteractType == EDungeoneerNearbyInteract::None && SpatialSys && SpatialSys->FindNearestLivingAlly(this, 340.0f))
	{
		NearbyInteractType = EDungeoneerNearbyInteract::AllyTrade;
	}
}

namespace
{
	/** Polled buttons, used as indices into PrevButtonDown. */
	enum EHeroButton : int32
	{
		Btn_Attack,
		Btn_Dodge,
		Btn_Interact,
		Btn_Parry,
		Btn_Skill1,
		Btn_Skill2,
		Btn_Oil,
		Btn_Antidote,
		Btn_Trade,
		Btn_CoopJoin,
		Btn_Count
	};
	static_assert(Btn_Count <= 12, "PrevButtonDown is sized for 12 buttons");

	bool AnyKeyDown(const APlayerController& PC, std::initializer_list<FKey> Keys)
	{
		for (const FKey& Key : Keys)
		{
			if (PC.IsInputKeyDown(Key))
			{
				return true;
			}
		}
		return false;
	}

	/** Gamepad stick in screen space: X = up, Y = right. Zero inside the dead zone. */
	FVector2D ReadStick(const APlayerController& PC, const FKey& AxisX, const FKey& AxisY, float DeadZone)
	{
		const FVector2D Stick(PC.GetInputAnalogKeyState(AxisY), PC.GetInputAnalogKeyState(AxisX));
		return (Stick.SizeSquared() >= static_cast<double>(DeadZone * DeadZone)) ? Stick : FVector2D::ZeroVector;
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
		bAttackHeld = false;
		return;
	}

	TimeSinceMouseMoved = FMath::Min(TimeSinceMouseMoved + DeltaSeconds, 10.0f);

	const ULocalPlayer* OwningLP = PC->GetLocalPlayer();
	const bool bIsPlayerOne = !OwningLP || OwningLP->GetControllerId() == 0;
	const UGameInstance* GI = GetGameInstance();
	const bool bLocalSplitActive = GI && GI->GetNumLocalPlayers() > 1;

	// UE delivers keyboard and mouse events to the first local player's controller only, so player 2's
	// keyboard keys are read from there. Gamepad buttons and sticks still come from each player's own controller.
	const APlayerController* KeyPC = PC;
	if (!bIsPlayerOne && GI && GI->GetNumLocalPlayers() > 0)
	{
		if (const ULocalPlayer* FirstLP = GI->GetLocalPlayers()[0])
		{
			if (const APlayerController* FirstPC = FirstLP->GetPlayerController(GetWorld()))
			{
				KeyPC = FirstPC;
			}
		}
	}
	ADungeoneerGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADungeoneerGameMode>() : nullptr;
	const bool bMainMenuOpen = GM && GM->GetActiveDioramaStage() && !GM->GetActiveDioramaStage()->IsHidden();

	// --- Movement: WASD for P1, arrows for P2 (or for P1 when solo), plus the left stick ---
	FVector2D RawMove = FVector2D::ZeroVector; // X = screen up, Y = screen right
	const bool bArrowsAreMine = !bIsPlayerOne || !bLocalSplitActive;
	if (bIsPlayerOne)
	{
		RawMove.X += PC->IsInputKeyDown(EKeys::W) ? 1.0 : 0.0;
		RawMove.X -= PC->IsInputKeyDown(EKeys::S) ? 1.0 : 0.0;
		RawMove.Y += PC->IsInputKeyDown(EKeys::D) ? 1.0 : 0.0;
		RawMove.Y -= PC->IsInputKeyDown(EKeys::A) ? 1.0 : 0.0;
	}
	if (bArrowsAreMine)
	{
		RawMove.X += KeyPC->IsInputKeyDown(EKeys::Up) ? 1.0 : 0.0;
		RawMove.X -= KeyPC->IsInputKeyDown(EKeys::Down) ? 1.0 : 0.0;
		RawMove.Y += KeyPC->IsInputKeyDown(EKeys::Right) ? 1.0 : 0.0;
		RawMove.Y -= KeyPC->IsInputKeyDown(EKeys::Left) ? 1.0 : 0.0;
	}
	RawMove += ReadStick(*PC, EKeys::Gamepad_LeftX, EKeys::Gamepad_LeftY, 0.25f);

	const FVector2D MoveVec = DungeoneerMath::RotateScreenInputToWorldForCameraYaw(RawMove, -45.0f);
	MoveComp->SetMoveInputVector(MoveVec);
	if (MoveComp->HasActiveMoveInput())
	{
		AddMovementInput(UDungeoneerMovementComponent::GetUnitDirectionForOctant(MoveComp->GetMoveOctant()), 1.0f);
	}

	// --- Aim: right stick wins, then a recently moved mouse (P1), then movement direction ---
	bool bExplicitAim = false;
	const FVector2D RightStick = ReadStick(*PC, EKeys::Gamepad_RightX, EKeys::Gamepad_RightY, 0.35f);
	if (!RightStick.IsZero())
	{
		MoveComp->SetAimInputVector(DungeoneerMath::RotateScreenInputToWorldForCameraYaw(RightStick, -45.0f));
		bExplicitAim = true;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	if (!bExplicitAim && bIsPlayerOne && PC->GetMousePosition(MouseX, MouseY))
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

		// The shared camera doesn't keep the hero at screen centre, so aim at the point under the cursor
		// on the hero's ground plane.
		FVector RayOrigin = FVector::ZeroVector;
		FVector RayDir = FVector::ZeroVector;
		if (TimeSinceMouseMoved <= 0.45f && PC->DeprojectMousePositionToWorld(RayOrigin, RayDir) && RayDir.Z < -0.01)
		{
			const FVector HeroLoc = GetActorLocation();
			const double T = (HeroLoc.Z - RayOrigin.Z) / RayDir.Z;
			const FVector Hit = RayOrigin + RayDir * T;
			const FVector2D AimVec(Hit.X - HeroLoc.X, Hit.Y - HeroLoc.Y);
			if (AimVec.SizeSquared() > 25.0 * 25.0)
			{
				MoveComp->SetAimInputVector(AimVec);
				bExplicitAim = true;
			}
		}
	}

	if (!bExplicitAim && MoveComp->HasActiveMoveInput())
	{
		MoveComp->SetAimInputVector(MoveVec);
	}

	// --- Buttons ---------------------------------------------------------------------------
	TStaticArray<bool, Btn_Count> Down;
	for (bool& bDown : Down)
	{
		bDown = false;
	}

	if (!bMainMenuOpen)
	{
		if (bIsPlayerOne)
		{
			Down[Btn_Attack]   = AnyKeyDown(*PC, { EKeys::LeftMouseButton, EKeys::J, EKeys::SpaceBar });
			Down[Btn_Dodge]    = AnyKeyDown(*PC, { EKeys::LeftShift, EKeys::K, EKeys::RightMouseButton })
				|| (!bLocalSplitActive && PC->IsInputKeyDown(EKeys::RightShift));
			Down[Btn_Parry]    = AnyKeyDown(*PC, { EKeys::L, EKeys::V, EKeys::MiddleMouseButton });
			Down[Btn_Skill1]   = AnyKeyDown(*PC, { EKeys::Q, EKeys::U });
			Down[Btn_Skill2]   = AnyKeyDown(*PC, { EKeys::R, EKeys::I });
			Down[Btn_Interact] = AnyKeyDown(*PC, { EKeys::F, EKeys::E });
			Down[Btn_Oil]      = PC->IsInputKeyDown(EKeys::One);
			Down[Btn_Antidote] = AnyKeyDown(*PC, { EKeys::C, EKeys::Two });
			Down[Btn_Trade]    = PC->IsInputKeyDown(EKeys::T);
			Down[Btn_CoopJoin] = AnyKeyDown(*PC, { EKeys::F2, EKeys::Gamepad_Special_Right });
		}
		else
		{
			Down[Btn_Attack]   = AnyKeyDown(*KeyPC, { EKeys::RightControl, EKeys::Enter, EKeys::NumPadZero });
			Down[Btn_Dodge]    = AnyKeyDown(*KeyPC, { EKeys::RightShift, EKeys::NumPadOne });
			Down[Btn_Parry]    = AnyKeyDown(*KeyPC, { EKeys::Semicolon, EKeys::NumPadSix });
			Down[Btn_Skill1]   = AnyKeyDown(*KeyPC, { EKeys::LeftBracket, EKeys::NumPadFive });
			Down[Btn_Skill2]   = AnyKeyDown(*KeyPC, { EKeys::RightBracket, EKeys::NumPadFour });
			Down[Btn_Interact] = AnyKeyDown(*KeyPC, { EKeys::Backslash, EKeys::NumPadTwo });
			Down[Btn_Oil]      = KeyPC->IsInputKeyDown(EKeys::NumPadSeven);
			Down[Btn_Antidote] = KeyPC->IsInputKeyDown(EKeys::NumPadEight);
			Down[Btn_Trade]    = AnyKeyDown(*KeyPC, { EKeys::P, EKeys::NumPadThree });
		}

		// Gamepad layout, same for both players.
		Down[Btn_Attack]   = Down[Btn_Attack]   || PC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Left);
		Down[Btn_Dodge]    = Down[Btn_Dodge]    || PC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom);
		Down[Btn_Parry]    = Down[Btn_Parry]    || PC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Right);
		Down[Btn_Interact] = Down[Btn_Interact] || PC->IsInputKeyDown(EKeys::Gamepad_FaceButton_Top);
		Down[Btn_Skill1]   = Down[Btn_Skill1]   || AnyKeyDown(*PC, { EKeys::Gamepad_LeftShoulder, EKeys::Gamepad_LeftTrigger });
		Down[Btn_Skill2]   = Down[Btn_Skill2]   || AnyKeyDown(*PC, { EKeys::Gamepad_RightShoulder, EKeys::Gamepad_RightTrigger });
		Down[Btn_Oil]      = Down[Btn_Oil]      || PC->IsInputKeyDown(EKeys::Gamepad_DPad_Up);
		Down[Btn_Antidote] = Down[Btn_Antidote] || PC->IsInputKeyDown(EKeys::Gamepad_DPad_Down);
		Down[Btn_Trade]    = Down[Btn_Trade]    || PC->IsInputKeyDown(EKeys::Gamepad_DPad_Right);
	}

	auto Pressed = [this, &Down](const EHeroButton Button) { return Down[Button] && !PrevButtonDown[Button]; };

	if (Pressed(Btn_Attack))
	{
		TriggerPrimaryAttack();
	}
	bAttackHeld = Down[Btn_Attack];
	if (!Down[Btn_Attack] && PrevButtonDown[Btn_Attack])
	{
		ReleaseHeavyCharge();
	}
	if (Pressed(Btn_Dodge))    { TriggerDodgeDash(); }
	if (Pressed(Btn_Parry))    { TriggerParry(); }
	if (Pressed(Btn_Skill1))   { TriggerSkillSlot(0); }
	if (Pressed(Btn_Skill2))   { TriggerSkillSlot(1); }
	if (Pressed(Btn_Interact)) { TriggerInteract(); }
	if (Pressed(Btn_Oil))      { TriggerQuickConsumable(EDungeoneerConsumableId::LanternOilPhial); }
	if (Pressed(Btn_Antidote)) { TriggerQuickConsumable(EDungeoneerConsumableId::AntidoteDraught); }
	if (Pressed(Btn_Trade))    { TriggerAllyTradeWithNearest(); }
	if (Pressed(Btn_CoopJoin) && GM && bIsPlayerOne)
	{
		GM->RequestLocalCoopJoin();
	}

	for (int32 i = 0; i < Btn_Count; ++i)
	{
		PrevButtonDown[i] = Down[i];
	}
}

bool ADungeoneerCharacter::HasPoseSheet(const EHeroPoseSheet Sheet) const noexcept
{
	const int32 First = static_cast<int32>(Sheet) * 16;
	return DirectionalHeroMIDs.IsValidIndex(First) && DirectionalHeroMIDs[First] != nullptr;
}

void ADungeoneerCharacter::ResolvePoseFrame(const float DeltaSeconds, const bool bMoving) noexcept
{
	EHeroPoseSheet Sheet = EHeroPoseSheet::Walk;
	int32 Col = 0;
	switch (CombatActionState)
	{
	case EDungeoneerCombatActionState::LanternSlash:
	{
		const bool bFinisher = (ComboStepIndex == ComboLength - 1);
		const float Alpha = FMath::Clamp(AttackSwingElapsed / (bFinisher ? 0.28f : 0.22f), 0.0f, 1.0f);
		if (bFinisher)
		{
			// The fourth hit is an overhead slam, so it borrows the heavy sheet.
			Sheet = EHeroPoseSheet::Heavy;
			Col = (Alpha < 0.35f) ? 1 : ((Alpha < 0.75f) ? 2 : 3);
		}
		else
		{
			Sheet = EHeroPoseSheet::Combo;
			Col = (Alpha < 0.2f) ? 0 : ((Alpha < 0.55f) ? 1 : ((Alpha < 0.8f) ? 2 : 3));
		}
		break;
	}
	case EDungeoneerCombatActionState::HeavyCharge:
		Sheet = EHeroPoseSheet::Heavy;
		Col = 0;
		break;
	case EDungeoneerCombatActionState::HeavySmash:
	{
		const float T = FMath::Clamp(HeavySmashElapsed / HeroMoveTuning::HeavySmashPoseSec, 0.0f, 1.0f);
		Sheet = EHeroPoseSheet::Heavy;
		Col = (T < 0.3f) ? 1 : ((T < 0.7f) ? 2 : 3);
		break;
	}
	case EDungeoneerCombatActionState::DodgeDash:
		Sheet = EHeroPoseSheet::Evade;
		Col = 1;
		break;
	case EDungeoneerCombatActionState::DashStrike:
		Sheet = EHeroPoseSheet::Evade;
		Col = 2;
		break;
	case EDungeoneerCombatActionState::Parry:
		Sheet = EHeroPoseSheet::Evade;
		Col = 3;
		break;
	case EDungeoneerCombatActionState::SkillCast:
		Sheet = SkillCastSheet;
		Col = SkillCastColumn;
		break;
	case EDungeoneerCombatActionState::Whirl:
		Sheet = EHeroPoseSheet::Skills;
		Col = 2;
		break;
	case EDungeoneerCombatActionState::ChargedFlare:
		Sheet = EHeroPoseSheet::Skills;
		Col = 3;
		break;
	case EDungeoneerCombatActionState::HitStagger:
		Sheet = EHeroPoseSheet::HurtIdle;
		Col = 2;
		break;
	case EDungeoneerCombatActionState::Downed:
		Sheet = EHeroPoseSheet::HurtIdle;
		Col = 3;
		break;
	default:
		if (!bMoving)
		{
			// Standing still: breathe between the two idle frames.
			IdleBreathTime += DeltaSeconds;
			Sheet = EHeroPoseSheet::HurtIdle;
			Col = (FMath::FloorToInt(IdleBreathTime / 0.7f) & 1);
		}
		else
		{
			IdleBreathTime = 0.0f;
		}
		break;
	}
	if (DownedRespawnRemaining > 0.0f)
	{
		Sheet = EHeroPoseSheet::HurtIdle;
		Col = 3;
	}
	ActivePoseSheet = Sheet;
	ActivePoseColumn = Col;
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

	ResolvePoseFrame(DeltaSeconds, bHasMoveInput);

	ECompassOctant VisualFacingOctant = ActiveFacingOctant;
	if (CombatActionState == EDungeoneerCombatActionState::Whirl && SkillComponent)
	{
		// The spin frame is drawn from each facing in turn so the hero visibly turns.
		VisualFacingOctant = static_cast<ECompassOctant>(FMath::FloorToInt(SkillComponent->GetWhirlElapsed() * 18.0f) & 7);
	}
	UpdateHeroDirectionalVisual(VisualFacingOctant, ActiveWalkFrameColumn);

	if (DirectionalRigComponent)
	{
		DirectionalRigComponent->SetHeadingOctant(VisualFacingOctant);
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
	const float AimYawRad = FMath::DegreesToRadians(CurrentFlashlightYawDeg);
	FlashlightComponent->SetRelativeLocation(FVector(FMath::Cos(AimYawRad) * 42.0, FMath::Sin(AimYawRad) * 42.0, 32.0));
	FlashlightComponent->SetWorldRotation(FRotator(-16.0f, CurrentFlashlightYawDeg, 0.0f));

	const bool bSunGlass = InventoryComponent && InventoryComponent->HasRelic(EDungeoneerRelicId::SunGlassLens);
	FlashlightComponent->SetOuterConeAngle(bSunGlass ? 30.0f : 22.0f);
	FlashlightComponent->SetInnerConeAngle(bSunGlass ? 12.0f : 8.0f);
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

	// Only zombies can be parried or perfect-dodged; hazard puddles and traps cannot.
	if (AZombieBase* AttackerZombie = Cast<AZombieBase>(DamageCauser))
	{
		const FVector ToAttacker = (AttackerZombie->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		if (ParryActiveRemaining > 0.0f && FVector::DotProduct(GetAimForwardXY(), ToAttacker) >= -0.2)
		{
			HandleSuccessfulParry(AttackerZombie);
			return;
		}
		if (DodgeActiveRemaining > 0.0f && !bPerfectDodgeUsedThisDash)
		{
			HandlePerfectDodge();
			return;
		}
	}

	if (IsInvulnerable())
	{
		return;
	}

	CurrentHealth = FMath::Max(0.0f, CurrentHealth - DamageAmount);

	// Make getting hit unmistakable: red flash on the sprite and screen edge, shake, a number, a kick.
	HurtFlashRemaining = HurtFlashDuration;
	if (HeroVisualMesh)
	{
		HeroVisualMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashR, 1.0f);
		HeroVisualMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashG, 0.12f);
		HeroVisualMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashB, 0.08f);
	}
	AddCameraTrauma(FMath::Clamp(0.25f + DamageAmount * 0.006f, 0.25f, 0.6f));
	PushFloatingCombatPopup(GetActorLocation() + FVector(0.0, 0.0, 90.0), FString::Printf(TEXT("-%d"), FMath::RoundToInt(DamageAmount)), EDungeoneerFloatingTextKind::Damage);
	UDungeonAcousticSubsystem::PlayCombatCueAt(GetWorld(), FName(TEXT("Combat_HeroHurt_0")), GetActorLocation(), 0.9f);
	if (DamageCauser)
	{
		TriggerCombatCameraImpulse((GetActorLocation() - DamageCauser->GetActorLocation()).GetSafeNormal2D(), 16.0f, 0.0f);
	}

	if (CurrentHealth <= 0.0f)
	{
		const UZombieSpatialSubsystem* SpatialSys = GetWorld() ? GetWorld()->GetSubsystem<UZombieSpatialSubsystem>() : nullptr;
		const bool bHasAliveAlly = SpatialSys && SpatialSys->GetAliveOtherPlayerCount(this) > 0;
		CombatActionState = EDungeoneerCombatActionState::Downed;
		bWasDownedWithAliveAllies = bHasAliveAlly;
		DownedRespawnRemaining = bHasAliveAlly ? 18.0f : 1.4f;
		PushFloatingCombatPopup(
			GetActorLocation() + FVector(0.0, 0.0, 96.0),
			bHasAliveAlly ? TEXT("DOWNED! 18s [F] REVIVE") : TEXT("FALLEN!"),
			EDungeoneerFloatingTextKind::Status);
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

void ADungeoneerCharacter::ResetForNewRun()
{
	if (!HasAuthority())
	{
		return;
	}
	CurrentHealth = MaxHealth;
	FlashlightBattery = 100.0f;
	EmpoweredRemaining = 0.0f;
	EmpoweredMultiplier = 1.0f;
	DownedRespawnRemaining = 0.0f;
	CombatActionState = EDungeoneerCombatActionState::None;
	if (InventoryComponent)
	{
		InventoryComponent->AuthorityResetForNewRun();
	}
	if (SkillComponent)
	{
		SkillComponent->ResetLoadoutForNewRun();
	}
	if (SensoryPerkComponent)
	{
		SensoryPerkComponent->SetPerkLevel(EDungeoneerSensoryPerk::DarkVision, 0);
		SensoryPerkComponent->SetPerkLevel(EDungeoneerSensoryPerk::AcuteHearing, 0);
		SensoryPerkComponent->SetPerkLevel(EDungeoneerSensoryPerk::ScentThermal, 0);
	}
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
