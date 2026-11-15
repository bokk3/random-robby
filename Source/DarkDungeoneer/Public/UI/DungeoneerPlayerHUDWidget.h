#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/DungeoneerTypes.h"
#include "DungeoneerPlayerHUDWidget.generated.h"

class ADungeoneerCharacter;
class UTextBlock;

/**
 * TASK-901: Per-Viewport Split-Screen Player HUD Widget.
 * Displays owning ULocalPlayer vitals (Health, Flashlight Battery), 8-compass movement/aim
 * octants, active Chrono-Leech slow ratio S in [0.0, MaxSlowRatio], and sensory perk tiers
 * isolated cleanly per split-screen pane.
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

	/** Pushes live character state into HUD readout properties and text blocks (zero Tick allocations). */
	UFUNCTION(BlueprintCallable, Category = "UI|HUD")
	void RefreshHUDFromCharacter(const ADungeoneerCharacter* Character) noexcept;

	/** Resolves owning ULocalPlayer pawn and updates HUD metrics without cross-viewport leakage. */
	UFUNCTION(BlueprintCallable, Category = "UI|HUD")
	void RefreshHUDFromOwningLocalPlayer() noexcept;

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

protected:
	void UpdateTextBlocks() noexcept;

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

private:
	float CachedCurrentHealth = 100.0f;
	float CachedMaxHealth = 100.0f;
	float HealthPercent = 1.0f;
	float BatteryPercent = 1.0f;
	float ChronoSlowRatio = 0.0f;
	ECompassOctant MoveOctant = ECompassOctant::S;
	ECompassOctant AimOctant = ECompassOctant::S;
	int32 DarkVisionTier = 0;
	int32 AcuteHearingTier = 0;
	int32 ScentThermalTier = 0;
};
