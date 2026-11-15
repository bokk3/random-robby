#pragma once

#include "CoreMinimal.h"
#include "AI/ZombieBase.h"
#include "ZombieFastThrower.generated.h"

UENUM(BlueprintType)
enum class EThrowPayload : uint8
{
	Fire     = 0 UMETA(DisplayName = "Fire Flask"),
	Poison   = 1 UMETA(DisplayName = "Poison Spore Flask"),
	SloMoGas = 2 UMETA(DisplayName = "Chrono-Leech Slo-Mo Gas")
};

/**
 * Fast Elemental / Chrono-Leech Thrower Zombie (`AZombieFastThrower`).
 * - High base speed; strafes along the 8 compass axes at OptimalThrowRange.
 * - Throws Fire, Poison, and SloMoGas pooled hazard zones.
 * - Chrono-Leech Conservation Law:
 *     ZombieMaxWalkSpeed = BaseWalkSpeed * (1.0f + TotalDrainedRatio * SpeedStealScalar)
 *     ZombieThrowCooldown = BaseCooldown / (1.0f + TotalDrainedRatio * CadenceStealScalar)
 */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API AZombieFastThrower : public AZombieBase
{
	GENERATED_BODY()

public:
	AZombieFastThrower();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Pushes the aggregate player speed ratio drained by linked AHazardZone_ChronoGas volumes.
	 * Enforces exact inverse speed-steal math.
	 */
	void UpdateChronoLeechBuff(float InTotalDrainedRatio, float DeltaTime) noexcept;

	/** Pure formula helpers exposed for unit verification. */
	static float ComputeLeechWalkSpeed(float InBaseSpeed, float InDrainedRatio, float InSpeedStealScalar) noexcept
	{
		return InBaseSpeed * (1.0f + FMath::Max(0.0f, InDrainedRatio) * InSpeedStealScalar);
	}

	static float ComputeLeechThrowCooldown(float InBaseCooldown, float InDrainedRatio, float InCadenceStealScalar) noexcept
	{
		return InBaseCooldown / (1.0f + FMath::Max(0.0f, InDrainedRatio) * InCadenceStealScalar);
	}

	UFUNCTION(BlueprintPure, Category = "ChronoLeech")
	float GetCurrentDrainedRatio() const noexcept { return CurrentDrainedRatio; }

	UFUNCTION(BlueprintPure, Category = "ChronoLeech")
	float GetCurrentThrowCooldown() const noexcept { return CurrentThrowCooldown; }

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnThrowPayloadReleased, EThrowPayload /*Payload*/, const FVector& /*TargetLocation*/);
	FOnThrowPayloadReleased OnThrowPayloadReleased;

	DECLARE_MULTICAST_DELEGATE(FOnChronoLeechDecaySweep);
	FOnChronoLeechDecaySweep OnChronoLeechDecaySweep;

protected:
	EThrowPayload SelectNextPayload(int32 ThrowCount) const noexcept;
	void ExecutePooledThrow(const FVector& TargetLocation);
	virtual void OnZombieDied() override;
	virtual FName GetArchetypeDiffuseKey() const noexcept override { return FName(TEXT("Thrower_Cell0_D")); }
	virtual FName GetArchetypeNormalKey() const noexcept override { return FName(TEXT("Thrower_Cell0_N")); }
	virtual FLinearColor GetArchetypeVisualTint() const noexcept override { return FLinearColor(0.15f, 0.88f, 0.96f); }
	virtual FVector GetArchetypeVisualScale() const noexcept override { return FVector(0.70, 0.70, 1.45); }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thrower|Config", meta = (ClampMin = "100.0"))
	float OptimalThrowRange = 620.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thrower|Config", meta = (ClampMin = "0.5"))
	float BaseThrowCooldown = 3.2f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thrower|ChronoLeech", meta = (ClampMin = "0.1"))
	float SpeedStealScalar = 1.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thrower|ChronoLeech", meta = (ClampMin = "0.1"))
	float CadenceStealScalar = 1.40f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Thrower|ChronoLeech", meta = (ClampMin = "0.1"))
	float DecayTime = 1.8f;

private:
	float CurrentDrainedRatio = 0.0f;
	float CurrentThrowCooldown = 3.2f;
	float ThrowCooldownRemaining = 1.0f;
	float FrameReceivedDrainedRatio = 0.0f;
	int32 ThrowSequenceCount = 0;
	int8 StrafeSign = 1;
	bool bWasLeechActive = false;
};
