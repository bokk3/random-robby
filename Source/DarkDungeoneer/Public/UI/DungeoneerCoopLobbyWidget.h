#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/DungeoneerTypes.h"
#include "DungeoneerCoopLobbyWidget.generated.h"

class UTextBlock;

UENUM(BlueprintType)
enum class EDungeoneerSplitscreenMode : uint8
{
	Solo1P      = 0 UMETA(DisplayName = "1P SOLO"),
	SplitHoriz2P = 1 UMETA(DisplayName = "2P HORIZ"),
	SplitTrio3P = 2 UMETA(DisplayName = "3P TRIO"),
	SplitQuad4P = 3 UMETA(DisplayName = "4P QUAD")
};

USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API FCoopSquadSlotViewModel
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "UI|Coop")
	int32 SlotIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Coop")
	bool bOccupied = false;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Coop")
	bool bIsLocalSlot = false;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Coop")
	int32 LocalControllerId = -1;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Coop")
	FString DisplayLabel = TEXT("OPEN SLOT");

	UPROPERTY(BlueprintReadOnly, Category = "UI|Coop")
	int32 DarkVisionTier = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Coop")
	int32 AcuteHearingTier = 0;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Coop")
	int32 ScentThermalTier = 0;
};

/**
 * TASK-803: Top-Right Co-Op & Split-Screen Squad Card ViewModel.
 * Polls ULocalPlayer, UDungeoneerGameViewportClient, UDungeoneerLocalPlayerSubsystem,
 * and UDungeoneerSocialSubsystem with strict multi-viewport isolation.
 */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API UDungeoneerCoopLobbyWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxSquadSlots = 4;

	UDungeoneerCoopLobbyWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;

	/** Polls ULocalPlayer array, per-viewport sensory tiers, and online social squad state. */
	UFUNCTION(BlueprintCallable, Category = "UI|Coop")
	void RefreshSquadSlotsFromSubsystems();

	/** Updates split-screen layout mode on UDungeoneerGameViewportClient. */
	UFUNCTION(BlueprintCallable, Category = "UI|Coop")
	void SetSplitscreenMode(EDungeoneerSplitscreenMode NewMode);

	UFUNCTION(BlueprintPure, Category = "UI|Coop")
	EDungeoneerSplitscreenMode GetActiveSplitscreenMode() const noexcept { return ActiveSplitscreenMode; }

	/** Attempts to join a local split-screen player controller via UDungeoneerGameViewportClient. */
	UFUNCTION(BlueprintCallable, Category = "UI|Coop")
	bool TryJoinLocalPlayerSlot(int32 ControllerId);

	/** Hosts a co-op session for the owning ULocalPlayer and invites a cached friend by NetId. */
	UFUNCTION(BlueprintCallable, Category = "UI|Coop")
	bool InviteFriendToCoopSlot(const FString& FriendNetId);

	UFUNCTION(BlueprintPure, Category = "UI|Coop")
	const TArray<FCoopSquadSlotViewModel>& GetSquadSlots() const noexcept { return SquadSlots; }

	UFUNCTION(BlueprintPure, Category = "UI|Coop")
	static FString GetSplitscreenBadgeText(EDungeoneerSplitscreenMode Mode);

protected:
	void EnsureFixedSquadSlots();

	UPROPERTY(BlueprintReadOnly, Category = "UI|Coop", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_SplitscreenBadge;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Coop")
	TArray<FCoopSquadSlotViewModel> SquadSlots;

private:
	EDungeoneerSplitscreenMode ActiveSplitscreenMode = EDungeoneerSplitscreenMode::Solo1P;
};
