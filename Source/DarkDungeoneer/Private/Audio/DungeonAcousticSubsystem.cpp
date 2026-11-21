#include "Audio/DungeonAcousticSubsystem.h"

#include "AI/ZombieBase.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Components/AudioComponent.h"
#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerMovementComponent.h"
#include "Dungeon/DungeoneerTileMapInstancer.h"
#include "Ecosystem/DungeonFaunaSubsystem.h"
#include "Ecosystem/DungeonFloraManagerComponent.h"
#include "Ecosystem/DungeonWaterSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "Sound/SoundWave.h"

namespace AcousticDetail
{
	inline constexpr float Tick15HzIntervalSec = 1.0f / 15.0f;
	inline constexpr float Tick4HzIntervalSec  = 0.25f;
	inline constexpr float MaxCutoffHz = 18000.0f;
	inline constexpr float OccludedCutoffHz = 900.0f;
	inline constexpr float BaseStepNoiseRadiusUU = 850.0f;
	template <typename TArrayType, typename TValueType>
	void FillStatic(TArrayType& Arr, const TValueType& Val) noexcept
	{
		for (auto& Elem : Arr)
		{
			Elem = Val;
		}
	}
}

UDungeonAcousticSubsystem::UDungeonAcousticSubsystem()
{
	AcousticDetail::FillStatic(SoundscapePositions, FVector::ZeroVector);
	AcousticDetail::FillStatic(SoundscapeTargetGain, 0.0f);
	AcousticDetail::FillStatic(SoundscapeCurrentGain, 0.0f);
	AcousticDetail::FillStatic(SoundscapeCutoffHz, AcousticDetail::MaxCutoffHz);
	AcousticDetail::FillStatic(SoundscapeOccluded, static_cast<uint8>(0));
	AcousticDetail::FillStatic(PlayerLoopCurrentGain, 0.0f);
}

void UDungeonAcousticSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RegisteredWalkerCount = 0;
	PreallocatedVoiceCount = 0;
	NextOneShotSlot = 0;
	OcclusionSweepCursor = 0;
	SimAccum15HzSec = 0.0f;
	SoundscapeAccum4HzSec = 0.0f;
	LayerAFireCount = 0;
	LayerBFireCount = 0;
	DripFireCount = 0;
	CrystalChordFireCount = 0;
	AcousticDetail::FillStatic(SoundscapeCutoffHz, AcousticDetail::MaxCutoffHz);
	AcousticDetail::FillStatic(SoundscapeOccluded, static_cast<uint8>(0));
	AcousticDetail::FillStatic(PlayerLoopCurrentGain, 0.0f);
}

void UDungeonAcousticSubsystem::Deinitialize()
{
	if (const UWorld* World = GetWorld())
	{
		if (UDungeonWaterSubsystem* WaterSub = World->GetSubsystem<UDungeonWaterSubsystem>())
		{
			WaterSub->OnCeilingDripImpact.RemoveAll(this);
		}
	}
	if (UDungeonFloraManagerComponent* FloraMgr = CachedFloraMgr.Get())
	{
		FloraMgr->OnEchoCrystalChime.RemoveAll(this);
	}
	CachedFloraMgr.Reset();
	CachedBootstrap.Reset();
	BoundPoolOwner.Reset();
	Super::Deinitialize();
}

void UDungeonAcousticSubsystem::SetBiomeAcousticFogFactor(const EDungeonBiomeType Biome) noexcept
{
	switch (Biome)
	{
	case EDungeonBiomeType::SporeFens:
		ActiveBiomeFogFactor = 0.72f; // dense spore air absorbs high frequencies
		break;
	case EDungeonBiomeType::EmberForge:
		ActiveBiomeFogFactor = 0.85f;
		break;
	case EDungeonBiomeType::MirrorPalace:
		ActiveBiomeFogFactor = 1.00f; // crisp glassy reflections
		break;
	case EDungeonBiomeType::Crypt:
	default:
		ActiveBiomeFogFactor = 0.90f;
		break;
	}
}

float UDungeonAcousticSubsystem::ComputeOcclusionLowPassHz(const float OccludedAlpha01, const float BiomeFogFactor) noexcept
{
	const float ClampedOcc = FMath::Clamp(OccludedAlpha01, 0.0f, 1.0f);
	const float ClampedFog = FMath::Clamp(BiomeFogFactor, 0.25f, 1.0f);
	return FMath::Lerp(AcousticDetail::MaxCutoffHz, AcousticDetail::OccludedCutoffHz, ClampedOcc) * ClampedFog;
}

void UDungeonAcousticSubsystem::OnFloorBuilt(
	AActor* PoolOwnerActor,
	const EDungeonBiomeType DominantBiome,
	const int32 FloorSeed,
	const FDungeonFloorGraph* InFloorGraph)
{
	SetBiomeAcousticFogFactor(DominantBiome);
	GroanRandState = static_cast<uint32>(FloorSeed ^ 0xA0571C45);
	GroanCooldownSec = 18.0f + static_cast<float>(GroanRandState % 220u) * 0.1f;

	// Choose farthest unexplored room as the distant groan anchor
	bHasDistantGroanOrigin = false;
	if (InFloorGraph && InFloorGraph->Nodes.Num() > 1)
	{
		const FVector EntryCenter = InFloorGraph->Nodes[0].WorldCenter;
		double FarthestSq = -1.0;
		for (int32 i = 1; i < InFloorGraph->Nodes.Num(); ++i)
		{
			const double DistSq = FVector::DistSquared2D(EntryCenter, InFloorGraph->Nodes[i].WorldCenter);
			if (DistSq > FarthestSq)
			{
				FarthestSq = DistSq;
				DistantGroanOrigin = InFloorGraph->Nodes[i].WorldCenter;
				bHasDistantGroanOrigin = true;
			}
		}
	}

	UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UDungeoneerAssetBootstrapSubsystem* Bootstrap = GI ? GI->GetSubsystem<UDungeoneerAssetBootstrapSubsystem>() : nullptr;
	CachedBootstrap = Bootstrap;
	if (Bootstrap)
	{
		Bootstrap->ImportAllFoleyFromDisk();
	}

	if (UDungeonWaterSubsystem* WaterSub = World ? World->GetSubsystem<UDungeonWaterSubsystem>() : nullptr)
	{
		WaterSub->OnCeilingDripImpact.RemoveAll(this);
		WaterSub->OnCeilingDripImpact.AddUObject(this, &UDungeonAcousticSubsystem::HandleCeilingDripImpact);
	}

	if (const ADungeoneerTileMapInstancer* Instancer = Cast<ADungeoneerTileMapInstancer>(PoolOwnerActor))
	{
		if (UDungeonFloraManagerComponent* FloraMgr = Instancer->GetFloraManagerComponent())
		{
			if (UDungeonFloraManagerComponent* PrevFlora = CachedFloraMgr.Get())
			{
				PrevFlora->OnEchoCrystalChime.RemoveAll(this);
			}
			CachedFloraMgr = FloraMgr;
			FloraMgr->OnEchoCrystalChime.RemoveAll(this);
			FloraMgr->OnEchoCrystalChime.AddUObject(this, &UDungeonAcousticSubsystem::HandleEchoCrystalChime);
		}
	}

	if (PoolOwnerActor && IsValid(PoolOwnerActor) && PoolOwnerActor->GetRootComponent())
	{
		BoundPoolOwner = PoolOwnerActor;

		auto CreatePooledVoice = [PoolOwnerActor](TObjectPtr<UAudioComponent>& OutSlot, const bool bSpatialize)
		{
			if (IsValid(OutSlot))
			{
				return true;
			}
			UAudioComponent* Comp = NewObject<UAudioComponent>(PoolOwnerActor);
			if (!Comp)
			{
				return false;
			}
			Comp->SetupAttachment(PoolOwnerActor->GetRootComponent());
			Comp->bAutoActivate = false;
			Comp->bAllowSpatialization = bSpatialize;
			Comp->bIsUISound = false;
			Comp->SetLowPassFilterEnabled(true);
			Comp->SetLowPassFilterFrequency(AcousticDetail::MaxCutoffHz);
			Comp->SetVolumeMultiplier(0.0f);
			Comp->RegisterComponent();
			OutSlot = Comp;
			return true;
		};

		int32 Count = 0;
		for (int32 i = 0; i < NumOneShotVoices; ++i)
		{
			if (CreatePooledVoice(OneShotVoices[i], true))
			{
				++Count;
			}
		}
		for (int32 i = 0; i < NumSoundscapeLoops; ++i)
		{
			if (CreatePooledVoice(SoundscapeLoops[i], true))
			{
				++Count;
			}
		}
		for (int32 i = 0; i < MaxLocalPlayerLoops; ++i)
		{
			if (CreatePooledVoice(PlayerContinuousLoops[i], false))
			{
				++Count;
			}
		}
		PreallocatedVoiceCount = Count;
	}
}

void UDungeonAcousticSubsystem::RegisterWalker(AActor* WalkerActor)
{
	if (!WalkerActor || !IsValid(WalkerActor))
	{
		return;
	}

	for (int32 i = 0; i < RegisteredWalkerCount; ++i)
	{
		if (Walkers[i].Actor.Get() == WalkerActor)
		{
			return;
		}
	}

	// Reclaim dead slots first if near capacity
	int32 TargetSlot = INDEX_NONE;
	for (int32 i = 0; i < RegisteredWalkerCount; ++i)
	{
		if (!Walkers[i].Actor.IsValid())
		{
			TargetSlot = i;
			break;
		}
	}
	if (TargetSlot == INDEX_NONE)
	{
		if (RegisteredWalkerCount >= MaxWalkers)
		{
			return;
		}
		TargetSlot = RegisteredWalkerCount++;
	}

	FAcousticWalkerEntry& Entry = Walkers[TargetSlot];
	Entry.Actor = WalkerActor;
	Entry.StepCounter = 0;
	Entry.PassingCounter = 0;
	Entry.bBound = false;

	if (ADungeoneerCharacter* Hero = Cast<ADungeoneerCharacter>(WalkerActor))
	{
		Entry.bIsHero = true;
		Hero->OnWalkColumnChanged.AddWeakLambda(this, [this, WeakActor = TWeakObjectPtr<AActor>(WalkerActor)](const int32 NewCol)
		{
			if (AActor* LiveActor = WeakActor.Get())
			{
				NotifyWalkColumnChanged(LiveActor, NewCol);
			}
		});
		Entry.bBound = true;
	}
	else if (AZombieBase* Zombie = Cast<AZombieBase>(WalkerActor))
	{
		Entry.bIsHero = false;
		Zombie->OnWalkColumnChanged.AddWeakLambda(this, [this, WeakActor = TWeakObjectPtr<AActor>(WalkerActor)](const int32 NewCol)
		{
			if (AActor* LiveActor = WeakActor.Get())
			{
				NotifyWalkColumnChanged(LiveActor, NewCol);
			}
		});
		Entry.bBound = true;
	}
}

bool UDungeonAcousticSubsystem::IsWalkerRegistered(const AActor* WalkerActor) const noexcept
{
	if (!WalkerActor)
	{
		return false;
	}
	for (int32 i = 0; i < RegisteredWalkerCount; ++i)
	{
		if (Walkers[i].Actor.Get() == WalkerActor && Walkers[i].bBound)
		{
			return true;
		}
	}
	return false;
}

FName UDungeonAcousticSubsystem::GetStepBankKey(const EDungeonSurfaceKind Surface, const uint32 VariantIndex) noexcept
{
	const uint32 SafeSurfaceIdx = static_cast<uint32>(Surface) < static_cast<uint32>(EDungeonSurfaceKind::Count)
		? static_cast<uint32>(Surface)
		: 0u;
	const uint32 Var = VariantIndex % 3u;
	static const FName StepNames[static_cast<int32>(EDungeonSurfaceKind::Count)][3] =
	{
		{ FName(TEXT("Step_Stone_0")),    FName(TEXT("Step_Stone_1")),    FName(TEXT("Step_Stone_2")) },
		{ FName(TEXT("Step_Puddle_0")),   FName(TEXT("Step_Puddle_1")),   FName(TEXT("Step_Puddle_2")) },
		{ FName(TEXT("Step_Wade_0")),     FName(TEXT("Step_Wade_1")),     FName(TEXT("Step_Wade_2")) },
		{ FName(TEXT("Step_Moss_0")),     FName(TEXT("Step_Moss_1")),     FName(TEXT("Step_Moss_2")) },
		{ FName(TEXT("Step_Crystal_0")),  FName(TEXT("Step_Crystal_1")),  FName(TEXT("Step_Crystal_2")) },
		{ FName(TEXT("Step_Rug_0")),      FName(TEXT("Step_Rug_1")),      FName(TEXT("Step_Rug_2")) },
		{ FName(TEXT("Step_Obsidian_0")), FName(TEXT("Step_Obsidian_1")), FName(TEXT("Step_Obsidian_2")) }
	};
	return StepNames[SafeSurfaceIdx][Var];
}

USoundWave* UDungeonAcousticSubsystem::ResolveFoleyWave(const FName CueKey) const noexcept
{
	if (UDungeoneerAssetBootstrapSubsystem* Bootstrap = CachedBootstrap.Get())
	{
		return Bootstrap->GetLoadedFoley(CueKey);
	}
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			if (UDungeoneerAssetBootstrapSubsystem* Bootstrap = GI->GetSubsystem<UDungeoneerAssetBootstrapSubsystem>())
			{
				return Bootstrap->GetLoadedFoley(CueKey);
			}
		}
	}
	return nullptr;
}

float UDungeonAcousticSubsystem::MeasurePointOcclusion01(const FVector& ListenerPos, const FVector& SourcePos) const noexcept
{
	const UWorld* World = GetWorld();
	if (!World || FVector::DistSquared(ListenerPos, SourcePos) < 40.0 * 40.0)
	{
		return 0.0f;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DungeonAcousticOcclusion), false);
	if (World->LineTraceSingleByChannel(Hit, ListenerPos + FVector(0.0, 0.0, 48.0), SourcePos + FVector(0.0, 0.0, 28.0), ECC_Visibility, Params))
	{
		return 1.0f;
	}
	return 0.0f;
}

void UDungeonAcousticSubsystem::PlayOneShotWave(
	const FName CueKey,
	const FVector& WorldLocation,
	const float Volume,
	const float Pitch,
	const float LowPassHz)
{
	if (NumOneShotVoices <= 0)
	{
		return;
	}

	UAudioComponent* Voice = OneShotVoices[NextOneShotSlot].Get();
	NextOneShotSlot = (NextOneShotSlot + 1) % NumOneShotVoices;
	if (!Voice)
	{
		return;
	}

	USoundWave* Wave = ResolveFoleyWave(CueKey);
	if (!Wave)
	{
		return;
	}

	Voice->SetWorldLocation(WorldLocation);
	Voice->SetSound(Wave);
	Voice->SetVolumeMultiplier(FMath::Clamp(Volume, 0.0f, 1.5f));
	Voice->SetPitchMultiplier(FMath::Clamp(Pitch, 0.5f, 1.8f));
	Voice->SetLowPassFilterEnabled(true);
	Voice->SetLowPassFilterFrequency(FMath::Clamp(LowPassHz, 400.0f, 20000.0f));
	Voice->Play(0.0f);
}

void UDungeonAcousticSubsystem::PlayCombatCue(const FName CueKey, const FVector& WorldLocation, const float Volume, const float PitchSpread)
{
	++CombatCueCounter;
	// Spread pitch with a hash of the counter: deterministic, no RNG state.
	uint32 H = CombatCueCounter * 0x9E3779B1u;
	H ^= H >> 15;
	const float Unit = static_cast<float>(H & 0xFFFFu) / 65535.0f;
	PlayOneShotWave(CueKey, WorldLocation, Volume, 1.0f + PitchSpread * (Unit * 2.0f - 1.0f), 20000.0f);
}

void UDungeonAcousticSubsystem::PlayCombatCueAt(const UWorld* World, const FName CueKey, const FVector& WorldLocation, const float Volume, const float PitchSpread)
{
	if (UDungeonAcousticSubsystem* Audio = World ? World->GetSubsystem<UDungeonAcousticSubsystem>() : nullptr)
	{
		Audio->PlayCombatCue(CueKey, WorldLocation, Volume, PitchSpread);
	}
}

void UDungeonAcousticSubsystem::NotifyWalkColumnChanged(AActor* WalkerActor, const int32 NewColumn)
{
	if (!WalkerActor)
	{
		return;
	}

	FAcousticWalkerEntry* WalkerEntry = nullptr;
	for (int32 i = 0; i < RegisteredWalkerCount; ++i)
	{
		if (Walkers[i].Actor.Get() == WalkerActor)
		{
			WalkerEntry = &Walkers[i];
			break;
		}
	}
	if (!WalkerEntry)
	{
		RegisterWalker(WalkerActor);
		for (int32 i = 0; i < RegisteredWalkerCount; ++i)
		{
			if (Walkers[i].Actor.Get() == WalkerActor)
			{
				WalkerEntry = &Walkers[i];
				break;
			}
		}
		if (!WalkerEntry)
		{
			return;
		}
	}

	const UWorld* World = GetWorld();
	const UDungeonWaterSubsystem* WaterSub = World ? World->GetSubsystem<UDungeonWaterSubsystem>() : nullptr;
	UDungeoneerSensoryWorldSubsystem* SensoryWorld = World ? World->GetSubsystem<UDungeoneerSensoryWorldSubsystem>() : nullptr;
	const UZombieSpatialSubsystem* SpatialSys = World ? World->GetSubsystem<UZombieSpatialSubsystem>() : nullptr;

	const FVector Pos = WalkerActor->GetActorLocation();
	const float Speed = static_cast<float>(WalkerActor->GetVelocity().Size2D());

	// Measure occlusion from nearest local listener
	float Occlusion01 = 0.0f;
	if (SpatialSys && SpatialSys->GetTrackedPlayerCount() > 0 && !WalkerEntry->bIsHero)
	{
		if (const ADungeoneerCharacter* Listener = SpatialSys->GetTrackedPlayerAt(0))
		{
			Occlusion01 = MeasurePointOcclusion01(Listener->GetActorLocation(), Pos);
		}
	}
	const float CutoffHz = ComputeOcclusionLowPassHz(Occlusion01, ActiveBiomeFogFactor);

	// Layer A: Foot contact on columns 1 (Left) and 3 (Right)
	if (NewColumn == 1 || NewColumn == 3)
	{
		const uint32 StepSeq = ++WalkerEntry->StepCounter;
		const EDungeonSurfaceKind Surface = WaterSub ? WaterSub->ResolveSurface(Pos) : EDungeonSurfaceKind::DryStone;
		const int32 SurfaceIdx = FMath::Clamp(static_cast<int32>(Surface), 0, static_cast<int32>(EDungeonSurfaceKind::Count) - 1);
		const FSurfaceAcoustics& SurfMeta = EcoSurfaceTable[SurfaceIdx];

		// Deterministic hash-selected variant (Step_{Surface}_{0..2})
		const uint32 HashVal = (StepSeq * 2654435761u) ^ static_cast<uint32>(SurfaceIdx * 97u);
		const FName CueKey = GetStepBankKey(Surface, HashVal % 3u);
		const float Loudness = WalkerEntry->bIsHero ? 1.0f : 0.72f;
		const float PitchJitter = 0.96f + 0.08f * static_cast<float>(HashVal & 1u);
		const float NoiseRadius = AcousticDetail::BaseStepNoiseRadiusUU * SurfMeta.NoiseRadiusMultiplier;

		PlayOneShotWave(CueKey, Pos, Loudness * 0.65f, PitchJitter, CutoffHz);

		if (SensoryWorld)
		{
			SensoryWorld->BroadcastNoiseEvent(Pos, Loudness, NoiseRadius, false);
		}

		LastTrigger.Location = Pos;
		LastTrigger.Surface = Surface;
		LastTrigger.Layer = EAcousticFoleyLayer::LayerA_FootImpact;
		LastTrigger.CueKey = CueKey;
		LastTrigger.NoiseRadiusUU = NoiseRadius;
		LastTrigger.LowPassCutoffHz = CutoffHz;
		LastTrigger.bWasHero = WalkerEntry->bIsHero;
		++LayerAFireCount;
	}
	// Layer B: Passing pose cloth & lantern chain micro-foley on columns 0 and 2 (Hero only)
	else if (WalkerEntry->bIsHero && (NewColumn == 0 || NewColumn == 2))
	{
		const uint32 PassSeq = ++WalkerEntry->PassingCounter;
		const float CloakVol = 0.35f * FMath::Clamp(Speed / 450.0f, 0.15f, 1.15f);
		static const FName CloakKeys[3] = { FName(TEXT("Gear_Cloak_0")), FName(TEXT("Gear_Cloak_1")), FName(TEXT("Gear_Cloak_2")) };
		static const FName ChainKeys[3] = { FName(TEXT("Gear_Chain_0")), FName(TEXT("Gear_Chain_1")), FName(TEXT("Gear_Chain_2")) };

		const FName CloakKey = CloakKeys[PassSeq % 3u];
		PlayOneShotWave(CloakKey, Pos, CloakVol, 0.97f + 0.06f * static_cast<float>(PassSeq & 1u), CutoffHz);

		// Lantern chain clink on every second passing pose
		if ((PassSeq & 1u) == 0u)
		{
			const FName ChainKey = ChainKeys[(PassSeq >> 1) % 3u];
			PlayOneShotWave(ChainKey, Pos, CloakVol * 0.78f, 1.0f, CutoffHz);
		}

		LastTrigger.Location = Pos;
		LastTrigger.Surface = WaterSub ? WaterSub->ResolveSurface(Pos) : EDungeonSurfaceKind::DryStone;
		LastTrigger.Layer = EAcousticFoleyLayer::LayerB_GearCloth;
		LastTrigger.CueKey = CloakKey;
		LastTrigger.NoiseRadiusUU = 0.0f;
		LastTrigger.LowPassCutoffHz = CutoffHz;
		LastTrigger.bWasHero = true;
		++LayerBFireCount;
	}
}

void UDungeonAcousticSubsystem::HandleEchoCrystalChime(const FVector& CrystalPos, const int32 ChordIndex)
{
	static constexpr float PentatonicPitch[5] = { 1.0f, 1.122f, 1.260f, 1.498f, 1.682f };
	const float Pitch = PentatonicPitch[FMath::Abs(ChordIndex) % 5];
	const FName ChordKey = (ChordIndex & 1) ? FName(TEXT("Echo_CrystalChord_1")) : FName(TEXT("Echo_CrystalChord_0"));

	float Occ01 = 0.0f;
	if (const UWorld* World = GetWorld())
	{
		if (const UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
		{
			if (const ADungeoneerCharacter* Listener = SpatialSys->GetTrackedPlayerAt(0))
			{
				Occ01 = MeasurePointOcclusion01(Listener->GetActorLocation(), CrystalPos);
			}
		}
	}
	PlayOneShotWave(ChordKey, CrystalPos, 0.55f, Pitch, ComputeOcclusionLowPassHz(Occ01, ActiveBiomeFogFactor));
	++CrystalChordFireCount;
}

void UDungeonAcousticSubsystem::HandleCeilingDripImpact(const FVector& ImpactLocation, const float CeilingHeightUU, const EWaterForm /*SourceForm*/)
{
	static const FName DripKeys[3] = { FName(TEXT("Water_Drip_0")), FName(TEXT("Water_Drip_1")), FName(TEXT("Water_Drip_2")) };
	const uint32 Idx = static_cast<uint32>(DripFireCount) % 3u;
	const float Pitch = FMath::Clamp(260.0f / FMath::Max(CeilingHeightUU, 120.0f), 0.75f, 1.38f);

	float Occ01 = 0.0f;
	if (const UWorld* World = GetWorld())
	{
		if (const UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
		{
			if (const ADungeoneerCharacter* Listener = SpatialSys->GetTrackedPlayerAt(0))
			{
				Occ01 = MeasurePointOcclusion01(Listener->GetActorLocation(), ImpactLocation);
			}
		}
	}
	PlayOneShotWave(DripKeys[Idx], ImpactLocation, 0.38f, Pitch, ComputeOcclusionLowPassHz(Occ01, ActiveBiomeFogFactor));
	++DripFireCount;
}

void UDungeonAcousticSubsystem::StepAcousticSimulation(const float DeltaSeconds)
{
	const UWorld* World = GetWorld();
	const UZombieSpatialSubsystem* SpatialSys = World ? World->GetSubsystem<UZombieSpatialSubsystem>() : nullptr;
	const UDungeonWaterSubsystem* WaterSub = World ? World->GetSubsystem<UDungeonWaterSubsystem>() : nullptr;
	const UDungeonFaunaSubsystem* FaunaSub = World ? World->GetSubsystem<UDungeonFaunaSubsystem>() : nullptr;
	const UDungeonFloraManagerComponent* FloraMgr = CachedFloraMgr.Get();

	FVector ListenerPos = FVector::ZeroVector;
	if (SpatialSys && SpatialSys->GetTrackedPlayerCount() > 0)
	{
		if (const ADungeoneerCharacter* Hero0 = SpatialSys->GetTrackedPlayerAt(0))
		{
			ListenerPos = Hero0->GetActorLocation();
		}
	}

	// 1. Layer C continuous loops per local player (smoothed over 0.2s)
	const int32 TrackedPlayers = SpatialSys ? FMath::Min(SpatialSys->GetTrackedPlayerCount(), 2) : 0;
	for (int32 p = 0; p < 2; ++p)
	{
		const int32 WadeSlot = p * 2 + 0;
		const int32 FloraSlot = p * 2 + 1;
		float TargetWadeGain = 0.0f;
		float TargetFloraGain = 0.0f;

		if (p < TrackedPlayers && SpatialSys)
		{
			if (const ADungeoneerCharacter* Player = SpatialSys->GetTrackedPlayerAt(p))
			{
				const FVector PPos = Player->GetActorLocation();
				const float Speed01 = FMath::Clamp(static_cast<float>(Player->GetVelocity().Size2D()) / 450.0f, 0.0f, 1.0f);
				const bool bWading = WaterSub && WaterSub->IsWadingLocation(PPos);
				TargetWadeGain = bWading ? (0.48f * Speed01) : 0.0f;

				const bool bNearFlora = FloraMgr && FloraMgr->IsPositionNearAnyFlora(PPos, 110.0f);
				TargetFloraGain = bNearFlora ? (0.28f * (0.25f + 0.75f * Speed01)) : 0.0f;
			}
		}

		auto UpdateContinuousSlot = [this, DeltaSeconds](const int32 SlotIdx, const FName LoopKey, const float TargetGain)
		{
			PlayerLoopCurrentGain[SlotIdx] = FMath::FInterpTo(PlayerLoopCurrentGain[SlotIdx], TargetGain, DeltaSeconds, 5.0f);
			if (UAudioComponent* Comp = PlayerContinuousLoops[SlotIdx].Get())
			{
				if (PlayerLoopCurrentGain[SlotIdx] > 0.01f)
				{
					if (!Comp->IsPlaying())
					{
						if (USoundWave* Wave = ResolveFoleyWave(LoopKey))
						{
							Comp->SetSound(Wave);
							Comp->Play(0.0f);
						}
					}
					Comp->SetVolumeMultiplier(PlayerLoopCurrentGain[SlotIdx]);
				}
				else
				{
					Comp->SetVolumeMultiplier(0.0f);
					if (Comp->IsPlaying())
					{
						Comp->Stop();
					}
				}
			}
		};

		UpdateContinuousSlot(WadeSlot,  FName(TEXT("Loop_WadeSlosh")), TargetWadeGain);
		UpdateContinuousSlot(FloraSlot, FName(TEXT("Loop_FloraChime")), TargetFloraGain);
	}

	// 2. 4 Hz Soundscape loop reassignment (Water emitters + Echo-Crystal hum + Fauna flutter/clicks)
	SoundscapeAccum4HzSec += DeltaSeconds;
	if (SoundscapeAccum4HzSec >= AcousticDetail::Tick4HzIntervalSec)
	{
		SoundscapeAccum4HzSec = 0.0f;
		// Slots 0..3: closest water emitters
		for (int32 s = 0; s < 4; ++s)
		{
			SoundscapeTargetGain[s] = 0.0f;
		}
		if (WaterSub)
		{
			const TArray<FWaterAudioEmitterRef>& Emitters = WaterSub->GetWaterAudioEmitters();
			// Pick up to 4 closest unique water emitters without heap allocation
			TStaticArray<int32, 4> UsedIndices;
			AcousticDetail::FillStatic(UsedIndices, INDEX_NONE);
			for (int32 Slot = 0; Slot < 4; ++Slot)
			{
				double BestDistSq = 1200.0 * 1200.0;
				int32 BestIdx = INDEX_NONE;
				for (int32 e = 0; e < Emitters.Num(); ++e)
				{
					bool bAlready = false;
					for (int32 k = 0; k < Slot; ++k)
					{
						if (UsedIndices[k] == e) { bAlready = true; break; }
					}
					if (bAlready) { continue; }
					const double DSq = FVector::DistSquared2D(ListenerPos, Emitters[e].WorldPos);
					if (DSq < BestDistSq)
					{
						BestDistSq = DSq;
						BestIdx = e;
					}
				}
				if (BestIdx != INDEX_NONE)
				{
					UsedIndices[Slot] = BestIdx;
					SoundscapePositions[Slot] = Emitters[BestIdx].WorldPos;
					const FName LoopKey = (Emitters[BestIdx].Form == EWaterForm::WeepingCascade) ? FName(TEXT("Water_Loop_WallCascade"))
						: (Emitters[BestIdx].Form == EWaterForm::BioThermalGeyser) ? FName(TEXT("Water_Loop_ThermalBubble"))
						: (Emitters[BestIdx].Form == EWaterForm::WadingStream) ? FName(TEXT("Water_Loop_StreamRush"))
						: FName(TEXT("Water_Loop_PuddleLap"));
					SoundscapeTargetGain[Slot] = 0.22f * (1.0f - static_cast<float>(FMath::Sqrt(BestDistSq)) / 1200.0f);
					if (UAudioComponent* Comp = SoundscapeLoops[Slot].Get())
					{
						Comp->SetWorldLocation(SoundscapePositions[Slot]);
						if (SoundscapeTargetGain[Slot] > 0.01f && !Comp->IsPlaying())
						{
							if (USoundWave* Wave = ResolveFoleyWave(LoopKey))
							{
								Comp->SetSound(Wave);
								Comp->Play(0.0f);
							}
						}
					}
				}
			}
		}

		// Slot 4: Nearest Echo-Crystal resonance hum
		FVector CrystalPos = FVector::ZeroVector;
		if (FloraMgr && FloraMgr->FindNearestEchoCrystal(ListenerPos, CrystalPos))
		{
			const double Dist = FVector::Dist2D(ListenerPos, CrystalPos);
			SoundscapePositions[4] = CrystalPos;
			SoundscapeTargetGain[4] = (Dist < 900.0) ? static_cast<float>(0.18 * (1.0 - Dist / 900.0)) : 0.0f;
			if (UAudioComponent* Comp = SoundscapeLoops[4].Get())
			{
				Comp->SetWorldLocation(CrystalPos);
				if (SoundscapeTargetGain[4] > 0.01f && !Comp->IsPlaying())
				{
					if (USoundWave* Wave = ResolveFoleyWave(FName(TEXT("Bio_Loop_CrystalHum"))))
					{
						Comp->SetSound(Wave);
						Comp->Play(0.0f);
					}
				}
			}
		}
		else
		{
			SoundscapeTargetGain[4] = 0.0f;
		}

		// Slot 5: Nearest bio emitter (Lumen-Moth flutter)
		FVector BioPos = FVector::ZeroVector;
		EFaunaSpecies BioSpec = EFaunaSpecies::LumenMoth;
		if (FaunaSub && FaunaSub->FindNearestAudibleBioEmitter(ListenerPos, BioPos, BioSpec))
		{
			const double Dist = FVector::Dist2D(ListenerPos, BioPos);
			SoundscapePositions[5] = BioPos;
			SoundscapeTargetGain[5] = (Dist < 650.0) ? static_cast<float>(0.16 * (1.0 - Dist / 650.0)) : 0.0f;
			if (UAudioComponent* Comp = SoundscapeLoops[5].Get())
			{
				Comp->SetWorldLocation(BioPos);
				if (SoundscapeTargetGain[5] > 0.01f && !Comp->IsPlaying())
				{
					if (USoundWave* Wave = ResolveFoleyWave(FName(TEXT("Bio_Loop_FireflyFlutter"))))
					{
						Comp->SetSound(Wave);
						Comp->Play(0.0f);
					}
				}
			}
		}
		else
		{
			SoundscapeTargetGain[5] = 0.0f;
		}
	}

	// 3. Round-robin 4 occlusion line traces per tick across SoundscapeLoops (zero allocs)
	for (int32 t = 0; t < 4 && NumSoundscapeLoops > 0; ++t)
	{
		const int32 Slot = OcclusionSweepCursor % NumSoundscapeLoops;
		OcclusionSweepCursor = (OcclusionSweepCursor + 1) % NumSoundscapeLoops;
		if (SoundscapeTargetGain[Slot] > 0.01f)
		{
			SoundscapeOccluded[Slot] = (MeasurePointOcclusion01(ListenerPos, SoundscapePositions[Slot]) > 0.5f) ? 1 : 0;
		}
		else
		{
			SoundscapeOccluded[Slot] = 0;
		}
	}

	for (int32 Slot = 0; Slot < NumSoundscapeLoops; ++Slot)
	{
		SoundscapeCurrentGain[Slot] = FMath::FInterpTo(SoundscapeCurrentGain[Slot], SoundscapeTargetGain[Slot], DeltaSeconds, 4.0f);
		const float TargetHz = ComputeOcclusionLowPassHz(static_cast<float>(SoundscapeOccluded[Slot]), ActiveBiomeFogFactor);
		SoundscapeCutoffHz[Slot] = FMath::FInterpTo(SoundscapeCutoffHz[Slot], TargetHz, DeltaSeconds, 10.0f);

		if (UAudioComponent* Comp = SoundscapeLoops[Slot].Get())
		{
			Comp->SetVolumeMultiplier(SoundscapeCurrentGain[Slot]);
			Comp->SetLowPassFilterEnabled(true);
			Comp->SetLowPassFilterFrequency(SoundscapeCutoffHz[Slot]);
			if (SoundscapeCurrentGain[Slot] <= 0.005f && Comp->IsPlaying())
			{
				Comp->Stop();
			}
		}
	}

	// 4. Deterministic distant groan schedule (18-40s interval from farthest room)
	if (bHasDistantGroanOrigin)
	{
		GroanCooldownSec -= DeltaSeconds;
		if (GroanCooldownSec <= 0.0f)
		{
			GroanRandState = GroanRandState * 1664525u + 1013904223u;
			GroanCooldownSec = 18.0f + static_cast<float>(GroanRandState % 220u) * 0.1f;
			const float GroanHz = ComputeOcclusionLowPassHz(1.0f, ActiveBiomeFogFactor);
			PlayOneShotWave(FName(TEXT("Ambient_DistantGroan_0")), DistantGroanOrigin, 0.34f, 0.92f, GroanHz);
		}
	}
}

void UDungeonAcousticSubsystem::Tick(const float DeltaTime)
{
	if (PreallocatedVoiceCount <= 0 && RegisteredWalkerCount <= 0)
	{
		return;
	}

	SimAccum15HzSec += FMath::Min(DeltaTime, 0.1f);
	if (SimAccum15HzSec >= AcousticDetail::Tick15HzIntervalSec)
	{
		const float Step = AcousticDetail::Tick15HzIntervalSec;
		SimAccum15HzSec -= Step;
		if (SimAccum15HzSec > AcousticDetail::Tick15HzIntervalSec)
		{
			SimAccum15HzSec = 0.0f;
		}
		StepAcousticSimulation(Step);
	}
}
