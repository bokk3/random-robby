#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/DungeoneerTypes.h"
#include "Core/DungeoneerGameState.h"
#include "DungeoneerMenuFooterWidget.generated.h"

class UTextBlock;

/**
 * TASK-804: Bottom Seed Telemetry & Prompt Bar Widget.
 * Binds ADungeoneerGameState::OnFloorSeedChanged for replicated seed hex readouts,
 * triggers deterministic [R] seed rerolls, and displays octant & Chrono-Leech conservation telemetry.
 */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API UDungeoneerMenuFooterWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UDungeoneerMenuFooterWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Subscribes to ADungeoneerGameState::OnFloorSeedChanged and synchronizes initial seed state. */
	UFUNCTION(BlueprintCallable, Category = "UI|Telemetry")
	void BindToGameState(ADungeoneerGameState* InGameState);

	/** Executes [R] deterministic floor seed reroll (server-authoritative when bound, or local preview). */
	UFUNCTION(BlueprintCallable, Category = "UI|Telemetry")
	void RequestDeterministicSeedReroll();

	/** Updates displayed compass octant speed & Chrono-Leech conservation ratio telemetry. */
	UFUNCTION(BlueprintCallable, Category = "UI|Telemetry")
	void UpdateOctantTelemetry(ECompassOctant Octant, float PlayerBaseSpeedUU = 450.0f, float PlayerSlowRatio = 0.0f) noexcept;

	UFUNCTION(BlueprintPure, Category = "UI|Telemetry")
	FString GetFormattedSeedHex() const;

	UFUNCTION(BlueprintPure, Category = "UI|Telemetry")
	FString GetOctantTelemetryText() const;

	UFUNCTION(BlueprintPure, Category = "UI|Telemetry")
	FString GetChronoConservationText() const;

	UFUNCTION(BlueprintPure, Category = "UI|Telemetry")
	const FDungeonFloorSeed& GetCachedFloorSeed() const noexcept { return CachedFloorSeed; }

	UFUNCTION(BlueprintPure, Category = "UI|Telemetry")
	int32 GetLastPreviewNodeCount() const noexcept { return LastPreviewNodeCount; }

protected:
	void HandleFloorSeedUpdated(const FDungeonFloorSeed& NewSeed);
	void RefreshTelemetryTextBlocks() noexcept;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Telemetry", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_FloorSeedHex;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Telemetry", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_OctantTelemetry;

	UPROPERTY(BlueprintReadOnly, Category = "UI|Telemetry", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_ChronoConservation;

private:
	TWeakObjectPtr<ADungeoneerGameState> BoundGameState;
	FDelegateHandle SeedChangedDelegateHandle;

	FDungeonFloorSeed CachedFloorSeed;
	ECompassOctant CurrentOctant = ECompassOctant::E;
	float CachedPlayerBaseSpeedUU = 450.0f;
	float CachedPlayerSlowRatio = 0.0f;
	int32 LastPreviewNodeCount = 0;
};
