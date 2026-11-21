#pragma once

#include "CoreMinimal.h"

class UMaterialInterface;

/**
 * Custom primitive data slots read by M_Dungeoneer_Sprite and M_Dungeoneer_FxAdditive. Set them on the
 * mesh component (SetCustomPrimitiveDataFloat), not on a material instance, so they survive the
 * per-frame material swaps of the walk flipbook. Slots 0-7 belong to the 25D rig layout (E25DCPDIndex).
 *
 * A slot a component never sets reads as 0 in the shader, so every slot is defined so that 0 means
 * "normal": no flash, no glow, not faded, and an all-zero colour means white.
 */
namespace DungeoneerSpriteCPD
{
	/** Sprite: extra warm glow (Exploder fuse, lantern pulse). 0 = none. Same slot as E25DCPDIndex::EmissivePulse. */
	inline constexpr int32 GlowPulse   = 5;
	/** Sprite: hit flash strength 0..1. Flash colour is FlashR/G/B; all zero means white. */
	inline constexpr int32 FlashAmount = 8;
	inline constexpr int32 FlashR      = 9;
	inline constexpr int32 FlashG      = 10;
	inline constexpr int32 FlashB      = 11;
	/** Sprite: dithered fade-out 0..1 (0 = fully drawn, 1 = gone). Used by afterimages, decoys and Slips. */
	inline constexpr int32 FadeOut     = 12;

	/** FX quads reuse slots 8-11: fade-out (0 = full brightness) and an RGB tint (all zero = white). */
	inline constexpr int32 FxFadeOut = 8;
	inline constexpr int32 FxR    = 9;
	inline constexpr int32 FxG    = 10;
	inline constexpr int32 FxB    = 11;
}

/**
 * Builds (in editor builds) and loads the two materials sprites and effects use:
 *
 * - M_Dungeoneer_Sprite: masked, two-sided, lit. Same texture parameters as the engine's
 *   M_Shader_SimpleMaskedTwoSided (Diffuse, OpacityMask, Normal, DiffuseColor, Roughness) plus
 *   hit flash, warm glow and a dithered fade, all driven by custom primitive data.
 * - M_Dungeoneer_FxAdditive: unlit, additive, two-sided. Emissive = Mask.rgb * Mask.a * Color *
 *   Intensity * per-primitive tint and fade. For slash arcs, beams, sparks, rings and blasts.
 *
 * Both are saved under /Game/Materials so packaged builds load them instead of building them.
 */
class DARKDUNGEONEER_API FDungeoneerSpriteMaterialFactory
{
public:
	static constexpr const TCHAR* SpritePackagePath = TEXT("/Game/Materials/M_Dungeoneer_Sprite");
	static constexpr const TCHAR* SpriteObjectPath  = TEXT("/Game/Materials/M_Dungeoneer_Sprite.M_Dungeoneer_Sprite");
	static constexpr const TCHAR* FxPackagePath     = TEXT("/Game/Materials/M_Dungeoneer_FxAdditive");
	static constexpr const TCHAR* FxObjectPath      = TEXT("/Game/Materials/M_Dungeoneer_FxAdditive.M_Dungeoneer_FxAdditive");

	/** Bump when the graph changes so editor builds rebuild the saved asset. */
	static constexpr float SpriteRevision = 3.0f;
	static constexpr float FxRevision = 2.0f;

	static UMaterialInterface* EnsureSpriteMaterial();
	static UMaterialInterface* EnsureFxAdditiveMaterial();
};
