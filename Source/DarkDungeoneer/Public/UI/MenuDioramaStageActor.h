#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/DungeoneerTypes.h"
#include "UI/DungeoneerMainMenuWidget.h"
#include "MenuDioramaStageActor.generated.h"

class UCameraComponent;
class USpotLightComponent;
class UStaticMeshComponent;

/**
 * TASK-802: Live 2.5D Main Menu Diorama Stage Actor.
 * Renders the -60 deg pitched dungeon showcase with Hero narrow flashlight sweep across
 * compass octants, Exploder fuse pulse, Chrono-Gas haze, Phantasm Mirror (Stencil 104),
 * Decoy reflection (Stencil 106), and deterministic phase-driven sonar rings (Stencil 105).
 * Strictly zero GetAllActorsOfClass, FTimerHandle, or Tick heap allocations.
 */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API AMenuDioramaStageActor : public AActor
{
	GENERATED_BODY()

public:
	AMenuDioramaStageActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	/** Steers the diorama showcase flashlight and spotlight highlight based on hovered menu item. */
	UFUNCTION(BlueprintCallable, Category = "UI|Diorama")
	void SetShowcaseModePreview(EDungeoneerMenuAction PreviewAction) noexcept;

	/** Sets the showcase flashlight target compass octant for smooth angular sweep. */
	UFUNCTION(BlueprintCallable, Category = "UI|Diorama")
	void SetTargetShowcaseOctant(ECompassOctant Octant) noexcept;

	UFUNCTION(BlueprintPure, Category = "UI|Diorama")
	float GetCurrentFlashlightYawDeg() const noexcept { return CurrentFlashlightYawDeg; }

	UFUNCTION(BlueprintPure, Category = "UI|Diorama")
	float GetExploderFusePulseAlpha() const noexcept { return ExploderFusePulseAlpha; }

	UFUNCTION(BlueprintPure, Category = "UI|Diorama")
	float GetChronoGasHazeAlpha() const noexcept { return ChronoGasHazeAlpha; }

	UFUNCTION(BlueprintPure, Category = "UI|Diorama")
	int32 GetEmittedSonarPulseCount() const noexcept { return EmittedSonarPulseCount; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diorama")
	TObjectPtr<USceneComponent> StageRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diorama|Camera")
	TObjectPtr<UCameraComponent> DioramaCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diorama|Lighting")
	TObjectPtr<USpotLightComponent> HeroFlashlightSpot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diorama|Meshes")
	TObjectPtr<UStaticMeshComponent> ShowcaseFloorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diorama|Meshes")
	TObjectPtr<UStaticMeshComponent> ExploderShowcaseProxy;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diorama|Meshes")
	TObjectPtr<UStaticMeshComponent> FastThrowerShowcaseProxy;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diorama|Meshes")
	TObjectPtr<UStaticMeshComponent> PhantasmMirrorShowcaseProxy;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Diorama|Meshes")
	TObjectPtr<UStaticMeshComponent> DecoyProjectionProxy;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Diorama|Config", meta = (ClampMin = "0.5"))
	float SonarPulsePeriodSeconds = 2.4f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Diorama|Config", meta = (ClampMin = "30.0"))
	float FlashlightSweepInterpSpeedDeg = 120.0f;

private:
	ECompassOctant TargetOctant = ECompassOctant::E;
	float CurrentFlashlightYawDeg = 0.0f;
	float DeterministicSweepAccumulator = 0.0f;
	float SonarPhaseAccumulator = 0.0f;
	float ExploderFusePulseAlpha = 0.0f;
	float ChronoGasHazeAlpha = 0.75f;
	int32 EmittedSonarPulseCount = 0;
	bool bAutoSweepOctants = true;
};
