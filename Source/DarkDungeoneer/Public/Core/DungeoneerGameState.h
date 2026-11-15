#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "DungeoneerGameState.generated.h"

/**
 * Deterministic replicated floor seed payload (Rule 06-fog-relevancy-determinism.md).
 * Individual procedural walls/rooms are never replicated; all clients construct
 * identical floor graphs and mirror layouts locally via FRandomStream.
 */
USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API FDungeonFloorSeed
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon")
	int64 MasterSeed = 1569663505; // 0x5D8F2A11

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon")
	int32 MutatedRoomBitmask = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dungeon")
	int32 FloorIndex = 1;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dungeon")
	int32 RerollCount = 0;
};

/**
 * Replicated GameState owning deterministic dungeon floor seeds for seed-only replication.
 */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API ADungeoneerGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ADungeoneerGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Server-only: assigns and replicates a fresh floor seed, updating the local dungeon graph. */
	UFUNCTION(BlueprintCallable, Category = "Dungeon")
	void AuthoritySetFloorSeed(int32 NewFloorIndex, int64 NewMasterSeed, int32 NewMutatedBitmask = 0);

	/** Server-only: deterministically advances the floor seed via Linear Congruential step. */
	UFUNCTION(BlueprintCallable, Category = "Dungeon")
	FDungeonFloorSeed AuthorityRerollSeedDeterministically();

	UFUNCTION(BlueprintPure, Category = "Dungeon")
	const FDungeonFloorSeed& GetReplicatedFloorSeed() const noexcept { return ReplicatedFloorSeed; }

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnFloorSeedChanged, const FDungeonFloorSeed&);
	FOnFloorSeedChanged OnFloorSeedChanged;

protected:
	UFUNCTION()
	void OnRep_DungeonFloorSeed();

	UPROPERTY(ReplicatedUsing = OnRep_DungeonFloorSeed, BlueprintReadOnly, Category = "Dungeon")
	FDungeonFloorSeed ReplicatedFloorSeed;

private:
	void ApplyFloorSeedLocally() noexcept;
};
