#pragma once

/**
 * DungeonAcousticSubsystem.h
 *
 * Layered Procedural Acoustic & Occlusion Subsystem for the Living Ecosystem:
 *  - Registers ADungeoneerCharacter and AZombieBase walkers, binding OnWalkColumnChanged (zero allocs).
 *  - Layer A (Cols 1 & 3): resolves floor surface via UDungeonWaterSubsystem::ResolveSurface, selects
 *    deterministic variant Step_{Surface}_{Hash(Step) % 3}, plays via 12-voice one-shot pool, and
 *    broadcasts scaled noise radius (1.4x ShallowPuddle / CrystalShard, etc.) into SensoryWorld.
 *  - Layer B (Cols 0 & 2, Hero only): plays speed-scaled Gear_Cloak_{0..2} & alternate Gear_Chain_{0..2}.
 *  - Layer C (Per local player): smooth 0.2s cross-faded continuous loops for Wading Slosh & Flora Brush Chime.
 *  - Soundscape Pool (6 loops reassigned at 4 Hz) + 12-slot One-Shot pool (including ceiling drips pitched
 *    by room height, Echo-Crystal chord harmonics, and seeded distant unexplored groans every 18-40s).
 *  - Acoustic Occlusion Sweep (15 Hz, 4 round-robin ECC_Visibility traces/tick) computing LPF cutoff
 *    lerp(18000, 900, Occluded) * BiomeFogFactor.
 */

#include "CoreMinimal.h"
#include "Containers/StaticArray.h"
#include "Core/DungeoneerTypes.h"
#include "Dungeon/DungeonTypes.h"
#include "Ecosystem/DungeonEcosystemTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "DungeonAcousticSubsystem.generated.h"

class AActor;
class UAudioComponent;
class USoundWave;

UENUM(BlueprintType)
enum class EAcousticFoleyLayer : uint8
{
	LayerA_FootImpact = 0,
	LayerB_GearCloth  = 1,
	LayerC_LoopBed    = 2,
	EnvironmentalFx   = 3
};

/** Fixed walker entry avoiding heap delegates or map lookups on Tick. */
struct FAcousticWalkerEntry
{
	TWeakObjectPtr<AActor> Actor;
	uint32 StepCounter = 0;
	uint32 PassingCounter = 0;
	bool bIsHero = false;
	bool bBound = false;
};

/** Snapshot emitted whenever Layer A/B triggers so headless tests & UI can observe exact surface & layer math. */
struct FAcousticTriggerRecord
{
	FVector Location = FVector::ZeroVector;
	EDungeonSurfaceKind Surface = EDungeonSurfaceKind::DryStone;
	EAcousticFoleyLayer Layer = EAcousticFoleyLayer::LayerA_FootImpact;
	FName CueKey = NAME_None;
	float NoiseRadiusUU = 850.0f;
	float LowPassCutoffHz = 18000.0f;
	bool bWasHero = false;
};

UCLASS()
class DARKDUNGEONEER_API UDungeonAcousticSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static constexpr int32 MaxWalkers = 64;
	static constexpr int32 NumOneShotVoices = 16;
	static constexpr int32 NumSoundscapeLoops = 6;
	static constexpr int32 MaxLocalPlayerLoops = 4; // 2 players x (WadeSlosh + FloraChime) or 4 slots
	static constexpr int32 TotalVoicePoolSize = NumOneShotVoices + NumSoundscapeLoops + MaxLocalPlayerLoops;

	UDungeonAcousticSubsystem();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UDungeonAcousticSubsystem, STATGROUP_Tickables); }
	virtual bool IsTickable() const override { return !IsTemplate(); }

	/** Creates all 22 pre-allocated UAudioComponent voices once on PoolOwnerActor at floor build. */
	void OnFloorBuilt(AActor* PoolOwnerActor, EDungeonBiomeType DominantBiome, int32 FloorSeed, const FDungeonFloorGraph* InFloorGraph = nullptr);

	/** Registers hero or zombie walker (idempotent; binds OnWalkColumnChanged cleanly). */
	void RegisterWalker(AActor* WalkerActor);

	/** Returns true once walker is managed by AcousticSubsystem so legacy footstep cue suppresses double-firing. */
	bool IsWalkerRegistered(const AActor* WalkerActor) const noexcept;

	/** Deterministic monotonic LPF cutoff calculation: lerp(18000, 900, Occluded01) * Clamp(FogFactor, 0.25, 1.0). */
	static float ComputeOcclusionLowPassHz(float OccludedAlpha01, float BiomeFogFactor) noexcept;

	/** Sets ambient biome attenuation factor (SporeFens dampens highs more than MirrorPalace). */
	void SetBiomeAcousticFogFactor(EDungeonBiomeType Biome) noexcept;
	float GetBiomeAcousticFogFactor() const noexcept { return ActiveBiomeFogFactor; }

	/** Public deterministic step handler (also bound automatically to ADungeoneerCharacter/AZombieBase). */
	void NotifyWalkColumnChanged(AActor* WalkerActor, int32 NewColumn);

	/** Deterministic Echo-Crystal chord playback triggered by UDungeonFloraManagerComponent. */
	void HandleEchoCrystalChime(const FVector& CrystalPos, int32 ChordIndex);

	/** Deterministic ceiling drip audio handler triggered by UDungeonWaterSubsystem. */
	void HandleCeilingDripImpact(const FVector& ImpactLocation, float CeilingHeightUU, EWaterForm SourceForm);

	/** Steps 15Hz occlusion traces, 4Hz soundscape loop reassignment & 18-40s distant groans. */
	void StepAcousticSimulation(float DeltaSeconds);

	UFUNCTION(BlueprintPure, Category = "Audio|Acoustics")
	int32 GetPreallocatedVoiceCount() const noexcept { return PreallocatedVoiceCount; }

	UFUNCTION(BlueprintPure, Category = "Audio|Acoustics")
	int32 GetRegisteredWalkerCount() const noexcept { return RegisteredWalkerCount; }

	const FAcousticTriggerRecord& GetLastTriggerRecord() const noexcept { return LastTrigger; }
	int32 GetLayerAFireCount() const noexcept { return LayerAFireCount; }
	int32 GetLayerBFireCount() const noexcept { return LayerBFireCount; }
	int32 GetDripFireCount() const noexcept { return DripFireCount; }
	int32 GetCrystalChordFireCount() const noexcept { return CrystalChordFireCount; }

	/** Returns current smoothed occlusion LPF cutoff (Hz) for soundscape loop slot [0..5]. */
	float GetSoundscapeCutoffHz(int32 LoopIndex) const noexcept
	{
		return (LoopIndex >= 0 && LoopIndex < NumSoundscapeLoops) ? SoundscapeCutoffHz[LoopIndex] : 18000.0f;
	}

	/**
	 * Plays a combat sound (Content/Audio/Foley/Combat, keyed by file name) at a location, with a small
	 * pitch spread so repeated hits don't sound identical. Fired by gameplay events, never by timers.
	 */
	void PlayCombatCue(FName CueKey, const FVector& WorldLocation, float Volume = 1.0f, float PitchSpread = 0.06f);

	/** Static helper: plays the cue if World has an acoustic subsystem (headless tests may not). */
	static void PlayCombatCueAt(const UWorld* World, FName CueKey, const FVector& WorldLocation, float Volume = 1.0f, float PitchSpread = 0.06f);

	int32 GetCombatCueCount() const noexcept { return static_cast<int32>(CombatCueCounter); }

private:
	USoundWave* ResolveFoleyWave(FName CueKey) const noexcept;
	void PlayOneShotWave(FName CueKey, const FVector& WorldLocation, float Volume, float Pitch, float LowPassHz);
	float MeasurePointOcclusion01(const FVector& ListenerPos, const FVector& SourcePos) const noexcept;
	static FName GetStepBankKey(EDungeonSurfaceKind Surface, uint32 VariantIndex) noexcept;

	uint32 CombatCueCounter = 0;
	TStaticArray<FAcousticWalkerEntry, MaxWalkers> Walkers;
	int32 RegisteredWalkerCount = 0;

	TStaticArray<TObjectPtr<UAudioComponent>, NumOneShotVoices> OneShotVoices;
	TStaticArray<TObjectPtr<UAudioComponent>, NumSoundscapeLoops> SoundscapeLoops;
	TStaticArray<TObjectPtr<UAudioComponent>, MaxLocalPlayerLoops> PlayerContinuousLoops; // [2*p + 0]=Wade, [2*p + 1]=Flora

	TStaticArray<FVector, NumSoundscapeLoops> SoundscapePositions;
	TStaticArray<float, NumSoundscapeLoops> SoundscapeTargetGain;
	TStaticArray<float, NumSoundscapeLoops> SoundscapeCurrentGain;
	TStaticArray<float, NumSoundscapeLoops> SoundscapeCutoffHz;
	TStaticArray<uint8, NumSoundscapeLoops> SoundscapeOccluded;

	TStaticArray<float, MaxLocalPlayerLoops> PlayerLoopCurrentGain;

	// Seeded far-room distant groan schedule (no FTimerHandle)
	FVector DistantGroanOrigin = FVector::ZeroVector;
	bool bHasDistantGroanOrigin = false;
	float GroanCooldownSec = 24.0f;
	uint32 GroanRandState = 1337u;

	int32 NextOneShotSlot = 0;
	int32 OcclusionSweepCursor = 0;
	int32 PreallocatedVoiceCount = 0;
	float SimAccum15HzSec = 0.0f;
	float SoundscapeAccum4HzSec = 0.0f;
	float ActiveBiomeFogFactor = 0.90f;

	TWeakObjectPtr<AActor> BoundPoolOwner;
	TWeakObjectPtr<class UDungeoneerAssetBootstrapSubsystem> CachedBootstrap;
	TWeakObjectPtr<class UDungeonFloraManagerComponent> CachedFloraMgr;

	FAcousticTriggerRecord LastTrigger;
	int32 LayerAFireCount = 0;
	int32 LayerBFireCount = 0;
	int32 DripFireCount = 0;
	int32 CrystalChordFireCount = 0;
};
