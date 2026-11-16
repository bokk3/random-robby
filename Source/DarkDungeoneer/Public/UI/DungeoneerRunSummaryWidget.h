#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DungeoneerRunSummaryWidget.generated.h"

class UButton;
class UTextBlock;

USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API FDungeoneerRunSummaryPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "UI|Summary")
	bool bVictory = false;

	UPROPERTY(BlueprintReadWrite, Category = "UI|Summary")
	bool bDailySeedRun = false;

	UPROPERTY(BlueprintReadWrite, Category = "UI|Summary")
	int32 FloorReached = 1;

	UPROPERTY(BlueprintReadWrite, Category = "UI|Summary")
	int32 KillsCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "UI|Summary")
	int32 GoldBalance = 0;

	UPROPERTY(BlueprintReadWrite, Category = "UI|Summary")
	int32 RelicsCount = 0;

	UPROPERTY(BlueprintReadWrite, Category = "UI|Summary")
	int32 RevivesPerformed = 0;

	UPROPERTY(BlueprintReadWrite, Category = "UI|Summary")
	int32 SecretsDiscovered = 0;

	UPROPERTY(BlueprintReadWrite, Category = "UI|Summary")
	float ElapsedRunSeconds = 0.0f;

	UPROPERTY(BlueprintReadWrite, Category = "UI|Summary")
	int64 MasterSeed = 0;

	UPROPERTY(BlueprintReadWrite, Category = "UI|Summary")
	uint8 PersistentBestiaryMask = 0;

	UPROPERTY(BlueprintReadWrite, Category = "UI|Summary")
	uint8 PersistentRelicMask = 0;

	UPROPERTY(BlueprintReadWrite, Category = "UI|Summary")
	int32 BestFloorRecord = 1;

	UPROPERTY(BlueprintReadWrite, Category = "UI|Summary")
	float BestClearDurationSeconds = 0.0f;
};

/**
 * TASK-1403: Obsidian & Gold Crypt Ledger Run Summary Modal.
 * Displays Victory/Defeat banner, Floor/Kills/Gold/Relics/Revives/Secrets/Duration/Seed,
 * persistent Meta-Ledger records, and [RETRY SAME SEED] / [RETURN TO MENU] actions.
 */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API UDungeoneerRunSummaryWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UDungeoneerRunSummaryWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

	UFUNCTION(BlueprintCallable, Category = "UI|Summary")
	void PopulateSummary(const FDungeoneerRunSummaryPayload& InPayload);

	UFUNCTION(BlueprintPure, Category = "UI|Summary")
	const FDungeoneerRunSummaryPayload& GetCachedSummary() const noexcept { return CachedPayload; }

	UFUNCTION(BlueprintCallable, Category = "UI|Summary")
	void TriggerRetrySameSeed();

	UFUNCTION(BlueprintCallable, Category = "UI|Summary")
	void TriggerReturnToMenu();

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRunSummaryAction, bool /*bRetrySameSeed*/);
	FOnRunSummaryAction OnRunSummaryAction;

protected:
	UFUNCTION() void HandleClickRetrySameSeed() { TriggerRetrySameSeed(); }
	UFUNCTION() void HandleClickReturnToMenu() { TriggerReturnToMenu(); }

	void RefreshDisplayTexts();

	UPROPERTY(BlueprintReadOnly, Category = "UI|Summary", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_BannerTitle;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Summary", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_RunStatsBody;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Summary", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_LedgerBody;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Summary", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_RetrySameSeed;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Summary", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_ReturnToMenu;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "UI|Summary")
	FDungeoneerRunSummaryPayload CachedPayload;
};
