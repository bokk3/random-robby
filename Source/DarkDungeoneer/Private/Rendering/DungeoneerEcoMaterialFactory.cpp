#include "Rendering/DungeoneerEcoMaterialFactory.h"

#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionPerInstanceCustomData.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "TextureResource.h"
#include "UObject/SavePackage.h"
#endif

namespace EcoMaterialPrivate
{
	static constexpr const TCHAR* FlatNormalPackagePath = TEXT("/Game/Materials/T_Dungeoneer_FlatNormal");
	static constexpr const TCHAR* FlatNormalObjectPath  = TEXT("/Game/Materials/T_Dungeoneer_FlatNormal.T_Dungeoneer_FlatNormal");

#if WITH_EDITOR
	static UTexture2D* EnsureFlatNormalTexture()
	{
		if (UTexture2D* Existing = LoadObject<UTexture2D>(nullptr, FlatNormalObjectPath))
		{
			if (Existing->CompressionSettings == TC_Normalmap && !Existing->SRGB)
			{
				return Existing;
			}
		}

		UPackage* Pkg = CreatePackage(FlatNormalPackagePath);
		UTexture2D* NormTex = FindObject<UTexture2D>(Pkg, TEXT("T_Dungeoneer_FlatNormal"));
		if (!NormTex)
		{
			NormTex = NewObject<UTexture2D>(Pkg, TEXT("T_Dungeoneer_FlatNormal"), RF_Public | RF_Standalone);
		}
		if (!NormTex)
		{
			return nullptr;
		}

		constexpr int32 Size = 16;
		NormTex->Source.Init(Size, Size, 1, 1, TSF_BGRA8);
		if (uint8* MipData = NormTex->Source.LockMip(0))
		{
			for (int32 i = 0; i < Size * Size; ++i)
			{
				MipData[i * 4 + 0] = 255; // B (Z=+1)
				MipData[i * 4 + 1] = 128; // G (Y=0)
				MipData[i * 4 + 2] = 128; // R (X=0)
				MipData[i * 4 + 3] = 255; // A
			}
			NormTex->Source.UnlockMip(0);
		}

		NormTex->LODGroup = TEXTUREGROUP_Pixels2D;
		NormTex->Filter = TF_Nearest;
		NormTex->MipGenSettings = TMGS_NoMipmaps;
		NormTex->SRGB = false;
		NormTex->CompressionSettings = TC_Normalmap;
		NormTex->PostEditChange();

		FString DiskFilename;
		if (FPackageName::TryConvertLongPackageNameToFilename(FlatNormalPackagePath, DiskFilename, FPackageName::GetAssetPackageExtension()))
		{
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(DiskFilename), true);
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			UPackage::SavePackage(Pkg, NormTex, *DiskFilename, SaveArgs);
		}
		return NormTex;
	}
#endif
}

UMaterialInterface* FDungeoneerEcoMaterialFactory::EnsureEcoInstancedMaterial(UMaterialParameterCollection* SensoryMPC)
{
	UMaterialInterface* Existing = LoadObject<UMaterialInterface>(nullptr, MasterMaterialObjectPath);
	if (Existing)
	{
#if WITH_EDITOR
		float RevVal = 0.0f;
		if (Existing->GetScalarParameterValue(FMaterialParameterInfo(TEXT("EcoMatRev6")), RevVal) && RevVal >= 6.0f)
		{
			return Existing;
		}
#else
		return Existing;
#endif
	}

#if WITH_EDITOR
	if (!SensoryMPC)
	{
		return nullptr;
	}

	UTexture2D* FlatNormalTex = EcoMaterialPrivate::EnsureFlatNormalTexture();

	UPackage* Pkg = CreatePackage(MasterMaterialPackagePath);
	UMaterial* Mat = FindObject<UMaterial>(Pkg, TEXT("M_Dungeoneer_EcoInstanced"));
	if (!Mat)
	{
		Mat = NewObject<UMaterial>(Pkg, TEXT("M_Dungeoneer_EcoInstanced"), RF_Public | RF_Standalone);
	}
	if (!Mat)
	{
		return nullptr;
	}

	Mat->PreEditChange(nullptr);
	Mat->GetExpressionCollection().Empty();

	Mat->BlendMode = BLEND_Masked;
	Mat->TwoSided = true;
	Mat->SetUsageByFlag(MATUSAGE_InstancedStaticMeshes, true);
	Mat->OpacityMaskClipValue = 0.333f;

	UMaterialEditorOnlyData* Ed = Mat->GetEditorOnlyData();
	if (!Ed)
	{
		return nullptr;
	}

	auto AddExpr = [Mat]<typename TExpr>(int32 PosX, int32 PosY) -> TExpr*
	{
		TExpr* Expr = NewObject<TExpr>(Mat);
		Expr->MaterialExpressionEditorX = PosX;
		Expr->MaterialExpressionEditorY = PosY;
		Mat->GetExpressionCollection().AddExpression(Expr);
		return Expr;
	};

	auto MakeScalar = [&AddExpr](const TCHAR* Name, float DefaultVal, int32 X, int32 Y) -> UMaterialExpressionScalarParameter*
	{
		UMaterialExpressionScalarParameter* Param = AddExpr.operator()<UMaterialExpressionScalarParameter>(X, Y);
		Param->ParameterName = FName(Name);
		Param->DefaultValue = DefaultVal;
		return Param;
	};

	auto MakeMPC = [&AddExpr, SensoryMPC](const FName& Name, int32 X, int32 Y) -> UMaterialExpressionCollectionParameter*
	{
		UMaterialExpressionCollectionParameter* Node = AddExpr.operator()<UMaterialExpressionCollectionParameter>(X, Y);
		Node->Collection = SensoryMPC;
		Node->ParameterName = Name;
		const FGuid ExistingId = SensoryMPC->GetParameterId(Name);
		if (ExistingId.IsValid())
		{
			Node->ParameterId = ExistingId;
		}
		return Node;
	};

	// Inputs: UV0, WorldPos, CustomData[0..7]
	UMaterialExpressionTextureCoordinate* UV0 = AddExpr.operator()<UMaterialExpressionTextureCoordinate>(-1600, -400);
	UMaterialExpressionWorldPosition* WorldPos = AddExpr.operator()<UMaterialExpressionWorldPosition>(-1600, 400);

	TStaticArray<UMaterialExpressionPerInstanceCustomData*, 8> CDNodes;
	for (int32 i = 0; i < 8; ++i)
	{
		CDNodes[i] = AddExpr.operator()<UMaterialExpressionPerInstanceCustomData>(-1600, -250 + i * 70);
		CDNodes[i]->DataIndex = static_cast<uint32>(i);
		CDNodes[i]->ConstDefaultValue = (i == 1) ? 1.0f : 0.0f;
	}

	UMaterialExpressionScalarParameter* ParamLegacyMode   = MakeScalar(TEXT("LegacyMode"), 0.0f, -1350, -500);
	UMaterialExpressionScalarParameter* ParamRev5         = MakeScalar(TEXT("EcoMatRev6"), 6.0f, -1350, -570);
	(void)ParamRev5;
	UMaterialExpressionScalarParameter* ParamReceiveGlow  = MakeScalar(TEXT("bReceiveEcoGlow"), 1.0f, -1350, 350);
	UMaterialExpressionScalarParameter* ParamGridCols     = MakeScalar(TEXT("GridCols"), 4.0f, -1350, -430);
	UMaterialExpressionScalarParameter* ParamGridRows     = MakeScalar(TEXT("GridRows"), 4.0f, -1350, -360);
	UMaterialExpressionScalarParameter* ParamTexSize      = MakeScalar(TEXT("TexSize"), 512.0f, -1350, -290);
	UMaterialExpressionScalarParameter* ParamFlipFrames   = MakeScalar(TEXT("FlipbookFrames"), 1.0f, -1350, -220);
	UMaterialExpressionScalarParameter* ParamFlipFPS      = MakeScalar(TEXT("FlipbookFPS"), 6.0f, -1350, -150);
	UMaterialExpressionScalarParameter* ParamFlowSpeed    = MakeScalar(TEXT("FlowScrollSpeed"), 0.0f, -1350, -80);
	UMaterialExpressionScalarParameter* ParamRoughness    = MakeScalar(TEXT("Roughness"), 0.78f, -400, 120);

	UMaterialExpressionVectorParameter* ParamDiffuseColor = AddExpr.operator()<UMaterialExpressionVectorParameter>(-500, -550);
	ParamDiffuseColor->ParameterName = FName(TEXT("DiffuseColor"));
	ParamDiffuseColor->DefaultValue = FLinearColor::White;

	UMaterialExpressionCollectionParameter* MpcEcoTime    = MakeMPC(FName(TEXT("EcoTime")), -1350, 10);
	UMaterialExpressionCollectionParameter* MpcGlowCount  = MakeMPC(FName(TEXT("EcoGlowCount")), -1350, 440);

	// 1) Custom HLSL UV + texel-center snap + flipbook/flow/fade controller (strict 32-bit float)
	UMaterialExpressionCustom* CustomUV = AddExpr.operator()<UMaterialExpressionCustom>(-1050, -250);
	CustomUV->Inputs.Reset();
	CustomUV->Description = TEXT("EcoUVSnapAndEventMask");
	CustomUV->OutputType = ECustomMaterialOutputType::CMOT_Float3; // XY = SnappedUV, Z = AlphaFadeMul
	CustomUV->Code = TEXT(
		"float cell = CD0;\n"
		"if (LegacyMode > 1.5 && LegacyMode < 2.5) {\n"
		"    cell = (abs(CD0 - 1.0) < 0.25) ? CD2 : CD1;\n"
		"} else if (LegacyMode >= 2.5) {\n"
		"    cell = CD1;\n"
		"} else if (LegacyMode <= 0.5 && FlipFrames > 1.5) {\n"
		"    float dt = (EcoTime >= CD5) ? (EcoTime - CD5) : (EcoTime + 512.0 - CD5);\n"
		"    cell = floor(CD0 + 0.5) + floor(fmod(dt * FlipFPS, FlipFrames));\n"
		"}\n"
		"float cols = max(floor(GridCols + 0.5), 1.0);\n"
		"float rows = max(floor(GridRows + 0.5), 1.0);\n"
		"float cIdx = floor(clamp(cell + 0.5, 0.0, cols * rows - 1.0));\n"
		"float col = fmod(cIdx, cols);\n"
		"float row = floor(cIdx / cols);\n"
		"float2 localUV = UV0;\n"
		"if (FlowSpeed > 0.001) {\n"
		"    float angle = CD6 * 0.785398163;\n"
		"    localUV = frac(localUV + float2(cos(angle), -sin(angle)) * (EcoTime * FlowSpeed));\n"
		"}\n"
		"float2 atlasUV = float2((col + localUV.x) / cols, (row + localUV.y) / rows);\n"
		"float texSz = max(TexSize, 16.0);\n"
		"float2 snappedUV = (floor(atlasUV * texSz) + 0.5) / texSz;\n"
		"float fadeMul = 1.0;\n"
		"if (LegacyMode <= 0.5 && CD6 > 0.05 && CD6 < 12.0) {\n"
		"    float kindVal = floor(CD7 / 16.0 + 0.01);\n"
		"    if (((kindVal >= 1.5 && kindVal < 3.5) || kindVal >= 5.5) && FlowSpeed <= 0.001) {\n"
		"        float age = (EcoTime >= CD5) ? (EcoTime - CD5) : (EcoTime + 512.0 - CD5);\n"
		"        fadeMul = saturate(1.0 - age / max(CD6, 0.1));\n"
		"    }\n"
		"}\n"
		"return float3(snappedUV.x, snappedUV.y, fadeMul);\n"
	);

	auto AddCustomIn = [](UMaterialExpressionCustom* CustomNode, const TCHAR* InName, UMaterialExpression* SrcExpr)
	{
		FCustomInput& NewIn = CustomNode->Inputs.AddDefaulted_GetRef();
		NewIn.InputName = FName(InName);
		NewIn.Input.Connect(0, SrcExpr);
	};

	AddCustomIn(CustomUV, TEXT("UV0"), UV0);
	AddCustomIn(CustomUV, TEXT("CD0"), CDNodes[0]);
	AddCustomIn(CustomUV, TEXT("CD1"), CDNodes[1]);
	AddCustomIn(CustomUV, TEXT("CD2"), CDNodes[2]);
	AddCustomIn(CustomUV, TEXT("CD5"), CDNodes[5]);
	AddCustomIn(CustomUV, TEXT("CD6"), CDNodes[6]);
	AddCustomIn(CustomUV, TEXT("CD7"), CDNodes[7]);
	AddCustomIn(CustomUV, TEXT("LegacyMode"), ParamLegacyMode);
	AddCustomIn(CustomUV, TEXT("GridCols"), ParamGridCols);
	AddCustomIn(CustomUV, TEXT("GridRows"), ParamGridRows);
	AddCustomIn(CustomUV, TEXT("TexSize"), ParamTexSize);
	AddCustomIn(CustomUV, TEXT("FlipFrames"), ParamFlipFrames);
	AddCustomIn(CustomUV, TEXT("FlipFPS"), ParamFlipFPS);
	AddCustomIn(CustomUV, TEXT("FlowSpeed"), ParamFlowSpeed);
	AddCustomIn(CustomUV, TEXT("EcoTime"), MpcEcoTime);
	CustomUV->RebuildOutputs();

	// Split snapped UV.xy and AlphaMul via tiny float2 Custom node (zero extra ComponentMask headers needed)
	UMaterialExpressionCustom* CustomUVXY = AddExpr.operator()<UMaterialExpressionCustom>(-820, -250);
	CustomUVXY->Inputs.Reset();
	CustomUVXY->Description = TEXT("ExtractSnappedUV");
	CustomUVXY->OutputType = ECustomMaterialOutputType::CMOT_Float2;
	CustomUVXY->Code = TEXT("return PackedUV.xy;");
	AddCustomIn(CustomUVXY, TEXT("PackedUV"), CustomUV);
	CustomUVXY->RebuildOutputs();

	// 2) Texture samples using nearest/snapped UV
	UTexture2D* DefaultWhiteTex = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"));
	UTexture2D* DefaultBlackTex = LoadObject<UTexture2D>(nullptr, TEXT("/Engine/EngineResources/Black.Black"));

	UMaterialExpressionTextureSampleParameter2D* TexDiff = AddExpr.operator()<UMaterialExpressionTextureSampleParameter2D>(-550, -350);
	TexDiff->ParameterName = FName(TEXT("Diffuse"));
	TexDiff->SamplerType = SAMPLERTYPE_Color;
	TexDiff->Texture = DefaultWhiteTex;
	TexDiff->Coordinates.Connect(0, CustomUVXY);

	UMaterialExpressionTextureSampleParameter2D* TexNorm = AddExpr.operator()<UMaterialExpressionTextureSampleParameter2D>(-550, -100);
	TexNorm->ParameterName = FName(TEXT("Normal"));
	TexNorm->SamplerType = SAMPLERTYPE_Normal;
	if (FlatNormalTex)
	{
		TexNorm->Texture = FlatNormalTex;
	}
	TexNorm->Coordinates.Connect(0, CustomUVXY);

	UMaterialExpressionTextureSampleParameter2D* TexEmis = AddExpr.operator()<UMaterialExpressionTextureSampleParameter2D>(-550, 180);
	TexEmis->ParameterName = FName(TEXT("Emissive"));
	TexEmis->SamplerType = SAMPLERTYPE_Color;
	TexEmis->Texture = DefaultBlackTex;
	TexEmis->Coordinates.Connect(0, CustomUVXY);

	// 3) Tier 1 32-slot Radial Distance-Field Glow HLSL Node (strict 32-bit float; squared-dist check BEFORE unpack)
	UMaterialExpressionCustom* CustomTier1Glow = AddExpr.operator()<UMaterialExpressionCustom>(-1050, 500);
	CustomTier1Glow->Inputs.Reset();
	CustomTier1Glow->Description = TEXT("Tier1RadialGlow32Bit");
	CustomTier1Glow->OutputType = ECustomMaterialOutputType::CMOT_Float3;
	AddCustomIn(CustomTier1Glow, TEXT("WorldPos"), WorldPos);
	AddCustomIn(CustomTier1Glow, TEXT("bReceive"), ParamReceiveGlow);
	AddCustomIn(CustomTier1Glow, TEXT("GlowCount"), MpcGlowCount);

	FString GlowCode = TEXT("float3 accum = float3(0.0, 0.0, 0.0);\n"
		"if (bReceive <= 0.5) { return accum; }\n"
		"int count = min(int(GlowCount + 0.5), 32);\n"
		"float4 G[32] = {\n");

	for (int32 i = 0; i < 32; ++i)
	{
		const FName MpcName(*FString::Printf(TEXT("EcoGlow%02d"), i));
		UMaterialExpressionCollectionParameter* MpcG = MakeMPC(MpcName, -1350, 510 + i * 32);
		const FString InPin = FString::Printf(TEXT("G%02d"), i);
		AddCustomIn(CustomTier1Glow, *InPin, MpcG);
		GlowCode += FString::Printf(TEXT("  %s%s"), *InPin, (i < 31) ? TEXT(", ") : TEXT(" "));
		if ((i & 7) == 7)
		{
			GlowCode += TEXT("\n");
		}
	}

	GlowCode += TEXT(
		"};\n"
		"for (int idx = 0; idx < 32; ++idx) {\n"
		"    if (idx >= count) { break; }\n"
		"    float r = G[idx].z;\n"
		"    float2 dXY = WorldPos.xy - G[idx].xy;\n"
		"    float dSq = dot(dXY, dXY);\n"
		"    float rSq = r * r;\n"
		"    if (dSq >= rSq || r <= 1.0) { continue; }\n"
		"    float packed = floor(G[idx].w + 0.5);\n"
		"    float cr = floor(packed / 65536.0);\n"
		"    float rem = packed - cr * 65536.0;\n"
		"    float cg = floor(rem / 256.0);\n"
		"    float cb = rem - cg * 256.0;\n"
		"    float3 col = float3(cr, cg, cb) / 255.0;\n"
		"    float atten = 1.0 - sqrt(dSq) / r;\n"
		"    accum += col * (atten * atten * 1.45);\n"
		"}\n"
		"return accum;\n"
	);
	CustomTier1Glow->Code = GlowCode;
	CustomTier1Glow->RebuildOutputs();

	// 4) Final BaseColor + Emissive + OpacityMask Combiner
	UMaterialExpressionCustom* FinalCombiner = AddExpr.operator()<UMaterialExpressionCustom>(-220, -100);
	FinalCombiner->Inputs.Reset();
	FinalCombiner->Description = TEXT("EcoFinalBaseColor");
	FinalCombiner->OutputType = ECustomMaterialOutputType::CMOT_Float3;
	FinalCombiner->Code = TEXT("return TexDiffRGB * TintRGB;");
	AddCustomIn(FinalCombiner, TEXT("TexDiffRGB"), TexDiff);
	AddCustomIn(FinalCombiner, TEXT("TintRGB"), ParamDiffuseColor);
	FinalCombiner->RebuildOutputs();

	UMaterialExpressionCustom* EmissiveCombiner = AddExpr.operator()<UMaterialExpressionCustom>(-220, 200);
	EmissiveCombiner->Inputs.Reset();
	EmissiveCombiner->Description = TEXT("EcoEmissiveCombiner");
	EmissiveCombiner->OutputType = ECustomMaterialOutputType::CMOT_Float3;
	EmissiveCombiner->Code = TEXT(
		"float3 litAlbedo = max(TexDiffRGB, float3(0.08, 0.08, 0.11));\n"
		"float3 tier1 = litAlbedo * Tier1Glow * 1.65;\n"
		"if (LegacyMode >= 2.5 && LegacyMode < 3.5) {\n"
		"    float3 seam = (CD0 > 0.5) ? float3(0.18, 0.85, 1.0) * 2.2 : float3(0.0, 0.0, 0.0);\n"
		"    return TexEmisRGB + seam + tier1 + TexDiffRGB * float3(0.08, 0.11, 0.16);\n"
		"}\n"
		"if (LegacyMode > 0.5) {\n"
		"    return TexEmisRGB * 1.45 + tier1 + TexDiffRGB * float3(0.08, 0.11, 0.16);\n"
		"}\n"
		"float wavePulse = 1.0 + 0.22 * sin(EcoTime * 3.0 + CD5);\n"
		"float3 hue = max(float3(CD2, CD3, CD4), float3(0.05, 0.05, 0.05));\n"
		"float rawBoost = max(CD1, 0.0) * wavePulse;\n"
		"float boost = (rawBoost <= 1.2) ? rawBoost : (1.2 + log2(1.0 + rawBoost - 1.2) * 0.55);\n"
		"float3 maskCore = TexEmisRGB * 1.05 + TexDiffRGB * 0.12;\n"
		"float3 vivid = lerp(maskCore, maskCore * hue, 0.72);\n"
		"return vivid * boost + tier1 * 0.65;\n"
	);
	AddCustomIn(EmissiveCombiner, TEXT("TexDiffRGB"), TexDiff);
	AddCustomIn(EmissiveCombiner, TEXT("TexEmisRGB"), TexEmis);
	AddCustomIn(EmissiveCombiner, TEXT("Tier1Glow"), CustomTier1Glow);
	AddCustomIn(EmissiveCombiner, TEXT("LegacyMode"), ParamLegacyMode);
	AddCustomIn(EmissiveCombiner, TEXT("EcoTime"), MpcEcoTime);
	AddCustomIn(EmissiveCombiner, TEXT("CD0"), CDNodes[0]);
	AddCustomIn(EmissiveCombiner, TEXT("CD1"), CDNodes[1]);
	AddCustomIn(EmissiveCombiner, TEXT("CD2"), CDNodes[2]);
	AddCustomIn(EmissiveCombiner, TEXT("CD3"), CDNodes[3]);
	AddCustomIn(EmissiveCombiner, TEXT("CD4"), CDNodes[4]);
	AddCustomIn(EmissiveCombiner, TEXT("CD5"), CDNodes[5]);
	EmissiveCombiner->RebuildOutputs();

	UMaterialExpressionCustom* MaskOut = AddExpr.operator()<UMaterialExpressionCustom>(0, 80);
	MaskOut->Inputs.Reset();
	MaskOut->Description = TEXT("ExtractMaskAlpha");
	MaskOut->OutputType = ECustomMaterialOutputType::CMOT_Float1;
	MaskOut->Code = TEXT("return saturate(TexRGBA.a * PackedUV.z);");
	{
		FCustomInput& RgbaIn = MaskOut->Inputs.AddDefaulted_GetRef();
		RgbaIn.InputName = FName(TEXT("TexRGBA"));
		RgbaIn.Input.Connect(5, TexDiff); // Output 5 = TextureSample RGBA float4
	}
	AddCustomIn(MaskOut, TEXT("PackedUV"), CustomUV);
	MaskOut->RebuildOutputs();

	Ed->BaseColor.Connect(0, FinalCombiner);
	Ed->Roughness.Connect(0, ParamRoughness);
	Ed->Normal.Connect(0, TexNorm);
	Ed->EmissiveColor.Connect(0, EmissiveCombiner);
	Ed->OpacityMask.Connect(0, MaskOut);

	Mat->PostEditChange();

	FString DiskFilename;
	if (FPackageName::TryConvertLongPackageNameToFilename(MasterMaterialPackagePath, DiskFilename, FPackageName::GetAssetPackageExtension()))
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(DiskFilename), true);
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(Pkg, Mat, *DiskFilename, SaveArgs);
	}

	return Mat;
#else
	return nullptr;
#endif
}

UMaterialInstanceDynamic* FDungeoneerEcoMaterialFactory::CreateInstancedAtlasMID(
	UObject* Outer,
	UMaterialInterface* BaseEcoMaterial,
	UTexture2D* DiffuseAtlas,
	UTexture2D* NormalAtlas,
	UTexture2D* EmissiveAtlas,
	const EDungeoneerMaterialLayoutMode LayoutMode,
	const bool bReceiveEcoGlow,
	const float TexSizePx,
	const int32 GridCols,
	const int32 GridRows,
	const float Roughness,
	const float FlipbookFrames,
	const float FlowScrollSpeed)
{
	if (!BaseEcoMaterial)
	{
		return nullptr;
	}

	UObject* EffectiveOuter = Outer ? Outer : BaseEcoMaterial;
	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseEcoMaterial, EffectiveOuter);
	if (!MID)
	{
		return nullptr;
	}

	if (DiffuseAtlas)
	{
		MID->SetTextureParameterValue(FName(TEXT("Diffuse")), DiffuseAtlas);
	}
	if (NormalAtlas)
	{
		MID->SetTextureParameterValue(FName(TEXT("Normal")), NormalAtlas);
	}
	if (EmissiveAtlas)
	{
		MID->SetTextureParameterValue(FName(TEXT("Emissive")), EmissiveAtlas);
	}

	MID->SetScalarParameterValue(FName(TEXT("LegacyMode")), static_cast<float>(static_cast<uint8>(LayoutMode)));
	MID->SetScalarParameterValue(FName(TEXT("bReceiveEcoGlow")), bReceiveEcoGlow ? 1.0f : 0.0f);
	MID->SetScalarParameterValue(FName(TEXT("TexSize")), TexSizePx);
	MID->SetScalarParameterValue(FName(TEXT("GridCols")), static_cast<float>(FMath::Max(GridCols, 1)));
	MID->SetScalarParameterValue(FName(TEXT("GridRows")), static_cast<float>(FMath::Max(GridRows, 1)));
	MID->SetScalarParameterValue(FName(TEXT("Roughness")), Roughness);
	MID->SetScalarParameterValue(FName(TEXT("FlipbookFrames")), FlipbookFrames);
	MID->SetScalarParameterValue(FName(TEXT("FlowScrollSpeed")), FlowScrollSpeed);
	MID->SetVectorParameterValue(FName(TEXT("DiffuseColor")), FLinearColor::White);
	return MID;
}
