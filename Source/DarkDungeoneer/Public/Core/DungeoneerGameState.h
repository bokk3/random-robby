#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Social/DungeoneerInventoryComponent.h"
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
 * Replicated GameState owning deterministic dungeon floor seeds, party stash, and run ledger.
 */
USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API FDungeoneerRunLedgerStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ledger")
	float ElapsedRunSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ledger")
	int32 SecretsDiscovered = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ledger")
	int32 RelicsCollected = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ledger")
	int32 RevivesPerformed = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ledger")
	int32 TotalKills = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ledger")
	int32 DailySeedDayKey = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ledger")
	uint8 SessionBestiaryMask = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Ledger")
	bool bDailySeedRun = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ledger")
	bool bRunEnded = false;

	UPROPERTY(BlueprintReadOnly, Category = "Ledger")
	bool bRunVictory = false;
};

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

	static constexpr int32 MaxPartyStashItems = 8;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnFloorSeedChanged, const FDungeonFloorSeed&);
	FOnFloorSeedChanged OnFloorSeedChanged;

	DECLARE_MULTICAST_DELEGATE(FOnPartyStashChanged);
	FOnPartyStashChanged OnPartyStashChangedEvent;

	/** TASK-1401: Shared Party Stash (Gold + max 8 items) replicated atomically. */
	UFUNCTION(BlueprintCallable, Category = "CoOp|Stash")
	bool AuthorityDepositGoldToStash(UDungeoneerInventoryComponent* PlayerInv, int32 Amount) noexcept;

	UFUNCTION(BlueprintCallable, Category = "CoOp|Stash")
	bool AuthorityWithdrawGoldFromStash(UDungeoneerInventoryComponent* PlayerInv, int32 Amount) noexcept;

	UFUNCTION(BlueprintCallable, Category = "CoOp|Stash")
	bool AuthorityDepositItemToStash(UDungeoneerInventoryComponent* PlayerInv, const FGuid& ItemGuid);

	UFUNCTION(BlueprintCallable, Category = "CoOp|Stash")
	bool AuthorityWithdrawItemFromStash(UDungeoneerInventoryComponent* PlayerInv, const FGuid& ItemGuid);

	UFUNCTION(BlueprintPure, Category = "CoOp|Stash")
	int32 GetPartyStashGold() const noexcept { return ReplicatedPartyStashGold; }

	UFUNCTION(BlueprintPure, Category = "CoOp|Stash")
	const TArray<FDungeoneerInventoryItem>& GetPartyStashItems() const noexcept { return PartyStashItems; }

	/** TASK-1403: Replicated Crypt Run Ledger metrics & completion tracking. */
	UFUNCTION(BlueprintCallable, Category = "Dungeon|Ledger")
	void AuthorityResetRunLedger(bool bDailySeed, int32 DailyDayKey) noexcept;

	void AuthorityRecordKillInLedger(uint8 BestiaryBitMask) noexcept;
	void AuthorityRecordSecretFound() noexcept;
	void AuthorityRecordRelicCollected() noexcept;
	void AuthorityRecordRevivePerformed() noexcept;
	void AuthorityAdvanceRunClock(float DeltaSeconds) noexcept;
	void AuthorityMarkRunEnded(bool bVictory) noexcept;

	UFUNCTION(BlueprintPure, Category = "Dungeon|Ledger")
	const FDungeoneerRunLedgerStats& GetRunLedgerStats() const noexcept { return RunLedgerStats; }

protected:
	UFUNCTION()
	void OnRep_DungeonFloorSeed();

	UFUNCTION()
	void OnRep_PartyStashChanged();

	UPROPERTY(ReplicatedUsing = OnRep_DungeonFloorSeed, BlueprintReadOnly, Category = "Dungeon")
	FDungeonFloorSeed ReplicatedFloorSeed;

	UPROPERTY(ReplicatedUsing = OnRep_PartyStashChanged, BlueprintReadOnly, Category = "CoOp|Stash")
	int32 ReplicatedPartyStashGold = 0;

	UPROPERTY(ReplicatedUsing = OnRep_PartyStashChanged, BlueprintReadOnly, Category = "CoOp|Stash")
	TArray<FDungeoneerInventoryItem> PartyStashItems;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Dungeon|Ledger")
	FDungeoneerRunLedgerStats RunLedgerStats;

private:
	void ApplyFloorSeedLocally() noexcept;
};
