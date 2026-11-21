#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "Core/DungeoneerTypes.h"
#include "DungeoneerCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UPointLightComponent;
class USpotLightComponent;
class UStaticMeshComponent;
class UInputMappingContext;
class UInputAction;
class UDungeoneerMovementComponent;
class UDungeoneerLocalPlayerSubsystem;
class USensoryPerkComponent;
class U25DDirectionalRigComponent;
class UDungeoneerTimedAudioComponent;
class UDungeoneerInventoryComponent;
class UTradeManagerComponent;
class UMirrorLightBounceComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UDungeoneerSkillComponent;
class AZombieBase;

UENUM(BlueprintType)
enum class EDungeoneerCombatActionState : uint8
{
	None         UMETA(DisplayName = "Ready"),
	LanternSlash UMETA(DisplayName = "Lantern Blade Slash"),
	ChargedFlare UMETA(DisplayName = "Solar Flare"),
	DodgeDash    UMETA(DisplayName = "Dodge Roll"),
	HitStagger   UMETA(DisplayName = "Hit Stagger"),
	Downed       UMETA(DisplayName = "Downed"),
	HeavyCharge  UMETA(DisplayName = "Charging Heavy Smash"),
	HeavySmash   UMETA(DisplayName = "Heavy Smash"),
	DashStrike   UMETA(DisplayName = "Dash Strike"),
	Parry        UMETA(DisplayName = "Parry"),
	SkillCast    UMETA(DisplayName = "Casting Skill"),
	Whirl        UMETA(DisplayName = "Whirling Lantern")
};

UENUM(BlueprintType)
enum class EDungeoneerNearbyInteract : uint8
{
	None             UMETA(DisplayName = "None"),
	VendorTrade      UMETA(DisplayName = "Vendor Purchase"),
	UnlockVault      UMETA(DisplayName = "Unlock Loot Vault"),
	DescendNextFloor UMETA(DisplayName = "Descend Next Floor"),
	SensoryAltar     UMETA(DisplayName = "Sanctuary Sensory Altar"),
	CollectPickup    UMETA(DisplayName = "Collect Dropped Loot"),
	ReviveAlly       UMETA(DisplayName = "Revive Downed Ally"),
	AllyTrade        UMETA(DisplayName = "Co-Op P2P Supply Swap")
};

struct DARKDUNGEONEER_API FDungeoneerFloatingPopup
{
	FVector WorldLocation = FVector::ZeroVector;
	FString Text;
	EDungeoneerFloatingTextKind Kind = EDungeoneerFloatingTextKind::Damage;
	float AgeSeconds = 0.0f;
	float LifetimeSeconds = 0.85f;
	bool bActive = false;
};

/**
 * The playable lantern-bearer.
 *
 * Movement snaps to 8 compass directions (UDungeoneerMovementComponent). Aim is separate from
 * movement and drives the flashlight cone. Combat: a 4-hit lantern combo, hold-to-charge heavy
 * smash, dodge roll with a dash-strike follow-up, parry, and two skill slots
 * (UDungeoneerSkillComponent). The sprite sheet only has walk frames, so attacks are sold with
 * procedural squash/stretch, lunges, row cycling and simple effect quads.
 */
/**
 * Which hero sprite sheet a frame comes from. Each is 4 rows (S, N, E, W) by 4 columns.
 * Walk is the walk cycle; the rest are the move sheets in Content/Art/Rigs/DungeoneerHeroMoves.
 */
enum class EHeroPoseSheet : uint8
{
	Walk = 0,  // columns: contact, left step, passing, right step
	Combo,     // wind-up, swing (hit), follow-through, recover
	Heavy,     // crouched charge, lantern overhead, slam (hit), recover
	Evade,     // standing, tucked roll, lunge (hit), parry brace
	Skills,    // flask throw, hook cast, lantern spin, flare held high
	HurtIdle,  // idle in, idle out, flinch, downed
	Count
};

UCLASS(Blueprintable)
class DARKDUNGEONEER_API ADungeoneerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	explicit ADungeoneerCharacter(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_Controller() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Locomotion")
	UDungeoneerMovementComponent* GetDungeoneerMovement() const noexcept;

	UFUNCTION(BlueprintPure, Category = "Lighting")
	USpotLightComponent* GetFlashlightComponent() const noexcept { return FlashlightComponent.Get(); }

	UFUNCTION(BlueprintPure, Category = "Lighting")
	UPointLightComponent* GetHeroLanternFillLight() const noexcept { return HeroLanternFillLight.Get(); }

	UFUNCTION(BlueprintPure, Category = "Rendering")
	UStaticMeshComponent* GetHeroVisualMesh() const noexcept { return HeroVisualMesh.Get(); }

	UFUNCTION(BlueprintPure, Category = "Sensory")
	USensoryPerkComponent* GetSensoryPerkComponent() const noexcept { return SensoryPerkComponent.Get(); }

	UFUNCTION(BlueprintPure, Category = "Rendering")
	U25DDirectionalRigComponent* GetDirectionalRigComponent() const noexcept { return DirectionalRigComponent.Get(); }

	UFUNCTION(BlueprintPure, Category = "Audio")
	UDungeoneerTimedAudioComponent* GetTimedAudioComponent() const noexcept { return TimedAudioComponent.Get(); }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	UDungeoneerInventoryComponent* GetInventoryComponent() const noexcept { return InventoryComponent.Get(); }

	UFUNCTION(BlueprintPure, Category = "Social|Trading")
	UTradeManagerComponent* GetTradeManagerComponent() const noexcept { return TradeManagerComponent.Get(); }

	UFUNCTION(BlueprintPure, Category = "Vitals")
	bool IsDowned() const noexcept { return CombatActionState == EDungeoneerCombatActionState::Downed || DownedRespawnRemaining > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Vitals")
	float GetDownedRemainingSeconds() const noexcept { return DownedRespawnRemaining; }

	UFUNCTION(BlueprintCallable, Category = "CoOp|Revive")
	bool ReviveByAlly(ADungeoneerCharacter* Rescuer);

	UFUNCTION(BlueprintCallable, Category = "Social|Trading")
	bool TriggerAllyTradeWithNearest();

	UFUNCTION(BlueprintCallable, Category = "Rendering|Cosmetics")
	void ApplyLanternTrimTint(int32 TrimIndex) noexcept;

	UFUNCTION(BlueprintPure, Category = "Rendering|Cosmetics")
	int32 GetEquippedLanternTrimIndex() const noexcept { return EquippedLanternTrimIndex; }

	UFUNCTION(BlueprintPure, Category = "Mirror|LightBounce")
	UMirrorLightBounceComponent* GetMirrorBounceComponent() const noexcept { return MirrorBounceComponent.Get(); }

	UFUNCTION(BlueprintPure, Category = "Locomotion")
	ECompassOctant GetReplicatedMoveOctant() const noexcept { return ReplicatedMoveOctant; }

	UFUNCTION(BlueprintPure, Category = "Locomotion")
	ECompassOctant GetReplicatedAimOctant() const noexcept { return ReplicatedAimOctant; }

	/** Returns the owning ULocalPlayer for this pawn, or nullptr for remote/AI pawns. */
	ULocalPlayer* GetOwningLocalPlayer() const noexcept;

	/** Applies damage / status updates cleanly from server authority. */
	void ApplyDungeoneerDamage(float DamageAmount, AActor* DamageCauser);

	/** Restores health & flashlight battery (used in Resting Grounds). */
	void RestoreAtRestingGround(float HealthDelta, float BatteryDelta);

	/** Full HP and oil, starting inventory, skills and senses, no combat state left over. Authority only. */
	void ResetForNewRun();

	UFUNCTION(BlueprintPure, Category = "Vitals")
	float GetCurrentHealth() const noexcept { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "Vitals")
	float GetMaxHealth() const noexcept { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "Vitals")
	float GetFlashlightBattery() const noexcept { return FlashlightBattery; }

	/** Tests whether a world location lies inside this player's active flashlight cone. */
	bool IsPointLitByFlashlight(const FVector& TargetWorldLocation) const noexcept;

	/** Advances distance-locked 4-frame walk cycle cleanly with zero heap allocation. */
	void AdvanceWalkFlipbookByDistance(float DeltaDistanceUU, bool bIsMoving) noexcept;

	UFUNCTION(BlueprintPure, Category = "Rendering")
	int32 GetActiveDirectionalRow() const noexcept { return ActiveDirectionalRow; }

	UFUNCTION(BlueprintPure, Category = "Rendering")
	int32 GetActiveWalkFrameColumn() const noexcept { return ActiveWalkFrameColumn; }

	/** The sheet and column the sprite shows this frame (Walk when no move sheet applies). */
	EHeroPoseSheet GetActivePoseSheet() const noexcept { return ActivePoseSheet; }
	int32 GetActivePoseColumn() const noexcept { return ActivePoseColumn; }

	/** True if the cells for that sheet were found on disk at startup. */
	bool HasPoseSheet(EHeroPoseSheet Sheet) const noexcept;

	/** Skills call this when they start their cast pose, to pick the frame shown during it. */
	void SetSkillCastFrame(EHeroPoseSheet Sheet, int32 Column) noexcept
	{
		SkillCastSheet = Sheet;
		SkillCastColumn = FMath::Clamp(Column, 0, 3);
	}

	/** Chooses the sheet and column from the current action. Called every Tick before the sprite updates; public for tests. */
	void ResolvePoseFrame(float DeltaSeconds, bool bMoving) noexcept;

	/** Triggers a 4-axis/8-compass Lantern Blade melee cleave + ember burst along AimOctant. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool TriggerPrimaryAttack();

	/** Triggers a 25% battery Charged Lantern Flare stun/bloom wave (`R`). */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool TriggerChargedFlare();

	/** Uses a quick-slot consumable (`LanternOilPhial` `[Q]`, `AntidoteDraught` `[C]`). */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Consumables")
	bool TriggerQuickConsumable(EDungeoneerConsumableId ConsumableId);

	/** Triggers an evasive 4-axis/8-compass Dodge Dash with brief invulnerability frames. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool TriggerDodgeDash();

	/** Interacts with nearby AVendorActor, Sanctuary Shrine, Loot Pickup, or AMajorLootVaultActor. */
	UFUNCTION(BlueprintCallable, Category = "Dungeon|Loop")
	bool TriggerInteract();

	/** Records a zombie kill and grants gold + battery siphon from slain targets. */
	void RegisterEnemySlain(int32 BonusGold, float BonusBattery, uint8 BestiaryMask = 0x01) noexcept;

	/** Pushes a floating damage/gold/relic popup into the fixed 32-slot zero-alloc ring buffer. */
	void PushFloatingCombatPopup(const FVector& WorldLoc, const FString& Text, EDungeoneerFloatingTextKind Kind) noexcept;

	static constexpr int32 MaxFloatingPopups = 32;
	const FDungeoneerFloatingPopup& GetFloatingPopupAt(int32 Index) const noexcept { return FloatingPopups[Index]; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	int32 GetComboStepIndex() const noexcept { return ComboStepIndex; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsFlareFlashActive() const noexcept { return FlareFlashRemaining > 0.0f; }

	/** Triggers deterministic hit-stop dilation & damped CameraBoom micro-impulse on heavy strikes. */
	void TriggerCombatCameraImpulse(const FVector& ImpulseDirXY, float StrengthUU = 18.0f, float HitStopSec = 0.035f) noexcept;

	UFUNCTION(BlueprintPure, Category = "Combat")
	EDungeoneerCombatActionState GetCombatActionState() const noexcept { return CombatActionState; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsInvulnerable() const noexcept { return DodgeActiveRemaining > 0.0f || DownedRespawnRemaining > 0.0f || InvulnRemaining > 0.0f; }

	// --- Hack-and-slash moves --------------------------------------------------------------

	static constexpr int32 ComboLength = 4;

	/** Starts charging a heavy smash (hold attack). Released by ReleaseHeavyCharge or auto-fires at full charge. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool BeginHeavyCharge();

	/** Releases a charging heavy smash. Fires if charged past the minimum, otherwise cancels. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool ReleaseHeavyCharge();

	/** Opens a short parry window. Parried melee attackers are stunned; parried flasks fly back. */
	UFUNCTION(BlueprintCallable, Category = "Combat")
	bool TriggerParry();

	/** Fires the skill equipped in slot 0 or 1. */
	UFUNCTION(BlueprintCallable, Category = "Skills")
	bool TriggerSkillSlot(int32 SlotIndex);

	/** Called by throwers when a flask lands near this hero. Returns true if the parry window bats it back. */
	bool TryDeflectThrownFlask(AActor* Thrower);

	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsParryActive() const noexcept { return ParryActiveRemaining > 0.0f; }

	/** True while the next melee hit is boosted (after a parry, perfect dodge, or Shade Step). */
	UFUNCTION(BlueprintPure, Category = "Combat")
	bool IsEmpowered() const noexcept { return EmpoweredRemaining > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetEmpoweredMultiplier() const noexcept { return IsEmpowered() ? EmpoweredMultiplier : 1.0f; }

	/** 0..1 while charging a heavy smash, 0 otherwise. */
	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetHeavyChargeAlpha() const noexcept;

	UFUNCTION(BlueprintPure, Category = "Combat")
	int32 GetPerfectDodgeCount() const noexcept { return PerfectDodgeCount; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	int32 GetParryCount() const noexcept { return ParryCount; }

	UFUNCTION(BlueprintPure, Category = "Skills")
	UDungeoneerSkillComponent* GetSkillComponent() const noexcept { return SkillComponent.Get(); }

	// --- Hooks used by UDungeoneerSkillComponent -------------------------------------------

	/** True when the hero is free to start an attack, parry or skill (light swings can be cancelled). */
	bool CanStartAction() const noexcept;

	/** Unit XY vector of the current aim direction. */
	FVector GetAimForwardXY() const noexcept;

	/** Current combat camera kick (spring offset). The party camera sums these across heroes. */
	FVector GetCameraImpulseOffset() const noexcept { return CameraImpulseOffset; }

	/** Screen shake: 0..1, decays over about half a second. The party camera shakes by the largest value. */
	void AddCameraTrauma(float Amount) noexcept { CameraTrauma = FMath::Clamp(CameraTrauma + Amount, 0.0f, 1.0f); }
	float GetCameraTrauma() const noexcept { return CameraTrauma; }

	/** 0..1 red flash after taking damage; the HUD draws an edge flash from it. */
	float GetHurtFlashAlpha() const noexcept { return HurtFlashRemaining / HurtFlashDuration; }
	static constexpr float HurtFlashDuration = 0.35f;

	/** Turns aim toward a world location (used after blinking behind a target). */
	void FaceTowards(const FVector& WorldLocation) noexcept;

	/** Removes oil. Returns false (and spends nothing) if there is not enough. Authority only. */
	bool SpendLanternOil(float Amount) noexcept;

	/** Boosts the next melee hit within WindowSec. Label is shown as a popup. */
	void GrantEmpoweredStrike(float Multiplier, float WindowSec, const TCHAR* Label) noexcept;

	void GrantInvulnerability(float Seconds) noexcept;

	/** Puts the hero into a timed skill pose (SkillCast or Whirl). */
	void BeginSkillPose(EDungeoneerCombatActionState PoseState, float DurationSec) noexcept;

	/** Draws a thin glowing quad from Start to End for a moment (chain, thrown flask arc). */
	void ShowSkillBeam(const FVector& Start, const FVector& End, float DurationSec, const FLinearColor& Color) noexcept;

	/** Leaves a shrinking copy of the hero sprite at Location (blinks, perfect dodges, dash strikes). */
	void ShowAfterimage(const FVector& Location, float DurationSec) noexcept;

	/** Shows or hides the Mirror Decoy copy of the hero. */
	void ShowDecoy(const FVector& Location, bool bVisible) noexcept;

	UFUNCTION(BlueprintPure, Category = "Combat")
	int32 GetKillsCount() const noexcept { return KillsCount; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	int32 GetLastAttackHitsCount() const noexcept { return LastAttackHitsCount; }

	UFUNCTION(BlueprintPure, Category = "Combat")
	UStaticMeshComponent* GetSlashArcMesh() const noexcept { return SlashArcMesh.Get(); }

	UFUNCTION(BlueprintPure, Category = "Dungeon|Loop")
	EDungeoneerNearbyInteract GetNearbyInteractType() const noexcept { return NearbyInteractType; }

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnCombatAttackPerformedSignature, ECompassOctant /*AttackOctant*/, int32 /*HitsLanded*/);
	FOnCombatAttackPerformedSignature OnCombatAttackPerformed;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnWalkColumnChangedSignature, int32 /*NewColumn*/);
	FOnWalkColumnChangedSignature OnWalkColumnChanged;

	int32 GetWalkStepSequenceCount() const noexcept { return WalkStepSequenceCount; }

protected:
	void HandleMoveInput(const FInputActionValue& Value);
	void HandleMoveCompleted(const FInputActionValue& Value);
	void HandleAimInput(const FInputActionValue& Value);
	void HandleAttackInput(const FInputActionValue& Value);
	void HandleDodgeInput(const FInputActionValue& Value);
	void HandleInteractInput(const FInputActionValue& Value);
	void BindLocalInputContext();
	void EnsureDefaultVisualsInitialized();
	void UpdateHeroDirectionalVisual(ECompassOctant FacingOctant, int32 FrameColumn = -1) noexcept;
	void UpdateCombatAndProceduralPose(float DeltaSeconds) noexcept;
	void RefreshNearbyInteractCandidate() noexcept;
	void PollDirectKeyboardAndMouseFallback(float DeltaSeconds) noexcept;
	void SyncFlashlightTransform(float DeltaSeconds) noexcept;
	void ExecutePrimaryAttackAuthority();
	void ExecuteDodgeDashAuthority();
	void ExecuteInteractAuthority();
	void ExecuteHeavySmashAuthority(float ChargeAlpha);
	void ExecuteDashStrikeAuthority();
	bool TriggerDashStrike();
	void StartHeavySmash(float ChargeAlpha);
	void HandleSuccessfulParry(AZombieBase* Attacker);
	void HandlePerfectDodge();
	/** Applies relic and empowered-strike multipliers to a melee hit, consuming the empowered strike. */
	float ComputeMeleeDamage(float BaseDamage, const FVector& Origin);
	ECompassOctant GetEffectiveAimOctant() const noexcept;
	bool UpdateNewMovePoses(float DeltaSeconds) noexcept;
	void UpdateEffectQuads(float DeltaSeconds) noexcept;
	UMaterialInterface* GetCurrentHeroMaterial() const noexcept;

	UFUNCTION(Server, Reliable)
	void Server_ExecuteHeavySmash(float ChargeAlpha);

	UFUNCTION(Server, Reliable)
	void Server_ExecuteDashStrike(FVector_NetQuantizeNormal Direction);

	UFUNCTION(Server, Reliable)
	void Server_TriggerParry();

	UFUNCTION(Server, Unreliable)
	void Server_UpdateOctants(ECompassOctant NewMoveOctant, ECompassOctant NewAimOctant);

	UFUNCTION(Server, Reliable)
	void Server_ExecutePrimaryAttack();

	UFUNCTION(Server, Reliable)
	void Server_ExecuteDodgeDash();

	UFUNCTION(Server, Reliable)
	void Server_ExecuteInteract();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> TopDownCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lighting")
	TObjectPtr<USpotLightComponent> FlashlightComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Lighting")
	TObjectPtr<UPointLightComponent> HeroLanternFillLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rendering")
	TObjectPtr<UStaticMeshComponent> HeroVisualMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Visual")
	TObjectPtr<UStaticMeshComponent> SlashArcMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Sensory")
	TObjectPtr<USensoryPerkComponent> SensoryPerkComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rendering")
	TObjectPtr<U25DDirectionalRigComponent> DirectionalRigComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Audio")
	TObjectPtr<UDungeoneerTimedAudioComponent> TimedAudioComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UDungeoneerInventoryComponent> InventoryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Social|Trading")
	TObjectPtr<UTradeManagerComponent> TradeManagerComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mirror|LightBounce")
	TObjectPtr<UMirrorLightBounceComponent> MirrorBounceComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Skills")
	TObjectPtr<UDungeoneerSkillComponent> SkillComponent;

	/** World-space quad for chains and flask arcs. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Visual")
	TObjectPtr<UStaticMeshComponent> SkillBeamMesh;

	/** World-space copy of the hero sprite left behind by blinks and dashes. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Visual")
	TObjectPtr<UStaticMeshComponent> AfterimageMesh;

	/** World-space copy of the hero sprite for the Mirror Decoy skill. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Visual")
	TObjectPtr<UStaticMeshComponent> DecoyMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AimAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> AttackAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> DodgeAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vitals", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Vitals")
	float CurrentHealth = 100.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Vitals")
	float FlashlightBattery = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "10.0"))
	float PrimaryAttackDamage = 55.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "50.0"))
	float PrimaryAttackRangeUU = 240.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.1"))
	float PrimaryAttackCooldown = 0.36f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.1"))
	float DodgeCooldown = 0.70f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "100.0"))
	float DodgeDashSpeedUU = 760.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Locomotion")
	ECompassOctant ReplicatedMoveOctant = ECompassOctant::E;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Locomotion")
	ECompassOctant ReplicatedAimOctant = ECompassOctant::E;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Combat")
	int32 KillsCount = 0;

	/** Pre-allocated 16 directional walk-cycle sprite materials (4 rows x 4 columns) for zero Tick allocation. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DirectionalHeroMIDs;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rendering", meta = (ClampMin = "20.0"))
	float WalkCycleStrideUU = 140.0f;

private:
	FVector2D LastMouseSample = FVector2D::ZeroVector;
	FVector PreviousWalkLocation = FVector::ZeroVector;
	FVector BaseVisualRelativeLocation = FVector(0.0, 0.0, 8.0);
	FRotator BaseVisualRelativeRotation = FRotator(45.0, 180.0, 0.0);
	FVector BaseVisualRelativeScale = FVector(1.65, 1.65, 1.65);
	FVector DashDirectionXY = FVector(1.0, 0.0, 0.0);
	ECompassOctant ActiveAttackOctant = ECompassOctant::E;
	EDungeoneerCombatActionState CombatActionState = EDungeoneerCombatActionState::None;
	EDungeoneerNearbyInteract NearbyInteractType = EDungeoneerNearbyInteract::None;
	float CurrentFlashlightYawDeg = 0.0f;
	float OctantSyncAccumulator = 0.0f;
	float TimeSinceMouseMoved = 10.0f;
	float WalkDistanceAccumulatorUU = 0.0f;
	float AttackSwingElapsed = 0.0f;
	float AttackCooldownRemaining = 0.0f;
	float DodgeActiveRemaining = 0.0f;
	float DodgeCooldownRemaining = 0.0f;
	float HitStaggerRemaining = 0.0f;
	float DownedRespawnRemaining = 0.0f;
	float InteractScanAccumulator = 0.0f;
	float ComboResetRemaining = 0.0f;
	float FlareFlashRemaining = 0.0f;
	float HitStopRemaining = 0.0f;
	FVector CameraImpulseOffset = FVector::ZeroVector;
	FVector CameraImpulseVelocity = FVector::ZeroVector;
	FVector BaseCameraSocketOffset = FVector::ZeroVector;
	int32 ComboStepIndex = 0;
	int32 FloatingPopupCursor = 0;
	TStaticArray<FDungeoneerFloatingPopup, MaxFloatingPopups> FloatingPopups;
	int32 ActiveDirectionalRow = -1;
	int32 ActiveWalkFrameColumn = 0;
	int32 WalkStepSequenceCount = 0;
	int32 ActiveDirectionalCellIndex = -1;
	EHeroPoseSheet ActivePoseSheet = EHeroPoseSheet::Walk;
	int32 ActivePoseColumn = 0;
	EHeroPoseSheet SkillCastSheet = EHeroPoseSheet::Skills;
	int32 SkillCastColumn = 0;
	float IdleBreathTime = 0.0f;
	int32 LastAttackHitsCount = 0;
	int32 EquippedLanternTrimIndex = 0;
	bool bAttackDamageApplied = false;
	bool bWasDownedWithAliveAllies = false;

	// Hack-and-slash move state.
	float AttackBufferRemaining = 0.0f;
	float DashStrikeWindowRemaining = 0.0f;
	float DashStrikeElapsed = 0.0f;
	float HeavyChargeElapsed = 0.0f;
	float HeavySmashElapsed = 0.0f;
	float HeavySmashAlpha = 0.0f;
	float ParryActiveRemaining = 0.0f;
	float ParryRecoveryRemaining = 0.0f;
	float ParryCooldownRemaining = 0.0f;
	float EmpoweredRemaining = 0.0f;
	float EmpoweredMultiplier = 1.0f;
	float InvulnRemaining = 0.0f;
	float SkillPoseRemaining = 0.0f;
	float SkillPoseDuration = 0.0f;
	float BeamRemaining = 0.0f;
	float AfterimageRemaining = 0.0f;
	float AfterimageDuration = 0.0f;
	FVector AfterimageBaseScale = FVector::OneVector;
	int32 PerfectDodgeCount = 0;
	int32 ParryCount = 0;
	bool bAttackHeld = false;
	bool bPerfectDodgeUsedThisDash = false;

	/** Edge detection for polled keyboard/gamepad buttons (index = EHeroButton in the .cpp). */
	TStaticArray<bool, 12> PrevButtonDown;
	bool bHasMouseSample = false;
	bool bHasPreviousWalkLocation = false;
	bool bTexturedVisualsApplied = false;
	bool bSlashArcTextured = false;
	float CameraTrauma = 0.0f;
	float HurtFlashRemaining = 0.0f;
};
