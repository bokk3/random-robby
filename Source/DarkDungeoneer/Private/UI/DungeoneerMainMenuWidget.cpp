#include "UI/DungeoneerMainMenuWidget.h"
#include "Core/DungeoneerGameMode.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/PackageName.h"
#include "Styling/CoreStyle.h"

UDungeoneerMainMenuWidget::UDungeoneerMainMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedRef<SWidget> UDungeoneerMainMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MainMenuRootCanvas"));
		WidgetTree->RootWidget = RootCanvas;

		UBorder* OuterGoldTrim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OuterGoldTrim"));
		OuterGoldTrim->SetBrushColor(FLinearColor(0.76f, 0.56f, 0.18f, 0.85f));
		OuterGoldTrim->SetPadding(FMargin(2.0f));
		if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(OuterGoldTrim))
		{
			PanelSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
			PanelSlot->SetPosition(FVector2D(40.0, 44.0));
			PanelSlot->SetSize(FVector2D(416.0, 560.0));
		}

		UBorder* LeftPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LeftGothicPanel"));
		LeftPanel->SetBrushColor(FLinearColor(0.022f, 0.026f, 0.042f, 0.92f));
		LeftPanel->SetPadding(FMargin(22.0f, 20.0f));
		OuterGoldTrim->SetContent(LeftPanel);

		UVerticalBox* MenuStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuStack"));
		LeftPanel->SetContent(MenuStack);

		UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_Title"));
		TitleText->SetText(FText::FromString(TEXT("DARK DUNGEONEER")));
		TitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 24));
		TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.96f, 0.76f, 0.30f, 1.0f)));
		MenuStack->AddChildToVerticalBox(TitleText);

		UTextBlock* SubtitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_Subtitle"));
		SubtitleText->SetText(FText::FromString(TEXT("LIVING ECOSYSTEM  •  v1.3.0")));
		SubtitleText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 10));
		SubtitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.45f, 0.82f, 0.90f, 0.90f)));
		MenuStack->AddChildToVerticalBox(SubtitleText);

		USpacer* HeaderSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("MenuHeaderSpacer"));
		HeaderSpacer->SetSize(FVector2D(1.0, 16.0));
		MenuStack->AddChildToVerticalBox(HeaderSpacer);

		struct FMenuButtonBinding
		{
			TObjectPtr<UButton>* TargetPtr;
			EDungeoneerMenuAction Action;
			const TCHAR* WidgetName;
		};
		const FMenuButtonBinding Bindings[] =
		{
			{ &Btn_EnterCatacombs,     EDungeoneerMenuAction::EnterCatacombs,     TEXT("Btn_EnterCatacombs") },
			{ &Btn_ContinueExpedition, EDungeoneerMenuAction::ContinueExpedition, TEXT("Btn_ContinueExpedition") },
			{ &Btn_CoopSplitScreen,    EDungeoneerMenuAction::CoopSplitScreen,    TEXT("Btn_CoopSplitScreen") },
			{ &Btn_SensoryLoadout,     EDungeoneerMenuAction::SensoryLoadout,     TEXT("Btn_SensoryLoadout") },
			{ &Btn_MirrorTrial,        EDungeoneerMenuAction::MirrorTrial,        TEXT("Btn_MirrorTrial") },
			{ &Btn_RestingGround,      EDungeoneerMenuAction::RestingGround,      TEXT("Btn_RestingGround") },
			{ &Btn_Options,            EDungeoneerMenuAction::Options,            TEXT("Btn_Options") },
			{ &Btn_Quit,               EDungeoneerMenuAction::Quit,               TEXT("Btn_Quit") }
		};

		for (const FMenuButtonBinding& Item : Bindings)
		{
			UButton* ButtonWidget = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Item.WidgetName);
			ButtonWidget->SetBackgroundColor(FLinearColor(0.08f, 0.10f, 0.15f, 0.96f));

			UBorder* BtnInnerPad = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			BtnInnerPad->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f));
			BtnInnerPad->SetPadding(FMargin(10.0f, 6.0f));

			UTextBlock* LabelWidget = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			LabelWidget->SetText(FText::FromString(GetActionDisplayLabel(Item.Action)));
			LabelWidget->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 12));
			LabelWidget->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.90f, 0.84f, 1.0f)));
			BtnInnerPad->SetContent(LabelWidget);
			ButtonWidget->AddChild(BtnInnerPad);

			MenuStack->AddChildToVerticalBox(ButtonWidget);

			USpacer* RowSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
			RowSpacer->SetSize(FVector2D(1.0, 5.0));
			MenuStack->AddChildToVerticalBox(RowSpacer);

			*(Item.TargetPtr) = ButtonWidget;
		}

		USpacer* FooterSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("MenuFooterSpacer"));
		FooterSpacer->SetSize(FVector2D(1.0, 8.0));
		MenuStack->AddChildToVerticalBox(FooterSpacer);

		Txt_ActiveActionDescription = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_ActiveActionDescription"));
		Txt_ActiveActionDescription->SetFont(FCoreStyle::GetDefaultFontStyle("Italic", 11));
		Txt_ActiveActionDescription->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.72f, 0.28f, 1.0f)));
		MenuStack->AddChildToVerticalBox(Txt_ActiveActionDescription);
	}

	return Super::RebuildWidget();
}

void UDungeoneerMainMenuWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_EnterCatacombs)     { Btn_EnterCatacombs->OnClicked.AddDynamic(this, &UDungeoneerMainMenuWidget::HandleClickEnterCatacombs); }
	if (Btn_ContinueExpedition) { Btn_ContinueExpedition->OnClicked.AddDynamic(this, &UDungeoneerMainMenuWidget::HandleClickContinueExpedition); }
	if (Btn_CoopSplitScreen)    { Btn_CoopSplitScreen->OnClicked.AddDynamic(this, &UDungeoneerMainMenuWidget::HandleClickCoopSplitScreen); }
	if (Btn_SensoryLoadout)     { Btn_SensoryLoadout->OnClicked.AddDynamic(this, &UDungeoneerMainMenuWidget::HandleClickSensoryLoadout); }
	if (Btn_MirrorTrial)        { Btn_MirrorTrial->OnClicked.AddDynamic(this, &UDungeoneerMainMenuWidget::HandleClickMirrorTrial); }
	if (Btn_RestingGround)      { Btn_RestingGround->OnClicked.AddDynamic(this, &UDungeoneerMainMenuWidget::HandleClickRestingGround); }
	if (Btn_Options)            { Btn_Options->OnClicked.AddDynamic(this, &UDungeoneerMainMenuWidget::HandleClickOptions); }
	if (Btn_Quit)               { Btn_Quit->OnClicked.AddDynamic(this, &UDungeoneerMainMenuWidget::HandleClickQuit); }

	RefreshSelectionVisuals();
}

void UDungeoneerMainMenuWidget::NativeDestruct()
{
	if (Btn_EnterCatacombs)     { Btn_EnterCatacombs->OnClicked.RemoveDynamic(this, &UDungeoneerMainMenuWidget::HandleClickEnterCatacombs); }
	if (Btn_ContinueExpedition) { Btn_ContinueExpedition->OnClicked.RemoveDynamic(this, &UDungeoneerMainMenuWidget::HandleClickContinueExpedition); }
	if (Btn_CoopSplitScreen)    { Btn_CoopSplitScreen->OnClicked.RemoveDynamic(this, &UDungeoneerMainMenuWidget::HandleClickCoopSplitScreen); }
	if (Btn_SensoryLoadout)     { Btn_SensoryLoadout->OnClicked.RemoveDynamic(this, &UDungeoneerMainMenuWidget::HandleClickSensoryLoadout); }
	if (Btn_MirrorTrial)        { Btn_MirrorTrial->OnClicked.RemoveDynamic(this, &UDungeoneerMainMenuWidget::HandleClickMirrorTrial); }
	if (Btn_RestingGround)      { Btn_RestingGround->OnClicked.RemoveDynamic(this, &UDungeoneerMainMenuWidget::HandleClickRestingGround); }
	if (Btn_Options)            { Btn_Options->OnClicked.RemoveDynamic(this, &UDungeoneerMainMenuWidget::HandleClickOptions); }
	if (Btn_Quit)               { Btn_Quit->OnClicked.RemoveDynamic(this, &UDungeoneerMainMenuWidget::HandleClickQuit); }

	Super::NativeDestruct();
}

void UDungeoneerMainMenuWidget::SelectMenuIndex(int32 NewIndex) noexcept
{
	int32 Wrapped = NewIndex % MenuActionCount;
	if (Wrapped < 0)
	{
		Wrapped += MenuActionCount;
	}

	if (SelectedMenuIndex == Wrapped)
	{
		return;
	}

	SelectedMenuIndex = Wrapped;
	RefreshSelectionVisuals();

	if (OnMenuSelectionChanged.IsBound())
	{
		OnMenuSelectionChanged.Broadcast(GetSelectedMenuAction());
	}
}

EDungeoneerMenuAction UDungeoneerMainMenuWidget::GetSelectedMenuAction() const noexcept
{
	return static_cast<EDungeoneerMenuAction>(FMath::Clamp(SelectedMenuIndex, 0, MenuActionCount - 1));
}

FName UDungeoneerMainMenuWidget::GetTargetTravelMapForAction(EDungeoneerMenuAction Action) const noexcept
{
	switch (Action)
	{
	case EDungeoneerMenuAction::EnterCatacombs:
	case EDungeoneerMenuAction::ContinueExpedition:
		return CatacombsMapName;
	case EDungeoneerMenuAction::MirrorTrial:
		return MirrorTrialMapName;
	case EDungeoneerMenuAction::RestingGround:
		return RestingGroundMapName;
	default:
		return NAME_None;
	}
}

const TCHAR* UDungeoneerMainMenuWidget::GetActionDisplayLabel(EDungeoneerMenuAction Action) noexcept
{
	switch (Action)
	{
	case EDungeoneerMenuAction::EnterCatacombs:     return TEXT("ENTER THE CATACOMBS");
	case EDungeoneerMenuAction::ContinueExpedition: return TEXT("CONTINUE EXPEDITION");
	case EDungeoneerMenuAction::CoopSplitScreen:    return TEXT("CO-OP & SPLIT-SCREEN");
	case EDungeoneerMenuAction::SensoryLoadout:     return TEXT("SENSORY PERKS & LOADOUT");
	case EDungeoneerMenuAction::MirrorTrial:        return TEXT("MIRROR-PALACE TRIAL");
	case EDungeoneerMenuAction::RestingGround:      return TEXT("RESTING GROUND & VENDORS");
	case EDungeoneerMenuAction::Options:            return TEXT("OPTIONS & DISPLAY");
	case EDungeoneerMenuAction::Quit:               return TEXT("QUIT TO DESKTOP");
	default:                                        return TEXT("UNKNOWN");
	}
}

void UDungeoneerMainMenuWidget::ActivateSelectedMenuItem()
{
	ActivateSpecificAction(GetSelectedMenuAction());
}

void UDungeoneerMainMenuWidget::ActivateSpecificAction(EDungeoneerMenuAction Action)
{
	SelectedMenuIndex = static_cast<int32>(Action);
	RefreshSelectionVisuals();

	if (OnMenuActionActivated.IsBound())
	{
		OnMenuActionActivated.Broadcast(Action);
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	ULocalPlayer* OwningLP = GetOwningLocalPlayer();
	APlayerController* OwningPC = OwningLP ? OwningLP->GetPlayerController(World) : nullptr;

	if (Action == EDungeoneerMenuAction::Quit)
	{
		UKismetSystemLibrary::QuitGame(World, OwningPC, EQuitPreference::Quit, false);
		return;
	}

	const FName TargetMap = GetTargetTravelMapForAction(Action);
	if (TargetMap.IsNone())
	{
		return;
	}

	if (!FPackageName::DoesPackageExist(TargetMap.ToString()))
	{
		if (ADungeoneerGameMode* GM = World->GetAuthGameMode<ADungeoneerGameMode>())
		{
			GM->EnterLiveDungeonFromMenu(Action);
		}
		return;
	}

	if (World->GetNetMode() < NM_Client)
	{
		World->ServerTravel(TargetMap.ToString());
	}
	else if (OwningPC)
	{
		OwningPC->ClientTravel(TargetMap.ToString(), TRAVEL_Absolute);
	}
}

void UDungeoneerMainMenuWidget::RefreshSelectionVisuals() noexcept
{
	if (Txt_ActiveActionDescription)
	{
		Txt_ActiveActionDescription->SetText(FText::FromString(GetActionDisplayLabel(GetSelectedMenuAction())));
	}

	UButton* Buttons[8] =
	{
		Btn_EnterCatacombs.Get(), Btn_ContinueExpedition.Get(), Btn_CoopSplitScreen.Get(),
		Btn_SensoryLoadout.Get(), Btn_MirrorTrial.Get(), Btn_RestingGround.Get(),
		Btn_Options.Get(), Btn_Quit.Get()
	};
	for (int32 i = 0; i < 8; ++i)
	{
		if (Buttons[i])
		{
			const bool bSelected = (i == SelectedMenuIndex);
			Buttons[i]->SetBackgroundColor(bSelected
				? FLinearColor(0.28f, 0.21f, 0.08f, 0.98f)
				: FLinearColor(0.08f, 0.10f, 0.15f, 0.96f));
		}
	}
}
