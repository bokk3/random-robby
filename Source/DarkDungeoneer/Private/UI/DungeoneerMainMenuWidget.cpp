#include "UI/DungeoneerMainMenuWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"

UDungeoneerMainMenuWidget::UDungeoneerMainMenuWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
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
