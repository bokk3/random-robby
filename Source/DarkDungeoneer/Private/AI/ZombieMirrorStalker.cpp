#include "AI/ZombieMirrorStalker.h"
#include "Rendering/DungeoneerSpriteMaterialFactory.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerMovementComponent.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "Rendering/SensoryPerkComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AZombieMirrorStalker::AZombieMirrorStalker()
{
	BaseWalkSpeed = 310.0f;
	MaxHealth = 95.0f;
	PerceptionHearingRange = 1750.0f;
	StepCycleDistanceUU = 95.0f;
	GoldKillReward = 35;
	BestiaryBitIndex = 2;
	BatteryKillReward = 12.0f;
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;

	// Quick lunge-bite about every 1.4 s: shorter wind-up than the others, but it can be parried.
	MeleeAttack.StartRangeUU = 150.0f;
	MeleeAttack.WindupSec = 0.45f;
	MeleeAttack.ReachUU = 160.0f;
	MeleeAttack.ArcMinCos = 0.3f;
	MeleeAttack.Damage = 12.0f;
	MeleeAttack.LungeSpeedUU = 620.0f;
	MeleeAttack.RecoverSec = 0.5f;
	MeleeAttack.CooldownSec = 0.9f;
	MeleeAttack.bLightHitsInterrupt = true;
}

void AZombieMirrorStalker::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;
	RevealHoldRemaining = 0.0f;
	RevealAlpha = 0.18f;
	bRevealedByLight = false;
	if (ZombieEmissiveLight)
	{
		ZombieEmissiveLight->SetIntensity(0.0f);
	}
}

void AZombieMirrorStalker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

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

	const FVector SelfLoc = GetActorLocation();
	bool bLitNow = false;
	const int32 PlayerCount = SpatialSys->GetTrackedPlayerCount();
	for (int32 i = 0; i < PlayerCount; ++i)
	{
		if (const ADungeoneerCharacter* Player = SpatialSys->GetTrackedPlayerAt(i))
		{
			if (Player->IsPointLitByFlashlight(SelfLoc))
			{
				bLitNow = true;
				break;
			}
			if (const USensoryPerkComponent* Perks = Player->GetSensoryPerkComponent())
			{
				if (Perks->GetPerkLevel(EDungeoneerSensoryPerk::DarkVision) >= 2 &&
					FVector::DistSquared2D(Player->GetActorLocation(), SelfLoc) <= FMath::Square(static_cast<double>(DarkVisionRevealRadiusUU)))
				{
					bLitNow = true;
					break;
				}
			}
		}
	}

	if (bLitNow)
	{
		RevealHoldRemaining = RevealFadeOutSeconds;
	}
	else if (RevealHoldRemaining > 0.0f)
	{
		RevealHoldRemaining = FMath::Max(0.0f, RevealHoldRemaining - DeltaSeconds);
	}

	bRevealedByLight = (RevealHoldRemaining > 0.0f);
	const float TargetAlpha = bRevealedByLight ? 1.0f : 0.18f;
	RevealAlpha = FMath::FInterpTo(RevealAlpha, TargetAlpha, DeltaSeconds, 8.0f);

	if (ZombieVisualMesh)
	{
		const FVector BaseScale = GetArchetypeVisualScale();
		// A Slip exists only in the light: it dithers out of sight instead of shrinking.
		ZombieVisualMesh->SetRelativeScale3D(BaseScale * (0.94f + 0.06f * RevealAlpha));
		ZombieVisualMesh->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FadeOut, 1.0f - FMath::Max(RevealAlpha, 0.05f));
	}
	if (ZombieEmissiveLight)
	{
		ZombieEmissiveLight->SetIntensity(bRevealedByLight ? (420.0f * RevealAlpha) : 0.0f);
	}

	// Pursue nearest lit/audible player cleanly along 8 compass headings
	bool bOutLit = false;
	if (ADungeoneerCharacter* TargetPlayer = SpatialSys->FindNearestTargetPlayer(SelfLoc, PerceptionHearingRange, bOutLit))
	{
		const FVector ToTarget = TargetPlayer->GetActorLocation() - SelfLoc;
		const double DistSq = ToTarget.SizeSquared2D();
		if (IsAttackBusy() || TryStartMeleeAttack(TargetPlayer))
		{
			return;
		}
		if (DistSq > FMath::Square(68.0))
		{
			ECompassOctant DesiredOctant = CurrentHeadingOctant;
			UDungeoneerMovementComponent::QuantizeInputToOctant(FVector2D(ToTarget.X, ToTarget.Y), 1.0f, DesiredOctant);
			MoveAlongOctant(DesiredOctant, bRevealedByLight ? 0.85f : 1.05f);
		}
	}
}

bool AZombieMirrorStalker::ResolveAttackFrame(const int32 FacingRow, int32& OutRow, int32& OutCol) const noexcept
{
	// Sheet columns: crouch, lunge, bite at full stretch, shattering into shards (death).
	OutRow = FacingRow;
	if (!IsZombieAlive())
	{
		OutCol = 3;
		return true;
	}
	if (GetAttackPhase() == EZombieAttackPhase::Windup)
	{
		const float Windup = FMath::Max(MeleeAttack.WindupSec * (IsElite() ? 0.75f : 1.0f), 0.01f);
		OutCol = (GetAttackPhaseRemaining() > Windup * 0.4f) ? 0 : 1;
		return true;
	}
	if (GetAttackPhase() == EZombieAttackPhase::Recover && GetAttackPhaseRemaining() > MeleeAttack.RecoverSec * 0.5f)
	{
		OutCol = 2;
		return true;
	}
	return false;
}
