#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MirrorMazeActors.generated.h"

class UStaticMeshComponent;
class USpotLightComponent;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class AZombieBase;

namespace MirrorStencilIds
{
	inline constexpr int32 SolidMirrorStencil = 101;
	inline constexpr int32 PhantasmMirrorStencil = 104;
	inline constexpr int32 DecoyZombieStencil = 106;
}

/**
 * True Physical Mirror Wall (`AMirrorWall_Solid`).
 * - CustomDepthStencil = 101.
 * - Blocks pawn movement cleanly.
 * - Reflects incoming player flashlight beams via bounce raycast into a secondary SpotLightComponent.
 */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API AMirrorWall_Solid : public AActor
{
	GENERATED_BODY()

public:
	AMirrorWall_Solid();

	virtual void BeginPlay() override;

	/** Evaluates whether a player's flashlight hits this mirror plane and updates the bounce beam. */
	void EvaluateFlashlightBounce(const FVector& BeamOrigin, const FVector& BeamDirection, float BeamRange) noexcept;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mirror")
	TObjectPtr<UStaticMeshComponent> MirrorPlaneMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mirror|Lighting")
	TObjectPtr<USpotLightComponent> ReflectedBounceLight;
};

/**
 * Illusory Pass-Through Mirror Wall (`AMirrorWall_Phantasm`).
 * - CustomDepthStencil = 104.
 * - Renders a fake corridor reflection via half-resolution frustum-culled USceneCaptureComponent2D.
 * - Permits players to step cardinally or diagonally right through it.
 * - Diverges subtly in normal-map specular roughness (CustomPrimitiveData[0]) and returns a
 *   phase-inverted acoustic ping (bIsIllusionPhaseInverted = true) so upgraded senses spot the trick.
 */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API AMirrorWall_Phantasm : public AActor
{
	GENERATED_BODY()

public:
	AMirrorWall_Phantasm();

	virtual void BeginPlay() override;

	/** Returns true so Acute Hearing renders an inverted-phase ripple upon acoustic ping reflection. */
	UFUNCTION(BlueprintPure, Category = "Mirror|Illusion")
	bool ShouldEmitInvertedAcousticPing() const noexcept { return true; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mirror|Illusion")
	TObjectPtr<UStaticMeshComponent> IllusionMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mirror|Illusion")
	TObjectPtr<USceneCaptureComponent2D> PortalReflectionCapture;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mirror|Illusion")
	float SubtleRoughnessDivergence = 0.22f;
};

/**
 * Mirror Decoy Spawner Node (`AMirrorDecoyNode`).
 * - CustomDepthStencil = 106.
 * - Projects reflected zombie clones that deal zero damage, cast no physical shadow,
 *   and emit zero acoustic footstep signatures.
 */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API AMirrorDecoyNode : public AActor
{
	GENERATED_BODY()

public:
	AMirrorDecoyNode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void BindTrueZombieSource(AZombieBase* InTrueZombie) noexcept { TrueSourceZombie = InTrueZombie; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mirror|Decoy")
	TObjectPtr<UStaticMeshComponent> DecoyVisualPlane;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mirror|Decoy")
	FVector MirrorPlaneNormal = FVector(1.0f, 0.0f, 0.0f);

private:
	TWeakObjectPtr<AZombieBase> TrueSourceZombie;
};
