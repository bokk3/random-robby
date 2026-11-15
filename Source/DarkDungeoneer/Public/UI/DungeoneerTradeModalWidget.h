#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Social/TradeManagerComponent.h"
#include "DungeoneerTradeModalWidget.generated.h"

class AVendorActor;
class UGibberishSpeechComponent;
class UDungeoneerInventoryComponent;
class UButton;
class UTextBlock;

/**
 * TASK-902: Resting Ground Vendor & Two-Phase Commit Trade Modal Widget.
 * Binds AVendorActor, UGibberishSpeechComponent, UTradeManagerComponent, and
 * UDungeoneerInventoryComponent cleanly in C++ with UTradeManagerComponent::OnTradeAckReceived
 * delegate handling and deterministic gibberish merchant feedback.
 */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API UDungeoneerTradeModalWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UDungeoneerTradeModalWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** Binds vendor, speech synthesizer, trade manager, and owning player inventory to this modal. */
	UFUNCTION(BlueprintCallable, Category = "UI|Trade")
	void BindTradeContext(
		AVendorActor* InVendor,
		UGibberishSpeechComponent* InSpeech,
		UTradeManagerComponent* InTradeManager,
		UDungeoneerInventoryComponent* InPlayerInventory);

	/** Selects an item index in the bound vendor's seeded catalog and triggers gibberish chatter. */
	UFUNCTION(BlueprintCallable, Category = "UI|Trade")
	void SelectVendorCatalogIndex(int32 NewIndex);

	/** Executes a server-authoritative vendor purchase for the currently selected catalog entry. */
	UFUNCTION(BlueprintCallable, Category = "UI|Trade")
	bool ExecuteSelectedVendorPurchase();

	/** Locks local offered items/gold into a two-phase commit transaction with a counterparty player. */
	UFUNCTION(BlueprintCallable, Category = "UI|Trade")
	FGuid LockPlayerOfferWithPartner(
		UDungeoneerInventoryComponent* PartnerInventory,
		const TArray<FGuid>& OfferedGuids,
		int32 OfferedGold);

	/** Sends final Phase-2 confirmation for ActiveTransactionId. */
	UFUNCTION(BlueprintCallable, Category = "UI|Trade")
	void ConfirmActivePlayerTradeCommit();

	/** Cancels and rolls back ActiveTransactionId cleanly. */
	UFUNCTION(BlueprintCallable, Category = "UI|Trade")
	void CancelActivePlayerTrade();

	UFUNCTION(BlueprintPure, Category = "UI|Trade")
	int32 GetSelectedCatalogIndex() const noexcept { return SelectedCatalogIndex; }

	UFUNCTION(BlueprintPure, Category = "UI|Trade")
	FGuid GetActiveTransactionId() const noexcept { return ActiveTransactionId; }

	UFUNCTION(BlueprintPure, Category = "UI|Trade")
	const FString& GetLastTradeStatusReason() const noexcept { return LastTradeStatusReason; }

	UFUNCTION(BlueprintPure, Category = "UI|Trade")
	bool WasLastTradeCommitted() const noexcept { return bLastTradeCommitted; }

	UFUNCTION(BlueprintPure, Category = "UI|Trade")
	FString GetFormattedVendorCatalogText() const;

	UFUNCTION(BlueprintPure, Category = "UI|Trade")
	FString GetFormattedGibberishFeedText() const;

protected:
	void HandleTradeAckReceived(const FGuid& TxId, bool bCommitted, const FString& StatusReason);
	void UnbindTradeAckDelegate();
	void RefreshTradeModalTexts() noexcept;

	UFUNCTION() void HandleClickBuySelected() { ExecuteSelectedVendorPurchase(); }
	UFUNCTION() void HandleClickConfirmTrade() { ConfirmActivePlayerTradeCommit(); }
	UFUNCTION() void HandleClickCancelTrade() { CancelActivePlayerTrade(); }

	UPROPERTY(BlueprintReadOnly, Category = "UI|Trade", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_VendorCatalog;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Trade", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_GibberishSubtitle;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Trade", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_TradeStatus;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Trade", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_BuySelected;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Trade", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_ConfirmTrade;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Trade", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_CancelTrade;

private:
	TWeakObjectPtr<AVendorActor> BoundVendor;
	TWeakObjectPtr<UGibberishSpeechComponent> BoundSpeech;
	TWeakObjectPtr<UTradeManagerComponent> BoundTradeManager;
	TWeakObjectPtr<UDungeoneerInventoryComponent> BoundPlayerInventory;
	FDelegateHandle TradeAckHandle;

	int32 SelectedCatalogIndex = 0;
	FGuid ActiveTransactionId;
	FString LastTradeStatusReason = TEXT("READY TO BARTER");
	bool bLastTradeCommitted = false;
};
