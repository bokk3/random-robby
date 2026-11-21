#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DungeoneerPartyCamera.generated.h"

class ADungeoneerCharacter;
class UCameraComponent;
class USpringArmComponent;
class UZombieSpatialSubsystem;

/** Rules for keeping local co-op heroes on one screen. Pure functions so tests can check them directly. */
namespace DungeoneerParty
{
	/** Furthest two local heroes may be apart (2D). Roughly what fits on screen at full zoom-out. */
	inline constexpr float LeashRadiusUU = 820.0f;

	/** Past this multiple of the leash (teleport, big knockback) the straggler is snapped back, not slid. */
	inline constexpr float LeashSnapFactor = 1.3f;

	/** Camera arm length when solo or when the heroes stand close, and at full spread. */
	inline constexpr float ArmLengthNearUU = 1320.0f;
	inline constexpr float ArmLengthFarUU = 1750.0f;

	/** Spread (UU) at which the camera starts zooming out. */
	inline constexpr float ZoomStartSpreadUU = 300.0f;

	/** Where Mover may stand: unchanged inside the leash, otherwise pulled onto the leash circle around Anchor. Keeps Mover's Z. */
	DARKDUNGEONEER_API FVector ClampToLeash(const FVector& Mover, const FVector& Anchor, float RadiusUU = LeashRadiusUU) noexcept;

	/** Camera arm length for a given 2D spread between heroes. */
	DARKDUNGEONEER_API float ArmLengthForSpread(float SpreadUU) noexcept;

	/** True if this hero belongs to the local party: controlled on this machine, or an uncontrolled test pawn. */
	DARKDUNGEONEER_API bool IsLocalPartyHero(const ADungeoneerCharacter* Hero) noexcept;
}

/**
 * The one camera every local player looks through. It frames all local heroes, zooms out as they
 * spread, keeps them within DungeoneerParty::LeashRadiusUU of each other, and carries the combat
 * camera kicks the heroes produce. Same angle as the old per-hero camera (pitch -42, yaw -45), which
 * input mapping and sprite-row selection depend on.
 */
UCLASS()
class DARKDUNGEONEER_API ADungeoneerPartyCamera : public AActor
{
	GENERATED_BODY()

public:
	ADungeoneerPartyCamera();

	virtual void Tick(float DeltaSeconds) override;

	/** Jumps straight to the heroes' framing (after floor changes and teleports) instead of easing there. */
	void SnapToParty();

	/** Runs one framing + leash step. Tick calls this; tests call it directly. */
	void UpdatePartyFraming(float DeltaSeconds);

	FVector GetFocusLocation() const noexcept { return FocusLocation; }
	float GetCurrentArmLength() const noexcept { return CurrentArmLength; }
	int32 GetFramedHeroCount() const noexcept { return FramedHeroCount; }
	UCameraComponent* GetCameraComponent() const noexcept { return Camera.Get(); }

	/** Turn the leash off (e.g. during floor transitions). On by default. */
	void SetLeashEnabled(bool bEnabled) noexcept { bLeashEnabled = bEnabled; }

	static constexpr int32 MaxPartyHeroes = 4;

	/** Screen-right in world space for the yaw -45 camera: offsets along it sit side by side on screen. */
	static FVector ScreenRightXY() noexcept { return FVector(0.70710678, 0.70710678, 0.0); }

protected:
	/** Pulls heroes back inside the leash. Authority only; positions replicate from there. */
	void EnforceLeash(ADungeoneerCharacter* const* Heroes, int32 Count) const;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USceneComponent> FocusRoot;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraArm;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

private:
	FVector FocusLocation = FVector::ZeroVector;
	float CurrentArmLength = DungeoneerParty::ArmLengthNearUU;
	int32 FramedHeroCount = 0;
	bool bHasFocus = false;
	bool bSnapNextUpdate = true;
	bool bLeashEnabled = true;
};
