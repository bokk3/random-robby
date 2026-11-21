#include "AI/ZombieBase.h"
#include "Rendering/DungeoneerFxPoolSubsystem.h"
#include "Rendering/DungeoneerSpriteMaterialFactory.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Audio/DungeonAcousticSubsystem.h"
#include "Ecosystem/DungeonWaterSubsystem.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "Rendering/SensoryPerkComponent.h"
#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "Core/DungeoneerMovementComponent.h"
#include "Core/DungeoneerCharacter.h"
#include "Social/DungeoneerInventoryComponent.h"
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
	ZombieVisualMesh->SetCastShadow(false);

	ZombieEmissiveLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ZombieEmissiveLight"));
	ZombieEmissiveLight->SetupAttachment(RootComponent);
	ZombieEmissiveLight->SetRelativeLocation(FVector(-45.0, 0.0, 65.0));
	ZombieEmissiveLight->SetIntensityUnits(ELightUnits::Lumens);
	ZombieEmissiveLight->SetIntensity(120.0f);
	ZombieEmissiveLight->SetAttenuationRadius(360.0f);
	ZombieEmissiveLight->CastShadows = false;

	DirectionalZombieMIDs.Reserve(16);
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
	ZombieVisualMesh->SetCastShadow(false);

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

					// Walk cells 0-15 (a redesigned walk sheet wins when present), attack cells 16-31 when available.
					auto HasCells = [Bootstrap](const TCHAR* CellPrefix)
					{
						return CellPrefix && Bootstrap->GetExtractedCellTexture(FName(*FString::Printf(TEXT("%s_R0_C0_D"), CellPrefix))) != nullptr;
					};
					const TCHAR* Redesign = GetRedesignedWalkPrefix();
					const bool bRedesignedWalk = HasCells(Redesign);
					const TCHAR* WalkPrefix = bRedesignedWalk ? Redesign : GetArchetypeKeyPrefix();
					const TCHAR* AttackPrefix = GetAttackSheetPrefix();
					bHasAttackFrames = HasCells(AttackPrefix) && (!AttackSheetNeedsRedesignedWalk() || bRedesignedWalk);

					DirectionalZombieMIDs.Reset();
					DirectionalZombieMIDs.SetNum(bHasAttackFrames ? 32 : 16);
					for (int32 Sheet = 0; Sheet < (bHasAttackFrames ? 2 : 1); ++Sheet)
					{
						const TCHAR* Prefix = (Sheet == 0) ? WalkPrefix : AttackPrefix;
						for (int32 RowIdx = 0; RowIdx < 4; ++RowIdx)
						{
							for (int32 ColIdx = 0; ColIdx < 4; ++ColIdx)
							{
								const FName DiffKey(*FString::Printf(TEXT("%s_R%d_C%d_D"), Prefix, RowIdx, ColIdx));
								const FName NormKey(*FString::Printf(TEXT("%s_R%d_C%d_N"), Prefix, RowIdx, ColIdx));
								DirectionalZombieMIDs[Sheet * 16 + RowIdx * 4 + ColIdx] = Bootstrap->CreateDungeonOrRigMaterial(
									this, DiffKey, NormKey, Tint, 0.45f, false, true);
							}
						}
					}

					bTexturedVisualsApplied = true;
					ActiveDirectionalRow = -1;
					ActiveWalkFrameColumn = 0;
					ActiveDirectionalCellIndex = -1;
					UpdateZombieDirectionalVisual(CurrentHeadingOctant, ActiveWalkFrameColumn);
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

bool AZombieBase::ResolveAttackFrame(const int32 FacingRow, int32& OutRow, int32& OutCol) const noexcept
{
	OutRow = FacingRow;
	if (!IsZombieAlive())
	{
		if (!UsesDeathFrame())
		{
			return false;
		}
		OutCol = 3;
		return true;
	}
	if (AttackPhase == EZombieAttackPhase::Windup)
	{
		OutCol = 0;
		return true;
	}
	if (AttackPhase == EZombieAttackPhase::Recover && AttackPhaseRemaining > MeleeAttack.RecoverSec * 0.4f)
	{
		OutCol = 1; // the strike pose holds through the first part of the recovery
		return true;
	}
	return false;
}

void AZombieBase::AdvanceWalkFlipbookByDistance(float DeltaDistanceUU, bool bIsMoving) noexcept
{
	const int32 PrevColumn = ActiveWalkFrameColumn;
	const float ClampedDist = FMath::Max(DeltaDistanceUU, 0.0f);
	if (bIsMoving || ClampedDist > 0.1f)
	{
		WalkCycleDistanceUU += ClampedDist;
		const float FullStrideCycleUU = FMath::Max(StepCycleDistanceUU * 2.0f, 40.0f);
		ActiveWalkFrameColumn = FMath::FloorToInt((WalkCycleDistanceUU / FullStrideCycleUU) * 4.0f) & 3;
	}
	else
	{
		// Lock to Column 0 (Contact / Idle Pose A) when stationary
		ActiveWalkFrameColumn = 0;
	}

	if (ActiveWalkFrameColumn != PrevColumn)
	{
		if ((ActiveWalkFrameColumn == 1 || ActiveWalkFrameColumn == 3) && !bIsMirrorDecoy)
		{
			++WalkStepSequenceCount;
			if (const UWorld* World = GetWorld())
			{
				if (UDungeonWaterSubsystem* WaterSub = World->GetSubsystem<UDungeonWaterSubsystem>())
				{
					WaterSub->NotifyWalkerStep(this, GetActorLocation(), CurrentHeadingOctant, false);
				}
			}
		}
		OnWalkColumnChanged.Broadcast(ActiveWalkFrameColumn);
	}
}

void AZombieBase::UpdateZombieDirectionalVisual(ECompassOctant HeadingOctant, int32 FrameColumn) noexcept
{
	if (!ZombieVisualMesh)
	{
		return;
	}

	const int32 DesiredRow = static_cast<int32>(DungeoneerMath::OctantToScreenDirectionalRowForCameraYaw(HeadingOctant, -45.0f));
	const int32 DesiredCol = FMath::Clamp((FrameColumn >= 0) ? FrameColumn : ActiveWalkFrameColumn, 0, 3);
	int32 DesiredCell = DesiredRow * 4 + DesiredCol;
	int32 AttackRow = 0;
	int32 AttackCol = 0;
	if (bHasAttackFrames && ResolveAttackFrame(DesiredRow, AttackRow, AttackCol))
	{
		DesiredCell = 16 + FMath::Clamp(AttackRow, 0, 3) * 4 + FMath::Clamp(AttackCol, 0, 3);
	}
	if (DesiredCell == ActiveDirectionalCellIndex)
	{
		return;
	}

	if (DirectionalZombieMIDs.IsValidIndex(DesiredCell) && DirectionalZombieMIDs[DesiredCell])
	{
		ZombieVisualMesh->SetMaterial(0, DirectionalZombieMIDs[DesiredCell].Get());
		ActiveDirectionalRow = DesiredRow;
		if (DesiredCell < 16)
		{
			ActiveWalkFrameColumn = DesiredCol;
		}
		ActiveDirectionalCellIndex = DesiredCell;
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
		if (UDungeonAcousticSubsystem* AcousticSys = World->GetSubsystem<UDungeonAcousticSubsystem>())
		{
			AcousticSys->RegisterWalker(this);
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

	if (IsZombieAlive() && !bIsMirrorDecoy)
	{
		// DeltaSeconds already includes CustomTimeDilation; status timers run on real time.
		TickStatusEffects(DeltaSeconds / FMath::Max(CustomTimeDilation, 0.01f));
		if (!IsZombieAlive())
		{
			UpdateZombieDirectionalVisual(CurrentHeadingOctant, ActiveWalkFrameColumn);
			return;
		}

		AdvanceDeterministicStepAccumulator(DeltaSeconds);
		if (!IsStunned())
		{
			TickMeleeAttack(DeltaSeconds);
		}
		if (HasAuthority() && !IsAttackBusy())
		{
			ApplySeparation();
		}

		if (HitFlashRemaining > 0.0f)
		{
			HitFlashRemaining = FMath::Max(0.0f, HitFlashRemaining - DeltaSeconds);
			const float Alpha = HitFlashRemaining / 0.16f;
			const FVector BaseScale = GetArchetypeVisualScale();
			if (ZombieVisualMesh)
			{
				ZombieVisualMesh->SetRelativeScale3D(BaseScale * (1.0f + 0.22f * Alpha));
				ZombieVisualMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashAmount, Alpha);
			}
			if (ZombieEmissiveLight)
			{
				ZombieEmissiveLight->SetIntensity(240.0f + 320.0f * Alpha);
			}
		}

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
	else if (DeathHideCountdown > 0.0f)
	{
		SecondsSinceDeath += DeltaSeconds;
		DeathHideCountdown = FMath::Max(0.0f, DeathHideCountdown - DeltaSeconds);
		if (DeathHideCountdown <= 0.0f)
		{
			SetActorHiddenInGame(true);
		}
	}

	UpdateZombieDirectionalVisual(CurrentHeadingOctant, ActiveWalkFrameColumn);
}

void AZombieBase::MoveAlongOctant(ECompassOctant Octant, float Scale) noexcept
{
	CurrentHeadingOctant = Octant;
	UpdateZombieDirectionalVisual(Octant, ActiveWalkFrameColumn);
	const FVector UnitDir = DungeoneerMath::OctantToDirection3D(Octant);
	AddMovementInput(UnitDir, Scale);
}

void AZombieBase::TriggerHitFeedback(const FVector& KnockbackDirXY, const float KnockbackSpeedUU) noexcept
{
	if (!IsZombieAlive())
	{
		return;
	}

	HitFlashRemaining = 0.16f;
	// Poise: small enemies lose their wind-up when hit.
	if (AttackPhase == EZombieAttackPhase::Windup && MeleeAttack.bLightHitsInterrupt)
	{
		InterruptAttack();
	}
	const FVector SafeDir = KnockbackDirXY.GetSafeNormal2D();
	if (!SafeDir.IsNearlyZero() && KnockbackSpeedUU > 0.0f)
	{
		if (UCharacterMovementComponent* CharMove = GetCharacterMovement())
		{
			CharMove->Velocity += SafeDir * KnockbackSpeedUU;
		}
	}
}

void AZombieBase::ApplyStun(const float Seconds) noexcept
{
	if (!IsZombieAlive() || Seconds <= 0.0f)
	{
		return;
	}
	const float Scaled = Seconds * (1.0f - FMath::Clamp(StunResistance, 0.0f, 1.0f));
	StunRemaining = FMath::Max(StunRemaining, Scaled);
	if (StunRemaining > 0.0f)
	{
		InterruptAttack();
		if (UCharacterMovementComponent* CharMove = GetCharacterMovement())
		{
			CharMove->StopActiveMovement();
		}
	}
}

void AZombieBase::ApplyBurn(const float DamagePerSecond, const float Seconds, AActor* InBurnInstigator) noexcept
{
	if (!IsZombieAlive() || DamagePerSecond <= 0.0f || Seconds <= 0.0f)
	{
		return;
	}
	BurnRemaining = FMath::Max(BurnRemaining, Seconds);
	BurnDamagePerSecond = FMath::Max(BurnDamagePerSecond, DamagePerSecond);
	if (InBurnInstigator)
	{
		BurnInstigator = InBurnInstigator;
	}
}

void AZombieBase::ApplyTimeSlow(const float Dilation, const float Seconds) noexcept
{
	if (!IsZombieAlive() || Seconds <= 0.0f)
	{
		return;
	}
	const bool bWasSlowed = TimeSlowRemaining > 0.0f;
	TimeSlowRemaining = FMath::Max(TimeSlowRemaining, Seconds);
	TimeSlowDilation = FMath::Clamp(bWasSlowed ? FMath::Min(TimeSlowDilation, Dilation) : Dilation, 0.05f, 1.0f);
	CustomTimeDilation = FMath::Clamp(FMath::Min(CustomTimeDilation, Dilation), 0.05f, 1.0f);
}

bool AZombieBase::TryStartMeleeAttack(ADungeoneerCharacter* Target) noexcept
{
	if (!Target || !IsZombieAlive() || bIsMirrorDecoy || IsStunned() || AttackPhase != EZombieAttackPhase::None
		|| AttackCooldownRemaining > 0.0f || Target->IsDowned())
	{
		return false;
	}
	const FVector ToTarget = Target->GetActorLocation() - GetActorLocation();
	if (ToTarget.SizeSquared2D() > FMath::Square(static_cast<double>(MeleeAttack.StartRangeUU)))
	{
		return false;
	}
	UWorld* World = GetWorld();
	UZombieSpatialSubsystem* Spatial = World ? World->GetSubsystem<UZombieSpatialSubsystem>() : nullptr;
	if (!Spatial || !Spatial->TryAcquireAttackToken(Target))
	{
		return false;
	}
	AttackTokenHero = Target;
	AttackDirXY = ToTarget.GetSafeNormal2D();
	if (AttackDirXY.IsNearlyZero())
	{
		AttackDirXY = FVector::ForwardVector;
	}
	AttackPhase = EZombieAttackPhase::Windup;
	AttackPhaseRemaining = MeleeAttack.WindupSec * (bIsElite ? 0.75f : 1.0f);
	if (UCharacterMovementComponent* CharMove = GetCharacterMovement())
	{
		CharMove->StopMovementImmediately();
	}
	UDungeoneerMovementComponent::QuantizeInputToOctant(FVector2D(AttackDirXY.X, AttackDirXY.Y), 1.0f, CurrentHeadingOctant);

	// Telegraph: a ring on the floor where the blow will land, closing in until it hits.
	if (UDungeoneerFxPoolSubsystem* Fx = World->GetSubsystem<UDungeoneerFxPoolSubsystem>())
	{
		const FVector Floor = GetActorLocation() + AttackDirXY * (MeleeAttack.ReachUU * 0.55) - FVector(0.0, 0.0, 85.0);
		Fx->SpawnGroundRing(Floor, FLinearColor(1.0f, 0.22f, 0.08f), MeleeAttack.ReachUU * 1.3f, MeleeAttack.ReachUU * 0.45f, AttackPhaseRemaining);
	}
	if (ZombieVisualMesh)
	{
		ZombieVisualMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashR, 1.0f);
		ZombieVisualMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashG, 0.25f);
		ZombieVisualMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashB, 0.1f);
	}
	UDungeonAcousticSubsystem::PlayCombatCueAt(World, FName(TEXT("Combat_Windup_0")), GetActorLocation(), 0.8f, 0.08f);
	return true;
}

void AZombieBase::InterruptAttack() noexcept
{
	if (AttackPhase == EZombieAttackPhase::None)
	{
		return;
	}
	AttackPhase = EZombieAttackPhase::None;
	AttackPhaseRemaining = 0.0f;
	AttackCooldownRemaining = FMath::Max(AttackCooldownRemaining, 1.0f);
	if (UZombieSpatialSubsystem* Spatial = GetWorld() ? GetWorld()->GetSubsystem<UZombieSpatialSubsystem>() : nullptr)
	{
		Spatial->ReleaseAttackToken(AttackTokenHero.Get());
	}
	AttackTokenHero = nullptr;
	if (ZombieVisualMesh)
	{
		// Back to a white hit flash.
		ZombieVisualMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashR, 0.0f);
		ZombieVisualMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashG, 0.0f);
		ZombieVisualMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashB, 0.0f);
		ZombieVisualMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashAmount, 0.0f);
	}
}

void AZombieBase::TickMeleeAttack(const float DeltaSeconds) noexcept
{
	if (AttackCooldownRemaining > 0.0f)
	{
		AttackCooldownRemaining = FMath::Max(0.0f, AttackCooldownRemaining - DeltaSeconds);
	}
	if (AttackPhase == EZombieAttackPhase::Windup)
	{
		AttackPhaseRemaining -= DeltaSeconds;
		const float WindupTotal = FMath::Max(MeleeAttack.WindupSec * (bIsElite ? 0.75f : 1.0f), 0.01f);
		const float Alpha = FMath::Clamp(1.0f - AttackPhaseRemaining / WindupTotal, 0.0f, 1.0f);
		if (ZombieVisualMesh && HitFlashRemaining <= 0.0f)
		{
			// Red glow builds toward the strike; a rear-back squash sells the coil.
			ZombieVisualMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashAmount, 0.1f + 0.55f * Alpha * Alpha);
			ZombieVisualMesh->SetRelativeScale3D(GetArchetypeVisualScale() * FVector(1.0f + 0.06f * Alpha, 1.0f - 0.04f * Alpha, 1.0f));
		}
		if (AttackPhaseRemaining > 0.0f)
		{
			return;
		}

		// Strike: lunge and one hit test in the wind-up direction.
		if (UCharacterMovementComponent* CharMove = GetCharacterMovement())
		{
			CharMove->Velocity += AttackDirXY * MeleeAttack.LungeSpeedUU;
		}
		bool bHit = false;
		if (HasAuthority())
		{
			if (UZombieSpatialSubsystem* Spatial = GetWorld() ? GetWorld()->GetSubsystem<UZombieSpatialSubsystem>() : nullptr)
			{
				const FVector Origin = GetActorLocation();
				for (int32 i = 0; i < Spatial->GetTrackedPlayerCount(); ++i)
				{
					ADungeoneerCharacter* Hero = Spatial->GetTrackedPlayerAt(i);
					if (!Hero || Hero->IsDowned())
					{
						continue;
					}
					const FVector Delta = Hero->GetActorLocation() - Origin;
					const double Dist = Delta.Size2D();
					if (Dist > MeleeAttack.ReachUU)
					{
						continue;
					}
					const FVector DirToHero = (Dist > 1.0) ? Delta.GetSafeNormal2D() : AttackDirXY;
					if (FVector::DotProduct(DirToHero, AttackDirXY) >= MeleeAttack.ArcMinCos || Dist < 60.0)
					{
						Hero->ApplyDungeoneerDamage(MeleeAttack.Damage * DamageScale, this);
						bHit = true;
					}
				}
				Spatial->ReleaseAttackToken(AttackTokenHero.Get());
			}
		}
		AttackTokenHero = nullptr;
		UDungeonAcousticSubsystem::PlayCombatCueAt(GetWorld(), FName(TEXT("Combat_Swing_1")), GetActorLocation(), 0.85f, 0.0f);
		AttackPhase = EZombieAttackPhase::Recover;
		AttackPhaseRemaining = MeleeAttack.RecoverSec;
		if (ZombieVisualMesh)
		{
			ZombieVisualMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashAmount, 0.0f);
			ZombieVisualMesh->SetRelativeScale3D(GetArchetypeVisualScale() * FVector(0.92f, 1.08f, 1.0f));
		}
		OnMeleeStrike(bHit);
		return;
	}
	if (AttackPhase == EZombieAttackPhase::Recover)
	{
		AttackPhaseRemaining -= DeltaSeconds;
		if (AttackPhaseRemaining <= 0.0f)
		{
			AttackPhase = EZombieAttackPhase::None;
			AttackCooldownRemaining = MeleeAttack.CooldownSec;
			if (ZombieVisualMesh)
			{
				ZombieVisualMesh->SetRelativeScale3D(GetArchetypeVisualScale());
				ZombieVisualMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashR, 0.0f);
				ZombieVisualMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashG, 0.0f);
				ZombieVisualMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashB, 0.0f);
			}
		}
	}
}

void AZombieBase::ApplySeparation() noexcept
{
	const UZombieSpatialSubsystem* Spatial = GetWorld() ? GetWorld()->GetSubsystem<UZombieSpatialSubsystem>() : nullptr;
	if (!Spatial)
	{
		return;
	}
	const FVector Self = GetActorLocation();
	FVector Push = FVector::ZeroVector;
	for (int32 i = 0; i < Spatial->GetTrackedZombieCount(); ++i)
	{
		const AZombieBase* Other = Spatial->GetTrackedZombieAt(i);
		if (!Other || Other == this || !Other->IsZombieAlive())
		{
			continue;
		}
		const FVector Away(Self.X - Other->GetActorLocation().X, Self.Y - Other->GetActorLocation().Y, 0.0);
		const double DistSq = Away.SizeSquared();
		if (DistSq < 95.0 * 95.0 && DistSq > 1.0)
		{
			const double Dist = FMath::Sqrt(DistSq);
			Push += Away / Dist * (1.0 - Dist / 95.0);
		}
	}
	if (!Push.IsNearlyZero())
	{
		AddMovementInput(Push.GetClampedToMaxSize(1.0), 0.6f);
	}
}

void AZombieBase::ApplyFloorScaling(const int32 FloorIndex) noexcept
{
	const float Steps = static_cast<float>(FMath::Max(FloorIndex, 1) - 1);
	MaxHealth *= 1.0f + 0.2f * Steps;
	CurrentHealth = MaxHealth;
	DamageScale *= 1.0f + 0.1f * Steps;
}

void AZombieBase::MakeElite() noexcept
{
	if (bIsElite)
	{
		return;
	}
	bIsElite = true;
	MaxHealth *= 1.6f;
	CurrentHealth = MaxHealth;
	DamageScale *= 1.25f;
	GoldKillReward *= 2;
	if (ZombieVisualMesh)
	{
		ZombieVisualMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::GlowPulse, 0.6f);
	}
	if (ZombieEmissiveLight)
	{
		ZombieEmissiveLight->SetLightColor(FLinearColor(1.0f, 0.8f, 0.3f));
	}
}

void AZombieBase::ApplyHitStop(const float RealSeconds) noexcept
{
	if (!IsZombieAlive() || RealSeconds <= 0.0f)
	{
		return;
	}
	HitStopRemaining = FMath::Max(HitStopRemaining, RealSeconds);
	CustomTimeDilation = 0.05f;
}

void AZombieBase::TickStatusEffects(const float RealDeltaSeconds) noexcept
{
	if (StunRemaining > 0.0f)
	{
		StunRemaining = FMath::Max(0.0f, StunRemaining - RealDeltaSeconds);
		if (ZombieVisualMesh)
		{
			// Dazed sway so a stunned zombie reads clearly at a glance.
			const float Sway = (StunRemaining > 0.0f) ? FMath::Sin(StunRemaining * 22.0f) * 7.0f : 0.0f;
			ZombieVisualMesh->SetRelativeLocation(FVector(Sway, 0.0, 8.0));
		}
	}

	if (BurnRemaining > 0.0f)
	{
		const float Step = FMath::Min(RealDeltaSeconds, BurnRemaining);
		BurnRemaining = FMath::Max(0.0f, BurnRemaining - RealDeltaSeconds);
		if (HasAuthority())
		{
			ApplyZombieDamage(BurnDamagePerSecond * Step, BurnInstigator.Get());
		}
		if (ZombieEmissiveLight)
		{
			ZombieEmissiveLight->SetLightColor(BurnRemaining > 0.0f ? FLinearColor(1.0f, 0.42f, 0.10f) : GetArchetypeVisualTint());
		}
		if (BurnRemaining <= 0.0f)
		{
			BurnDamagePerSecond = 0.0f;
		}
	}

	if (TimeSlowRemaining > 0.0f)
	{
		TimeSlowRemaining = FMath::Max(0.0f, TimeSlowRemaining - RealDeltaSeconds);
		if (TimeSlowRemaining <= 0.0f)
		{
			TimeSlowDilation = 1.0f;
		}
	}

	// Hit-stop wins over time slow while it lasts; afterwards the slow (if any) applies again.
	if (HitStopRemaining > 0.0f)
	{
		HitStopRemaining = FMath::Max(0.0f, HitStopRemaining - RealDeltaSeconds);
	}
	CustomTimeDilation = (HitStopRemaining > 0.0f) ? 0.05f : ((TimeSlowRemaining > 0.0f) ? TimeSlowDilation : 1.0f);
}

void AZombieBase::AdvanceDeterministicStepAccumulator(float DeltaSeconds) noexcept
{
	const FVector CurrentLocation = GetActorLocation();
	const double DistSq = FVector::DistSquared2D(PreviousLocation, CurrentLocation);
	PreviousLocation = CurrentLocation;

	if (DistSq <= static_cast<double>(KINDA_SMALL_NUMBER))
	{
		AdvanceWalkFlipbookByDistance(0.0f, GetVelocity().SizeSquared2D() > 25.0);
		return;
	}

	const float DeltaDistUU = static_cast<float>(FMath::Sqrt(DistSq));
	AdvanceWalkFlipbookByDistance(DeltaDistUU, true);
	StepDistanceAccumulator += DeltaDistUU;
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

	if (DamageInstigator)
	{
		LastDamageInstigator = DamageInstigator;
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
	DeathHideCountdown = 0.65f;
	InterruptAttack();
	CustomTimeDilation = 1.0f;
	HitStopRemaining = 0.0f;
	if (ZombieVisualMesh)
	{
		ZombieVisualMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FlashAmount, 0.0f);
	}
	if (UDungeoneerFxPoolSubsystem* Fx = GetWorld() ? GetWorld()->GetSubsystem<UDungeoneerFxPoolSubsystem>() : nullptr)
	{
		const FLinearColor Tint = GetArchetypeVisualTint();
		Fx->SpawnBurst(GetActorLocation() + FVector(0.0, 0.0, 40.0), FLinearColor(0.5f + Tint.R * 0.5f, 0.5f + Tint.G * 0.5f, 0.5f + Tint.B * 0.5f), 9, 300.0f, 0.36f);
		Fx->SpawnDust(GetActorLocation() - FVector(0.0, 0.0, 85.0));
	}
	UDungeonAcousticSubsystem::PlayCombatCueAt(GetWorld(), FName(TEXT("Combat_ZombieDeath_0")), GetActorLocation(), 0.85f, 0.1f);

	if (ZombieVisualMesh && bHasAttackFrames && UsesDeathFrame())
	{
		DeathHideCountdown = 1.4f; // the death frame plays upright, then the body is gone
		ZombieVisualMesh->SetRelativeScale3D(GetArchetypeVisualScale());
	}
	else if (ZombieVisualMesh)
	{
		// No death art: fold the sprite flat onto the floor.
		ZombieVisualMesh->SetRelativeLocation(FVector(0.0, 0.0, -48.0));
		ZombieVisualMesh->SetRelativeRotation(FRotator(85.0f, 0.0f, 0.0f));
	}
	if (ZombieEmissiveLight)
	{
		ZombieEmissiveLight->SetIntensity(0.0f);
	}

	if (!bKillRewardsGranted)
	{
		bKillRewardsGranted = true;
		if (ADungeoneerCharacter* Slayer = Cast<ADungeoneerCharacter>(LastDamageInstigator.Get()))
		{
			Slayer->RegisterEnemySlain(GoldKillReward, BatteryKillReward, static_cast<uint8>(1u << (BestiaryBitIndex & 7u)));
		}
	}
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
