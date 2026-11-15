#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TradeManagerComponent.generated.h"

class UDungeoneerInventoryComponent;
class AVendorActor;

UENUM(BlueprintType)
enum class ETradePhaseState : uint8
{
	Idle          = 0,
	OfferLocked   = 1,
	Committed     = 2,
	RolledBack    = 3
};

USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API FTradeOfferPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Trade")
	TArray<FGuid> OfferedItemGuids;

	UPROPERTY(BlueprintReadWrite, Category = "Trade")
	int32 OfferedGold = 0;
};

/**
 * Server-Authoritative Two-Phase Commit Trade Transaction (`FTradeTransaction`).
 * Implements Rule 03-multiplayer-splitscreen.md:
 *   Phase 1: Lock Offer (Lock Item GUIDs + Escrow Currency)
 *   Phase 2: Validate Item GUIDs & Currency -> Atomic Swap -> Unlock -> Client Ack
 * Guarantees zero item duplication or loss on validation failure or disconnect.
 */
USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API FTradeTransaction
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Trade")
	FGuid TransactionId;

	UPROPERTY(BlueprintReadOnly, Category = "Trade")
	ETradePhaseState Phase = ETradePhaseState::Idle;

	UPROPERTY()
	TWeakObjectPtr<UDungeoneerInventoryComponent> InitiatorInventory;

	UPROPERTY()
	TWeakObjectPtr<UDungeoneerInventoryComponent> CounterpartyInventory;

	UPROPERTY()
	TWeakObjectPtr<AVendorActor> VendorCounterparty;

	UPROPERTY()
	FTradeOfferPayload InitiatorOffer;

	UPROPERTY()
	FTradeOfferPayload CounterpartyOffer;

	UPROPERTY()
	bool bInitiatorConfirmed = false;

	UPROPERTY()
	bool bCounterpartyConfirmed = false;

	UPROPERTY()
	bool bIsVendorTrade = false;
};

/**
 * Replicated Two-Phase Commit Trade Manager (`UTradeManagerComponent`).
 * Attached to PlayerState / Controller to execute atomic Player-to-Vendor and
 * Player-to-Player trades in Resting Grounds without direct client array mutation.
 */
UCLASS(ClassGroup = (Social), meta = (BlueprintSpawnableComponent))
class DARKDUNGEONEER_API UTradeManagerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTradeManagerComponent();

	/** Client -> Server RPC: Phase 1 (Lock Player-to-Player offer). */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Trade")
	void Server_LockPlayerTradeOffer(
		const FGuid& TransactionId,
		UDungeoneerInventoryComponent* PartnerInventory,
		const FTradeOfferPayload& MyOffer);

	/** Client -> Server RPC: Phase 2 (Confirm commit on locked Player-to-Player transaction). */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Trade")
	void Server_ConfirmPlayerTradeCommit(const FGuid& TransactionId);

	/** Client -> Server RPC: Atomic Player-to-Vendor purchase/sale via Two-Phase Commit. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Trade")
	void Server_ExecuteVendorPurchase(AVendorActor* Vendor, const FGuid& VendorItemGuid);

	/** Direct server-side Two-Phase Commit execution helper (also callable from unit tests). */
	bool AuthorityExecuteTwoPhaseCommit(FTradeTransaction& InOutTx);

	/** Cancels and rolls back any locked offer cleanly. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Trade")
	void Server_CancelTrade(const FGuid& TransactionId);

	UFUNCTION(Client, Reliable)
	void Client_ReceiveTradeAck(const FGuid& TransactionId, bool bCommitted, const FString& StatusReason);

	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnTradeAckReceived, const FGuid& /*TxId*/, bool /*bCommitted*/, const FString& /*Reason*/);
	FOnTradeAckReceived OnTradeAckReceived;

private:
	UDungeoneerInventoryComponent* ResolveOwnerInventory() const noexcept;
	void RollbackLockedItems(FTradeTransaction& Tx) noexcept;

	TMap<FGuid, FTradeTransaction> ActiveTransactions;
};
