#include "UI/DungeoneerPlayerHUDWidget.h"
#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerMovementComponent.h"
#include "Rendering/SensoryPerkComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UDungeoneerPlayerHUDWidget::UDungeoneerPlayerHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedRef<SWidget> UDungeoneerPlayerHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PlayerHUDRootCanvas"));
		WidgetTree->RootWidget = RootCanvas;

		UBorder* HUDFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PlayerHUDFrame"));
		HUDFrame->SetBrushColor(FLinearColor(0.02f, 0.025f, 0.035f, 0.82f));
		HUDFrame->SetPadding(FMargin(14.0f, 10.0f));
		if (UCanvasPanelSlot* FrameSlot = RootCanvas->AddChildToCanvas(HUDFrame))
		{
			FrameSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
			FrameSlot->SetPosition(FVector2D(24.0, 24.0));
			FrameSlot->SetSize(FVector2D(360.0, 148.0));
		}

		UVerticalBox* HUDStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PlayerHUDStack"));
		HUDFrame->SetContent(HUDStack);

		Txt_HealthReadout = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_HealthReadout"));
		Txt_HealthReadout->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.35f, 0.32f, 1.0f)));
		HUDStack->AddChildToVerticalBox(Txt_HealthReadout);

		Txt_BatteryReadout = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_BatteryReadout"));
		Txt_BatteryReadout->SetColorAndOpacity(FSlateColor(FLinearColor(0.96f, 0.78f, 0.32f, 1.0f)));
		HUDStack->AddChildToVerticalBox(Txt_BatteryReadout);

		Txt_OctantIndicator = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_OctantIndicator"));
		Txt_OctantIndicator->SetColorAndOpacity(FSlateColor(FLinearColor(0.86f, 0.90f, 0.92f, 1.0f)));
		HUDStack->AddChildToVerticalBox(Txt_OctantIndicator);

		Txt_ChronoSlowReadout = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_ChronoSlowReadout"));
		Txt_ChronoSlowReadout->SetColorAndOpacity(FSlateColor(FLinearColor(0.35f, 0.92f, 0.76f, 1.0f)));
		HUDStack->AddChildToVerticalBox(Txt_ChronoSlowReadout);

		Txt_SensoryPerksReadout = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_SensoryPerksReadout"));
		Txt_SensoryPerksReadout->SetColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.78f, 0.98f, 1.0f)));
		HUDStack->AddChildToVerticalBox(Txt_SensoryPerksReadout);
	}

	return Super::RebuildWidget();
}

void UDungeoneerPlayerHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	RefreshHUDFromOwningLocalPlayer();
}

void UDungeoneerPlayerHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshHUDFromOwningLocalPlayer();
}

void UDungeoneerPlayerHUDWidget::RefreshHUDFromOwningLocalPlayer() noexcept
{
	const UWorld* World = GetWorld();
	const ULocalPlayer* OwningLP = GetOwningLocalPlayer();
	if (!World || !OwningLP)
	{
		return;
	}

	if (const APlayerController* OwningPC = OwningLP->GetPlayerController(World))
	{
		if (const ADungeoneerCharacter* Character = Cast<ADungeoneerCharacter>(OwningPC->GetPawn()))
		{
			RefreshHUDFromCharacter(Character);
		}
	}
}

void UDungeoneerPlayerHUDWidget::RefreshHUDFromCharacter(const ADungeoneerCharacter* Character) noexcept
{
	if (!Character)
	{
		return;
	}

	CachedCurrentHealth = FMath::Max(0.0f, Character->GetCurrentHealth());
	CachedMaxHealth = FMath::Max(1.0f, Character->GetMaxHealth());
	HealthPercent = FMath::Clamp(CachedCurrentHealth / CachedMaxHealth, 0.0f, 1.0f);
	BatteryPercent = FMath::Clamp(Character->GetFlashlightBattery() / 100.0f, 0.0f, 1.0f);
	if (const UDungeoneerMovementComponent* MoveComp = Character->GetDungeoneerMovement())
	{
		ChronoSlowRatio = MoveComp->GetChronoSlowRatio();
		if (Character->IsLocallyControlled() || !Character->GetController())
		{
			MoveOctant = MoveComp->GetMoveOctant();
			AimOctant = MoveComp->GetAimOctant();
		}
		else
		{
			MoveOctant = Character->GetReplicatedMoveOctant();
			AimOctant = Character->GetReplicatedAimOctant();
		}
	}
	else
	{
		MoveOctant = Character->GetReplicatedMoveOctant();
		AimOctant = Character->GetReplicatedAimOctant();
		ChronoSlowRatio = 0.0f;
	}

	if (const USensoryPerkComponent* Perks = Character->GetSensoryPerkComponent())
	{
		DarkVisionTier = Perks->GetPerkLevel(EDungeoneerSensoryPerk::DarkVision);
		AcuteHearingTier = Perks->GetPerkLevel(EDungeoneerSensoryPerk::AcuteHearing);
		ScentThermalTier = Perks->GetPerkLevel(EDungeoneerSensoryPerk::ScentThermal);
	}

	UpdateTextBlocks();
}

FString UDungeoneerPlayerHUDWidget::GetFormattedVitalsText() const
{
	return FString::Printf(TEXT("HP %.0f/%.0f (%.0f%%) | BEAM %.0f%%"),
		CachedCurrentHealth,
		CachedMaxHealth,
		HealthPercent * 100.0f,
		BatteryPercent * 100.0f);
}

FString UDungeoneerPlayerHUDWidget::GetFormattedOctantText() const
{
	const int32 MoveDeg = static_cast<int32>(DungeoneerMath::GCompassYawDegrees[static_cast<uint8>(MoveOctant) & 7]);
	const int32 AimDeg = static_cast<int32>(DungeoneerMath::GCompassYawDegrees[static_cast<uint8>(AimOctant) & 7]);
	return FString::Printf(TEXT("MOVE %d DEG | AIM %d DEG"), MoveDeg, AimDeg);
}

FString UDungeoneerPlayerHUDWidget::GetFormattedChronoSlowText() const
{
	return FString::Printf(TEXT("CHRONO-SLOW S=%.2f (%.0f%% SPEED)"),
		ChronoSlowRatio,
		FMath::Clamp(1.0f - ChronoSlowRatio, 0.05f, 1.0f) * 100.0f);
}

void UDungeoneerPlayerHUDWidget::UpdateTextBlocks() noexcept
{
	if (Txt_HealthReadout)
	{
		Txt_HealthReadout->SetText(FText::FromString(FString::Printf(TEXT("HEALTH: %.0f / %.0f"), CachedCurrentHealth, CachedMaxHealth)));
	}
	if (Txt_BatteryReadout)
	{
		Txt_BatteryReadout->SetText(FText::FromString(FString::Printf(TEXT("FLASHLIGHT: %.0f%%"), BatteryPercent * 100.0f)));
	}
	if (Txt_OctantIndicator)
	{
		Txt_OctantIndicator->SetText(FText::FromString(GetFormattedOctantText()));
	}
	if (Txt_ChronoSlowReadout)
	{
		Txt_ChronoSlowReadout->SetText(FText::FromString(GetFormattedChronoSlowText()));
	}
	if (Txt_SensoryPerksReadout)
	{
		Txt_SensoryPerksReadout->SetText(FText::FromString(FString::Printf(TEXT("SENSES [DV:%d AH:%d ST:%d]"),
			DarkVisionTier, AcuteHearingTier, ScentThermalTier)));
	}
}
