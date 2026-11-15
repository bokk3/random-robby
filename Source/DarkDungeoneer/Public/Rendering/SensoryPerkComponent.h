#pragma once

/**
 * SensoryPerkComponent.h
 * 
 * Component attached to ADungeoneerCharacter managing upgradeable sensory perks:
 * - Dark Vision (level 0..3): Spherical ambient rim light + volumetric fog thinning
 * - Acute Hearing (level 0..3): High-contrast acoustic ripple ring overlays
 * - Scent / Thermal (level 0..3): Biological trail highlights
 * 
 * Queries ULocalPlayer strictly via GetOwningLocalPlayer() (zero GetFirstPlayerController()
 * assumptions) and pushes isolated per-viewport FDungeoneerSensoryViewState every tick
 * with zero heap allocations.
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/DungeoneerTypes.h"
#include "SensoryPerkComponent.generated.h"

class ADungeoneerCharacter;
class UDungeoneerLocalPlayerSubsystem;
class UDungeoneerSensoryWorldSubsystem;

UCLASS(ClassGroup = (Sensory), meta = (BlueprintSpawnableComponent))
class DARKDUNGEONEER_API USensoryPerkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USensoryPerkComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Returns the level (0..3) of the specified sensory perk. */
	UFUNCTION(BlueprintPure, Category = "Sensory")
	int32 GetPerkLevel(EDungeoneerSensoryPerk Perk) const noexcept;

	/** Sets the perk level, clamped to [0, 3]. */
	UFUNCTION(BlueprintCallable, Category = "Sensory")
	void SetPerkLevel(EDungeoneerSensoryPerk Perk, int32 NewLevel) noexcept;

	/** Increments the specified perk by 1 up to maximum level 3. */
	UFUNCTION(BlueprintCallable, Category = "Sensory")
	void UpgradePerk(EDungeoneerSensoryPerk Perk) noexcept;

	UFUNCTION(BlueprintPure, Category = "Sensory")
	float GetDarkVisionRadius() const noexcept;

	UFUNCTION(BlueprintPure, Category = "Sensory")
	float GetDarkVisionFogExtinctionScale() const noexcept;

	UFUNCTION(BlueprintPure, Category = "Sensory")
	float GetAcuteHearingIntensity() const noexcept;

	UFUNCTION(BlueprintPure, Category = "Sensory")
	float GetAcuteHearingRangeMultiplier() const noexcept;

	UFUNCTION(BlueprintPure, Category = "Sensory")
	float GetScentThermalIntensity() const noexcept;

protected:
	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = "Sensory", meta = (ClampMin = "0", ClampMax = "3"))
	int32 DarkVisionLevel = 0;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = "Sensory", meta = (ClampMin = "0", ClampMax = "3"))
	int32 AcuteHearingLevel = 0;

	UPROPERTY(Replicated, EditDefaultsOnly, BlueprintReadOnly, Category = "Sensory", meta = (ClampMin = "0", ClampMax = "3"))
	int32 ScentThermalLevel = 0;

private:
	TWeakObjectPtr<ADungeoneerCharacter> CachedCharacter;
	TWeakObjectPtr<UDungeoneerLocalPlayerSubsystem> CachedLocalPlayerSubsystem;
};
