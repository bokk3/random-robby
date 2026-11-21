#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Containers/StaticArray.h"
#include "Core/DungeoneerTypes.h"
#include "Dungeon/DungeonTypes.h"
#include "DungeoneerPlayerHUDWidget.generated.h"

class ADungeoneerCharacter;
class UBorder;
class UNativeWidgetHost;
class UProgressBar;
class UTextBlock;
class SDungeoneerMinimapWidget;
class SDungeoneerFloatingPopupWidget;

struct DARKDUNGEONEER_API FDungeoneerMinimapRoomSlot
{
	FVector2D CenterXY = FVector2D::ZeroVector;
	FVector2D ExtentsXY = FVector2D(600.0, 600.0);
	EDungeonRoomType RoomType = EDungeonRoomType::Combat;
	uint16 OutgoingMask = 0;
	bool bSecretRoom = false;
	bool bDiscovered = false;
	bool bActive = false;
};

/**
 * TASK-901 & TASK-1301: Per-Viewport Split-Screen Player HUD Widget.
 * Renders Gothic Vitals (HP & Lantern Oil bars), 8-compass Move/Aim telemetry,
 * Chrono-Leech ratio, Sensory Perk tiers, Boss Guardian HP bar, fixed-array Floor Minimap,
 * Relic & Consumable Dock, and 32-slot pooled Floating Combat Text (zero Tick allocations).
 */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API UDungeoneerPlayerHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UDungeoneerPlayerHUDWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual TSharedRef<SWidget> RebuildWidget() override;

protected:
	void BuildCompanionCardLayout(class UCanvasPanel* RootCanvas);

public:

	/** Pushes live character state into HUD readout properties and text blocks (zero Tick allocations). */
	UFUNCTION(BlueprintCallable, Category = "UI|HUD")
	void RefreshHUDFromCharacter(const ADungeoneerCharacter* Character) noexcept;

	/** Resolves owning ULocalPlayer pawn and updates HUD metrics without cross-viewport leakage. */
	UFUNCTION(BlueprintCallable, Category = "UI|HUD")
	void RefreshHUDFromOwningLocalPlayer() noexcept;

	/**
	 * Turns this widget into a compact vitals card for another local player (top-right, no minimap,
	 * boss bar or popup layer), drawn on the owning player's screen. Call before adding it to the screen.
	 */
	void ConfigureAsCompanionCard(int32 InObservedLocalPlayerIndex) noexcept
	{
		bCompanionCard = true;
		ObservedLocalPlayerIndex = InObservedLocalPlayerIndex;
	}

	bool IsCompanionCard() const noexcept { return bCompanionCard; }

	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	float GetHealthPercent() const noexcept { return HealthPercent; }

	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	float GetBatteryPercent() const noexcept { return BatteryPercent; }

	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	float GetChronoSlowRatio() const noexcept { return ChronoSlowRatio; }

	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	ECompassOctant GetMoveOctant() const noexcept { return MoveOctant; }

	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	ECompassOctant GetAimOctant() const noexcept { return AimOctant; }

	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	FString GetFormattedVitalsText() const;

	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	FString GetFormattedOctantText() const;

	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	FString GetFormattedChronoSlowText() const;

	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	FString GetFormattedCombatAndLoopText() const;

	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	FString GetFormattedInteractHintText() const;

	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	int32 GetCachedFloorIndex() const noexcept { return CachedFloorIndex; }

	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	int32 GetCachedGoldBalance() const noexcept { return CachedGoldBalance; }

	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	int32 GetCachedKillsCount() const noexcept { return CachedKillsCount; }

	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	FString GetCachedCombatStateLabel() const { return CachedCombatStateLabel; }

	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	FString GetCachedSkillsLine() const { return CachedSkillsLine; }

	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	int32 GetMinimapRoomCount() const noexcept { return MinimapRoomCount; }

	UFUNCTION(BlueprintPure, Category = "UI|HUD")
	bool IsBossBarVisible() const noexcept { return bBossActive; }

	static constexpr int32 MaxMinimapRooms = 16;

	const FDungeoneerMinimapRoomSlot& GetMinimapRoomAt(int32 Index) const noexcept { return MinimapRooms[Index]; }
	FVector2D GetMinimapPlayerXY() const noexcept { return MinimapPlayerXY; }
	FVector2D GetMinimapBoundsMin() const noexcept { return MinimapBoundsMin; }
	FVector2D GetMinimapBoundsMax() const noexcept { return MinimapBoundsMax; }
	const ADungeoneerCharacter* GetObservedCharacter() const noexcept { return ObservedCharacter.Get(); }

protected:
	void UpdateTextBlocks() noexcept;
	void RefreshMinimapFromWorld(const ADungeoneerCharacter* Character) noexcept;

	UPROPERTY(BlueprintReadOnly, Category = "UI|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> PB_Health;

	UPROPERTY(BlueprintReadOnly, Category = "UI|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> PB_Battery;

	UPROPERTY(BlueprintReadOnly, Category = "UI|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> PB_BossHealth;

	UPROPERTY(BlueprintReadOnly, Category = "UI|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UBorder> BossBarContainer;

	/** Full-screen red wash, alpha driven by the watched hero's hurt flash. */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> HurtFlashOverlay;

	UPROPERTY(BlueprintReadOnly, Category = "UI|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_BossTitle;

	UPROPERTY(BlueprintReadOnly, Category = "UI|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_HealthReadout;

	UPROPERTY(BlueprintReadOnly, Category = "UI|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_BatteryReadout;

	UPROPERTY(BlueprintReadOnly, Category = "UI|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_OctantIndicator;

	UPROPERTY(BlueprintReadOnly, Category = "UI|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_ChronoSlowReadout;

	UPROPERTY(BlueprintReadOnly, Category = "UI|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_SensoryPerksReadout;

	UPROPERTY(BlueprintReadOnly, Category = "UI|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_CombatAndLoopReadout;

	UPROPERTY(BlueprintReadOnly, Category = "UI|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_RelicsAndSupplies;

	UPROPERTY(BlueprintReadOnly, Category = "UI|HUD", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_InteractHint;

	UPROPERTY(Transient)
	TObjectPtr<UNativeWidgetHost> MinimapSlateHost;

	UPROPERTY(Transient)
	TObjectPtr<UNativeWidgetHost> FloatingPopupSlateHost;

private:
	TSharedPtr<SDungeoneerMinimapWidget> MinimapSlateWidget;
	TSharedPtr<SDungeoneerFloatingPopupWidget> FloatingPopupSlateWidget;
	TWeakObjectPtr<const ADungeoneerCharacter> ObservedCharacter;
	int32 ObservedLocalPlayerIndex = -1;
	float CachedHurtFlash = 0.0f;
	bool bCompanionCard = false;

	TStaticArray<FDungeoneerMinimapRoomSlot, MaxMinimapRooms> MinimapRooms;
	int32 MinimapRoomCount = 0;
	int32 CachedMinimapFloor = -1;
	uint32 DiscoveredRoomMask = 0u;
	FVector2D MinimapPlayerXY = FVector2D::ZeroVector;
	FVector2D MinimapBoundsMin = FVector2D(-2000.0, -2000.0);
	FVector2D MinimapBoundsMax = FVector2D(2000.0, 2000.0);

	float CachedCurrentHealth = 100.0f;
	float CachedMaxHealth = 100.0f;
	float HealthPercent = 1.0f;
	float BatteryPercent = 1.0f;
	float ChronoSlowRatio = 0.0f;
	float CachedBossHealthPercent = 0.0f;
	int32 CachedBossPhase = 1;
	bool bBossActive = false;
	ECompassOctant MoveOctant = ECompassOctant::S;
	ECompassOctant AimOctant = ECompassOctant::S;
	int32 DarkVisionTier = 0;
	int32 AcuteHearingTier = 0;
	int32 ScentThermalTier = 0;
	int32 CachedFloorIndex = 1;
	int32 CachedGoldBalance = 250;
	int32 CachedKillsCount = 0;
	int32 CachedComboStep = 0;
	int32 CachedOilPhials = 1;
	int32 CachedAntidotes = 1;
	uint8 CachedRelicMask = 0;
	FString CachedCombatStateLabel = TEXT("READY");
	FString CachedInteractHint;
	FString CachedSkillsLine;
};
