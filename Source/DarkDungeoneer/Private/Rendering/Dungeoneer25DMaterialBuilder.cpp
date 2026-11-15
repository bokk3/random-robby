/**
 * Dungeoneer25DMaterialBuilder.cpp
 * 
 * Implementation of 2.5D material builder and texture configuration utilities.
 */

#include "Rendering/Dungeoneer25DMaterialBuilder.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "Engine/Texture2D.h"

bool FDungeoneer25DMaterialBuilder::ConfigureTextureFor25D(UTexture2D* Texture, bool bIsNormalMap, bool bSRGB)
{
	if (!Texture)
	{
		return false;
	}

	Texture->LODGroup = TEXTUREGROUP_Pixels2D;
	Texture->Filter = TF_Nearest;
	Texture->MipGenSettings = TMGS_NoMipmaps;
	Texture->SRGB = bSRGB;
	Texture->CompressionSettings = bIsNormalMap ? TC_Normalmap : (bSRGB ? TC_Default : TC_Masks);

	Texture->UpdateResource();
	return true;
}

FVector2D FDungeoneer25DMaterialBuilder::CalculateWorldQuadDimensions(
	int32 PixelWidth,
	int32 PixelHeight,
	float TexelsPerMeter,
	float UUPerMeter) noexcept
{
	const float SafeTexelsPerMeter = FMath::Max(TexelsPerMeter, 1.0f);
	const double ScaleFactor = static_cast<double>(UUPerMeter) / static_cast<double>(SafeTexelsPerMeter);

	return FVector2D(
		static_cast<double>(PixelWidth) * ScaleFactor,
		static_cast<double>(PixelHeight) * ScaleFactor
	);
}

bool FDungeoneer25DMaterialBuilder::ValidateTexelDensity(
	const FVector2D& QuadWorldSizeUU,
	const FIntPoint& TextureDimensions,
	float TargetTexelsPerMeter,
	float Tolerance) noexcept
{
	if (QuadWorldSizeUU.X <= KINDA_SMALL_NUMBER || QuadWorldSizeUU.Y <= KINDA_SMALL_NUMBER ||
		TextureDimensions.X <= 0 || TextureDimensions.Y <= 0)
	{
		return false;
	}

	const double ActualTexelsPerMeterX = (static_cast<double>(TextureDimensions.X) / QuadWorldSizeUU.X) * DungeoneerMath::UNREAL_UNITS_PER_METER;
	const double ActualTexelsPerMeterY = (static_cast<double>(TextureDimensions.Y) / QuadWorldSizeUU.Y) * DungeoneerMath::UNREAL_UNITS_PER_METER;

	const double ErrorMargin = static_cast<double>(TargetTexelsPerMeter) * static_cast<double>(Tolerance);

	return FMath::IsNearlyEqual(ActualTexelsPerMeterX, static_cast<double>(TargetTexelsPerMeter), ErrorMargin) &&
	       FMath::IsNearlyEqual(ActualTexelsPerMeterY, static_cast<double>(TargetTexelsPerMeter), ErrorMargin);
}

UMaterialInstanceDynamic* FDungeoneer25DMaterialBuilder::Create25DPixelLitMID(
	UObject* Outer,
	UMaterialInterface* BaseMaterial,
	UTexture2D* DiffuseTexture,
	UTexture2D* NormalTexture,
	UTexture2D* ORMTexture)
{
	if (!BaseMaterial || !Outer)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseMaterial, Outer);
	if (!MID)
	{
		return nullptr;
	}

	if (DiffuseTexture)
	{
		ConfigureTextureFor25D(DiffuseTexture, false, true);
		MID->SetTextureParameterValue(FName(TEXT("DiffuseTexture")), DiffuseTexture);
	}

	if (NormalTexture)
	{
		ConfigureTextureFor25D(NormalTexture, true, false);
		MID->SetTextureParameterValue(FName(TEXT("NormalTexture")), NormalTexture);
	}

	if (ORMTexture)
	{
		ConfigureTextureFor25D(ORMTexture, false, false);
		MID->SetTextureParameterValue(FName(TEXT("ORMTexture")), ORMTexture);
	}

	MID->SetScalarParameterValue(FName(TEXT("TexelsPerMeter")), DungeoneerMath::TEXELS_PER_METER);

	return MID;
}
