#pragma once

#include "CoreMinimal.h"
#include "AI/ZombieBase.h"
#include "ZombieMirrorStalker.generated.h"

/**
 * Obsidian Mirror-Palace Stealth Archetype (`AZombieMirrorStalker`).
 * - Cloaked in dark-glass shroud until pierced by any player's flashlight cone,
 *   mirror-bounced beam, or Dark Vision Tier >= 2.
 * - Phases cleanly across illusory Phantasm mirror thresholds and glides at 310 UU/s.
 */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API AZombieMirrorStalker : public AZombieBase
{
	GENERATED_BODY()

public:
	AZombieMirrorStalker();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "MirrorStalker")
	bool IsRevealedByLight() const noexcept { return bRevealedByLight; }

	UFUNCTION(BlueprintPure, Category = "MirrorStalker")
	float GetRevealAlpha() const noexcept { return RevealAlpha; }

protected:
	virtual const TCHAR* GetArchetypeKeyPrefix() const noexcept override { return TEXT("MirrorStalker"); }
	virtual FName GetArchetypeDiffuseKey() const noexcept override { return FName(TEXT("MirrorStalker_Row0_D")); }
	virtual FName GetArchetypeNormalKey() const noexcept override { return FName(TEXT("MirrorStalker_Row0_N")); }
	virtual FLinearColor GetArchetypeVisualTint() const noexcept override { return FLinearColor(0.24f, 0.85f, 1.0f); }
	virtual FVector GetArchetypeVisualScale() const noexcept override { return FVector(1.58, 1.58, 1.58); }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorStalker|Config", meta = (ClampMin = "100.0"))
	float DarkVisionRevealRadiusUU = 540.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MirrorStalker|Config", meta = (ClampMin = "0.5"))
	float RevealFadeOutSeconds = 1.6f;

private:
	float RevealHoldRemaining = 0.0f;
	float RevealAlpha = 0.18f;
	bool bRevealedByLight = false;
};
