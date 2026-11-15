#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DungeoneerInventoryComponent.generated.h"

USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API FDungeoneerInventoryItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FGuid ItemGuid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	int32 Quantity = 1;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Inventory")
	bool bLockedForTrade = false;
};

/**
 * Server-Authoritative Replicated Inventory Component (`UDungeoneerInventoryComponent`).
 * Client RPCs never mutate inventory arrays directly; all mutations flow through
 * server validation and UTradeManagerComponent atomic swaps.
 */
UCLASS(ClassGroup = (Inventory), meta = (BlueprintSpawnableComponent))
class DARKDUNGEONEER_API UDungeoneerInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDungeoneerInventoryComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Inventory")
	int32 GetGoldBalance() const noexcept { return GoldBalance; }

	UFUNCTION(BlueprintPure, Category = "Inventory")
	const TArray<FDungeoneerInventoryItem>& GetItems() const noexcept { return Items; }

	/** Server-only: grants an item with a fresh deterministic or unique FGuid. */
	void AuthorityAddItem(const FGuid& ItemGuid, FName ItemId, int32 Quantity);

	/** Server-only: adjusts currency safely (returns false if insufficient funds). */
	bool AuthorityAdjustGold(int32 DeltaGold) noexcept;

	/** Server-only: validates whether an unlocked item with ItemGuid exists. */
	bool AuthorityHasUnlockedItem(const FGuid& ItemGuid) const noexcept;

	/** Server-only: validates whether an item locked for TxId exists. */
	bool AuthorityHasLockedItem(const FGuid& ItemGuid) const noexcept;

	/** Server-only: Phase-1 lock / unlock for two-phase commit trading. */
	bool AuthoritySetItemTradeLock(const FGuid& ItemGuid, bool bLocked) noexcept;

	/** Server-only: extracts and removes a locked item during Phase-2 atomic swap. */
	bool AuthorityExtractLockedItem(const FGuid& ItemGuid, FDungeoneerInventoryItem& OutExtracted);

protected:
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Inventory")
	TArray<FDungeoneerInventoryItem> Items;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	int32 GoldBalance = 250;
};
