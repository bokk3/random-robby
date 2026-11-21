#include "UI/DungeoneerTradeModalWidget.h"
#include "Dungeon/DungeonEncounterActors.h"
#include "Audio/GibberishSpeechComponent.h"
#include "Social/DungeoneerInventoryComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"

UDungeoneerTradeModalWidget::UDungeoneerTradeModalWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedRef<SWidget> UDungeoneerTradeModalWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TradeModalRootCanvas"));
		WidgetTree->RootWidget = RootCanvas;

		UBorder* ModalFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TradeModalFrame"));
		ModalFrame->SetBrushColor(FLinearColor(0.03f, 0.03f, 0.04f, 0.90f));
		ModalFrame->SetPadding(FMargin(20.0f, 18.0f));
		if (UCanvasPanelSlot* FrameSlot = RootCanvas->AddChildToCanvas(ModalFrame))
		{
			FrameSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			FrameSlot->SetPosition(FVector2D(-260.0, -180.0));
			FrameSlot->SetSize(FVector2D(520.0, 360.0));
		}

		UVerticalBox* ModalStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TradeModalStack"));
		ModalFrame->SetContent(ModalStack);

		Txt_VendorCatalog = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_VendorCatalog"));
		Txt_VendorCatalog->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.78f, 0.36f, 1.0f)));
		ModalStack->AddChildToVerticalBox(Txt_VendorCatalog);

		Txt_GibberishSubtitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_GibberishSubtitle"));
		Txt_GibberishSubtitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.84f, 0.88f, 0.92f, 1.0f)));
		ModalStack->AddChildToVerticalBox(Txt_GibberishSubtitle);

		Txt_TradeStatus = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_TradeStatus"));
		Txt_TradeStatus->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.92f, 0.72f, 1.0f)));
		ModalStack->AddChildToVerticalBox(Txt_TradeStatus);

		struct FTradeBtnSpec
		{
			TObjectPtr<UButton>* TargetPtr;
			const TCHAR* Name;
			const TCHAR* Label;
		};
		const FTradeBtnSpec BtnSpecs[] =
		{
			{ &Btn_BuySelected,  TEXT("Btn_BuySelected"),  TEXT("PURCHASE SELECTED RELIC") },
			{ &Btn_ConfirmTrade, TEXT("Btn_ConfirmTrade"), TEXT("CONFIRM TRADE") },
			{ &Btn_CancelTrade,  TEXT("Btn_CancelTrade"),  TEXT("CANCEL / ROLLBACK") }
		};

		for (const FTradeBtnSpec& Spec : BtnSpecs)
		{
			UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Spec.Name);
			Btn->SetBackgroundColor(FLinearColor(0.12f, 0.13f, 0.16f, 0.95f));
			UTextBlock* Txt = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			Txt->SetText(FText::FromString(Spec.Label));
			Txt->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.90f, 0.84f, 1.0f)));
			Btn->AddChild(Txt);
			ModalStack->AddChildToVerticalBox(Btn);
			*(Spec.TargetPtr) = Btn;
		}
	}

	return Super::RebuildWidget();
}

void UDungeoneerTradeModalWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_BuySelected)  { Btn_BuySelected->OnClicked.AddDynamic(this, &UDungeoneerTradeModalWidget::HandleClickBuySelected); }
	if (Btn_ConfirmTrade) { Btn_ConfirmTrade->OnClicked.AddDynamic(this, &UDungeoneerTradeModalWidget::HandleClickConfirmTrade); }
	if (Btn_CancelTrade)  { Btn_CancelTrade->OnClicked.AddDynamic(this, &UDungeoneerTradeModalWidget::HandleClickCancelTrade); }

	RefreshTradeModalTexts();
}

void UDungeoneerTradeModalWidget::NativeDestruct()
{
	if (Btn_BuySelected)  { Btn_BuySelected->OnClicked.RemoveDynamic(this, &UDungeoneerTradeModalWidget::HandleClickBuySelected); }
	if (Btn_ConfirmTrade) { Btn_ConfirmTrade->OnClicked.RemoveDynamic(this, &UDungeoneerTradeModalWidget::HandleClickConfirmTrade); }
	if (Btn_CancelTrade)  { Btn_CancelTrade->OnClicked.RemoveDynamic(this, &UDungeoneerTradeModalWidget::HandleClickCancelTrade); }

	UnbindTradeAckDelegate();
	Super::NativeDestruct();
}

void UDungeoneerTradeModalWidget::UnbindTradeAckDelegate()
{
	if (BoundTradeManager.IsValid() && TradeAckHandle.IsValid())
	{
		BoundTradeManager->OnTradeAckReceived.Remove(TradeAckHandle);
	}
	TradeAckHandle.Reset();
}

void UDungeoneerTradeModalWidget::BindTradeContext(
	AVendorActor* InVendor,
	UGibberishSpeechComponent* InSpeech,
	UTradeManagerComponent* InTradeManager,
	UDungeoneerInventoryComponent* InPlayerInventory)
{
	UnbindTradeAckDelegate();

	BoundVendor = InVendor;
	BoundSpeech = InSpeech;
	BoundTradeManager = InTradeManager;
	BoundPlayerInventory = InPlayerInventory;
	SelectedCatalogIndex = 0;

	if (InTradeManager)
	{
		TradeAckHandle = InTradeManager->OnTradeAckReceived.AddUObject(this, &UDungeoneerTradeModalWidget::HandleTradeAckReceived);
	}

	RefreshTradeModalTexts();
}

void UDungeoneerTradeModalWidget::SelectVendorCatalogIndex(int32 NewIndex)
{
	const AVendorActor* Vendor = BoundVendor.Get();
	if (!Vendor || Vendor->GetVendorCatalog().Num() == 0)
	{
		SelectedCatalogIndex = 0;
		RefreshTradeModalTexts();
		return;
	}

	const int32 Count = Vendor->GetVendorCatalog().Num();
	int32 Wrapped = NewIndex % Count;
	if (Wrapped < 0)
	{
		Wrapped += Count;
	}
	SelectedCatalogIndex = Wrapped;

	if (UGibberishSpeechComponent* Speech = BoundSpeech.Get())
	{
		const FVendorCatalogItem& Item = Vendor->GetVendorCatalog()[SelectedCatalogIndex];
		const FString ChatterLine = FString::Printf(
			TEXT("Ah, %s for %d brass? Finest relic for dark corridors!"),
			*Item.ItemId.ToString(),
			Item.GoldPrice);
		Speech->SpeakProceduralGibberish(ChatterLine);
	}

	RefreshTradeModalTexts();
}

bool UDungeoneerTradeModalWidget::ExecuteSelectedVendorPurchase()
{
	AVendorActor* Vendor = BoundVendor.Get();
	UTradeManagerComponent* TradeMgr = BoundTradeManager.Get();
	if (!Vendor || !TradeMgr)
	{
		LastTradeStatusReason = TEXT("NO ACTIVE VENDOR OR TRADE MANAGER");
		bLastTradeCommitted = false;
		RefreshTradeModalTexts();
		return false;
	}

	const TArray<FVendorCatalogItem>& Catalog = Vendor->GetVendorCatalog();
	if (!Catalog.IsValidIndex(SelectedCatalogIndex))
	{
		LastTradeStatusReason = TEXT("INVALID CATALOG SELECTION");
		bLastTradeCommitted = false;
		RefreshTradeModalTexts();
		return false;
	}

	const FVendorCatalogItem& SelectedItem = Catalog[SelectedCatalogIndex];
	bLastTradeCommitted = false;
	TradeMgr->Server_ExecuteVendorPurchase(Vendor, SelectedItem.ItemGuid);

	if (UGibberishSpeechComponent* Speech = BoundSpeech.Get())
	{
		Speech->SpeakProceduralGibberish(bLastTradeCommitted
			? TEXT("Pleasure trading. Mind the mirror shadows!")
			: TEXT("Coin purse feels light, explorer?"));
	}

	RefreshTradeModalTexts();
	return (TradeMgr->GetOwner() && TradeMgr->GetOwner()->HasAuthority()) ? bLastTradeCommitted : true;
}

FGuid UDungeoneerTradeModalWidget::LockPlayerOfferWithPartner(
	UDungeoneerInventoryComponent* PartnerInventory,
	const TArray<FGuid>& OfferedGuids,
	int32 OfferedGold)
{
	UTradeManagerComponent* TradeMgr = BoundTradeManager.Get();
	if (!TradeMgr)
	{
		return FGuid();
	}

	ActiveTransactionId = FGuid::NewGuid();

	FTradeOfferPayload Offer;
	Offer.OfferedItemGuids = OfferedGuids;
	Offer.OfferedGold = OfferedGold;

	TradeMgr->Server_LockPlayerTradeOffer(ActiveTransactionId, PartnerInventory, Offer);
	LastTradeStatusReason = TEXT("OFFER LOCKED - AWAITING CONFIRM");
	bLastTradeCommitted = false;
	RefreshTradeModalTexts();

	return ActiveTransactionId;
}

void UDungeoneerTradeModalWidget::ConfirmActivePlayerTradeCommit()
{
	if (UTradeManagerComponent* TradeMgr = BoundTradeManager.Get())
	{
		if (ActiveTransactionId.IsValid())
		{
			TradeMgr->Server_ConfirmPlayerTradeCommit(ActiveTransactionId);
		}
	}
	RefreshTradeModalTexts();
}

void UDungeoneerTradeModalWidget::CancelActivePlayerTrade()
{
	if (UTradeManagerComponent* TradeMgr = BoundTradeManager.Get())
	{
		if (ActiveTransactionId.IsValid())
		{
			TradeMgr->Server_CancelTrade(ActiveTransactionId);
		}
	}
	ActiveTransactionId.Invalidate();
	RefreshTradeModalTexts();
}

void UDungeoneerTradeModalWidget::HandleTradeAckReceived(const FGuid& TxId, bool bCommitted, const FString& StatusReason)
{
	bLastTradeCommitted = bCommitted;
	LastTradeStatusReason = StatusReason;
	RefreshTradeModalTexts();
}

FString UDungeoneerTradeModalWidget::GetFormattedVendorCatalogText() const
{
	const AVendorActor* Vendor = BoundVendor.Get();
	if (!Vendor || Vendor->GetVendorCatalog().Num() == 0)
	{
		return TEXT("VENDOR CATALOG: [EMPTY]");
	}

	const TArray<FVendorCatalogItem>& Catalog = Vendor->GetVendorCatalog();
	const int32 Idx = Catalog.IsValidIndex(SelectedCatalogIndex) ? SelectedCatalogIndex : 0;
	const FVendorCatalogItem& Item = Catalog[Idx];
	const int32 PlayerGold = BoundPlayerInventory.IsValid() ? BoundPlayerInventory->GetGoldBalance() : 0;

	return FString::Printf(
		TEXT("[%d/%d] %s | COST %dg | STOCK %d | PURSE %dg"),
		Idx + 1,
		Catalog.Num(),
		*Item.ItemId.ToString(),
		Item.GoldPrice,
		Item.StockCount,
		PlayerGold);
}

FString UDungeoneerTradeModalWidget::GetFormattedGibberishFeedText() const
{
	const UGibberishSpeechComponent* Speech = BoundSpeech.Get();
	if (!Speech || Speech->GetActiveDialogueLine().IsEmpty())
	{
		return TEXT("MERCHANT: \"Step closer to the fire, explorer.\"");
	}

	return FString::Printf(
		TEXT("MERCHANT [GRAIN #%d %.1fst]: \"%s\""),
		Speech->GetLastSyllableIndex(),
		Speech->GetLastPitchOffsetSemitones(),
		*Speech->GetActiveDialogueLine());
}

void UDungeoneerTradeModalWidget::RefreshTradeModalTexts() noexcept
{
	if (Txt_VendorCatalog)
	{
		Txt_VendorCatalog->SetText(FText::FromString(GetFormattedVendorCatalogText()));
	}
	if (Txt_GibberishSubtitle)
	{
		Txt_GibberishSubtitle->SetText(FText::FromString(GetFormattedGibberishFeedText()));
	}
	if (Txt_TradeStatus)
	{
		Txt_TradeStatus->SetText(FText::FromString(LastTradeStatusReason));
	}
}
