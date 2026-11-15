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

protected:
	void HandleMoveInput(const FInputActionValue& Value);
	void HandleMoveCompleted(const FInputActionValue& Value);
	void HandleAimInput(const FInputActionValue& Value);
	void BindLocalInputContext();
	void EnsureDefaultVisualsInitialized();
	void UpdateHeroDirectionalVisual(ECompassOctant FacingOctant, int32 FrameColumn = -1) noexcept;
	void PollDirectKeyboardAndMouseFallback(float DeltaSeconds) noexcept;
	void SyncFlashlightTransform(float DeltaSeconds) noexcept;

	UFUNCTION(Server, Unreliable)
	void Server_UpdateOctants(ECompassOctant NewMoveOctant, ECompassOctant NewAimOctant);

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vitals", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Vitals")
	float CurrentHealth = 100.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Vitals")
	float FlashlightBattery = 100.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Locomotion")
	ECompassOctant ReplicatedMoveOctant = ECompassOctant::E;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Locomotion")
	ECompassOctant ReplicatedAimOctant = ECompassOctant::E;

	/** Pre-allocated 16 directional walk-cycle sprite materials (4 rows x 4 columns) for zero Tick allocation. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DirectionalHeroMIDs;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rendering", meta = (ClampMin = "20.0"))
	float WalkCycleStrideUU = 140.0f;

private:
	FVector2D LastMouseSample = FVector2D::ZeroVector;
	FVector PreviousWalkLocation = FVector::ZeroVector;
	float CurrentFlashlightYawDeg = 0.0f;
	float OctantSyncAccumulator = 0.0f;
	float TimeSinceMouseMoved = 10.0f;
	float WalkDistanceAccumulatorUU = 0.0f;
	int32 ActiveDirectionalRow = -1;
	int32 ActiveWalkFrameColumn = 0;
	int32 ActiveDirectionalCellIndex = -1;
	bool bHasMouseSample = false;
	bool bHasPreviousWalkLocation = false;
	bool bTexturedVisualsApplied = false;
};
