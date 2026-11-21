#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Core/DungeoneerTypes.h"
#include "ZombieBase.generated.h"

class ADungeoneerCharacter;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UStaticMeshComponent;
class UZombieSpatialSubsystem;

/**
 * One telegraphed melee attack. Wind-up is the warning (a closing floor ring, a red glow, a sound);
 * the strike is a short lunge plus one hit test; recover is the opening to punish. Strikes go through
 * ADungeoneerCharacter::ApplyDungeoneerDamage, so parry and perfect dodge answer them.
 */
struct FZombieMeleeAttackDef
{
	float StartRangeUU = 170.0f;   // starts winding up when the target is this close
	float WindupSec = 0.6f;
	float ReachUU = 190.0f;        // hit test distance at the strike
	float ArcMinCos = 0.2f;        // hit test cone around the wind-up direction
	float Damage = 14.0f;
	float LungeSpeedUU = 520.0f;
	float RecoverSec = 0.7f;
	float CooldownSec = 1.6f;
	bool bLightHitsInterrupt = true; // any hit during the wind-up cancels it (small enemies)
};

enum class EZombieAttackPhase : uint8
{
	None,
	Windup,
	Recover
};

/**
 * Base 2.5D Zombie Archetype sharing 4-axis/8-compass quantized stepping,
 * zero-allocation spatial registration, and deterministic step accumulator hooks.
 */
UCLASS(Abstract)
class DARKDUNGEONEER_API AZombieBase : public ACharacter
{
	GENERATED_BODY()

public:
	AZombieBase();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;

	/** Ensures the visible 2.5D placeholder/textured mesh and emissive light are initialized cleanly. */
	void EnsureVisualMeshInitialized();

	UFUNCTION(BlueprintPure, Category = "Zombie|Visual")
	UStaticMeshComponent* GetZombieVisualMesh() const noexcept { return ZombieVisualMesh.Get(); }

	/** Evaluates whether this zombie falls inside any flashlight cone, Dark Vision radius, or active noise pulse. */
	bool IsDetectedByViewer(const AActor* ViewerActor, const FVector& ViewerOrigin) const noexcept;

	UFUNCTION(BlueprintPure, Category = "Zombie")
	ECompassOctant GetCurrentHeadingOctant() const noexcept { return CurrentHeadingOctant; }

	UFUNCTION(BlueprintPure, Category = "Zombie")
	bool IsZombieAlive() const noexcept { return CurrentHealth > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Zombie")
	float GetCurrentHealth() const noexcept { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Zombie")
	float GetMaxHealth() const noexcept { return MaxHealth; }

	/** True zombies cast physical shadows and emit footsteps; mirror decoys do not. */
	UFUNCTION(BlueprintPure, Category = "Zombie")
	bool IsRealPhysicalZombie() const noexcept { return !bIsMirrorDecoy; }

	UFUNCTION(BlueprintPure, Category = "Zombie|Visual")
	int32 GetActiveDirectionalRow() const noexcept { return ActiveDirectionalRow; }

	UFUNCTION(BlueprintPure, Category = "Zombie|Visual")
	int32 GetActiveWalkFrameColumn() const noexcept { return ActiveWalkFrameColumn; }

	void AdvanceWalkFlipbookByDistance(float DeltaDistanceUU, bool bIsMoving) noexcept;

	void ApplyZombieDamage(float DamageAmount, AActor* DamageInstigator);
	void TriggerHitFeedback(const FVector& KnockbackDirXY, float KnockbackSpeedUU = 340.0f) noexcept;

	// --- Status effects applied by the hero's moves and skills -------------------------------

	/** Freezes AI for a while. Bosses shorten it by StunResistance. Longer stuns replace shorter ones. */
	void ApplyStun(float Seconds) noexcept;

	/** Damage over time. Re-applying refreshes the duration and keeps the stronger tick rate. */
	void ApplyBurn(float DamagePerSecond, float Seconds, AActor* BurnInstigator) noexcept;

	/** Slows this zombie's whole clock (movement, fuses, throw cadence) via CustomTimeDilation. */
	/** Freezes the zombie for a moment (real seconds) when a hit lands. Stacks with time slow. */
	void ApplyHitStop(float RealSeconds) noexcept;

	void ApplyTimeSlow(float Dilation, float Seconds) noexcept;

	// --- Telegraphed melee -------------------------------------------------------------------

	/** Starts a wind-up against Target if in range, off cooldown, and the hero has a free attack token. */
	bool TryStartMeleeAttack(ADungeoneerCharacter* Target) noexcept;

	/** Cancels a wind-up (stun, light hit on a small enemy). Releases the attack token. */
	void InterruptAttack() noexcept;

	EZombieAttackPhase GetAttackPhase() const noexcept { return AttackPhase; }
	bool IsAttackBusy() const noexcept { return AttackPhase != EZombieAttackPhase::None; }
	float GetAttackPhaseRemaining() const noexcept { return AttackPhaseRemaining; }
	const FZombieMeleeAttackDef& GetMeleeAttackDef() const noexcept { return MeleeAttack; }

	// --- Difficulty --------------------------------------------------------------------------

	/** Deeper floors hit harder and take longer to kill: +20% health and +10% damage per floor past the first. */
	void ApplyFloorScaling(int32 FloorIndex) noexcept;

	/** Elite variant: +60% health, +25% damage, 25% faster wind-ups, double gold, a gold glow. */
	void MakeElite() noexcept;

	bool IsElite() const noexcept { return bIsElite; }
	float GetDamageScale() const noexcept { return DamageScale; }

	UFUNCTION(BlueprintPure, Category = "Zombie|Status")
	bool IsStunned() const noexcept { return StunRemaining > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Zombie|Status")
	bool IsBurning() const noexcept { return BurnRemaining > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Zombie|Status")
	bool IsTimeSlowed() const noexcept { return TimeSlowRemaining > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Zombie|Status")
	float GetStunResistance() const noexcept { return StunResistance; }

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnZombieStepTriggered, const FDungeoneerStepEvent&);
	FOnZombieStepTriggered OnZombieStepTriggered;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnWalkColumnChangedSignature, int32 /*NewColumn*/);
	FOnWalkColumnChangedSignature OnWalkColumnChanged;

	int32 GetWalkStepSequenceCount() const noexcept { return WalkStepSequenceCount; }

protected:
	void MoveAlongOctant(ECompassOctant Octant, float Scale = 1.0f) noexcept;
	void UpdateZombieDirectionalVisual(ECompassOctant HeadingOctant, int32 FrameColumn = -1) noexcept;
	void AdvanceDeterministicStepAccumulator(float DeltaSeconds) noexcept;
	virtual void OnZombieDied();

	/** Called right after a melee strike resolves (hit or miss). */
	virtual void OnMeleeStrike(bool bHitSomething) {}

	/** Runs the wind-up / strike / recover timers. Called from Tick on dilated time, so slows and hit-stop stretch it. */
	void TickMeleeAttack(float DeltaSeconds) noexcept;

	/** Nudges this zombie away from others closer than about a body width, so packs don't stack. */
	void ApplySeparation() noexcept;

	FZombieMeleeAttackDef MeleeAttack;
	float DamageScale = 1.0f;
	bool bIsElite = false;
	virtual const TCHAR* GetArchetypeKeyPrefix() const noexcept { return TEXT("Exploder"); }
	virtual FName GetArchetypeDiffuseKey() const noexcept { return FName(TEXT("Exploder_Row0_D")); }
	virtual FName GetArchetypeNormalKey() const noexcept { return FName(TEXT("Exploder_Row0_N")); }
	virtual FLinearColor GetArchetypeVisualTint() const noexcept { return FLinearColor(0.85f, 0.30f, 0.12f); }
	virtual FVector GetArchetypeVisualScale() const noexcept { return FVector(1.55, 1.55, 1.55); }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zombie|Visual")
	TObjectPtr<UStaticMeshComponent> ZombieVisualMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Zombie|Visual")
	TObjectPtr<UPointLightComponent> ZombieEmissiveLight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config", meta = (ClampMin = "1.0"))
	float MaxHealth = 120.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config", meta = (ClampMin = "50.0"))
	float BaseWalkSpeed = 240.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config", meta = (ClampMin = "100.0"))
	float PerceptionHearingRange = 1400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config", meta = (ClampMin = "10.0"))
	float StepCycleDistanceUU = 110.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config", meta = (ClampMin = "0"))
	int32 GoldKillReward = 25;

	/** Bit index in the Ledger bestiary (EDungeoneerBestiaryBit): 0 Exploder, 1 Thrower, 2 Stalker, 3 Colossus. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config", meta = (ClampMin = "0", ClampMax = "7"))
	uint8 BestiaryBitIndex = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config", meta = (ClampMin = "0.0"))
	float BatteryKillReward = 8.0f;

	/** 0 = takes full stun duration, 1 = immune. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float StunResistance = 0.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	float CurrentHealth = 120.0f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "State")
	ECompassOctant CurrentHeadingOctant = ECompassOctant::W;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirror")
	bool bIsMirrorDecoy = false;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DirectionalZombieMIDs;

	float HitFlashRemaining = 0.0f;

private:
	/** Ticks stun/burn/slow timers on real (undilated) time. */
	void TickStatusEffects(float RealDeltaSeconds) noexcept;

	TWeakObjectPtr<AActor> BurnInstigator;
	float StunRemaining = 0.0f;
	float BurnRemaining = 0.0f;
	float BurnDamagePerSecond = 0.0f;
	float HitStopRemaining = 0.0f;
	float TimeSlowDilation = 1.0f;
	EZombieAttackPhase AttackPhase = EZombieAttackPhase::None;
	float AttackPhaseRemaining = 0.0f;
	float AttackCooldownRemaining = 0.0f;
	FVector AttackDirXY = FVector::ForwardVector;
	TWeakObjectPtr<ADungeoneerCharacter> AttackTokenHero;
	float TimeSlowRemaining = 0.0f;

	FVector PreviousLocation = FVector::ZeroVector;
	TWeakObjectPtr<AActor> LastDamageInstigator;
	float StepDistanceAccumulator = 0.0f;
	float WalkCycleDistanceUU = 0.0f;
	float DeathHideCountdown = -1.0f;
	int32 StepSequenceCounter = 0;
	int32 ActiveDirectionalRow = -1;
	int32 ActiveWalkFrameColumn = 0;
	int32 WalkStepSequenceCount = 0;
	int32 ActiveDirectionalCellIndex = -1;
	bool bTexturedVisualsApplied = false;
	bool bKillRewardsGranted = false;
};
