#pragma once

/**
 * DungeoneerEcoMaterialFactory.h
 *
 * Programmatic C++ material builder (WITH_EDITOR) and runtime loader for
 * /Game/Materials/M_Dungeoneer_EcoInstanced. Reads EEcoCPD[0..7], performs
 * 64 px/m texel-center UV snapping, and evaluates the 32-slot Tier 1 radial
 * glow array from MPC_SensoryState in strict 32-bit float HLSL precision.
 */

#include "CoreMinimal.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UMaterialParameterCollection;
class UTexture2D;

/** Runtime legacy/eco layout selector mapped to the shader scalar parameter `LegacyMode`. */
enum class EDungeoneerMaterialLayoutMode : uint8
{
	Eco8Float       = 0, // CD0=UVCell (plus flipbook), CD1=Emissive, CD2..4=Hue, CD5=EventTime, CD6=ParamA
	LegacyDirectCD0 = 1, // Legacy direct UV cell in CD0
	LegacyDualCell  = 2, // Legacy Wall / Breakable: abs(CD0 - 1) < 0.25 ? CD2 : CD1
	LegacyMirror    = 3, // Legacy MirrorHISM: CD1=UVCell, CD0=SeamRevealed glow
	LegacyDirectCD1 = 4  // Legacy Floor / WetFloor / Sconce: CD1=UVCell (CD0 is NodeId/flag, no mirror glow)
};

class DARKDUNGEONEER_API FDungeoneerEcoMaterialFactory
{
public:
	static constexpr const TCHAR* MasterMaterialPackagePath = TEXT("/Game/Materials/M_Dungeoneer_EcoInstanced");
	static constexpr const TCHAR* MasterMaterialObjectPath  = TEXT("/Game/Materials/M_Dungeoneer_EcoInstanced.M_Dungeoneer_EcoInstanced");

	/**
	 * Loads /Game/Materials/M_Dungeoneer_EcoInstanced if already present on disk; in Editor
	 * builds, builds the full expression graph against SensoryMPC and saves the .uasset.
	 */
	static UMaterialInterface* EnsureEcoInstancedMaterial(UMaterialParameterCollection* SensoryMPC);

	/** Creates a configured UMaterialInstanceDynamic bound cleanly to diffuse/normal/emissive atlases. */
	static UMaterialInstanceDynamic* CreateInstancedAtlasMID(
		UObject* Outer,
		UMaterialInterface* BaseEcoMaterial,
		UTexture2D* DiffuseAtlas,
		UTexture2D* NormalAtlas,
		UTexture2D* EmissiveAtlas,
		EDungeoneerMaterialLayoutMode LayoutMode,
		bool bReceiveEcoGlow,
		float TexSizePx = 512.0f,
		int32 GridCols = 4,
		int32 GridRows = 4,
		float Roughness = 0.78f,
		float FlipbookFrames = 1.0f,
		float FlowScrollSpeed = 0.0f);
};
