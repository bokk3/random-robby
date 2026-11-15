#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Core/DungeoneerTypes.h"
#include "DungeoneerMovementComponent.generated.h"

class UDungeoneerMovementConfig;

/**
 * 4-Axis (8-Compass) Quantized Character Movement Component.
 * - Snaps 2D input to 45-degree compass octants (N, NE, E, SE, S, SW, W, NW).
 * - Guarantees diagonal speed strictly equals cardinal speed via INV_SQRT_2 normalization.
 * - Decouples movement octant from flashlight aim octant.
 * - Integrates deterministic step-cycle distance accumulator and Chrono-Leech velocity conservation.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class DARKDUNGEONEER_API UDungeoneerMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UDungeoneerMovementComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual float GetMaxSpeed() const override;
	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;

	/**
	 * Protocol A: Quantizes a 2D input vector to the nearest 45-degree octant (PI/4 rad).
	 * Returns false and preserves OutOctant if squared length is below DeadZoneSq.
	 */
	static bool QuantizeInputToOctant(const FVector2D& InVector, float DeadZoneSq, ECompassOctant& InOutOctant) noexcept;

	/** Returns the exact normalized 3D unit vector for a compass octant (|v| == 1.0 within 1e-5). */
	static FVector GetUnitDirectionForOctant(ECompassOctant Octant) noexcept;

	/** Feeds quantized raw movement input without allocating. */
	void SetMoveInputVector(const FVector2D& InMove) noexcept;

	/** Feeds independent aim/flashlight input without altering movement octant. */
	void SetAimInputVector(const FVector2D& InAim) noexcept;

	/** Directly sets aim octant (e.g., from mouse cursor world projection relative to pawn). */
	void SetAimDirectionWorld(const FVector2D& InAimDelta) noexcept;

	/** Applies Chrono-Leech slow ratio S in [0.0, MaxSlow], reducing V_player = V_base * (1.0f - S). */
	void SetChronoSlowRatio(float InSlowRatio) noexcept;

	/** Resets Chrono-Leech slow cleanly to zero. */
	void ClearChronoSlow() noexcept { ChronoSlowRatio = 0.0f; }

	float GetChronoSlowRatio() const noexcept { return ChronoSlowRatio; }
	float GetBaseWalkSpeedConfig() const noexcept;
	float GetMaxChronoSlowConfig() const noexcept;
	ECompassOctant GetMoveOctant() const noexcept { return CurrentMoveOctant; }
	ECompassOctant GetAimOctant() const noexcept { return CurrentAimOctant; }
	bool HasActiveMoveInput() const noexcept { return bHasActiveMoveInput; }
	float GetStepPhaseNormalized() const noexcept;

	const UDungeoneerMovementConfig* GetMovementConfig() const noexcept { return MovementConfig.Get(); }

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnStepCycleTriggered, const FDungeoneerStepEvent&);
	FOnStepCycleTriggered OnStepCycleTriggered;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	TObjectPtr<UDungeoneerMovementConfig> MovementConfig;

private:
	FVector2D CachedRawMoveInput = FVector2D::ZeroVector;
	FVector2D CachedRawAimInput = FVector2D::ZeroVector;
	ECompassOctant CurrentMoveOctant = ECompassOctant::S;
	ECompassOctant CurrentAimOctant = ECompassOctant::S;
	bool bHasActiveMoveInput = false;

	// Chrono-Leech debuff ratio S in [0.0, MaxSlowRatio]
	float ChronoSlowRatio = 0.0f;

	// Fixed-step distance accumulator synced to the 4-axis step cycle
	float StepDistanceAccumulator = 0.0f;
	int32 StepSequenceCounter = 0;

	// Fallback defaults used if MovementConfig DataAsset is unassigned in unit tests / bare maps
	static constexpr float DefaultBaseWalkSpeed = 450.0f;
	static constexpr float DefaultDeadZoneSq = 0.04f;
	static constexpr float DefaultStepCycleUU = 120.0f;
	static constexpr float DefaultMaxSlowRatio = 0.65f;
};

/**
 * Custom client SavedMove packing quantized uint8 ChronoSlowRatio alongside octant inputs
 * so client-side prediction replays cleanly across Slo-Mo Gas boundaries without rubber-banding.
 */
class DARKDUNGEONEER_API FSavedMove_Dungeoneer : public FSavedMove_Character
{
public:
	using Super = FSavedMove_Character;

	static uint8 QuantizeSlowRatioToByte(float SlowRatio, float MaxSlowRatio) noexcept
	{
		if (MaxSlowRatio <= KINDA_SMALL_NUMBER)
		{
			return 0;
		}
		const float Normalized = FMath::Clamp(SlowRatio / MaxSlowRatio, 0.0f, 1.0f);
		return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Normalized * 255.0f), 0, 255));
	}

	static float DequantizeByteToSlowRatio(uint8 QuantizedByte, float MaxSlowRatio) noexcept
	{
		if (MaxSlowRatio <= KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}
		const float Normalized = static_cast<float>(QuantizedByte) / 255.0f;
		return Normalized * MaxSlowRatio;
	}

	virtual void Clear() override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;

	uint8 SavedQuantizedChronoSlow = 0;
};

class DARKDUNGEONEER_API FNetworkPredictionData_Client_Dungeoneer : public FNetworkPredictionData_Client_Character
{
public:
	using Super = FNetworkPredictionData_Client_Character;

	explicit FNetworkPredictionData_Client_Dungeoneer(const UCharacterMovementComponent& ClientMovement)
		: Super(ClientMovement)
	{
	}

	virtual FSavedMovePtr AllocateNewMove() override;
};
