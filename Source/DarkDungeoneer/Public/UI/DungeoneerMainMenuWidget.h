#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DungeoneerMainMenuWidget.generated.h"

class UButton;
class UTextBlock;

UENUM(BlueprintType)
enum class EDungeoneerMenuAction : uint8
{
	EnterCatacombs     = 0 UMETA(DisplayName = "Enter the Catacombs"),
	ContinueExpedition = 1 UMETA(DisplayName = "Continue Expedition"),
	CoopSplitScreen    = 2 UMETA(DisplayName = "Co-Op & Split-Screen"),
	SensoryLoadout     = 3 UMETA(DisplayName = "Sensory Perks & Loadout"),
	MirrorTrial        = 4 UMETA(DisplayName = "Mirror-Palace Trial"),
	RestingGround      = 5 UMETA(DisplayName = "Resting Ground & Vendors"),
	Options            = 6 UMETA(DisplayName = "Options & Display"),
	Quit               = 7 UMETA(DisplayName = "Quit to Desktop")
};

/**
 * TASK-801: C++20 Main Menu Stack & Mode Dispatcher backing docs/MENU_MOCKUP.md.
 * Handles the 8 navigation entries purely in C++ with optional UMG data-only widget bindings.
 */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API UDungeoneerMainMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static constexpr int32 MenuActionCount = 8;

	UDungeoneerMainMenuWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Selects one of the 8 menu entries cleanly (wraps around boundaries) and emits preview callbacks. */
	UFUNCTION(BlueprintCallable, Category = "UI|Menu")
	void SelectMenuIndex(int32 NewIndex) noexcept;

	UFUNCTION(BlueprintCallable, Category = "UI|Menu")
	void MoveSelectionUp() noexcept { SelectMenuIndex(SelectedMenuIndex - 1); }

	UFUNCTION(BlueprintCallable, Category = "UI|Menu")
	void MoveSelectionDown() noexcept { SelectMenuIndex(SelectedMenuIndex + 1); }

	UFUNCTION(BlueprintPure, Category = "UI|Menu")
	int32 GetSelectedMenuIndex() const noexcept { return SelectedMenuIndex; }

	UFUNCTION(BlueprintPure, Category = "UI|Menu")
	EDungeoneerMenuAction GetSelectedMenuAction() const noexcept;

	/** Executes the currently selected menu action (server/client travel or Quit). */
	UFUNCTION(BlueprintCallable, Category = "UI|Menu")
	void ActivateSelectedMenuItem();

	/** Returns the target Unreal map asset name mapped to the given menu action. */
	UFUNCTION(BlueprintPure, Category = "UI|Menu")
	FName GetTargetTravelMapForAction(EDungeoneerMenuAction Action) const noexcept;

	/** Returns static human-readable label for a menu action without heap-allocating temporary strings. */
	static const TCHAR* GetActionDisplayLabel(EDungeoneerMenuAction Action) noexcept;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMenuSelectionChanged, EDungeoneerMenuAction);
	FOnMenuSelectionChanged OnMenuSelectionChanged;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMenuActionActivated, EDungeoneerMenuAction);
	FOnMenuActionActivated OnMenuActionActivated;

protected:
	UFUNCTION() void HandleClickEnterCatacombs() { ActivateSpecificAction(EDungeoneerMenuAction::EnterCatacombs); }
	UFUNCTION() void HandleClickContinueExpedition() { ActivateSpecificAction(EDungeoneerMenuAction::ContinueExpedition); }
	UFUNCTION() void HandleClickCoopSplitScreen() { ActivateSpecificAction(EDungeoneerMenuAction::CoopSplitScreen); }
	UFUNCTION() void HandleClickSensoryLoadout() { ActivateSpecificAction(EDungeoneerMenuAction::SensoryLoadout); }
	UFUNCTION() void HandleClickMirrorTrial() { ActivateSpecificAction(EDungeoneerMenuAction::MirrorTrial); }
	UFUNCTION() void HandleClickRestingGround() { ActivateSpecificAction(EDungeoneerMenuAction::RestingGround); }
	UFUNCTION() void HandleClickOptions() { ActivateSpecificAction(EDungeoneerMenuAction::Options); }
	UFUNCTION() void HandleClickQuit() { ActivateSpecificAction(EDungeoneerMenuAction::Quit); }

	void ActivateSpecificAction(EDungeoneerMenuAction Action);
	void RefreshSelectionVisuals() noexcept;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Menu", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_EnterCatacombs;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Menu", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_ContinueExpedition;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Menu", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_CoopSplitScreen;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Menu", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_SensoryLoadout;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Menu", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_MirrorTrial;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Menu", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_RestingGround;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Menu", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Options;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Menu", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Quit;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Menu", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_ActiveActionDescription;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Maps")
	FName CatacombsMapName = FName(TEXT("/Game/Maps/L_DungeonRun"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Maps")
	FName MirrorTrialMapName = FName(TEXT("/Game/Maps/L_MirrorTrial"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI|Maps")
	FName RestingGroundMapName = FName(TEXT("/Game/Maps/L_RestingGround"));

private:
	int32 SelectedMenuIndex = 0;
};
