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
class UMirrorLightBounceComponent;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class EDungeoneerCombatActionState : uint8
{
	None         UMETA(DisplayName = "Ready"),
	LanternSlash UMETA(DisplayName = "Lantern Blade Slash"),
	ChargedFlare UMETA(DisplayName = "Charged Lantern Flare"),
	DodgeDash    UMETA(DisplayName = "Compass Dodge Dash"),
	HitStagger   UMETA(DisplayName = "Hit Stagger"),
	Downed       UMETA(DisplayName = "Downed")
};

UENUM(BlueprintType)
enum class EDungeoneerNearbyInteract : uint8
{
	None             UMETA(DisplayName = "None"),
	VendorTrade      UMETA(DisplayName = "Vendor Purchase"),
	UnlockVault      UMETA(DisplayName = "Unlock Loot Vault"),
	DescendNextFloor UMETA(DisplayName = "Descend Next Floor"),
	SensoryAltar     UMETA(DisplayName = "Sanctuary Sensory Altar"),
	CollectPickup    UMETA(DisplayName = "Collect Dropped Loot")
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
 * Primary playable 2.5D Dark Dungeoneer character.
 * - Uses UDungeoneerMovementComponent for 4-axis (8-compass) quantized movement.
 * - Drives a decoupled narrow-cone USpotLightComponent flashlight with volumetric scattering.
 * - Queries ULocalPlayer and APlayerState strictly (zero GetFirstPlayerController assumptions).
 */
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
	void RegisterEnemySlain(int32 BonusGold, float BonusBattery) noexcept;

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
	bool IsInvulnerable() const noexcept { return DodgeActiveRemaining > 0.0f || DownedRespawnRemaining > 0.0f; }

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mirror|LightBounce")
	TObjectPtr<UMirrorLightBounceComponent> MirrorBounceComponent;

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
	int32 LastAttackHitsCount = 0;
	bool bAttackDamageApplied = false;
	bool bPrevAttackKeyDown = false;
	bool bPrevFlareKeyDown = false;
	bool bPrevOilKeyDown = false;
	bool bPrevAntidoteKeyDown = false;
	bool bPrevDodgeKeyDown = false;
	bool bPrevInteractKeyDown = false;
	bool bHasMouseSample = false;
	bool bHasPreviousWalkLocation = false;
	bool bTexturedVisualsApplied = false;
};
