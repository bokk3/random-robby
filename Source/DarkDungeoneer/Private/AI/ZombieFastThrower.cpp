#include "AI/ZombieFastThrower.h"
#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "Rendering/DungeoneerFxPoolSubsystem.h"
#include "Rendering/DungeoneerSpriteMaterialFactory.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Audio/DungeonAcousticSubsystem.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "AI/HazardZoneActors.h"
#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerMovementComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AZombieFastThrower::AZombieFastThrower()
{
	BaseWalkSpeed = 410.0f;
	MaxHealth = 95.0f;
	GoldKillReward = 35;
	BestiaryBitIndex = 1;
	PerceptionHearingRange = 1800.0f;
	CurrentThrowCooldown = BaseThrowCooldown;

	// Close-range knife slash for heroes who rush it: quick, weak, easy to interrupt.
	MeleeAttack.StartRangeUU = 150.0f;
	MeleeAttack.WindupSec = 0.4f;
	MeleeAttack.ReachUU = 165.0f;
	MeleeAttack.ArcMinCos = 0.3f;
	MeleeAttack.Damage = 10.0f;
	MeleeAttack.LungeSpeedUU = 320.0f;
	MeleeAttack.RecoverSec = 0.45f;
	MeleeAttack.CooldownSec = 1.8f;
	MeleeAttack.bLightHitsInterrupt = true;

	FlaskMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FlaskMesh"));
	FlaskMesh->SetupAttachment(RootComponent);
	FlaskMesh->SetUsingAbsoluteLocation(true);
	FlaskMesh->SetUsingAbsoluteRotation(true);
	FlaskMesh->SetUsingAbsoluteScale(true);
	FlaskMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FlaskMesh->SetCastShadow(false);
	FlaskMesh->SetVisibility(false);
}

void AZombieFastThrower::BeginPlay()
{
	Super::BeginPlay();
	CurrentDrainedRatio = 0.0f;
	FrameReceivedDrainedRatio = 0.0f;
	CurrentThrowCooldown = BaseThrowCooldown;
	ThrowCooldownRemaining = 0.8f;
	bWasLeechActive = false;
	FlaskPhase = EFlaskPhase::None;

	// The flask quad: plane mesh, additive spark material, upright toward the camera.
	if (FlaskMesh && !bFlaskMaterialApplied)
	{
		if (UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
		{
			FlaskMesh->SetStaticMesh(PlaneMesh);
		}
		const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
		if (UDungeoneerAssetBootstrapSubsystem* Bootstrap = GI ? GI->GetSubsystem<UDungeoneerAssetBootstrapSubsystem>() : nullptr)
		{
			FlaskMesh->SetMaterial(0, Bootstrap->CreateFxMaterial(this, FName(TEXT("FxSpark_D")), FLinearColor::White, 3.0f));
			FlaskMesh->SetWorldRotation(Bootstrap->GetCameraFacingSpritePlaneRotation());
			bFlaskMaterialApplied = true;
		}
		FlaskMesh->SetWorldScale3D(FVector(0.32, 0.32, 1.0));
		FlaskMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FxB, 1.0f); // size the custom data once
	}
}

namespace ThrowerPrivate
{
	FLinearColor PayloadColor(const EThrowPayload Payload)
	{
		switch (Payload)
		{
		case EThrowPayload::Fire:   return FLinearColor(1.0f, 0.45f, 0.1f);
		case EThrowPayload::Poison: return FLinearColor(0.45f, 1.0f, 0.25f);
		default:                    return FLinearColor(0.55f, 0.45f, 1.0f); // stillsmoke
		}
	}
}

void AZombieFastThrower::BeginFlaskThrow(const FVector& TargetLocation) noexcept
{
	if (FlaskPhase != EFlaskPhase::None)
	{
		return;
	}
	FlaskPayload = SelectNextPayload(ThrowSequenceCount++);
	FlaskPhase = EFlaskPhase::Windup;
	FlaskTimer = FlaskWindupSec;
	FlaskTarget = TargetLocation;
	FlaskDeflector = nullptr;

	// Landing marker for the whole wind-up and flight, so the hero can step out or get ready to parry.
	if (UDungeoneerFxPoolSubsystem* Fx = GetWorld() ? GetWorld()->GetSubsystem<UDungeoneerFxPoolSubsystem>() : nullptr)
	{
		Fx->SpawnGroundRing(TargetLocation - FVector(0.0, 0.0, 85.0), ThrowerPrivate::PayloadColor(FlaskPayload), 260.0f, 120.0f, FlaskWindupSec + FlaskFlightSec);
	}
	if (ZombieVisualMesh)
	{
		ZombieVisualMesh->SetRelativeScale3D(GetArchetypeVisualScale() * FVector(1.06, 0.95, 1.0));
	}
}

void AZombieFastThrower::TickFlask(const float DeltaSeconds) noexcept
{
	if (FlaskPhase == EFlaskPhase::None)
	{
		return;
	}
	FlaskTimer -= DeltaSeconds;
	const FLinearColor Color = ThrowerPrivate::PayloadColor(FlaskPayload);

	if (FlaskPhase == EFlaskPhase::Windup)
	{
		if (FlaskTimer > 0.0f)
		{
			return;
		}
		FlaskPhase = EFlaskPhase::Flight;
		FlaskTimer = FlaskFlightSec;
		FlaskStart = GetActorLocation() + FVector(0.0, 0.0, 40.0);
		FlaskLocation = FlaskStart;
		if (ZombieVisualMesh)
		{
			ZombieVisualMesh->SetRelativeScale3D(GetArchetypeVisualScale());
		}
		if (FlaskMesh)
		{
			FlaskMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FxR, Color.R);
			FlaskMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FxG, Color.G);
			FlaskMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FxB, Color.B);
			FlaskMesh->SetVisibility(true);
		}
		UDungeonAcousticSubsystem::PlayCombatCueAt(GetWorld(), FName(TEXT("Combat_Swing_2")), GetActorLocation(), 0.6f, 0.1f);
		return;
	}

	const bool bReturning = (FlaskPhase == EFlaskPhase::Returning);
	if (bReturning)
	{
		FlaskTarget = GetActorLocation() + FVector(0.0, 0.0, 40.0); // it homes in on the thrower
	}
	const float Total = bReturning ? FlaskReturnSec : FlaskFlightSec;
	const float T = FMath::Clamp(1.0f - FlaskTimer / Total, 0.0f, 1.0f);
	const double ArcHeight = (bReturning ? 70.0 : 170.0) * 4.0 * T * (1.0 - T);
	FlaskLocation = FMath::Lerp(FlaskStart, FlaskTarget, static_cast<double>(T)) + FVector(0.0, 0.0, ArcHeight);
	if (FlaskMesh)
	{
		FlaskMesh->SetWorldLocation(FlaskLocation);
	}

	// A hero parrying near the flask bats it back.
	if (!bReturning && HasAuthority())
	{
		if (UZombieSpatialSubsystem* Spatial = GetWorld() ? GetWorld()->GetSubsystem<UZombieSpatialSubsystem>() : nullptr)
		{
			for (int32 i = 0; i < Spatial->GetTrackedPlayerCount(); ++i)
			{
				ADungeoneerCharacter* Player = Spatial->GetTrackedPlayerAt(i);
				if (Player && FVector::DistSquared2D(Player->GetActorLocation(), FlaskLocation) <= FMath::Square(static_cast<double>(FlaskDeflectRadiusUU))
					&& Player->TryDeflectThrownFlask(this))
				{
					FlaskPhase = EFlaskPhase::Returning;
					FlaskTimer = FlaskReturnSec;
					FlaskStart = FlaskLocation;
					FlaskDeflector = Player;
					UDungeonAcousticSubsystem::PlayCombatCueAt(GetWorld(), FName(TEXT("Combat_Parry_0")), FlaskLocation, 0.9f);
					return;
				}
			}
		}
	}

	if (FlaskTimer > 0.0f)
	{
		return;
	}
	if (FlaskMesh)
	{
		FlaskMesh->SetVisibility(false);
	}
	FlaskPhase = EFlaskPhase::None;
	if (bReturning)
	{
		SufferReturnedFlask(FlaskDeflector.Get());
	}
	else
	{
		ExecutePooledThrow(FlaskTarget);
	}
}

void AZombieFastThrower::SufferReturnedFlask(AActor* Deflector) noexcept
{
	ApplyStun(1.1f);
	if (FlaskPayload == EThrowPayload::Fire)
	{
		ApplyBurn(14.0f, 3.0f, Deflector);
		if (UZombieSpatialSubsystem* Spatial = GetWorld() ? GetWorld()->GetSubsystem<UZombieSpatialSubsystem>() : nullptr)
		{
			Spatial->ActivatePooledScorchDecal(GetActorLocation());
		}
	}
	else if (FlaskPayload == EThrowPayload::SloMoGas)
	{
		ApplyTimeSlow(0.4f, 2.5f);
	}
	ApplyZombieDamage(35.0f, Deflector);
	UDungeonAcousticSubsystem::PlayCombatCueAt(GetWorld(), FName(TEXT("Combat_Shatter_0")), GetActorLocation(), 1.0f);
	if (OnThrowPayloadReleased.IsBound())
	{
		OnThrowPayloadReleased.Broadcast(FlaskPayload, GetActorLocation());
	}
}

void AZombieFastThrower::UpdateChronoLeechBuff(float InTotalDrainedRatio, float DeltaTime) noexcept
{
	if (InTotalDrainedRatio > 0.0f)
	{
		FrameReceivedDrainedRatio = FMath::Max(FrameReceivedDrainedRatio, InTotalDrainedRatio);
		CurrentDrainedRatio = FrameReceivedDrainedRatio;
		bWasLeechActive = true;
	}
	else if (CurrentDrainedRatio > 0.0f)
	{
		CurrentDrainedRatio = FMath::Max(0.0f, CurrentDrainedRatio - (DeltaTime / FMath::Max(DecayTime, KINDA_SMALL_NUMBER)));
		if (bWasLeechActive && CurrentDrainedRatio <= 0.01f)
		{
			bWasLeechActive = false;
			if (OnChronoLeechDecaySweep.IsBound())
			{
				OnChronoLeechDecaySweep.Broadcast();
			}
		}
	}

	GetCharacterMovement()->MaxWalkSpeed = ComputeLeechWalkSpeed(BaseWalkSpeed, CurrentDrainedRatio, SpeedStealScalar);
	CurrentThrowCooldown = ComputeLeechThrowCooldown(BaseThrowCooldown, CurrentDrainedRatio, CadenceStealScalar);
}

void AZombieFastThrower::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Decay leech buff smoothly when no gas volume pushed a drain ratio this frame
	if (FrameReceivedDrainedRatio <= 0.0f && CurrentDrainedRatio > 0.0f)
	{
		UpdateChronoLeechBuff(0.0f, DeltaSeconds);
	}
	FrameReceivedDrainedRatio = 0.0f;

	if (!IsZombieAlive() || bIsMirrorDecoy || IsStunned())
	{
		return;
	}

	UWorld* World = GetWorld();
	UZombieSpatialSubsystem* SpatialSys = World ? World->GetSubsystem<UZombieSpatialSubsystem>() : nullptr;
	if (!SpatialSys)
	{
		return;
	}

	bool bLit = false;
	ADungeoneerCharacter* TargetPlayer = SpatialSys->FindNearestTargetPlayer(GetActorLocation(), PerceptionHearingRange, bLit);
	if (!TargetPlayer)
	{
		if (const UDungeoneerSensoryWorldSubsystem* SensorySub = World->GetSubsystem<UDungeoneerSensoryWorldSubsystem>())
		{
			FDungeoneerNoiseEvent HeardNoise;
			if (SensorySub->QueryStrongestActivePulse(GetActorLocation(), 1.25f, HeardNoise) && HeardNoise.Loudness >= 1.55f)
			{
				const FVector ToNoise = HeardNoise.WorldOrigin - GetActorLocation();
				if (ToNoise.SizeSquared2D() > (95.0 * 95.0))
				{
					ECompassOctant LureOctant = CurrentHeadingOctant;
					UDungeoneerMovementComponent::QuantizeInputToOctant(FVector2D(ToNoise.X, ToNoise.Y), 1.0f, LureOctant);
					MoveAlongOctant(LureOctant, 0.90f);
				}
			}
		}
		return;
	}

	const FVector TargetLoc = TargetPlayer->GetActorLocation();
	const FVector DeltaToTarget = TargetLoc - GetActorLocation();
	const float Dist = static_cast<float>(DeltaToTarget.Size2D());

	ECompassOctant ToTargetOctant = CurrentHeadingOctant;
	UDungeoneerMovementComponent::QuantizeInputToOctant(FVector2D(DeltaToTarget.X, DeltaToTarget.Y), 1.0f, ToTargetOctant);

	// Strafe along 8 compass octants at OptimalThrowRange
	ECompassOctant MoveOctant = ToTargetOctant;
	if (Dist < OptimalThrowRange * 0.72f)
	{
		// Kite away along opposite octant
		MoveOctant = static_cast<ECompassOctant>((static_cast<uint8>(ToTargetOctant) + 4) & 7);
	}
	else if (Dist <= OptimalThrowRange * 1.15f)
	{
		// Orthogonal compass strafe
		MoveOctant = static_cast<ECompassOctant>((static_cast<uint8>(ToTargetOctant) + (StrafeSign > 0 ? 2 : 6)) & 7);
	}

	TickFlask(DeltaSeconds);
	if (IsAttackBusy() || TryStartMeleeAttack(TargetPlayer))
	{
		return; // Knife slash in progress.
	}
	if (FlaskPhase == EFlaskPhase::Windup)
	{
		CurrentHeadingOctant = ToTargetOctant;
		return; // Plants its feet to throw.
	}

	MoveAlongOctant(MoveOctant, 1.0f);
	CurrentHeadingOctant = ToTargetOctant;

	ThrowCooldownRemaining -= DeltaSeconds;
	if (ThrowCooldownRemaining <= 0.0f && Dist <= OptimalThrowRange * 1.4f && FlaskPhase == EFlaskPhase::None)
	{
		BeginFlaskThrow(TargetLoc);
		ThrowCooldownRemaining = CurrentThrowCooldown;
		if ((ThrowSequenceCount & 1) == 0)
		{
			StrafeSign = -StrafeSign;
		}
	}
}

EThrowPayload AZombieFastThrower::SelectNextPayload(int32 ThrowCount) const noexcept
{
	switch (ThrowCount % 3)
	{
	case 0:
		return EThrowPayload::SloMoGas;
	case 1:
		return EThrowPayload::Fire;
	default:
		return EThrowPayload::Poison;
	}
}

void AZombieFastThrower::ExecutePooledThrow(const FVector& TargetLocation)
{
	const EThrowPayload Payload = FlaskPayload;
	if (HasAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			if (UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
			{
				// A hero parrying near the landing spot bats the flask straight back at us.
				const int32 PlayerCount = SpatialSys->GetTrackedPlayerCount();
				for (int32 i = 0; i < PlayerCount; ++i)
				{
					ADungeoneerCharacter* Player = SpatialSys->GetTrackedPlayerAt(i);
					if (Player && FVector::DistSquared2D(Player->GetActorLocation(), TargetLocation) <= FMath::Square(240.0)
						&& Player->TryDeflectThrownFlask(this))
					{
						ApplyStun(1.1f);
						if (Payload == EThrowPayload::Fire)
						{
							ApplyBurn(14.0f, 3.0f, Player);
							SpatialSys->ActivatePooledScorchDecal(GetActorLocation());
						}
						else if (Payload == EThrowPayload::SloMoGas)
						{
							ApplyTimeSlow(0.4f, 2.5f);
						}
						ApplyZombieDamage(35.0f, Player);
						if (OnThrowPayloadReleased.IsBound())
						{
							OnThrowPayloadReleased.Broadcast(Payload, GetActorLocation());
						}
						return;
					}
				}

				EHazardPoolType PoolType = EHazardPoolType::ChronoGas;
				if (Payload == EThrowPayload::Fire)
				{
					PoolType = EHazardPoolType::Fire;
				}
				else if (Payload == EThrowPayload::Poison)
				{
					PoolType = EHazardPoolType::Poison;
				}

				SpatialSys->AcquirePooledHazard(PoolType, TargetLocation, this);
				UDungeonAcousticSubsystem::PlayCombatCueAt(GetWorld(), FName(TEXT("Combat_Shatter_0")), TargetLocation, 0.9f);
			}
		}
	}

	if (OnThrowPayloadReleased.IsBound())
	{
		OnThrowPayloadReleased.Broadcast(Payload, TargetLocation);
	}
}

void AZombieFastThrower::OnZombieDied()
{
	Super::OnZombieDied();
	FlaskPhase = EFlaskPhase::None;
	if (FlaskMesh)
	{
		FlaskMesh->SetVisibility(false);
	}
	if (CurrentDrainedRatio > 0.0f)
	{
		CurrentDrainedRatio = 0.0f;
		if (OnChronoLeechDecaySweep.IsBound())
		{
			OnChronoLeechDecaySweep.Broadcast();
		}
	}
}
