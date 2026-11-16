#pragma once

/**
 * DungeoneerSensoryWorldSubsystem.h
 * 
 * World subsystem managing pre-allocated ring buffers for acoustic noise pulses
 * and scent/thermal trails. Eliminates heap allocations and dynamic actor spawning
 * on the Tick hot path while driving global and per-viewport sensory progression.
 */

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Containers/StaticArray.h"
#include "Core/DungeoneerTypes.h"
#include "DungeoneerSensoryWorldSubsystem.generated.h"

class UMaterialParameterCollection;

/**
 * World subsystem coordinating acoustic noise events and scent trail rings.
 */
UCLASS()
class DARKDUNGEONEER_API UDungeoneerSensoryWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxNoiseEvents = 64;
	static constexpr int32 MaxScentNodes = 128;

	UDungeoneerSensoryWorldSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UDungeoneerSensoryWorldSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return !IsTemplate(); }

	/**
	 * Broadcasts an acoustic noise event into the pre-allocated ring buffer.
	 * Overwrites the oldest slot when full with zero heap allocations.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sensory")
	void BroadcastNoiseEvent(
		const FVector& Origin,
		float Loudness = 1.0f,
		float MaxRadius = 900.0f,
		bool bIllusionInverted = false);

	/**
	 * Records a player or creature scent trail node into the ring buffer.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sensory")
	void RecordScentTrailNode(
		const FVector& Location,
		float Intensity = 1.0f,
		float Lifetime = 10.0f,
		bool bHostile = false);

	/**
	 * Queries the most audible active acoustic pulse relative to a listener location.
	 * Returns true if an active pulse was within hearing range.
	 */
	bool QueryStrongestActivePulse(
		const FVector& ListenerLocation,
		float HearingPerkMultiplier,
		FDungeoneerNoiseEvent& OutStrongestNoise) const noexcept;

	/**
	 * Pushes the most audible active pulse parameters into the global MPC_SensoryState.
	 */
	UFUNCTION(BlueprintCallable, Category = "Sensory")
	void PushGlobalSensoryMPCParameters(UMaterialParameterCollection* SensoryMPC);

	/** Sets the default MPC to automatically update on tick if provided. */
	UFUNCTION(BlueprintCallable, Category = "Sensory")
	void SetGlobalSensoryMPC(UMaterialParameterCollection* InMPC) { GlobalSensoryMPC = InMPC; }

	/** Const-access to active scent nodes for AI hunting or thermal vision rendering. */
	const TStaticArray<FDungeoneerScentNode, MaxScentNodes>& GetScentTrailRingBuffer() const noexcept { return ScentTrailRingBuffer; }

	/** Const-access to active acoustic noise events for crystal resonance and listener queries. */
	const TStaticArray<FDungeoneerNoiseEvent, MaxNoiseEvents>& GetNoiseRingBuffer() const noexcept { return NoiseRingBuffer; }

	/** Monotonic counter incremented on every BroadcastNoiseEvent so consumers never re-trigger the same pulse. */
	UFUNCTION(BlueprintPure, Category = "Sensory")
	int64 GetNoiseEventSerial() const noexcept { return NoiseEventSerial; }

private:
	TStaticArray<FDungeoneerNoiseEvent, MaxNoiseEvents> NoiseRingBuffer;
	TStaticArray<FDungeoneerScentNode, MaxScentNodes> ScentTrailRingBuffer;

	uint32 NextNoiseIndex = 0;
	uint32 NextScentIndex = 0;
	int64 NoiseEventSerial = 0;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> GlobalSensoryMPC;
};
