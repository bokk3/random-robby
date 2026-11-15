#include "UI/DungeoneerMainMenuWidget.h"
#include "Core/DungeoneerGameMode.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/PackageName.h"

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

		UBorder* LeftPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LeftGothicPanel"));
		LeftPanel->SetBrushColor(FLinearColor(0.03f, 0.035f, 0.045f, 0.88f));
		LeftPanel->SetPadding(FMargin(24.0f, 24.0f));
		if (UCanvasPanelSlot* PanelSlot = RootCanvas->AddChildToCanvas(LeftPanel))
		{
			PanelSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
			PanelSlot->SetPosition(FVector2D(48.0, 48.0));
			PanelSlot->SetSize(FVector2D(430.0, 560.0));
		}

		UVerticalBox* MenuStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuStack"));
		LeftPanel->SetContent(MenuStack);

		UTextBlock* TitleText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_Title"));
		TitleText->SetText(FText::FromString(TEXT("DARK DUNGEONEER")));
		TitleText->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.72f, 0.32f, 1.0f)));
		MenuStack->AddChildToVerticalBox(TitleText);

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
			ButtonWidget->SetBackgroundColor(FLinearColor(0.11f, 0.12f, 0.15f, 0.95f));

			UTextBlock* LabelWidget = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			LabelWidget->SetText(FText::FromString(GetActionDisplayLabel(Item.Action)));
			LabelWidget->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.88f, 0.82f, 1.0f)));
			ButtonWidget->AddChild(LabelWidget);

			MenuStack->AddChildToVerticalBox(ButtonWidget);
			*(Item.TargetPtr) = ButtonWidget;
		}

		Txt_ActiveActionDescription = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_ActiveActionDescription"));
		Txt_ActiveActionDescription->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.65f, 0.25f, 1.0f)));
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
}
