#include "UI/DungeoneerRunSummaryWidget.h"
#include "Core/DungeoneerGameMode.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"

namespace DungeoneerSummaryHelpers
{
	static int32 CountBitsSet(uint8 Mask, int32 NumBits) noexcept
	{
		int32 Count = 0;
		for (int32 Bit = 0; Bit < NumBits; ++Bit)
		{
			if ((Mask & (1u << Bit)) != 0u)
			{
				++Count;
			}
		}
		return Count;
	}

	static FString FormatDurationMMSS(float Seconds)
	{
		const int32 TotalSec = FMath::Max(0, FMath::RoundToInt(Seconds));
		const int32 Mins = TotalSec / 60;
		const int32 Secs = TotalSec % 60;
		return FString::Printf(TEXT("%02d:%02d"), Mins, Secs);
	}
}

UDungeoneerRunSummaryWidget::UDungeoneerRunSummaryWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedRef<SWidget> UDungeoneerRunSummaryWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SummaryRootCanvas"));
		WidgetTree->RootWidget = RootCanvas;

		UBorder* BackdropDim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SummaryBackdrop"));
		BackdropDim->SetBrushColor(FLinearColor(0.01f, 0.012f, 0.02f, 0.72f));
		if (UCanvasPanelSlot* DimSlot = RootCanvas->AddChildToCanvas(BackdropDim))
		{
			DimSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			DimSlot->SetOffsets(FMargin(0.0f));
		}

		UBorder* GoldOuterFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SummaryGoldFrame"));
		GoldOuterFrame->SetBrushColor(FLinearColor(0.84f, 0.64f, 0.22f, 0.96f));
		GoldOuterFrame->SetPadding(FMargin(2.5f));
		if (UCanvasPanelSlot* FrameSlot = RootCanvas->AddChildToCanvas(GoldOuterFrame))
		{
			FrameSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			FrameSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			FrameSlot->SetPosition(FVector2D(0.0, 0.0));
			FrameSlot->SetSize(FVector2D(590.0, 440.0));
		}

		UBorder* ObsidianPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SummaryObsidianPanel"));
		ObsidianPanel->SetBrushColor(FLinearColor(0.024f, 0.028f, 0.044f, 0.97f));
		ObsidianPanel->SetPadding(FMargin(26.0f, 22.0f));
		GoldOuterFrame->SetContent(ObsidianPanel);

		UVerticalBox* Stack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SummaryStack"));
		ObsidianPanel->SetContent(Stack);

		Txt_BannerTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_BannerTitle"));
		Txt_BannerTitle->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 20));
		Txt_BannerTitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.78f, 0.30f, 1.0f)));
		Stack->AddChildToVerticalBox(Txt_BannerTitle);

		USpacer* TopSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		TopSpacer->SetSize(FVector2D(1.0, 12.0));
		Stack->AddChildToVerticalBox(TopSpacer);

		Txt_RunStatsBody = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_RunStatsBody"));
		Txt_RunStatsBody->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 12));
		Txt_RunStatsBody->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.90f, 0.86f, 1.0f)));
		Stack->AddChildToVerticalBox(Txt_RunStatsBody);

		USpacer* MidSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		MidSpacer->SetSize(FVector2D(1.0, 14.0));
		Stack->AddChildToVerticalBox(MidSpacer);

		UBorder* LedgerBox = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("LedgerMetaBox"));
		LedgerBox->SetBrushColor(FLinearColor(0.05f, 0.06f, 0.10f, 0.92f));
		LedgerBox->SetPadding(FMargin(12.0f, 10.0f));
		Stack->AddChildToVerticalBox(LedgerBox);

		Txt_LedgerBody = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_LedgerBody"));
		Txt_LedgerBody->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 11));
		Txt_LedgerBody->SetColorAndOpacity(FSlateColor(FLinearColor(0.56f, 0.88f, 0.92f, 1.0f)));
		LedgerBox->SetContent(Txt_LedgerBody);

		USpacer* ActionSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass());
		ActionSpacer->SetSize(FVector2D(1.0, 18.0));
		Stack->AddChildToVerticalBox(ActionSpacer);

		UHorizontalBox* ButtonRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("SummaryButtons"));
		Stack->AddChildToVerticalBox(ButtonRow);

		Btn_RetrySameSeed = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Btn_RetrySameSeed"));
		Btn_RetrySameSeed->SetBackgroundColor(FLinearColor(0.28f, 0.21f, 0.08f, 0.98f));
		UTextBlock* RetryLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		RetryLabel->SetText(FText::FromString(TEXT("RETRY SAME SEED")));
		RetryLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 12));
		RetryLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.88f, 0.56f, 1.0f)));
		Btn_RetrySameSeed->AddChild(RetryLabel);
		if (UHorizontalBoxSlot* RetrySlot = ButtonRow->AddChildToHorizontalBox(Btn_RetrySameSeed))
		{
			RetrySlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
		}

		Btn_ReturnToMenu = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Btn_ReturnToMenu"));
		Btn_ReturnToMenu->SetBackgroundColor(FLinearColor(0.10f, 0.12f, 0.18f, 0.98f));
		UTextBlock* MenuLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		MenuLabel->SetText(FText::FromString(TEXT("RETURN TO MENU")));
		MenuLabel->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 12));
		MenuLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.90f, 0.92f, 0.96f, 1.0f)));
		Btn_ReturnToMenu->AddChild(MenuLabel);
		ButtonRow->AddChildToHorizontalBox(Btn_ReturnToMenu);
	}

	return Super::RebuildWidget();
}

void UDungeoneerRunSummaryWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (Btn_RetrySameSeed)
	{
		Btn_RetrySameSeed->OnClicked.AddDynamic(this, &UDungeoneerRunSummaryWidget::HandleClickRetrySameSeed);
	}
	if (Btn_ReturnToMenu)
	{
		Btn_ReturnToMenu->OnClicked.AddDynamic(this, &UDungeoneerRunSummaryWidget::HandleClickReturnToMenu);
	}

	RefreshDisplayTexts();
}

void UDungeoneerRunSummaryWidget::NativeDestruct()
{
	if (Btn_RetrySameSeed)
	{
		Btn_RetrySameSeed->OnClicked.RemoveDynamic(this, &UDungeoneerRunSummaryWidget::HandleClickRetrySameSeed);
	}
	if (Btn_ReturnToMenu)
	{
		Btn_ReturnToMenu->OnClicked.RemoveDynamic(this, &UDungeoneerRunSummaryWidget::HandleClickReturnToMenu);
	}

	Super::NativeDestruct();
}

void UDungeoneerRunSummaryWidget::PopulateSummary(const FDungeoneerRunSummaryPayload& InPayload)
{
	CachedPayload = InPayload;
	RefreshDisplayTexts();
}

void UDungeoneerRunSummaryWidget::RefreshDisplayTexts()
{
	if (Txt_BannerTitle)
	{
		Txt_BannerTitle->SetText(FText::FromString(CachedPayload.bVictory
			? TEXT("CRYPT GUARDIAN SLAIN  •  EXPEDITION CLEARED")
			: TEXT("EXPEDITION FALLEN IN THE DARK")));
		Txt_BannerTitle->SetColorAndOpacity(FSlateColor(CachedPayload.bVictory
			? FLinearColor(0.98f, 0.80f, 0.28f, 1.0f)
			: FLinearColor(0.94f, 0.34f, 0.28f, 1.0f)));
	}

	if (Txt_RunStatsBody)
	{
		const FString TimeStr = DungeoneerSummaryHelpers::FormatDurationMMSS(CachedPayload.ElapsedRunSeconds);
		const TCHAR* SeedTag = CachedPayload.bDailySeedRun ? TEXT("DAILY") : TEXT("STANDARD");
		Txt_RunStatsBody->SetText(FText::FromString(FString::Printf(
			TEXT("FLOOR REACHED: %d / 3   |   CLEAR TIME: %s\n")
			TEXT("ZOMBIES SLAIN: %d   |   GOLD LEDGER: %d G\n")
			TEXT("RELICS FOUND: %d / 5   |   SECRETS FOUND: %d   |   ALLIES REVIVED: %d\n")
			TEXT("SEED [%s]: %lld"),
			CachedPayload.FloorReached,
			*TimeStr,
			CachedPayload.KillsCount,
			CachedPayload.GoldBalance,
			CachedPayload.RelicsCount,
			CachedPayload.SecretsDiscovered,
			CachedPayload.RevivesPerformed,
			SeedTag,
			CachedPayload.MasterSeed)));
	}

	if (Txt_LedgerBody)
	{
		const int32 BestiaryCount = DungeoneerSummaryHelpers::CountBitsSet(CachedPayload.PersistentBestiaryMask, 4);
		const int32 RelicCount = DungeoneerSummaryHelpers::CountBitsSet(CachedPayload.PersistentRelicMask, 5);
		const FString BestTime = (CachedPayload.BestClearDurationSeconds > 0.0f)
			? DungeoneerSummaryHelpers::FormatDurationMMSS(CachedPayload.BestClearDurationSeconds)
			: FString(TEXT("--:--"));

		Txt_LedgerBody->SetText(FText::FromString(FString::Printf(
			TEXT("PERSISTENT META-LEDGER\n")
			TEXT("BEST FLOOR: %d   |   FASTEST CLEAR: %s   |   BESTIARY: %d / 4   |   RELICS: %d / 5"),
			FMath::Max(1, CachedPayload.BestFloorRecord),
			*BestTime,
			BestiaryCount,
			RelicCount)));
	}
}

void UDungeoneerRunSummaryWidget::TriggerRetrySameSeed()
{
	if (OnRunSummaryAction.IsBound())
	{
		OnRunSummaryAction.Broadcast(true);
	}
	if (UWorld* World = GetWorld())
	{
		if (ADungeoneerGameMode* GM = World->GetAuthGameMode<ADungeoneerGameMode>())
		{
			GM->RetryCurrentSeedRun();
		}
	}
}

void UDungeoneerRunSummaryWidget::TriggerReturnToMenu()
{
	if (OnRunSummaryAction.IsBound())
	{
		OnRunSummaryAction.Broadcast(false);
	}
	if (UWorld* World = GetWorld())
	{
		if (ADungeoneerGameMode* GM = World->GetAuthGameMode<ADungeoneerGameMode>())
		{
			GM->ReturnToMainMenuFromSummary();
		}
	}
}
