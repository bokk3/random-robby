#include "Rendering/DungeoneerSpriteMaterialFactory.h"

#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "UObject/SavePackage.h"
#endif

namespace SpriteMaterialPrivate
{
	static constexpr const TCHAR* LinearWhitePackagePath = TEXT("/Game/Materials/T_Dungeoneer_LinearWhite");
	static constexpr const TCHAR* LinearWhiteObjectPath  = TEXT("/Game/Materials/T_Dungeoneer_LinearWhite.T_Dungeoneer_LinearWhite");
	static constexpr const TCHAR* FlatNormalObjectPath   = TEXT("/Game/Materials/T_Dungeoneer_FlatNormal.T_Dungeoneer_FlatNormal");

	/** Returns the saved material if it is already at least Revision (checked through its revision parameter). */
	static UMaterialInterface* LoadIfCurrent(const TCHAR* ObjectPath, const TCHAR* RevisionParam, const float Revision)
	{
		UMaterialInterface* Existing = LoadObject<UMaterialInterface>(nullptr, ObjectPath);
		if (!Existing)
		{
			return nullptr;
		}
#if WITH_EDITOR
		float Rev = 0.0f;
		if (Existing->GetScalarParameterValue(FMaterialParameterInfo(RevisionParam), Rev) && Rev >= Revision)
		{
			return Existing;
		}
		return nullptr;
#else
		return Existing;
#endif
	}

#if WITH_EDITOR
	static void SaveAsset(UPackage* Pkg, UObject* Asset, const TCHAR* PackagePath)
	{
		FString DiskFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(PackagePath, DiskFilename, FPackageName::GetAssetPackageExtension()))
		{
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(DiskFilename), true);
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			UPackage::SavePackage(Pkg, Asset, *DiskFilename, SaveArgs);
		}
	}

	/** Linear (non-sRGB) white, the default for the opacity mask sampler. Sprite masks are imported linear. */
	static UTexture2D* EnsureLinearWhiteTexture()
	{
		if (UTexture2D* Existing = LoadObject<UTexture2D>(nullptr, LinearWhiteObjectPath))
		{
			return Existing;
		}
		UPackage* Pkg = CreatePackage(LinearWhitePackagePath);
		UTexture2D* Tex = NewObject<UTexture2D>(Pkg, TEXT("T_Dungeoneer_LinearWhite"), RF_Public | RF_Standalone);
		if (!Tex)
		{
			return nullptr;
		}
		constexpr int32 Size = 4;
		Tex->Source.Init(Size, Size, 1, 1, TSF_BGRA8);
		if (uint8* Mip = Tex->Source.LockMip(0))
		{
			FMemory::Memset(Mip, 0xFF, Size * Size * 4);
			Tex->Source.UnlockMip(0);
		}
		Tex->LODGroup = TEXTUREGROUP_Pixels2D;
		Tex->Filter = TF_Nearest;
		Tex->MipGenSettings = TMGS_NoMipmaps;
		Tex->SRGB = false;
		Tex->CompressionSettings = TC_Default;
		Tex->PostEditChange();
		SaveAsset(Pkg, Tex, LinearWhitePackagePath);
		return Tex;
	}

	/** Small helper around a material's expression collection. */
	struct FGraph
	{
		UMaterial* Mat = nullptr;

		template <typename TExpr>
		TExpr* Add(const int32 X, const int32 Y) const
		{
			TExpr* Expr = NewObject<TExpr>(Mat);
			Expr->MaterialExpressionEditorX = X;
			Expr->MaterialExpressionEditorY = Y;
			Mat->GetExpressionCollection().AddExpression(Expr);
			return Expr;
		}

		UMaterialExpressionScalarParameter* Scalar(const TCHAR* Name, const float Default, const int32 X, const int32 Y, const int32 PrimitiveDataIndex = -1) const
		{
			UMaterialExpressionScalarParameter* Param = Add<UMaterialExpressionScalarParameter>(X, Y);
			Param->ParameterName = FName(Name);
			Param->DefaultValue = Default;
			if (PrimitiveDataIndex >= 0)
			{
				Param->bUseCustomPrimitiveData = true;
				Param->PrimitiveDataIndex = static_cast<uint8>(PrimitiveDataIndex);
			}
			return Param;
		}

		UMaterialExpressionVectorParameter* Vector(const TCHAR* Name, const FLinearColor& Default, const int32 X, const int32 Y) const
		{
			UMaterialExpressionVectorParameter* Param = Add<UMaterialExpressionVectorParameter>(X, Y);
			Param->ParameterName = FName(Name);
			Param->DefaultValue = Default;
			return Param;
		}

		UMaterialExpressionTextureSampleParameter2D* Texture(const TCHAR* Name, UTexture2D* Default, const EMaterialSamplerType Sampler, UMaterialExpression* UV, const int32 X, const int32 Y) const
		{
			UMaterialExpressionTextureSampleParameter2D* Tex = Add<UMaterialExpressionTextureSampleParameter2D>(X, Y);
			Tex->ParameterName = FName(Name);
			Tex->SamplerType = Sampler;
			Tex->Texture = Default;
			Tex->Coordinates.Connect(0, UV);
			return Tex;
		}

		UMaterialExpressionCustom* Custom(const TCHAR* Description, const ECustomMaterialOutputType Output, const TCHAR* Code, const int32 X, const int32 Y) const
		{
			UMaterialExpressionCustom* Node = Add<UMaterialExpressionCustom>(X, Y);
			Node->Inputs.Reset();
			Node->Description = Description;
			Node->OutputType = Output;
			Node->Code = Code;
			return Node;
		}
	};

	/** OutputIndex 5 on a texture sample is RGBA; 0 is RGB. */
	static void AddInput(UMaterialExpressionCustom* Node, const TCHAR* Name, UMaterialExpression* Source, const int32 OutputIndex = 0)
	{
		FCustomInput& In = Node->Inputs.AddDefaulted_GetRef();
		In.InputName = FName(Name);
		In.Input.Connect(OutputIndex, Source);
	}

	static UMaterial* BeginMaterial(UPackage* Pkg, const TCHAR* Name)
	{
		UMaterial* Mat = FindObject<UMaterial>(Pkg, Name);
		if (!Mat)
		{
			Mat = NewObject<UMaterial>(Pkg, Name, RF_Public | RF_Standalone);
		}
		if (Mat)
		{
			Mat->PreEditChange(nullptr);
			Mat->GetExpressionCollection().Empty();
		}
		return Mat;
	}
#endif
}

UMaterialInterface* FDungeoneerSpriteMaterialFactory::EnsureSpriteMaterial()
{
	using namespace SpriteMaterialPrivate;
	if (UMaterialInterface* Current = LoadIfCurrent(SpriteObjectPath, TEXT("SpriteMatRev"), SpriteRevision))
	{
		return Current;
	}

#if WITH_EDITOR
	UPackage* Pkg = CreatePackage(SpritePackagePath);
	UMaterial* Mat = BeginMaterial(Pkg, TEXT("M_Dungeoneer_Sprite"));
	UMaterialEditorOnlyData* Ed = Mat ? Mat->GetEditorOnlyData() : nullptr;
	if (!Mat || !Ed)
	{
		return nullptr;
	}
	Mat->BlendMode = BLEND_Masked;
	Mat->TwoSided = true;
	Mat->OpacityMaskClipValue = 0.333f;

	const FGraph G{ Mat };
	UTexture2D* WhiteTex = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
	UTexture2D* LinearWhite = EnsureLinearWhiteTexture();
	UTexture2D* FlatNormal = LoadObject<UTexture2D>(nullptr, FlatNormalObjectPath);

	UMaterialExpressionTextureCoordinate* UV0 = G.Add<UMaterialExpressionTextureCoordinate>(-1400, 0);
	UMaterialExpressionTextureSampleParameter2D* TexDiff = G.Texture(TEXT("Diffuse"), WhiteTex, SAMPLERTYPE_Color, UV0, -1100, -300);
	UMaterialExpressionTextureSampleParameter2D* TexMask = G.Texture(TEXT("OpacityMask"), LinearWhite, SAMPLERTYPE_LinearColor, UV0, -1100, 300);

	G.Scalar(TEXT("SpriteMatRev"), SpriteRevision, -1100, -600);
	UMaterialExpressionVectorParameter* Tint = G.Vector(TEXT("DiffuseColor"), FLinearColor::White, -800, -500);
	UMaterialExpressionVectorParameter* GlowColor = G.Vector(TEXT("GlowColor"), FLinearColor(1.0f, 0.45f, 0.12f), -800, 150);
	UMaterialExpressionScalarParameter* Roughness = G.Scalar(TEXT("Roughness"), 0.88f, -300, 100);
	UMaterialExpressionScalarParameter* Flash  = G.Scalar(TEXT("FlashAmount"), 0.0f, -800, -50, DungeoneerSpriteCPD::FlashAmount);
	UMaterialExpressionScalarParameter* FlashR = G.Scalar(TEXT("FlashR"), 0.0f, -800, 0, DungeoneerSpriteCPD::FlashR);
	UMaterialExpressionScalarParameter* FlashG = G.Scalar(TEXT("FlashG"), 0.0f, -800, 50, DungeoneerSpriteCPD::FlashG);
	UMaterialExpressionScalarParameter* FlashB = G.Scalar(TEXT("FlashB"), 0.0f, -800, 100, DungeoneerSpriteCPD::FlashB);
	UMaterialExpressionScalarParameter* Pulse  = G.Scalar(TEXT("GlowPulse"), 0.0f, -800, 200, DungeoneerSpriteCPD::GlowPulse);
	UMaterialExpressionScalarParameter* Fade   = G.Scalar(TEXT("FadeOut"), 0.0f, -800, 400, DungeoneerSpriteCPD::FadeOut);

	// A hit flash pushes the colour toward the flash colour; the emissive term carries the brightness.
	UMaterialExpressionCustom* Base = G.Custom(TEXT("SpriteBaseColor"), CMOT_Float3,
		TEXT("return Diff * TintRGB * (1.0 - saturate(Flash) * 0.35);"), -400, -300);
	AddInput(Base, TEXT("Diff"), TexDiff);
	AddInput(Base, TEXT("TintRGB"), Tint);
	AddInput(Base, TEXT("Flash"), Flash);
	Base->RebuildOutputs();

	UMaterialExpressionCustom* Emissive = G.Custom(TEXT("SpriteEmissive"), CMOT_Float3,
		TEXT("float3 fc = float3(FR, FG, FB);\n"
		     "if (dot(fc, float3(1.0, 1.0, 1.0)) < 0.001) { fc = float3(1.0, 1.0, 1.0); }\n"
		     "float3 flash = fc * saturate(Flash) * 1.1;\n"
		     "float warm = saturate(dot(Diff, float3(0.6, 0.35, -0.2)));\n"
		     "return flash + Diff * GlowRGB * max(Pulse, 0.0) * (0.4 + warm * 2.0);"), -400, 0);
	AddInput(Emissive, TEXT("Diff"), TexDiff);
	AddInput(Emissive, TEXT("Flash"), Flash);
	AddInput(Emissive, TEXT("FR"), FlashR);
	AddInput(Emissive, TEXT("FG"), FlashG);
	AddInput(Emissive, TEXT("FB"), FlashB);
	AddInput(Emissive, TEXT("Pulse"), Pulse);
	AddInput(Emissive, TEXT("GlowRGB"), GlowColor);
	Emissive->RebuildOutputs();

	// Dithered fade (4x4 Bayer) keeps the material masked, so fading sprites still sort and light like the rest.
	UMaterialExpressionCustom* Mask = G.Custom(TEXT("SpriteDitherMask"), CMOT_Float1,
		TEXT("float a = MaskRGB.r;\n"
		     "float FadeV = 1.0 - saturate(FadeOutV);\n"
		     "if (FadeV >= 0.999) { return a; }\n"
		     "float2 p = fmod(floor(Parameters.SvPosition.xy), 4.0);\n"
		     "float4x4 bayer = float4x4(0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5);\n"
		     "float threshold = (bayer[(int)p.y][(int)p.x] + 0.5) / 16.0;\n"
		     "return (FadeV > threshold) ? a : 0.0;"), -400, 300);
	AddInput(Mask, TEXT("MaskRGB"), TexMask);
	AddInput(Mask, TEXT("FadeOutV"), Fade);
	Mask->RebuildOutputs();

	Ed->BaseColor.Connect(0, Base);
	Ed->EmissiveColor.Connect(0, Emissive);
	Ed->OpacityMask.Connect(0, Mask);
	Ed->Roughness.Connect(0, Roughness);
	if (FlatNormal)
	{
		UMaterialExpressionTextureSampleParameter2D* TexNorm = G.Texture(TEXT("Normal"), FlatNormal, SAMPLERTYPE_Normal, UV0, -1100, 0);
		Ed->Normal.Connect(0, TexNorm);
	}

	Mat->PostEditChange();
	SaveAsset(Pkg, Mat, SpritePackagePath);
	return Mat;
#else
	return nullptr;
#endif
}

UMaterialInterface* FDungeoneerSpriteMaterialFactory::EnsureFxAdditiveMaterial()
{
	using namespace SpriteMaterialPrivate;
	if (UMaterialInterface* Current = LoadIfCurrent(FxObjectPath, TEXT("FxMatRev"), FxRevision))
	{
		return Current;
	}

#if WITH_EDITOR
	UPackage* Pkg = CreatePackage(FxPackagePath);
	UMaterial* Mat = BeginMaterial(Pkg, TEXT("M_Dungeoneer_FxAdditive"));
	UMaterialEditorOnlyData* Ed = Mat ? Mat->GetEditorOnlyData() : nullptr;
	if (!Mat || !Ed)
	{
		return nullptr;
	}
	Mat->BlendMode = BLEND_Additive;
	Mat->SetShadingModel(MSM_Unlit);
	Mat->TwoSided = true;

	const FGraph G{ Mat };
	UTexture2D* WhiteTex = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
	UMaterialExpressionTextureCoordinate* UV0 = G.Add<UMaterialExpressionTextureCoordinate>(-1000, 0);
	UMaterialExpressionTextureSampleParameter2D* TexMask = G.Texture(TEXT("Mask"), WhiteTex, SAMPLERTYPE_Color, UV0, -700, -200);

	G.Scalar(TEXT("FxMatRev"), FxRevision, -700, -400);
	UMaterialExpressionVectorParameter* Color = G.Vector(TEXT("Color"), FLinearColor(1.0f, 0.75f, 0.3f), -700, 100);
	UMaterialExpressionScalarParameter* Intensity = G.Scalar(TEXT("Intensity"), 2.0f, -700, 250);
	UMaterialExpressionScalarParameter* FxFade = G.Scalar(TEXT("FxFadeOut"), 0.0f, -700, 320, DungeoneerSpriteCPD::FxFadeOut);
	UMaterialExpressionScalarParameter* FxR = G.Scalar(TEXT("FxR"), 0.0f, -700, 390, DungeoneerSpriteCPD::FxR);
	UMaterialExpressionScalarParameter* FxG = G.Scalar(TEXT("FxG"), 0.0f, -700, 460, DungeoneerSpriteCPD::FxG);
	UMaterialExpressionScalarParameter* FxB = G.Scalar(TEXT("FxB"), 0.0f, -700, 530, DungeoneerSpriteCPD::FxB);

	// Unset custom data reads as 0, so 0 fade-out is full brightness and an all-zero tint is white.
	UMaterialExpressionCustom* Emissive = G.Custom(TEXT("FxEmissive"), CMOT_Float3,
		TEXT("float3 tint = float3(R, Gc, B);\n"
		     "if (dot(tint, float3(1.0, 1.0, 1.0)) < 0.001) { tint = float3(1.0, 1.0, 1.0); }\n"
		     "return Tex.rgb * Tex.a * ColorRGB * tint * Intensity * (1.0 - saturate(FadeOut));"), -300, 0);
	AddInput(Emissive, TEXT("Tex"), TexMask, 5);
	AddInput(Emissive, TEXT("ColorRGB"), Color);
	AddInput(Emissive, TEXT("Intensity"), Intensity);
	AddInput(Emissive, TEXT("FadeOut"), FxFade);
	AddInput(Emissive, TEXT("R"), FxR);
	AddInput(Emissive, TEXT("Gc"), FxG);
	AddInput(Emissive, TEXT("B"), FxB);
	Emissive->RebuildOutputs();

	Ed->EmissiveColor.Connect(0, Emissive);

	Mat->PostEditChange();
	SaveAsset(Pkg, Mat, FxPackagePath);
	return Mat;
#else
	return nullptr;
#endif
}
