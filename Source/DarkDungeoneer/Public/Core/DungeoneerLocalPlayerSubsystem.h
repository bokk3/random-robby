#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "SceneViewExtension.h"
#include "Core/DungeoneerTypes.h"
#include "DungeoneerLocalPlayerSubsystem.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UDungeoneerLocalPlayerSubsystem;

/**
 * Per-ULocalPlayer SceneViewExtension isolating sensory post-process parameters
 * (Dark Vision, Acute Hearing ripples, Scent/Thermal overlays) to the owning split-screen viewport.
 * Ensures Player 1's sensory buffs never bleed into Player 2's viewport.
 */
class DARKDUNGEONEER_API FDungeoneerSensoryViewExtension : public FSceneViewExtensionBase
{
public:
	FDungeoneerSensoryViewExtension(const FAutoRegister& AutoRegister, ULocalPlayer* InLocalPlayer, UDungeoneerLocalPlayerSubsystem* InOwnerSubsystem);

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override;
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

private:
	TWeakObjectPtr<ULocalPlayer> LocalPlayerPtr;
	TWeakObjectPtr<UDungeoneerLocalPlayerSubsystem> OwnerSubsystem;
};

/**
 * LocalPlayerSubsystem holding per-viewport sensory state and MID_SensoryOverlay.
 * All player-facing systems query ULocalPlayer rather than GetFirstPlayerController().
 */
UCLASS()
class DARKDUNGEONEER_API UDungeoneerLocalPlayerSubsystem : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Assigns the base overlay post-process material for this specific local player's viewport. */
	UFUNCTION(BlueprintCallable, Category = "Sensory")
	void SetSensoryOverlayBaseMaterial(UMaterialInterface* BaseMaterial);

	/** Updates this viewport's isolated sensory state and pushes scalars to MID_SensoryOverlay. */
	void UpdateViewportSensoryState(const FDungeoneerSensoryViewState& NewState) noexcept;

	const FDungeoneerSensoryViewState& GetViewportSensoryState() const noexcept { return CurrentViewState; }
	UMaterialInstanceDynamic* GetViewportSensoryMID() const noexcept { return SensoryOverlayMID.Get(); }

private:
	TSharedPtr<FDungeoneerSensoryViewExtension, ESPMode::ThreadSafe> ViewExtension;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> SensoryOverlayMID;

	FDungeoneerSensoryViewState CurrentViewState;
};
