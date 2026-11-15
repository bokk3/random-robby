#pragma once

/**
 * 25DDirectionalRigComponent.h
 * 
 * Manages multi-plane 2.5D cutout sockets (Head, Torso, Limbs, WeaponFlashlight),
 * supporting both 4-directional asymmetric rigs (S, N, E, W) and 8-compass billboards.
 * Updates sprite cell UV offsets, directional row, and emissive pulse intensity via
 * SetCustomPrimitiveDataFloat with zero heap allocations on the Tick hot path.
 */

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Containers/StaticArray.h"
#include "Core/DungeoneerTypes.h"
#include "25DDirectionalRigComponent.generated.h"

class UPrimitiveComponent;

UENUM(BlueprintType)
enum class E25DRigSocket : uint8
{
	Torso            = 0 UMETA(DisplayName = "Torso"),
	Head             = 1 UMETA(DisplayName = "Head"),
	Limbs            = 2 UMETA(DisplayName = "Limbs"),
	WeaponFlashlight = 3 UMETA(DisplayName = "Weapon / Flashlight"),
	COUNT            = 4 UMETA(Hidden)
};

namespace E25DCPDIndex
{
	inline constexpr int32 CellUVOffsetX  = 0;
	inline constexpr int32 CellUVOffsetY  = 1;
	inline constexpr int32 CellUVScaleX   = 2;
	inline constexpr int32 CellUVScaleY   = 3;
	inline constexpr int32 DirectionalRow = 4;
	inline constexpr int32 EmissivePulse  = 5;
	inline constexpr int32 HorizontalFlip = 6;
	inline constexpr int32 DepthOffset    = 7;
}

USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API F25DSocketConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig")
	FVector LocalOffset = FVector::ZeroVector;

	/** Multi-plane depth offset along Y-axis to guarantee proper rendering order and avoid Z-fighting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rig")
	float DepthLayerY = 0.0f;
};

/**
 * Component handling 2.5D directional cutout multi-plane animation and Custom Primitive Data updates.
 */
UCLASS(ClassGroup = (Rendering), meta = (BlueprintSpawnableComponent))
class DARKDUNGEONEER_API U25DDirectionalRigComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	U25DDirectionalRigComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Registers a primitive component to be driven as a specific cutout socket. */
	UFUNCTION(BlueprintCallable, Category = "2.5D Rig")
	void RegisterSocketPrimitive(E25DRigSocket Socket, UPrimitiveComponent* PrimitiveComponent);

	/** Sets the compass heading octant, updating the directional row on the next tick. */
	UFUNCTION(BlueprintCallable, Category = "2.5D Rig")
	void SetHeadingOctant(ECompassOctant NewOctant) noexcept;

	/** Starts or stops frame animation advancement. */
	UFUNCTION(BlueprintCallable, Category = "2.5D Rig")
	void SetAnimationPlaying(bool bPlaying) noexcept { bIsAnimating = bPlaying; }

	/** Sets the active animation frame index explicitly. */
	UFUNCTION(BlueprintCallable, Category = "2.5D Rig")
	void SetFrameIndex(int32 Frame) noexcept;

	/** Triggers a momentary emissive brightness flash (e.g. on damage taken or fuse tick). */
	UFUNCTION(BlueprintCallable, Category = "2.5D Rig")
	void TriggerEmissivePulse(float Intensity) noexcept;

	/** Configures atlas grid slicing dimensions (e.g. 4x4 or 8x8). */
	UFUNCTION(BlueprintCallable, Category = "2.5D Rig")
	void SetGridDimensions(int32 InColumns, int32 InRows) noexcept;

	UFUNCTION(BlueprintPure, Category = "2.5D Rig")
	ECompassOctant GetHeadingOctant() const noexcept { return CurrentOctant; }

	UFUNCTION(BlueprintPure, Category = "2.5D Rig")
	int32 GetFrameIndex() const noexcept { return CurrentFrameIndex; }

	UFUNCTION(BlueprintPure, Category = "2.5D Rig")
	bool Is8CompassBillboard() const noexcept { return bIs8CompassBillboard; }

	UFUNCTION(BlueprintCallable, Category = "2.5D Rig")
	void SetIs8CompassBillboard(bool bEnable8Compass) noexcept { bIs8CompassBillboard = bEnable8Compass; }

	/** Configures an unrendered volumetric shadow proxy (bRenderInMainPass=false, bCastHiddenShadow=true, CastShadow=true). */
	UFUNCTION(BlueprintCallable, Category = "2.5D Rig")
	void ConfigureVolumetricShadowProxy(UPrimitiveComponent* ProxyMesh) noexcept;

protected:
	void PushCustomPrimitiveData() noexcept;

	/** If false, uses 4-directional asymmetric rig (S, N, E, W); if true, full 8-octant compass rows. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	bool bIs8CompassBillboard = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config", meta = (ClampMin = "1"))
	int32 GridColumns = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config", meta = (ClampMin = "1"))
	int32 GridRows = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config", meta = (ClampMin = "1.0"))
	float AnimationFps = 8.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config")
	bool bIsAnimating = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Config", meta = (ClampMin = "0.5"))
	float EmissivePulseDecaySpeed = 5.0f;

	UPROPERTY(Transient)
	ECompassOctant CurrentOctant = ECompassOctant::S;

	UPROPERTY(Transient)
	int32 CurrentFrameIndex = 0;

	UPROPERTY(Transient)
	float AnimationTimer = 0.0f;

	UPROPERTY(Transient)
	float EmissivePulseIntensity = 0.0f;

private:
	static constexpr int32 NumSockets = static_cast<int32>(E25DRigSocket::COUNT);
	TStaticArray<TWeakObjectPtr<UPrimitiveComponent>, NumSockets> SocketPrimitives;
	TStaticArray<F25DSocketConfig, NumSockets> SocketConfigs;
};
