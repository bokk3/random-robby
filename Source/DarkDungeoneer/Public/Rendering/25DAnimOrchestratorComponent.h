#pragma once

/**
 * 25DAnimOrchestratorComponent.h
 *
 * Zero-allocation 2.5D Hybrid Animation Orchestrator for UE 5.8 C++20.
 * - Distance-driven 4-frame walk phase (Col 0 Idle/Contact A -> Col 1 Left Step -> Col 2 Passing B -> Col 3 Right Step).
 * - Velocity.IsNearlyZero() locks UV selection to Column 0 (Contact / Idle Pose A) of the active facing octant.
 * - Deterministic OnFootstepContact(EFoot::Left / EFoot::Right) and OnStepContact triggers on frames 1 and 3 with
 *   PhysicalMaterial surface resolution and Acute Hearing sensory ripple forwarding.
 * - 120Hz procedural sub-frame springs (stride hop, directional velocity lean, belly-swell).
 * - Per-instance Custom Primitive Data slots [0..4] (zero per-actor MID allocations) and
 *   invisible 3D volumetric shadow proxy capsule configuration.
 */

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Containers/StaticArray.h"
#include "Core/DungeoneerTypes.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "25DAnimOrchestratorComponent.generated.h"

class UPrimitiveComponent;

UENUM(BlueprintType)
enum class EFoot : uint8
{
	Left  = 0 UMETA(DisplayName = "Left Foot"),
	Right = 1 UMETA(DisplayName = "Right Foot")
};

namespace E25DAnimCPD
{
	inline constexpr int32 UVCellIndex       = 0; // CustomData[0]: Cell index 0..15 (Row * 4 + Col)
	inline constexpr int32 EmissiveFusePulse = 1; // CustomData[1]: Emissive fuse / lantern intensity
	inline constexpr int32 FlaskElementR     = 2; // CustomData[2]: Alchemical flask element tint R
	inline constexpr int32 FlaskElementG     = 3; // CustomData[3]: Alchemical flask element tint G
	inline constexpr int32 FlaskElementB     = 4; // CustomData[4]: Alchemical flask element tint B
	inline constexpr int32 Count             = 5;
}

USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API F25DStepContactEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "2.5D Animation")
	EFoot Foot = EFoot::Left;

	UPROPERTY(BlueprintReadOnly, Category = "2.5D Animation")
	int32 FrameIndex = 1;

	UPROPERTY(BlueprintReadOnly, Category = "2.5D Animation")
	int32 CellIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "2.5D Animation")
	TEnumAsByte<EPhysicalSurface> SurfaceType = SurfaceType_Default;

	UPROPERTY(BlueprintReadOnly, Category = "2.5D Animation")
	TWeakObjectPtr<UPhysicalMaterial> PhysicalMaterial;

	UPROPERTY(BlueprintReadOnly, Category = "2.5D Animation")
	float AudioLoudness = 1.0f;

	UPROPERTY(BlueprintReadOnly, Category = "2.5D Animation")
	FVector WorldLocation = FVector::ZeroVector;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOn25DStepContactSignature, const F25DStepContactEvent&);
DECLARE_MULTICAST_DELEGATE_OneParam(FOn25DFootstepContactSignature, EFoot);

/**
 * Drives 4x4 sprite-sheet cutouts with distance-locked walk cycles, 120Hz springs,
 * deterministic step contacts, and Custom Primitive Data [0..4].
 */
UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent))
class DARKDUNGEONEER_API U25DAnimOrchestratorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	U25DAnimOrchestratorComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Binds the rendered 2.5D cutout plane and configures the invisible volumetric shadow capsule. */
	UFUNCTION(BlueprintCallable, Category = "2.5D Animation")
	void BindRigPrimitives(UPrimitiveComponent* InCutoutPlane, UPrimitiveComponent* InShadowProxyCapsule) noexcept;

	/** Sets the directional sprite-sheet row explicitly (0..GridRows-1). */
	UFUNCTION(BlueprintCallable, Category = "2.5D Animation")
	void SetDirectionalRow(int32 NewRowIndex) noexcept;

	/** Maps a 4-axis/8-compass octant onto the active 4x4 sprite sheet row. */
	UFUNCTION(BlueprintCallable, Category = "2.5D Animation")
	void SetDirectionFromOctant(ECompassOctant Octant, bool bUseDiagonalSheetOrder = false) noexcept;

	/**
	 * Advances the walk phase strictly by XY distance traveled (zero foot-sliding under
	 * Chrono-Leech speed shifts), locks to Column 0 when idle, triggers OnFootstepContact
	 * and OnStepContact on frames 1 (Left) and 3 (Right), evaluates 120Hz springs, and pushes CPD.
	 */
	UFUNCTION(BlueprintCallable, Category = "2.5D Animation")
	void AdvanceWalkDistance(float DeltaDistanceUU, const FVector& CurrentVelocityXY, float DeltaTime);

	/** Updates pre-explosion belly-swell progress [0..1] and CustomData[1] Emissive Fuse Pulse. */
	UFUNCTION(BlueprintCallable, Category = "2.5D Animation")
	void SetPreExplosionFuseState(float InFuseProgress01, float InEmissivePulseIntensity) noexcept;

	/** Sets CustomData[2..4] RGB alchemical flask tint directly without MIDs. */
	UFUNCTION(BlueprintCallable, Category = "2.5D Animation")
	void SetFlaskElementTint(const FLinearColor& InElementRGB) noexcept;

	/** Applies preset element tint: 0 = Fire Orange, 1 = Poison Green, 2 = Chrono Violet-Blue. */
	UFUNCTION(BlueprintCallable, Category = "2.5D Animation")
	void SetFlaskElementByType(uint8 ElementPresetIndex) noexcept;

	/** Resets walk phase accumulation cleanly (e.g. when returning to actor pool). */
	UFUNCTION(BlueprintCallable, Category = "2.5D Animation")
	void ResetOrchestratorState() noexcept;

	UFUNCTION(BlueprintPure, Category = "2.5D Animation")
	int32 GetDirectionalRow() const noexcept { return DirectionalRow; }

	UFUNCTION(BlueprintPure, Category = "2.5D Animation")
	int32 GetCurrentFrameColumn() const noexcept { return CurrentFrameColumn; }

	UFUNCTION(BlueprintPure, Category = "2.5D Animation")
	int32 GetUVCellIndex() const noexcept { return DirectionalRow * GridColumns + CurrentFrameColumn; }

	UFUNCTION(BlueprintPure, Category = "2.5D Animation")
	float GetNormalizedWalkPhase() const noexcept { return NormalizedWalkPhase; }

	UFUNCTION(BlueprintPure, Category = "2.5D Animation")
	float GetAccumulatedDistanceUU() const noexcept { return AccumulatedDistanceUU; }

	UFUNCTION(BlueprintPure, Category = "2.5D Animation")
	float GetCustomPrimitiveDataValue(int32 SlotIndex) const noexcept;

	UFUNCTION(BlueprintPure, Category = "2.5D Animation")
	float GetCurrentStrideHopZ() const noexcept { return CurrentStrideHopZ; }

	UFUNCTION(BlueprintPure, Category = "2.5D Animation")
	FVector2D GetCurrentVelocityLeanDeg() const noexcept { return FVector2D(CurrentLeanPitchDeg, CurrentLeanRollDeg); }

	UFUNCTION(BlueprintPure, Category = "2.5D Animation")
	FVector GetCurrentBellySwellScale() const noexcept { return CurrentBellySwellScale; }

	UFUNCTION(BlueprintPure, Category = "2.5D Animation")
	int32 GetStepContactCount() const noexcept { return StepContactCount; }

	FOn25DStepContactSignature OnStepContact;
	FOn25DFootstepContactSignature OnFootstepContact;

protected:
	void EvaluateSubFrameSprings(float DeltaTime, const FVector& VelocityXY, bool bHasMovement) noexcept;
	void ApplyPlaneTransform() noexcept;
	void PushCustomPrimitiveData() noexcept;
	void DispatchStepContact(int32 FrameCol) noexcept;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config", meta = (ClampMin = "1"))
	int32 GridColumns = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config", meta = (ClampMin = "1"))
	int32 GridRows = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Distance", meta = (ClampMin = "10.0"))
	float StrideLengthUU = 140.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Distance", meta = (ClampMin = "50.0"))
	float ReferenceWalkSpeedUU = 450.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Audio", meta = (ClampMin = "0.0"))
	float BaseStepAudioLoudness = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Audio", meta = (ClampMin = "50.0"))
	float AcousticRippleBaseRadiusUU = 850.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Springs", meta = (ClampMin = "0.0"))
	float MaxStrideHopZ = 7.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Springs", meta = (ClampMin = "0.0"))
	float MaxVelocityLeanDeg = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Springs", meta = (ClampMin = "0.0"))
	float MaxBellySwellXY = 0.42f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Springs", meta = (ClampMin = "0.0"))
	float MaxBellySwellZ = 0.24f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config|Springs", meta = (ClampMin = "1.0"))
	float SpringStiffness = 28.0f;

private:
	TWeakObjectPtr<UPrimitiveComponent> CutoutPlane;
	TWeakObjectPtr<UPrimitiveComponent> ShadowProxyCapsule;

	FVector BasePlaneLocation = FVector::ZeroVector;
	FRotator BasePlaneRotation = FRotator::ZeroRotator;
	FVector BasePlaneScale = FVector::OneVector;

	FVector LastOwnerLocation = FVector::ZeroVector;
	bool bHasLastLocation = false;

	int32 DirectionalRow = 0;
	int32 CurrentFrameColumn = 0;
	int32 StepContactCount = 0;
	float AccumulatedDistanceUU = 0.0f;
	float NormalizedWalkPhase = 0.0f;

	float FuseProgress = 0.0f;
	float EmissivePulse = 0.0f;
	float SwellTremorPhase = 0.0f;

	float CurrentStrideHopZ = 0.0f;
	float CurrentLeanPitchDeg = 0.0f;
	float CurrentLeanRollDeg = 0.0f;
	FVector CurrentBellySwellScale = FVector::OneVector;

	TStaticArray<float, E25DAnimCPD::Count> CachedCustomData;
};
