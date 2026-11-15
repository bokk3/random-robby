#include "UI/DungeoneerPlayerHUDWidget.h"
#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerGameMode.h"
#include "Core/DungeoneerGameState.h"
#include "Core/DungeoneerMovementComponent.h"
#include "Rendering/SensoryPerkComponent.h"
#include "Social/DungeoneerInventoryComponent.h"
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
			FrameSlot->SetSize(FVector2D(400.0, 196.0));
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

		Txt_CombatAndLoopReadout = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_CombatAndLoopReadout"));
		Txt_CombatAndLoopReadout->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.86f, 0.48f, 1.0f)));
		HUDStack->AddChildToVerticalBox(Txt_CombatAndLoopReadout);

		Txt_InteractHint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_InteractHint"));
		Txt_InteractHint->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f, 0.98f, 0.65f, 1.0f)));
		HUDStack->AddChildToVerticalBox(Txt_InteractHint);
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

	CachedKillsCount = Character->GetKillsCount();
	if (const UDungeoneerInventoryComponent* Inv = Character->GetInventoryComponent())
	{
		CachedGoldBalance = Inv->GetGoldBalance();
	}

	CachedFloorIndex = 1;
	if (const UWorld* World = Character->GetWorld())
	{
		if (const ADungeoneerGameState* GS = World->GetGameState<ADungeoneerGameState>())
		{
			CachedFloorIndex = FMath::Max(1, GS->GetReplicatedFloorSeed().FloorIndex);
		}
		else if (const ADungeoneerGameMode* GM = World->GetAuthGameMode<ADungeoneerGameMode>())
		{
			CachedFloorIndex = FMath::Max(1, GM->GetCurrentFloorIndex());
		}
	}

	switch (Character->GetCombatActionState())
	{
	case EDungeoneerCombatActionState::LanternSlash: CachedCombatStateLabel = TEXT("SLASH!"); break;
	case EDungeoneerCombatActionState::DodgeDash:    CachedCombatStateLabel = TEXT("DODGE!"); break;
	case EDungeoneerCombatActionState::HitStagger:   CachedCombatStateLabel = TEXT("STAGGER"); break;
	case EDungeoneerCombatActionState::Downed:       CachedCombatStateLabel = TEXT("DOWNED"); break;
	default:                                         CachedCombatStateLabel = TEXT("READY"); break;
	}

	switch (Character->GetNearbyInteractType())
	{
	case EDungeoneerNearbyInteract::VendorTrade:      CachedInteractHint = TEXT("[F] BUY SUPPLY FROM VENDOR"); break;
	case EDungeoneerNearbyInteract::UnlockVault:      CachedInteractHint = TEXT("[F] UNLOCK TERMINAL LOOT VAULT"); break;
	case EDungeoneerNearbyInteract::DescendNextFloor: CachedInteractHint = TEXT("[F] DESCEND TO NEXT FLOOR"); break;
	default:                                          CachedInteractHint = TEXT("LMB/SPACE: SLASH | SHIFT: DODGE | WASD: MOVE"); break;
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

FString UDungeoneerPlayerHUDWidget::GetFormattedCombatAndLoopText() const
{
	return FString::Printf(TEXT("FLOOR %d | GOLD %d | KILLS %d | [%s]"),
		CachedFloorIndex,
		CachedGoldBalance,
		CachedKillsCount,
		*CachedCombatStateLabel);
}

FString UDungeoneerPlayerHUDWidget::GetFormattedInteractHintText() const
{
	return CachedInteractHint;
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
	if (Txt_CombatAndLoopReadout)
	{
		Txt_CombatAndLoopReadout->SetText(FText::FromString(GetFormattedCombatAndLoopText()));
	}
	if (Txt_InteractHint)
	{
		Txt_InteractHint->SetText(FText::FromString(CachedInteractHint));
	}
}
