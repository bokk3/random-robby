#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "DungeoneerFxPoolSubsystem.generated.h"

class AActor;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/**
 * Pre-built pool of additive effect quads (sparks, death bursts, blast rings, dust). Stands in for
 * Niagara, which has no assets in this project. All quads are created when the world begins play;
 * spawning and ticking reuse them and never allocate. Colour and fade go through custom primitive data
 * (DungeoneerSpriteCPD::Fx*), so every quad of a shape shares one material instance.
 */
UCLASS()
class DARKDUNGEONEER_API UDungeoneerFxPoolSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 PoolSize = 96;

	enum class EShape : uint8 { Spark, Ring, Crescent, Puff, Count };

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return bPoolReady; }

	/** A few sparks flying away from a hit, mostly along DirXY. */
	void SpawnHitSparks(const FVector& At, const FVector& DirXY, const FLinearColor& Color, int32 Count = 4, float Speed = 420.0f);

	/** Sparks in every direction (deaths, parries, flares). */
	void SpawnBurst(const FVector& At, const FLinearColor& Color, int32 Count = 8, float Speed = 320.0f, float Size = 0.32f);

	/** A flat ring on the floor that grows from StartRadius to EndRadius over Life seconds and fades. */
	void SpawnGroundRing(const FVector& At, const FLinearColor& Color, float StartRadiusUU, float EndRadiusUU, float LifeSec);

	/** One slow, drifting cloud puff (hazard gas, smoke). Size is in plane-scale units (1 = 100 UU). */
	void SpawnPuff(const FVector& At, const FLinearColor& Color, float Size = 0.9f, float LifeSec = 1.2f);

	/** Two grey puffs at the feet (rolls, landings). */
	void SpawnDust(const FVector& FeetAt);

	int32 GetActiveCount() const noexcept { return ActiveCount; }
	bool IsPoolReady() const noexcept { return bPoolReady; }

private:
	struct FParticle
	{
		FVector Location = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FLinearColor Color = FLinearColor::White;
		float Age = 0.0f;
		float Life = 0.0f;
		float StartScale = 1.0f;
		float EndScale = 1.0f;
		float Drag = 0.0f;
		float Gravity = 0.0f;
		EShape Shape = EShape::Spark;
		bool bGround = false;
		bool bActive = false;
	};

	/** Takes the next pool slot (oldest effect is reused when the pool is full). */
	FParticle* Acquire(EShape Shape, int32& OutIndex) noexcept;
	void ApplyToComponent(int32 Index) const noexcept;

	UPROPERTY(Transient)
	TObjectPtr<AActor> PoolOwner;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Quads;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ShapeMaterials;

	TStaticArray<FParticle, PoolSize> Particles;
	FRotator UprightRotation = FRotator(48.0f, 135.0f, 0.0f);
	int32 NextSlot = 0;
	int32 ActiveCount = 0;
	uint32 SpawnCounter = 0;
	bool bPoolReady = false;
};
