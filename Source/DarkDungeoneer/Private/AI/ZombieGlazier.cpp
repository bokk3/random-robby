#include "AI/ZombieGlazier.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Audio/DungeonAcousticSubsystem.h"
#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerMovementComponent.h"
#include "Rendering/DungeoneerFxPoolSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

AZombieGlazier::AZombieGlazier()
{
	BaseWalkSpeed = 200.0f;
	MaxHealth = 170.0f;
	PerceptionHearingRange = 1500.0f;
	GoldKillReward = 45;
	BestiaryBitIndex = 4;
	BatteryKillReward = 10.0f;
	StunResistance = 0.2f;
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
	GetCharacterMovement()->Mass = 220.0f;

	// Shield bash: slow and heavy, can't be cancelled by light hits, long recovery to punish.
	MeleeAttack.StartRangeUU = 170.0f;
	MeleeAttack.WindupSec = 0.75f;
	MeleeAttack.ReachUU = 190.0f;
	MeleeAttack.ArcMinCos = 0.35f;
	MeleeAttack.Damage = 16.0f;
	MeleeAttack.LungeSpeedUU = 420.0f;
	MeleeAttack.RecoverSec = 0.95f;
	MeleeAttack.CooldownSec = 1.4f;
	MeleeAttack.bLightHitsInterrupt = false;
}

void AZombieGlazier::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;
	GuardBrokenRemaining = 0.0f;
	BlockPoseRemaining = 0.0f;
}

void AZombieGlazier::FaceLocation(const FVector& Target) noexcept
{
	const FVector To = Target - GetActorLocation();
	if (To.SizeSquared2D() > 1.0)
	{
		UDungeoneerMovementComponent::QuantizeInputToOctant(FVector2D(To.X, To.Y), 1.0f, CurrentHeadingOctant);
	}
}

void AZombieGlazier::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	GuardBrokenRemaining = FMath::Max(0.0f, GuardBrokenRemaining - DeltaSeconds);
	BlockPoseRemaining = FMath::Max(0.0f, BlockPoseRemaining - DeltaSeconds);

	if (!IsZombieAlive() || bIsMirrorDecoy || IsStunned() || IsAttackBusy())
	{
		return;
	}
	UZombieSpatialSubsystem* Spatial = GetWorld() ? GetWorld()->GetSubsystem<UZombieSpatialSubsystem>() : nullptr;
	if (!Spatial)
	{
		return;
	}
	bool bLit = false;
	ADungeoneerCharacter* Target = Spatial->FindNearestTargetPlayer(GetActorLocation(), PerceptionHearingRange, bLit);
	if (!Target)
	{
		return;
	}
	FaceLocation(Target->GetActorLocation());
	if (TryStartMeleeAttack(Target))
	{
		return;
	}
	// Shuffles forward behind the pane, always turned toward the hero.
	if (FVector::DistSquared2D(Target->GetActorLocation(), GetActorLocation()) > FMath::Square(120.0))
	{
		MoveAlongOctant(CurrentHeadingOctant, 0.8f);
	}
}

bool AZombieGlazier::TryBlockMeleeHit(const FVector& AttackerLocation, const float StunSeconds) noexcept
{
	if (!IsZombieAlive() || IsStunned() || IsGuardBroken() || GetAttackPhase() == EZombieAttackPhase::Recover)
	{
		return false; // no guard while reeling or after a bash
	}
	const FVector Facing = DungeoneerMath::OctantToDirection3D(CurrentHeadingOctant).GetSafeNormal2D();
	const FVector ToAttacker = (AttackerLocation - GetActorLocation()).GetSafeNormal2D();
	if (FVector::DotProduct(Facing, ToAttacker) < FrontMinCos)
	{
		return false; // from the side or behind: the pane doesn't cover it
	}
	if (StunSeconds >= GuardBreakStunSec)
	{
		// A heavy enough blow cracks the pane: the hit lands and the guard stays down for a while.
		GuardBrokenRemaining = GuardBrokenSec;
		InterruptAttack();
		UDungeonAcousticSubsystem::PlayCombatCueAt(GetWorld(), FName(TEXT("Combat_Shatter_0")), GetActorLocation(), 1.0f, 0.05f);
		return false;
	}

	BlockPoseRemaining = 0.3f;
	if (UDungeoneerFxPoolSubsystem* Fx = GetWorld() ? GetWorld()->GetSubsystem<UDungeoneerFxPoolSubsystem>() : nullptr)
	{
		Fx->SpawnHitSparks(GetActorLocation() + Facing * 60.0 + FVector(0.0, 0.0, 50.0), Facing, FLinearColor(0.6f, 0.85f, 1.0f), 5, 380.0f);
	}
	UDungeonAcousticSubsystem::PlayCombatCueAt(GetWorld(), FName(TEXT("Combat_Parry_0")), GetActorLocation(), 0.8f, 0.1f);
	return true;
}

bool AZombieGlazier::ResolveAttackFrame(const int32 FacingRow, int32& OutRow, int32& OutCol) const noexcept
{
	// Sheet columns: braced behind the shield, pulling back, bash, shattered (death).
	OutRow = FacingRow;
	if (!IsZombieAlive())
	{
		OutCol = 3;
		return true;
	}
	const float Windup = FMath::Max(MeleeAttack.WindupSec * (IsElite() ? 0.75f : 1.0f), 0.01f);
	if (BlockPoseRemaining > 0.0f || (GetAttackPhase() == EZombieAttackPhase::Windup && GetAttackPhaseRemaining() > Windup * 0.5f))
	{
		OutCol = 0;
		return true;
	}
	if (GetAttackPhase() == EZombieAttackPhase::Windup)
	{
		OutCol = 1;
		return true;
	}
	if (GetAttackPhase() == EZombieAttackPhase::Recover && GetAttackPhaseRemaining() > MeleeAttack.RecoverSec * 0.5f)
	{
		OutCol = 2;
		return true;
	}
	return false;
}
