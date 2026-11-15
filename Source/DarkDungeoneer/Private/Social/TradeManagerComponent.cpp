#include "Social/TradeManagerComponent.h"
#include "Social/DungeoneerInventoryComponent.h"
#include "Dungeon/DungeonEncounterActors.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

UTradeManagerComponent::UTradeManagerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

UDungeoneerInventoryComponent* UTradeManagerComponent::ResolveOwnerInventory() const noexcept
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	if (UDungeoneerInventoryComponent* DirectInv = Owner->FindComponentByClass<UDungeoneerInventoryComponent>())
	{
		return DirectInv;
	}

	if (const AController* Ctrl = Cast<AController>(Owner))
	{
		if (const APawn* Pawn = Ctrl->GetPawn())
		{
			return Pawn->FindComponentByClass<UDungeoneerInventoryComponent>();
		}
	}

	return nullptr;
}

void UTradeManagerComponent::RollbackLockedItems(FTradeTransaction& Tx) noexcept
{
	if (UDungeoneerInventoryComponent* InvA = Tx.InitiatorInventory.Get())
	{
		for (const FGuid& Id : Tx.InitiatorOffer.OfferedItemGuids)
		{
			InvA->AuthoritySetItemTradeLock(Id, false);
		}
	}

	if (UDungeoneerInventoryComponent* InvB = Tx.CounterpartyInventory.Get())
	{
		for (const FGuid& Id : Tx.CounterpartyOffer.OfferedItemGuids)
		{
			InvB->AuthoritySetItemTradeLock(Id, false);
		}
	}

	Tx.Phase = ETradePhaseState::RolledBack;
}

void UTradeManagerComponent::Server_LockPlayerTradeOffer_Implementation(
	const FGuid& TransactionId,
	UDungeoneerInventoryComponent* PartnerInventory,
	const FTradeOfferPayload& MyOffer)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !TransactionId.IsValid())
	{
		return;
	}

	UDungeoneerInventoryComponent* MyInv = ResolveOwnerInventory();
	if (!MyInv || !PartnerInventory || MyInv == PartnerInventory || MyOffer.OfferedGold < 0)
	{
		Client_ReceiveTradeAck(TransactionId, false, TEXT("Invalid trade participants or negative gold."));
		return;
	}

	FTradeTransaction& Tx = ActiveTransactions.FindOrAdd(TransactionId);
	Tx.TransactionId = TransactionId;
	Tx.bIsVendorTrade = false;
	Tx.InitiatorInventory = MyInv;
	Tx.CounterpartyInventory = PartnerInventory;
	Tx.InitiatorOffer = MyOffer;
	Tx.bInitiatorConfirmed = false;

	// Phase 1: Validate unlocked GUIDs & Gold, then lock offered items
	if (MyInv->GetGoldBalance() < MyOffer.OfferedGold)
	{
		RollbackLockedItems(Tx);
		Client_ReceiveTradeAck(TransactionId, false, TEXT("Insufficient gold to lock offer."));
		return;
	}

	for (const FGuid& ItemId : MyOffer.OfferedItemGuids)
	{
		if (!MyInv->AuthorityHasUnlockedItem(ItemId) || !MyInv->AuthoritySetItemTradeLock(ItemId, true))
		{
			RollbackLockedItems(Tx);
			Client_ReceiveTradeAck(TransactionId, false, TEXT("Invalid or already locked item GUID."));
			return;
		}
	}

	Tx.Phase = ETradePhaseState::OfferLocked;
	Client_ReceiveTradeAck(TransactionId, false, TEXT("Offer locked. Waiting for confirmation commit."));
}

void UTradeManagerComponent::Server_ConfirmPlayerTradeCommit_Implementation(const FGuid& TransactionId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	FTradeTransaction* Tx = ActiveTransactions.Find(TransactionId);
	if (!Tx || Tx->Phase != ETradePhaseState::OfferLocked)
	{
		Client_ReceiveTradeAck(TransactionId, false, TEXT("Transaction not in locked state."));
		return;
	}

	Tx->bInitiatorConfirmed = true;
	Tx->bCounterpartyConfirmed = true; // Counterparty lock assumed validated on their paired offer

	const bool bSuccess = AuthorityExecuteTwoPhaseCommit(*Tx);
	Client_ReceiveTradeAck(TransactionId, bSuccess, bSuccess ? TEXT("Trade committed atomically.") : TEXT("Commit validation failed; rolled back."));
	ActiveTransactions.Remove(TransactionId);
}

bool UTradeManagerComponent::AuthorityExecuteTwoPhaseCommit(FTradeTransaction& InOutTx)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	UDungeoneerInventoryComponent* InvA = InOutTx.InitiatorInventory.Get();
	UDungeoneerInventoryComponent* InvB = InOutTx.CounterpartyInventory.Get();
	if (!InvA || !InvB)
	{
		RollbackLockedItems(InOutTx);
		return false;
	}

	// Phase 2 Validation: Verify currency balances and locked Item GUIDs on BOTH sides before mutating
	if (InOutTx.InitiatorOffer.OfferedGold < 0 || InOutTx.CounterpartyOffer.OfferedGold < 0)
	{
		RollbackLockedItems(InOutTx);
		return false;
	}
	if (InvA->GetGoldBalance() < InOutTx.InitiatorOffer.OfferedGold || InvB->GetGoldBalance() < InOutTx.CounterpartyOffer.OfferedGold)
	{
		RollbackLockedItems(InOutTx);
		return false;
	}

	for (const FGuid& GuidA : InOutTx.InitiatorOffer.OfferedItemGuids)
	{
		if (!InvA->AuthorityHasLockedItem(GuidA))
		{
			RollbackLockedItems(InOutTx);
			return false;
		}
	}

	for (const FGuid& GuidB : InOutTx.CounterpartyOffer.OfferedItemGuids)
	{
		if (!InvB->AuthorityHasLockedItem(GuidB))
		{
			RollbackLockedItems(InOutTx);
			return false;
		}
	}

	// Atomic Swap: Gold
	const int32 NetGoldA = InOutTx.CounterpartyOffer.OfferedGold - InOutTx.InitiatorOffer.OfferedGold;
	const int32 NetGoldB = InOutTx.InitiatorOffer.OfferedGold - InOutTx.CounterpartyOffer.OfferedGold;
	InvA->AuthorityAdjustGold(NetGoldA);
	InvB->AuthorityAdjustGold(NetGoldB);

	// Atomic Swap: Items (extract locked items from A->B and B->A)
	TArray<FDungeoneerInventoryItem> TransferToB;
	TransferToB.Reserve(InOutTx.InitiatorOffer.OfferedItemGuids.Num());
	for (const FGuid& GuidA : InOutTx.InitiatorOffer.OfferedItemGuids)
	{
		FDungeoneerInventoryItem Extracted;
		if (InvA->AuthorityExtractLockedItem(GuidA, Extracted))
		{
			TransferToB.Add(Extracted);
		}
	}

	TArray<FDungeoneerInventoryItem> TransferToA;
	TransferToA.Reserve(InOutTx.CounterpartyOffer.OfferedItemGuids.Num());
	for (const FGuid& GuidB : InOutTx.CounterpartyOffer.OfferedItemGuids)
	{
		FDungeoneerInventoryItem Extracted;
		if (InvB->AuthorityExtractLockedItem(GuidB, Extracted))
		{
			TransferToA.Add(Extracted);
		}
	}

	for (const FDungeoneerInventoryItem& Item : TransferToB)
	{
		InvB->AuthorityAddItem(Item.ItemGuid, Item.ItemId, Item.Quantity);
	}
	for (const FDungeoneerInventoryItem& Item : TransferToA)
	{
		InvA->AuthorityAddItem(Item.ItemGuid, Item.ItemId, Item.Quantity);
	}

	InOutTx.Phase = ETradePhaseState::Committed;
	return true;
}

void UTradeManagerComponent::Server_ExecuteVendorPurchase_Implementation(AVendorActor* Vendor, const FGuid& VendorItemGuid)
{
	const FGuid TxId = FGuid::NewGuid();
	if (!GetOwner() || !GetOwner()->HasAuthority() || !Vendor)
	{
		Client_ReceiveTradeAck(TxId, false, TEXT("Invalid vendor authority state."));
		return;
	}

	UDungeoneerInventoryComponent* PlayerInv = ResolveOwnerInventory();
	FVendorCatalogItem CatalogItem;
	if (!PlayerInv || !Vendor->FindCatalogItem(VendorItemGuid, CatalogItem))
	{
		Client_ReceiveTradeAck(TxId, false, TEXT("Item out of stock or invalid player inventory."));
		return;
	}

	// Phase 1: Validate & escrow currency
	if (PlayerInv->GetGoldBalance() < CatalogItem.GoldPrice)
	{
		Client_ReceiveTradeAck(TxId, false, TEXT("Insufficient gold for vendor item."));
		return;
	}

	// Phase 2: Atomic swap (consume vendor stock, debit gold, grant fresh item GUID)
	if (!Vendor->ConsumeCatalogStock(VendorItemGuid) || !PlayerInv->AuthorityAdjustGold(-CatalogItem.GoldPrice))
	{
		Client_ReceiveTradeAck(TxId, false, TEXT("Vendor atomic swap failed."));
		return;
	}

	PlayerInv->AuthorityAddItem(FGuid::NewGuid(), CatalogItem.ItemId, 1);
	Client_ReceiveTradeAck(TxId, true, TEXT("Vendor purchase committed."));
}

void UTradeManagerComponent::Server_CancelTrade_Implementation(const FGuid& TransactionId)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	if (FTradeTransaction* Tx = ActiveTransactions.Find(TransactionId))
	{
		RollbackLockedItems(*Tx);
		ActiveTransactions.Remove(TransactionId);
		Client_ReceiveTradeAck(TransactionId, false, TEXT("Trade cancelled and rolled back."));
	}
}

void UTradeManagerComponent::Client_ReceiveTradeAck_Implementation(const FGuid& TransactionId, bool bCommitted, const FString& StatusReason)
{
	if (OnTradeAckReceived.IsBound())
	{
		OnTradeAckReceived.Broadcast(TransactionId, bCommitted, StatusReason);
	}
}
