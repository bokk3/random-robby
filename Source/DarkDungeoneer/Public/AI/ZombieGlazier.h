#pragma once

#include "CoreMinimal.h"
#include "AI/ZombieBase.h"
#include "ZombieGlazier.generated.h"

/**
 * The Glazier: a dead Mirror Palace glass-setter behind an iron-framed mirror pane.
 *
 * The pane blocks melee hits from the front. A hit with real weight breaks the guard (combo finisher, heavy
 * smash, dash strike: anything with at least GuardBreakStunSec of stun), and so does hitting it from behind
 * (Their Own Shadow puts the hero there). Its shield bash winds up slowly, can't be interrupted by light hits,
 * and leaves a long recovery with the guard down.
 */
UCLASS()
class DARKDUNGEONEER_API AZombieGlazier : public AZombieBase
{
	GENERATED_BODY()

public:
	AZombieGlazier();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual bool TryBlockMeleeHit(const FVector& AttackerLocation, float StunSeconds) noexcept override;

	/** Turns the shield toward a point (the hero it is fighting). */
	void FaceLocation(const FVector& Target) noexcept;

	bool IsGuardBroken() const noexcept { return GuardBrokenRemaining > 0.0f; }

	static constexpr float GuardBreakStunSec = 0.25f;
	static constexpr float GuardBrokenSec = 2.5f;
	static constexpr float FrontMinCos = 0.3f;

protected:
	virtual const TCHAR* GetArchetypeKeyPrefix() const noexcept override { return TEXT("GlazierWalk"); }
	virtual FName GetArchetypeDiffuseKey() const noexcept override { return FName(TEXT("GlazierWalk_R0_C0_D")); }
	virtual FName GetArchetypeNormalKey() const noexcept override { return FName(TEXT("GlazierWalk_R0_C0_N")); }
	virtual FLinearColor GetArchetypeVisualTint() const noexcept override { return FLinearColor(0.7f, 0.85f, 1.0f); }
	virtual FVector GetArchetypeVisualScale() const noexcept override { return FVector(1.6, 1.6, 1.6); }
	virtual const TCHAR* GetAttackSheetPrefix() const noexcept override { return TEXT("GlazierAttack"); }
	virtual bool UsesDeathFrame() const noexcept override { return true; }
	virtual bool ResolveAttackFrame(int32 FacingRow, int32& OutRow, int32& OutCol) const noexcept override;

private:
	float GuardBrokenRemaining = 0.0f;
	float BlockPoseRemaining = 0.0f;
};
