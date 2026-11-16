#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/DungeoneerTypes.h"
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

	/** Grants a passive Relic (idempotent bitmask storage, zero heap growth). */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Relics")
	bool AuthorityGrantRelic(EDungeoneerRelicId RelicId) noexcept;

	UFUNCTION(BlueprintPure, Category = "Inventory|Relics")
	bool HasRelic(EDungeoneerRelicId RelicId) const noexcept;

	UFUNCTION(BlueprintPure, Category = "Inventory|Relics")
	int32 GetActiveRelicCount() const noexcept;

	UFUNCTION(BlueprintPure, Category = "Inventory|Relics")
	uint8 GetRelicBitmask() const noexcept { return ReplicatedRelicMask; }

	/** Quick-use Consumable helpers (`LanternOilPhial`, `AntidoteDraught`). */
	UFUNCTION(BlueprintCallable, Category = "Inventory|Consumables")
	void AuthorityAddConsumable(EDungeoneerConsumableId ConsumableId, int32 DeltaCount = 1) noexcept;

	UFUNCTION(BlueprintCallable, Category = "Inventory|Consumables")
	bool AuthorityConsumeQuickItem(EDungeoneerConsumableId ConsumableId) noexcept;

	UFUNCTION(BlueprintPure, Category = "Inventory|Consumables")
	int32 GetConsumableCount(EDungeoneerConsumableId ConsumableId) const noexcept;

	static const TCHAR* GetRelicDisplayName(EDungeoneerRelicId RelicId) noexcept;
	static EDungeoneerRelicId MapItemIdToRelic(FName ItemId) noexcept;

protected:
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Inventory")
	TArray<FDungeoneerInventoryItem> Items;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Inventory")
	int32 GoldBalance = 250;

	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "Inventory|Relics")
	uint8 ReplicatedRelicMask = 0;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Inventory|Consumables")
	int32 OilPhialCount = 1;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadOnly, Category = "Inventory|Consumables")
	int32 AntidoteCount = 1;
};
