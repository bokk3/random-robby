#pragma once

/**
 * Dungeoneer25DMaterialBuilder.h
 * 
 * Manages M_25D_PixelLit shader assets, texel density enforcement (64 px / 100 UU),
 * nearest-neighbor texture policy (TEXTUREGROUP_Pixels2D), and sensory MPC parameter bindings.
 */

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureDefines.h"
#include "Core/DungeoneerTypes.h"
#include "Dungeoneer25DMaterialBuilder.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMaterialParameterCollection;

/**
 * Config asset specifying 2.5D material pipelines, texel density, and sensory parameter targets.
 */
UCLASS(BlueprintType)
class DARKDUNGEONEER_API UDungeoneer25DMaterialConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Material")
	TObjectPtr<UMaterialInterface> BasePixelLitMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sensory")
	TObjectPtr<UMaterialParameterCollection> SensoryMaterialParameterCollection;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TexelDensity", meta = (ClampMin = "1.0", ClampMax = "512.0"))
	float TargetTexelsPerMeter = DungeoneerMath::TEXELS_PER_METER; // 64.0f

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TexelDensity", meta = (ClampMin = "1.0"))
	float UnrealUnitsPerMeter = DungeoneerMath::UNREAL_UNITS_PER_METER; // 100.0f

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TexturePolicy")
	bool bEnforcePixels2DGroup = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TexturePolicy")
	bool bEnforceNearestNeighbor = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "TexturePolicy")
	bool bEnforceNoMipmaps = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SensoryVisuals")
	FLinearColor DefaultDarkVisionColor = FLinearColor(0.18f, 0.38f, 0.72f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SensoryVisuals")
	FLinearColor DefaultAcousticRippleColor = FLinearColor(0.05f, 0.85f, 0.98f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SensoryVisuals")
	float DefaultAcousticRippleWidthUU = 40.0f;
};

/**
 * Static utility for configuring 2.5D textures, verifying 64 px / 100 UU scale,
 * and instantiating M_25D_PixelLit dynamic material instances.
 */
class DARKDUNGEONEER_API FDungeoneer25DMaterialBuilder
{
public:
	/**
	 * Configures an imported texture for semi-pixelated 2.5D rendering:
	 * Nearest-neighbor filtering, TEXTUREGROUP_Pixels2D, no mipmaps, and appropriate compression.
	 */
	static bool ConfigureTextureFor25D(UTexture2D* Texture, bool bIsNormalMap, bool bSRGB);

	/**
	 * Calculates the required world quad size in Unreal Units (UU) to preserve 64 texels per meter.
	 */
	static FVector2D CalculateWorldQuadDimensions(
		int32 PixelWidth,
		int32 PixelHeight,
		float TexelsPerMeter = DungeoneerMath::TEXELS_PER_METER,
		float UUPerMeter = DungeoneerMath::UNREAL_UNITS_PER_METER) noexcept;

	/**
	 * Validates whether a mesh quad's world size matches the 64 px / 100 UU density within tolerance.
	 */
	static bool ValidateTexelDensity(
		const FVector2D& QuadWorldSizeUU,
		const FIntPoint& TextureDimensions,
		float TargetTexelsPerMeter = DungeoneerMath::TEXELS_PER_METER,
		float Tolerance = 0.05f) noexcept;

	/**
	 * Instantiates and binds a dynamic material instance using M_25D_PixelLit and the provided texture atlas.
	 */
	static UMaterialInstanceDynamic* Create25DPixelLitMID(
		UObject* Outer,
		UMaterialInterface* BaseMaterial,
		UTexture2D* DiffuseTexture,
		UTexture2D* NormalTexture = nullptr,
		UTexture2D* ORMTexture = nullptr);
};
