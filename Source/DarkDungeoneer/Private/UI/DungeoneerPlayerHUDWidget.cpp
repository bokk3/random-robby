#include "UI/DungeoneerPlayerHUDWidget.h"
#include "AI/BossGuardianActor.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerGameMode.h"
#include "Core/DungeoneerGameState.h"
#include "Core/DungeoneerMovementComponent.h"
#include "Dungeon/DungeonGeneratorSubsystem.h"
#include "Rendering/SensoryPerkComponent.h"
#include "Social/DungeoneerInventoryComponent.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/NativeWidgetHost.h"
#include "Components/ProgressBar.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"

/**
 * Zero-allocation Slate custom OnPaint floor minimap drawing discovered DAG rooms,
 * corridors, room icons (Sanctuary/Vendor/Vault/Mirror), and 8-compass player aim ray.
 */
class SDungeoneerMinimapWidget : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SDungeoneerMinimapWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UDungeoneerPlayerHUDWidget>, OwnerHUD)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		OwnerHUD = InArgs._OwnerHUD;
		ScratchLinePts.SetNum(2);
	}

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
	{
		return FVector2D(176.0, 176.0);
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override
	{
		const UDungeoneerPlayerHUDWidget* HUD = OwnerHUD.Get();
		if (!HUD)
		{
			return LayerId;
		}

		const FVector2D Size = AllottedGeometry.GetLocalSize();
		const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush(TEXT("WhiteBrush"));

		// Deep obsidian backdrop
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(0.015f, 0.018f, 0.032f, 0.88f));

		const int32 RoomCount = HUD->GetMinimapRoomCount();
		if (RoomCount <= 0)
		{
			return LayerId + 1;
		}

		const FVector2D BoundsMin = HUD->GetMinimapBoundsMin();
		const FVector2D BoundsMax = HUD->GetMinimapBoundsMax();
		const double SpanX = FMath::Max(BoundsMax.X - BoundsMin.X, 400.0);
		const double SpanY = FMath::Max(BoundsMax.Y - BoundsMin.Y, 400.0);
		const double MaxSpan = FMath::Max(SpanX, SpanY);
		const double Pad = 14.0;
		const double DrawScale = FMath::Min(Size.X - 2.0 * Pad, Size.Y - 2.0 * Pad) / MaxSpan;
		const FVector2D CenterWorld = (BoundsMin + BoundsMax) * 0.5;
		const FVector2D CenterMap = Size * 0.5;

		auto WorldToMap = [&](const FVector2D& WorldXY) -> FVector2D
		{
			const FVector2D Rel = (WorldXY - CenterWorld) * DrawScale;
			// Match camera Yaw=-45 roughly: +X upper-right, -Y upper-left
			return FVector2D(CenterMap.X + Rel.X, CenterMap.Y - Rel.Y);
		};

		// 1. Connecting corridor lines between visible rooms
		for (int32 i = 0; i < RoomCount; ++i)
		{
			const FDungeoneerMinimapRoomSlot& RoomA = HUD->GetMinimapRoomAt(i);
			if (!RoomA.bDiscovered && RoomA.bSecretRoom)
			{
				continue;
			}
			for (int32 j = 0; j < RoomCount; ++j)
			{
				if ((RoomA.OutgoingMask & (1u << j)) != 0)
				{
					const FDungeoneerMinimapRoomSlot& RoomB = HUD->GetMinimapRoomAt(j);
					if (!RoomB.bDiscovered && RoomB.bSecretRoom)
					{
						continue;
					}
					ScratchLinePts[0] = FVector2f(WorldToMap(RoomA.CenterXY));
					ScratchLinePts[1] = FVector2f(WorldToMap(RoomB.CenterXY));
					FSlateDrawElement::MakeLines(
						OutDrawElements,
						LayerId + 1,
						AllottedGeometry.ToPaintGeometry(),
						ScratchLinePts,
						ESlateDrawEffect::None,
						FLinearColor(0.32f, 0.38f, 0.52f, 0.72f),
						true,
						1.5f);
				}
			}
		}

		// 2. Color-coded room nodes
		for (int32 i = 0; i < RoomCount; ++i)
		{
			const FDungeoneerMinimapRoomSlot& Room = HUD->GetMinimapRoomAt(i);
			if (Room.bSecretRoom && !Room.bDiscovered)
			{
				continue;
			}

			FLinearColor NodeColor(0.24f, 0.28f, 0.38f, 0.75f);
			switch (Room.RoomType)
			{
			case EDungeonRoomType::RestingGround:    NodeColor = FLinearColor(0.18f, 0.78f, 0.42f, 0.90f); break;
			case EDungeonRoomType::Vendor:           NodeColor = FLinearColor(0.16f, 0.78f, 0.88f, 0.90f); break;
			case EDungeonRoomType::MirrorPalaceMaze: NodeColor = FLinearColor(0.58f, 0.34f, 0.92f, 0.88f); break;
			case EDungeonRoomType::MajorLootVault:   NodeColor = FLinearColor(0.95f, 0.76f, 0.22f, 0.95f); break;
			case EDungeonRoomType::DeepFogGauntlet:  NodeColor = FLinearColor(0.78f, 0.32f, 0.28f, 0.85f); break;
			default: break;
			}
			if (!Room.bDiscovered)
			{
				NodeColor.A *= 0.42f;
			}
			if (Room.bActive)
			{
				NodeColor = FLinearColor::LerpUsingHSV(NodeColor, FLinearColor(1.0f, 0.96f, 0.78f, 1.0f), 0.35f);
			}

			const FVector2D BoxCenter = WorldToMap(Room.CenterXY);
			const FVector2D BoxSize(
				FMath::Max(10.0, Room.ExtentsXY.X * DrawScale * 1.45),
				FMath::Max(10.0, Room.ExtentsXY.Y * DrawScale * 1.45));
			FSlateDrawElement::MakeBox(
				OutDrawElements,
				LayerId + 2,
				AllottedGeometry.ToPaintGeometry(BoxSize, FSlateLayoutTransform(BoxCenter - BoxSize * 0.5)),
				WhiteBrush,
				ESlateDrawEffect::None,
				NodeColor);
		}

		// 3. Player indicator + Aim octant ray
		const FVector2D PlayerMapPos = WorldToMap(HUD->GetMinimapPlayerXY());
		const FVector2f AimVec = DungeoneerMath::GCompassUnitVectors[static_cast<uint8>(HUD->GetAimOctant()) & 7];
		ScratchLinePts[0] = FVector2f(PlayerMapPos);
		ScratchLinePts[1] = FVector2f(PlayerMapPos + FVector2D(AimVec.X * 14.0, -AimVec.Y * 14.0));
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId + 3,
			AllottedGeometry.ToPaintGeometry(),
			ScratchLinePts,
			ESlateDrawEffect::None,
			FLinearColor(1.0f, 0.82f, 0.28f, 0.95f),
			true,
			2.0f);

		const FVector2D PipSize(6.0, 6.0);
		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId + 4,
			AllottedGeometry.ToPaintGeometry(PipSize, FSlateLayoutTransform(PlayerMapPos - PipSize * 0.5)),
			WhiteBrush,
			ESlateDrawEffect::None,
			FLinearColor(1.0f, 0.96f, 0.72f, 1.0f));

		return LayerId + 5;
	}

private:
	TWeakObjectPtr<UDungeoneerPlayerHUDWidget> OwnerHUD;
	mutable TArray<FVector2f> ScratchLinePts;
};

/**
 * Zero-allocation screen-space renderer for ADungeoneerCharacter's 32-slot FDungeoneerFloatingPopup ring buffer.
 */
class SDungeoneerFloatingPopupWidget : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SDungeoneerFloatingPopupWidget) {}
		SLATE_ARGUMENT(TWeakObjectPtr<UDungeoneerPlayerHUDWidget>, OwnerHUD)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		OwnerHUD = InArgs._OwnerHUD;
	}

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override
	{
		return FVector2D(1280.0, 720.0);
	}

	virtual int32 OnPaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override
	{
		const UDungeoneerPlayerHUDWidget* HUD = OwnerHUD.Get();
		const ADungeoneerCharacter* Character = HUD ? HUD->GetObservedCharacter() : nullptr;
		const APlayerController* PC = Character ? Character->GetController<APlayerController>() : nullptr;
		if (!HUD || !Character || !PC)
		{
			return LayerId;
		}

		const FSlateFontInfo FontBold = FCoreStyle::GetDefaultFontStyle("Bold", 13);
		const float DPIScale = FMath::Max(AllottedGeometry.Scale, 0.5f);

		for (int32 i = 0; i < ADungeoneerCharacter::MaxFloatingPopups; ++i)
		{
			const FDungeoneerFloatingPopup& Pop = Character->GetFloatingPopupAt(i);
			if (!Pop.bActive || Pop.Text.IsEmpty())
			{
				continue;
			}

			FVector2D ScreenPos;
			if (!PC->ProjectWorldLocationToScreen(Pop.WorldLocation, ScreenPos, true))
			{
				continue;
			}

			const float Alpha = 1.0f - FMath::Clamp(Pop.AgeSeconds / FMath::Max(Pop.LifetimeSeconds, 0.1f), 0.0f, 1.0f);
			FLinearColor Color(0.95f, 0.35f, 0.32f, Alpha);
			switch (Pop.Kind)
			{
			case EDungeoneerFloatingTextKind::CritDamage: Color = FLinearColor(1.00f, 0.65f, 0.18f, Alpha); break;
			case EDungeoneerFloatingTextKind::Gold:       Color = FLinearColor(1.00f, 0.85f, 0.25f, Alpha); break;
			case EDungeoneerFloatingTextKind::Relic:      Color = FLinearColor(0.28f, 0.94f, 1.00f, Alpha); break;
			case EDungeoneerFloatingTextKind::Status:     Color = FLinearColor(0.55f, 0.98f, 0.68f, Alpha); break;
			default: break;
			}

			const FVector2D LocalPos = ScreenPos / static_cast<double>(DPIScale);
			FSlateDrawElement::MakeText(
				OutDrawElements,
				LayerId + 1,
				AllottedGeometry.ToPaintGeometry(FVector2D(220.0, 28.0), FSlateLayoutTransform(LocalPos - FVector2D(40.0, 14.0))),
				Pop.Text,
				FontBold,
				ESlateDrawEffect::None,
				Color);
		}

		return LayerId + 2;
	}

private:
	TWeakObjectPtr<UDungeoneerPlayerHUDWidget> OwnerHUD;
};

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

		// Fullscreen pooled floating combat text layer (non-hit-testable)
		FloatingPopupSlateHost = WidgetTree->ConstructWidget<UNativeWidgetHost>(UNativeWidgetHost::StaticClass(), TEXT("FloatingPopupSlateHost"));
		FloatingPopupSlateHost->SetVisibility(ESlateVisibility::HitTestInvisible);
		FloatingPopupSlateWidget = SNew(SDungeoneerFloatingPopupWidget).OwnerHUD(this);
		FloatingPopupSlateHost->SetContent(FloatingPopupSlateWidget.ToSharedRef());
		if (UCanvasPanelSlot* PopSlot = RootCanvas->AddChildToCanvas(FloatingPopupSlateHost))
		{
			PopSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
			PopSlot->SetOffsets(FMargin(0.0f));
		}

		// Top-Left Gothic Vitals & Telemetry Card (dark obsidian with warm gold outer trim)
		UBorder* OuterVitalsTrim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OuterVitalsTrim"));
		OuterVitalsTrim->SetBrushColor(FLinearColor(0.72f, 0.54f, 0.18f, 0.78f));
		OuterVitalsTrim->SetPadding(FMargin(1.5f));
		if (UCanvasPanelSlot* FrameSlot = RootCanvas->AddChildToCanvas(OuterVitalsTrim))
		{
			FrameSlot->SetAnchors(FAnchors(0.0f, 0.0f, 0.0f, 0.0f));
			FrameSlot->SetPosition(FVector2D(20.0, 20.0));
			FrameSlot->SetSize(FVector2D(390.0, 178.0));
		}

		UBorder* HUDFrame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PlayerHUDFrame"));
		HUDFrame->SetBrushColor(FLinearColor(0.02f, 0.024f, 0.038f, 0.88f));
		HUDFrame->SetPadding(FMargin(12.0f, 9.0f));
		OuterVitalsTrim->SetContent(HUDFrame);

		UVerticalBox* HUDStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PlayerHUDStack"));
		HUDFrame->SetContent(HUDStack);

		Txt_HealthReadout = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_HealthReadout"));
		Txt_HealthReadout->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 11));
		Txt_HealthReadout->SetColorAndOpacity(FSlateColor(FLinearColor(0.96f, 0.42f, 0.38f, 1.0f)));
		HUDStack->AddChildToVerticalBox(Txt_HealthReadout);

		PB_Health = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("PB_Health"));
		PB_Health->SetPercent(1.0f);
		PB_Health->SetFillColorAndOpacity(FLinearColor(0.84f, 0.16f, 0.18f, 1.0f));
		HUDStack->AddChildToVerticalBox(PB_Health);

		Txt_BatteryReadout = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_BatteryReadout"));
		Txt_BatteryReadout->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 11));
		Txt_BatteryReadout->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.80f, 0.34f, 1.0f)));
		HUDStack->AddChildToVerticalBox(Txt_BatteryReadout);

		PB_Battery = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("PB_Battery"));
		PB_Battery->SetPercent(1.0f);
		PB_Battery->SetFillColorAndOpacity(FLinearColor(0.96f, 0.72f, 0.18f, 1.0f));
		HUDStack->AddChildToVerticalBox(PB_Battery);

		Txt_OctantIndicator = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_OctantIndicator"));
		Txt_OctantIndicator->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 10));
		Txt_OctantIndicator->SetColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.88f, 0.94f, 1.0f)));
		HUDStack->AddChildToVerticalBox(Txt_OctantIndicator);

		Txt_ChronoSlowReadout = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_ChronoSlowReadout"));
		Txt_ChronoSlowReadout->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 10));
		Txt_ChronoSlowReadout->SetColorAndOpacity(FSlateColor(FLinearColor(0.35f, 0.92f, 0.76f, 1.0f)));
		HUDStack->AddChildToVerticalBox(Txt_ChronoSlowReadout);

		Txt_SensoryPerksReadout = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_SensoryPerksReadout"));
		Txt_SensoryPerksReadout->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 10));
		Txt_SensoryPerksReadout->SetColorAndOpacity(FSlateColor(FLinearColor(0.54f, 0.82f, 0.98f, 1.0f)));
		HUDStack->AddChildToVerticalBox(Txt_SensoryPerksReadout);

		Txt_CombatAndLoopReadout = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_CombatAndLoopReadout"));
		Txt_CombatAndLoopReadout->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 11));
		Txt_CombatAndLoopReadout->SetColorAndOpacity(FSlateColor(FLinearColor(0.98f, 0.84f, 0.42f, 1.0f)));
		HUDStack->AddChildToVerticalBox(Txt_CombatAndLoopReadout);

		// Top-Center Boss Health Plate (shown automatically when Boss Guardian is active)
		BossBarContainer = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BossBarContainer"));
		BossBarContainer->SetBrushColor(FLinearColor(0.03f, 0.02f, 0.03f, 0.90f));
		BossBarContainer->SetPadding(FMargin(14.0f, 8.0f));
		BossBarContainer->SetVisibility(ESlateVisibility::Collapsed);
		if (UCanvasPanelSlot* BossSlot = RootCanvas->AddChildToCanvas(BossBarContainer))
		{
			BossSlot->SetAnchors(FAnchors(0.5f, 0.0f, 0.5f, 0.0f));
			BossSlot->SetPosition(FVector2D(-220.0, 20.0));
			BossSlot->SetSize(FVector2D(440.0, 56.0));
		}
		UVerticalBox* BossStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("BossStack"));
		BossBarContainer->SetContent(BossStack);
		Txt_BossTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_BossTitle"));
		Txt_BossTitle->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 11));
		Txt_BossTitle->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.58f, 0.22f, 1.0f)));
		BossStack->AddChildToVerticalBox(Txt_BossTitle);
		PB_BossHealth = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("PB_BossHealth"));
		PB_BossHealth->SetPercent(1.0f);
		PB_BossHealth->SetFillColorAndOpacity(FLinearColor(0.96f, 0.34f, 0.12f, 1.0f));
		BossStack->AddChildToVerticalBox(PB_BossHealth);

		// Top-Right Gothic Floor Minimap
		UBorder* MinimapTrim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MinimapTrim"));
		MinimapTrim->SetBrushColor(FLinearColor(0.72f, 0.54f, 0.18f, 0.80f));
		MinimapTrim->SetPadding(FMargin(1.5f));
		if (UCanvasPanelSlot* MapSlot = RootCanvas->AddChildToCanvas(MinimapTrim))
		{
			MapSlot->SetAnchors(FAnchors(1.0f, 0.0f, 1.0f, 0.0f));
			MapSlot->SetPosition(FVector2D(-198.0, 20.0));
			MapSlot->SetSize(FVector2D(178.0, 178.0));
		}
		MinimapSlateHost = WidgetTree->ConstructWidget<UNativeWidgetHost>(UNativeWidgetHost::StaticClass(), TEXT("MinimapSlateHost"));
		MinimapSlateWidget = SNew(SDungeoneerMinimapWidget).OwnerHUD(this);
		MinimapSlateHost->SetContent(MinimapSlateWidget.ToSharedRef());
		MinimapTrim->SetContent(MinimapSlateHost);

		// Bottom-Center Supplies, Relics & Interact Prompt Dock
		UBorder* BottomDockTrim = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BottomDockTrim"));
		BottomDockTrim->SetBrushColor(FLinearColor(0.52f, 0.42f, 0.18f, 0.78f));
		BottomDockTrim->SetPadding(FMargin(1.5f));
		if (UCanvasPanelSlot* DockSlot = RootCanvas->AddChildToCanvas(BottomDockTrim))
		{
			DockSlot->SetAnchors(FAnchors(0.5f, 1.0f, 0.5f, 1.0f));
			DockSlot->SetPosition(FVector2D(-290.0, -78.0));
			DockSlot->SetSize(FVector2D(580.0, 60.0));
		}
		UBorder* BottomDock = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BottomDock"));
		BottomDock->SetBrushColor(FLinearColor(0.02f, 0.025f, 0.04f, 0.86f));
		BottomDock->SetPadding(FMargin(14.0f, 7.0f));
		BottomDockTrim->SetContent(BottomDock);

		UVerticalBox* DockStack = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DockStack"));
		BottomDock->SetContent(DockStack);

		Txt_RelicsAndSupplies = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_RelicsAndSupplies"));
		Txt_RelicsAndSupplies->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 10));
		Txt_RelicsAndSupplies->SetColorAndOpacity(FSlateColor(FLinearColor(0.38f, 0.90f, 0.98f, 1.0f)));
		DockStack->AddChildToVerticalBox(Txt_RelicsAndSupplies);

		Txt_InteractHint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Txt_InteractHint"));
		Txt_InteractHint->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 11));
		Txt_InteractHint->SetColorAndOpacity(FSlateColor(FLinearColor(0.56f, 0.98f, 0.68f, 1.0f)));
		DockStack->AddChildToVerticalBox(Txt_InteractHint);
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
	ObservedCharacter = Character;

	CachedCurrentHealth = FMath::Max(0.0f, Character->GetCurrentHealth());
	CachedMaxHealth = FMath::Max(1.0f, Character->GetMaxHealth());
	HealthPercent = FMath::Clamp(CachedCurrentHealth / CachedMaxHealth, 0.0f, 1.0f);
	BatteryPercent = FMath::Clamp(Character->GetFlashlightBattery() / 100.0f, 0.0f, 1.0f);
	CachedComboStep = Character->GetComboStepIndex();

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
		CachedOilPhials = Inv->GetConsumableCount(EDungeoneerConsumableId::LanternOilPhial);
		CachedAntidotes = Inv->GetConsumableCount(EDungeoneerConsumableId::AntidoteDraught);
		CachedRelicMask = Inv->GetRelicBitmask();
	}

	CachedFloorIndex = 1;
	bBossActive = false;
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

		if (const UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
		{
			if (const ABossGuardianActor* Boss = SpatialSys->GetActiveBossGuardian())
			{
				if (Boss->IsZombieAlive())
				{
					bBossActive = true;
					CachedBossHealthPercent = Boss->GetBossHealthPercent();
					CachedBossPhase = Boss->GetBossPhase();
				}
			}
		}
	}

	switch (Character->GetCombatActionState())
	{
	case EDungeoneerCombatActionState::LanternSlash: CachedCombatStateLabel = TEXT("SLASH!"); break;
	case EDungeoneerCombatActionState::ChargedFlare: CachedCombatStateLabel = TEXT("FLARE!"); break;
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
	case EDungeoneerNearbyInteract::SensoryAltar:     CachedInteractHint = TEXT("[F] SANCTUARY ALTAR: AWAKEN SENSE (90g)"); break;
	case EDungeoneerNearbyInteract::CollectPickup:    CachedInteractHint = TEXT("[F] COLLECT DROPPED LOOT"); break;
	case EDungeoneerNearbyInteract::ReviveAlly:       CachedInteractHint = TEXT("[F] REVIVE DOWNED ALLY (50% HP)"); break;
	case EDungeoneerNearbyInteract::AllyTrade:        CachedInteractHint = TEXT("[T] CO-OP P2P SUPPLY SWAP (2PC)  |  [F2] SPLIT-SCREEN"); break;
	default:                                          CachedInteractHint = TEXT("LMB: COMBO | R: FLARE | Q/C: SUPPLIES | SHIFT: DODGE | F2: 2P CO-OP"); break;
	}

	RefreshMinimapFromWorld(Character);
	UpdateTextBlocks();
}

void UDungeoneerPlayerHUDWidget::RefreshMinimapFromWorld(const ADungeoneerCharacter* Character) noexcept
{
	if (!Character)
	{
		return;
	}
	const FVector PlayerLoc = Character->GetActorLocation();
	MinimapPlayerXY = FVector2D(PlayerLoc.X, PlayerLoc.Y);

	const UWorld* World = Character->GetWorld();
	const UDungeonGeneratorSubsystem* GenSub = World ? World->GetSubsystem<UDungeonGeneratorSubsystem>() : nullptr;
	if (!GenSub)
	{
		MinimapRoomCount = 0;
		return;
	}

	const FDungeonFloorGraph& Graph = GenSub->GetActiveFloorGraph();
	if (Graph.FloorIndex != CachedMinimapFloor || MinimapRoomCount != FMath::Min(Graph.Nodes.Num(), MaxMinimapRooms))
	{
		CachedMinimapFloor = Graph.FloorIndex;
		DiscoveredRoomMask = 1u; // Entry room starts discovered
		MinimapRoomCount = FMath::Min(Graph.Nodes.Num(), MaxMinimapRooms);
		MinimapBoundsMin = FVector2D(-1000.0, -1000.0);
		MinimapBoundsMax = FVector2D(1000.0, 1000.0);

		for (int32 i = 0; i < MinimapRoomCount; ++i)
		{
			const FDungeonNode& Node = Graph.Nodes[i];
			FDungeoneerMinimapRoomSlot& RoomSlot = MinimapRooms[i];
			RoomSlot.CenterXY = FVector2D(Node.WorldCenter.X, Node.WorldCenter.Y);
			RoomSlot.ExtentsXY = FVector2D(Node.RoomExtents.X, Node.RoomExtents.Y);
			RoomSlot.RoomType = Node.RoomType;
			RoomSlot.bSecretRoom = Node.bIsSecretRoom;
			RoomSlot.OutgoingMask = 0;
			for (int32 OutId : Node.OutgoingNodeIds)
			{
				if (OutId >= 0 && OutId < MinimapRoomCount)
				{
					RoomSlot.OutgoingMask |= static_cast<uint16>(1u << OutId);
				}
			}

			MinimapBoundsMin.X = FMath::Min(MinimapBoundsMin.X, RoomSlot.CenterXY.X - RoomSlot.ExtentsXY.X);
			MinimapBoundsMin.Y = FMath::Min(MinimapBoundsMin.Y, RoomSlot.CenterXY.Y - RoomSlot.ExtentsXY.Y);
			MinimapBoundsMax.X = FMath::Max(MinimapBoundsMax.X, RoomSlot.CenterXY.X + RoomSlot.ExtentsXY.X);
			MinimapBoundsMax.Y = FMath::Max(MinimapBoundsMax.Y, RoomSlot.CenterXY.Y + RoomSlot.ExtentsXY.Y);
		}
	}

	for (int32 i = 0; i < MinimapRoomCount; ++i)
	{
		FDungeoneerMinimapRoomSlot& RoomSlot = MinimapRooms[i];
		const bool bInside =
			FMath::Abs(MinimapPlayerXY.X - RoomSlot.CenterXY.X) <= RoomSlot.ExtentsXY.X &&
			FMath::Abs(MinimapPlayerXY.Y - RoomSlot.CenterXY.Y) <= RoomSlot.ExtentsXY.Y;
		if (bInside)
		{
			DiscoveredRoomMask |= static_cast<uint32>(1u << i);
			// Reveal non-secret adjacent exits when entering a room
			for (int32 j = 0; j < MinimapRoomCount; ++j)
			{
				if ((RoomSlot.OutgoingMask & (1u << j)) != 0 && !MinimapRooms[j].bSecretRoom)
				{
					DiscoveredRoomMask |= static_cast<uint32>(1u << j);
				}
			}
		}
		RoomSlot.bActive = bInside;
		RoomSlot.bDiscovered = (DiscoveredRoomMask & static_cast<uint32>(1u << i)) != 0;
	}
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
	if (PB_Health)
	{
		PB_Health->SetPercent(HealthPercent);
	}
	if (PB_Battery)
	{
		PB_Battery->SetPercent(BatteryPercent);
	}
	if (BossBarContainer)
	{
		BossBarContainer->SetVisibility(bBossActive ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		if (bBossActive && PB_BossHealth)
		{
			PB_BossHealth->SetPercent(CachedBossHealthPercent);
		}
		if (bBossActive && Txt_BossTitle)
		{
			Txt_BossTitle->SetText(FText::FromString(FString::Printf(TEXT("CINDER COLOSSUS — VAULT GUARDIAN [PHASE %d]"), CachedBossPhase)));
		}
	}
	if (Txt_HealthReadout)
	{
		Txt_HealthReadout->SetText(FText::FromString(FString::Printf(TEXT("HEALTH  %.0f / %.0f"), CachedCurrentHealth, CachedMaxHealth)));
	}
	if (Txt_BatteryReadout)
	{
		Txt_BatteryReadout->SetText(FText::FromString(FString::Printf(TEXT("LANTERN OIL  %.0f%%"), BatteryPercent * 100.0f)));
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
		Txt_SensoryPerksReadout->SetText(FText::FromString(FString::Printf(TEXT("SENSES [DV:%d  AH:%d  ST:%d]"),
			DarkVisionTier, AcuteHearingTier, ScentThermalTier)));
	}
	if (Txt_CombatAndLoopReadout)
	{
		Txt_CombatAndLoopReadout->SetText(FText::FromString(GetFormattedCombatAndLoopText()));
	}
	if (Txt_RelicsAndSupplies)
	{
		FString RelicsLine = FString::Printf(TEXT("[Q] OIL x%d  |  [C] ANTIDOTE x%d"), CachedOilPhials, CachedAntidotes);
		for (uint8 r = 1; r <= 5; ++r)
		{
			if ((CachedRelicMask & static_cast<uint8>(1u << (r - 1u))) != 0)
			{
				RelicsLine += FString::Printf(TEXT("  • %s"), UDungeoneerInventoryComponent::GetRelicDisplayName(static_cast<EDungeoneerRelicId>(r)));
			}
		}
		Txt_RelicsAndSupplies->SetText(FText::FromString(RelicsLine));
	}
	if (Txt_InteractHint)
	{
		Txt_InteractHint->SetText(FText::FromString(CachedInteractHint));
	}
}
