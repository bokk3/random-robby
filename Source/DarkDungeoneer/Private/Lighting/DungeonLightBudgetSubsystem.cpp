#include "Lighting/DungeonLightBudgetSubsystem.h"

#include "AI/ZombieSpatialSubsystem.h"
#include "Components/PointLightComponent.h"
#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "Core/DungeoneerCharacter.h"
#include "Ecosystem/DungeonEcosystemTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"

UDungeonLightBudgetSubsystem::UDungeonLightBudgetSubsystem()
{
	for (int32 i = 0; i < MaxTier1; ++i)
	{
		PackedTier1[i] = FVector4f::Zero();
		Tier1MPCNames[i] = FName(*FString::Printf(TEXT("EcoGlow%02d"), i));
	}
	for (int32 i = 0; i < MaxTier2; ++i)
	{
		Tier2Pool[i] = nullptr;
		Tier2SmoothedIntensity[i] = 0.0f;
	}
	for (int32 i = 0; i < MaxTier3World; ++i)
	{
		Tier3Pool[i] = nullptr;
		Tier3SmoothedIntensity[i] = 0.0f;
	}
}

void UDungeonLightBudgetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	Candidates.Reserve(MaxCandidates);
	ScratchTier2Chosen.Reserve(MaxCandidates);
	ScratchTier3Chosen.Reserve(MaxTier3World);
	TransientStartIndex = INDEX_NONE;
	NextTransientOffset = 0;
	AssignmentAccumulator = 0.0f;
	bBudgetDirty = true;
}

void UDungeonLightBudgetSubsystem::Deinitialize()
{
	ClearCandidates();

	for (int32 i = 0; i < MaxTier2; ++i)
	{
		if (UPointLightComponent* Light = Tier2Pool[i])
		{
			Light->DestroyComponent();
		}
		Tier2Pool[i] = nullptr;
		Tier2SmoothedIntensity[i] = 0.0f;
	}
	for (int32 i = 0; i < MaxTier3World; ++i)
	{
		if (UPointLightComponent* Light = Tier3Pool[i])
		{
			Light->DestroyComponent();
		}
		Tier3Pool[i] = nullptr;
		Tier3SmoothedIntensity[i] = 0.0f;
	}

	LightPoolOwner = nullptr;
	CachedSensoryMPC = nullptr;
	Super::Deinitialize();
}

bool UDungeonLightBudgetSubsystem::IsLowQualityMode() const noexcept
{
	if (ShadowQualityOverride >= 0)
	{
		return ShadowQualityOverride <= 1;
	}

	if (const IConsoleVariable* ShadowCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("sg.ShadowQuality")))
	{
		return ShadowCVar->GetInt() <= 1;
	}

	return false;
}

void UDungeonLightBudgetSubsystem::ClearCandidates() noexcept
{
	Candidates.Reset();
	TransientStartIndex = INDEX_NONE;
	NextTransientOffset = 0;
	ActiveTier1Count = 0;
	ActiveTier2Count = 0;
	ActiveTier3WorldCount = 0;
	bBudgetDirty = true;
}

void UDungeonLightBudgetSubsystem::OnFloorBuilt(AActor* PoolOwnerActor)
{
	// Pre-allocate transient Tier 1 slots at tail if not yet appended
	if (TransientStartIndex == INDEX_NONE || !Candidates.IsValidIndex(TransientStartIndex))
	{
		TransientStartIndex = Candidates.Num();
		for (int32 i = 0; i < MaxTransientSlots && Candidates.Num() < MaxCandidates; ++i)
		{
			FDungeonLightCandidate Transient;
			Transient.RequestedTier = EDungeonLightTier::Tier1_MPCGlow;
			Transient.RadiusUU = 140.0f;
			Transient.BaseIntensity = 1.0f;
			Transient.bEnabled = false;
			Transient.LeaseExpiryTime = 0.0;
			Candidates.Add(Transient);
		}
	}

	if (PoolOwnerActor && IsValid(PoolOwnerActor) && PoolOwnerActor->GetRootComponent())
	{
		LightPoolOwner = PoolOwnerActor;

		// Ensure 16 unshadowed Tier 2 pooled point lights
		for (int32 i = 0; i < MaxTier2; ++i)
		{
			if (!IsValid(Tier2Pool[i]))
			{
				UPointLightComponent* Light = NewObject<UPointLightComponent>(PoolOwnerActor);
				Light->SetupAttachment(PoolOwnerActor->GetRootComponent());
				Light->SetMobility(EComponentMobility::Movable);
				Light->SetCastShadows(false);
				Light->SetUseInverseSquaredFalloff(true);
				Light->SetIntensityUnits(ELightUnits::Lumens);
				Light->SetSourceRadius(56.0f);
				Light->SetSoftSourceRadius(72.0f);
				Light->SetVolumetricScatteringIntensity(0.0f);
				Light->SetAttenuationRadius(360.0f);
				Light->SetIntensity(0.0f);
				Light->SetVisibility(false);
				Light->RegisterComponent();
				Tier2Pool[i] = Light;
			}
		}

		// Ensure 2 shadow-casting + fog-piercing Tier 3 world hero point lights
		for (int32 i = 0; i < MaxTier3World; ++i)
		{
			if (!IsValid(Tier3Pool[i]))
			{
				UPointLightComponent* HeroLight = NewObject<UPointLightComponent>(PoolOwnerActor);
				HeroLight->SetupAttachment(PoolOwnerActor->GetRootComponent());
				HeroLight->SetMobility(EComponentMobility::Movable);
				HeroLight->SetCastShadows(true);
				HeroLight->SetUseInverseSquaredFalloff(true);
				HeroLight->SetIntensityUnits(ELightUnits::Lumens);
				HeroLight->SetSourceRadius(36.0f);
				HeroLight->SetSoftSourceRadius(52.0f);
				HeroLight->SetVolumetricScatteringIntensity(1.2f);
				HeroLight->SetAttenuationRadius(820.0f);
				HeroLight->SetIntensity(0.0f);
				HeroLight->SetVisibility(false);
				HeroLight->RegisterComponent();
				Tier3Pool[i] = HeroLight;
			}
		}
	}

	if (!CachedSensoryMPC)
	{
		if (const UWorld* World = GetWorld())
		{
			if (const UGameInstance* GI = World->GetGameInstance())
			{
				if (UDungeoneerAssetBootstrapSubsystem* Bootstrap = GI->GetSubsystem<UDungeoneerAssetBootstrapSubsystem>())
				{
					CachedSensoryMPC = Bootstrap->EnsureSensoryMPC();
				}
			}
		}
	}

	bBudgetDirty = true;
	EvaluateLightBudgetNow(0.1f);
}

int32 UDungeonLightBudgetSubsystem::RegisterCandidate(const FDungeonLightCandidate& Candidate)
{
	if (Candidates.Num() >= MaxCandidates)
	{
		return INDEX_NONE;
	}

	FDungeonLightCandidate Copy = Candidate;
	Copy.AssignedSlot = INDEX_NONE;
	Copy.EffectiveAssignedTier = EDungeonLightTier::Tier0_Emissive;
	bBudgetDirty = true;
	return Candidates.Add(Copy);
}

void UDungeonLightBudgetSubsystem::UpdateMovingCandidate(int32 CandidateId, const FVector& NewWorldPosition) noexcept
{
	if (Candidates.IsValidIndex(CandidateId))
	{
		Candidates[CandidateId].WorldPosition = NewWorldPosition;
	}
}

bool UDungeonLightBudgetSubsystem::RequestTier2Lease(int32 CandidateId, float DurationSeconds) noexcept
{
	if (!Candidates.IsValidIndex(CandidateId) || DurationSeconds <= 0.0f)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;
	FDungeonLightCandidate& Cand = Candidates[CandidateId];
	Cand.bEnabled = true;
	Cand.RequestedTier = EDungeonLightTier::Tier2_Pooled;
	Cand.PriorityBoost = FMath::Max(Cand.PriorityBoost, 1.85f);
	Cand.LeaseExpiryTime = Now + static_cast<double>(DurationSeconds);
	bBudgetDirty = true;
	return true;
}

void UDungeonLightBudgetSubsystem::SetCandidateEnabled(int32 CandidateId, bool bInEnabled) noexcept
{
	if (Candidates.IsValidIndex(CandidateId))
	{
		Candidates[CandidateId].bEnabled = bInEnabled;
		bBudgetDirty = true;
	}
}

void UDungeonLightBudgetSubsystem::TriggerTransientTier1Glow(
	const FVector& WorldPosition,
	const FLinearColor& Color,
	const float RadiusUU,
	const float DurationSeconds) noexcept
{
	if (TransientStartIndex == INDEX_NONE || !Candidates.IsValidIndex(TransientStartIndex))
	{
		return;
	}

	const int32 MaxIdx = FMath::Min(TransientStartIndex + MaxTransientSlots, Candidates.Num());
	const int32 Count = MaxIdx - TransientStartIndex;
	if (Count <= 0)
	{
		return;
	}

	const int32 Slot = TransientStartIndex + (NextTransientOffset % Count);
	++NextTransientOffset;

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetTimeSeconds() : 0.0;

	FDungeonLightCandidate& Cand = Candidates[Slot];
	Cand.WorldPosition = WorldPosition;
	Cand.Color = Color;
	Cand.RadiusUU = FMath::Clamp(RadiusUU, 75.0f, 160.0f);
	Cand.BaseIntensity = 1.0f;
	Cand.RequestedTier = EDungeonLightTier::Tier1_MPCGlow;
	Cand.bEnabled = true;
	Cand.LeaseExpiryTime = Now + static_cast<double>(FMath::Max(DurationSeconds, 0.25f));
	bBudgetDirty = true;
}

void UDungeonLightBudgetSubsystem::GatherViewerLocations(TStaticArray<FVector, 4>& OutLocations, int32& OutCount) const noexcept
{
	OutCount = 0;
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (const UZombieSpatialSubsystem* Spatial = World->GetSubsystem<UZombieSpatialSubsystem>())
	{
		const int32 Tracked = Spatial->GetTrackedPlayerCount();
		for (int32 i = 0; i < Tracked && OutCount < 4; ++i)
		{
			if (const ADungeoneerCharacter* Player = Spatial->GetTrackedPlayerAt(i))
			{
				OutLocations[OutCount++] = Player->GetActorLocation();
			}
		}
	}

	if (OutCount == 0)
	{
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It && OutCount < 4; ++It)
		{
			if (const APlayerController* PC = It->Get())
			{
				OutLocations[OutCount++] = PC->GetFocalLocation();
			}
		}
	}

	if (OutCount == 0)
	{
		OutLocations[OutCount++] = FVector::ZeroVector;
	}
}

void UDungeonLightBudgetSubsystem::ExpireTimedLeases(double NowSec) noexcept
{
	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		FDungeonLightCandidate& Cand = Candidates[i];
		if (Cand.bEnabled && Cand.LeaseExpiryTime >= 0.0 && NowSec >= Cand.LeaseExpiryTime)
		{
			Cand.bEnabled = false;
			Cand.LeaseExpiryTime = -1.0;
			Cand.AssignedSlot = INDEX_NONE;
			Cand.EffectiveAssignedTier = EDungeonLightTier::Tier0_Emissive;
			bBudgetDirty = true;
		}
	}
}

void UDungeonLightBudgetSubsystem::EvaluateLightBudgetNow(float DeltaTime)
{
	const UWorld* World = GetWorld();
	const double NowSec = World ? World->GetTimeSeconds() : 0.0;
	ExpireTimedLeases(NowSec);

	TStaticArray<FVector, 4> Viewers;
	int32 ViewerCount = 0;
	GatherViewerLocations(Viewers, ViewerCount);

	auto MinDistSqToViewers = [&Viewers, ViewerCount](const FVector& Pos) -> float
	{
		double BestSq = TNumericLimits<double>::Max();
		for (int32 v = 0; v < ViewerCount; ++v)
		{
			const double dSq = FVector::DistSquaredXY(Pos, Viewers[v]);
			if (dSq < BestSq)
			{
				BestSq = dSq;
			}
		}
		return static_cast<float>(FMath::Max(BestSq, 1.0));
	};

	const int32 BudgetTier1 = GetEffectiveMaxTier1();
	const int32 BudgetTier2 = GetEffectiveMaxTier2();

	// 1. Assign Tier 3 World Hero Lights (Max 2); overflow demotes cleanly to Tier 2 pool
	ScratchTier3Chosen.Reset();
	ScratchTier2Chosen.Reset();

	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		const FDungeonLightCandidate& Cand = Candidates[i];
		if (!Cand.bEnabled || Cand.RequestedTier != EDungeonLightTier::Tier3_Hero)
		{
			continue;
		}
		const float Hysteresis = (Cand.AssignedSlot != INDEX_NONE && Cand.EffectiveAssignedTier == EDungeonLightTier::Tier3_Hero) ? 0.90f : 1.0f;
		const float Score = (MinDistSqToViewers(Cand.WorldPosition) * Hysteresis) / (1.0f + FMath::Max(Cand.PriorityBoost, 0.0f));

		// Insert sorted ascending into ScratchTier3Chosen (capacity 2)
		int32 InsertAt = 0;
		while (InsertAt < ScratchTier3Chosen.Num())
		{
			const int32 OtherIdx = ScratchTier3Chosen[InsertAt];
			const FDungeonLightCandidate& Other = Candidates[OtherIdx];
			const float OHyst = (Other.AssignedSlot != INDEX_NONE && Other.EffectiveAssignedTier == EDungeonLightTier::Tier3_Hero) ? 0.90f : 1.0f;
			const float OScore = (MinDistSqToViewers(Other.WorldPosition) * OHyst) / (1.0f + FMath::Max(Other.PriorityBoost, 0.0f));
			if (Score < OScore)
			{
				break;
			}
			++InsertAt;
		}
		if (InsertAt < MaxTier3World)
		{
			ScratchTier3Chosen.Insert(i, InsertAt);
			if (ScratchTier3Chosen.Num() > MaxTier3World)
			{
				const int32 DemotedIdx = ScratchTier3Chosen.Pop(EAllowShrinking::No);
				ScratchTier2Chosen.Add(DemotedIdx);
			}
		}
		else
		{
			// Overflow Tier 3 demotes into Tier 2 unshadowed candidates
			ScratchTier2Chosen.Add(i);
		}
	}

	// 2. Gather Tier 2 candidates + demoted Tier 3 overflow and keep closest BudgetTier2
	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		const FDungeonLightCandidate& Cand = Candidates[i];
		if (!Cand.bEnabled || Cand.RequestedTier != EDungeonLightTier::Tier2_Pooled)
		{
			continue;
		}
		ScratchTier2Chosen.Add(i);
	}

	auto ComputeTier2Score = [&](int32 CandIdx) -> float
	{
		const FDungeonLightCandidate& Cand = Candidates[CandIdx];
		const float Hysteresis = (Cand.AssignedSlot != INDEX_NONE && Cand.EffectiveAssignedTier == EDungeonLightTier::Tier2_Pooled) ? 0.90f : 1.0f;
		const float DemoteBonus = (Cand.RequestedTier == EDungeonLightTier::Tier3_Hero) ? 0.65f : 1.0f;
		return (MinDistSqToViewers(Cand.WorldPosition) * Hysteresis * DemoteBonus) / (1.0f + FMath::Max(Cand.PriorityBoost, 0.0f));
	};

	if (ScratchTier2Chosen.Num() > 1)
	{
		ScratchTier2Chosen.Sort([&](int32 A, int32 B)
		{
			return ComputeTier2Score(A) < ComputeTier2Score(B);
		});
		if (ScratchTier2Chosen.Num() > BudgetTier2)
		{
			ScratchTier2Chosen.SetNum(BudgetTier2, EAllowShrinking::No);
		}
	}

	// Clear old slot markers then commit Tier 3 & Tier 2 assignments
	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		Candidates[i].AssignedSlot = INDEX_NONE;
		Candidates[i].EffectiveAssignedTier = EDungeonLightTier::Tier0_Emissive;
	}

	ActiveTier3WorldCount = ScratchTier3Chosen.Num();
	for (int32 Slot = 0; Slot < ScratchTier3Chosen.Num(); ++Slot)
	{
		FDungeonLightCandidate& Cand = Candidates[ScratchTier3Chosen[Slot]];
		Cand.AssignedSlot = Slot;
		Cand.EffectiveAssignedTier = EDungeonLightTier::Tier3_Hero;
	}

	ActiveTier2Count = FMath::Min(ScratchTier2Chosen.Num(), BudgetTier2);
	for (int32 Slot = 0; Slot < ActiveTier2Count; ++Slot)
	{
		FDungeonLightCandidate& Cand = Candidates[ScratchTier2Chosen[Slot]];
		Cand.AssignedSlot = Slot;
		Cand.EffectiveAssignedTier = EDungeonLightTier::Tier2_Pooled;
	}

	// 3. Gather up to BudgetTier1 closest Tier 1 candidates (plus unassigned lower priority glows if room)
	ActiveTier1Count = 0;
	TStaticArray<float, MaxTier1> BestTier1DistSq;
	for (int32 s = 0; s < MaxTier1; ++s)
	{
		PackedTier1[s] = FVector4f::Zero();
		BestTier1DistSq[s] = TNumericLimits<float>::Max();
	}

	for (int32 i = 0; i < Candidates.Num(); ++i)
	{
		const FDungeonLightCandidate& Cand = Candidates[i];
		if (!Cand.bEnabled)
		{
			continue;
		}
		if (Cand.EffectiveAssignedTier != EDungeonLightTier::Tier0_Emissive)
		{
			continue; // Already using a Tier 2 or Tier 3 hardware light
		}
		if (Cand.RequestedTier != EDungeonLightTier::Tier1_MPCGlow && Cand.RequestedTier != EDungeonLightTier::Tier2_Pooled)
		{
			continue;
		}

		const float DistSq = MinDistSqToViewers(Cand.WorldPosition) / (1.0f + FMath::Max(Cand.PriorityBoost, 0.0f));
		if (ActiveTier1Count < BudgetTier1)
		{
			BestTier1DistSq[ActiveTier1Count] = DistSq;
			const float RadiusClamped = FMath::Clamp(Cand.RadiusUU, 75.0f, 160.0f);
			PackedTier1[ActiveTier1Count] = FVector4f(
				static_cast<float>(Cand.WorldPosition.X),
				static_cast<float>(Cand.WorldPosition.Y),
				RadiusClamped,
				EcoColors::PackLinearColorRGB24(Cand.Color));
			Candidates[i].AssignedSlot = ActiveTier1Count;
			Candidates[i].EffectiveAssignedTier = EDungeonLightTier::Tier1_MPCGlow;
			++ActiveTier1Count;
		}
		else if (BudgetTier1 > 0)
		{
			int32 WorstSlot = 0;
			for (int32 s = 1; s < BudgetTier1; ++s)
			{
				if (BestTier1DistSq[s] > BestTier1DistSq[WorstSlot])
				{
					WorstSlot = s;
				}
			}
			if (DistSq < BestTier1DistSq[WorstSlot])
			{
				BestTier1DistSq[WorstSlot] = DistSq;
				const float RadiusClamped = FMath::Clamp(Cand.RadiusUU, 75.0f, 160.0f);
				PackedTier1[WorstSlot] = FVector4f(
					static_cast<float>(Cand.WorldPosition.X),
					static_cast<float>(Cand.WorldPosition.Y),
					RadiusClamped,
					EcoColors::PackLinearColorRGB24(Cand.Color));
			}
		}
	}

	UpdatePointLightPools(DeltaTime, NowSec);
	PushTier1ToMPC();
	bBudgetDirty = false;
}

void UDungeonLightBudgetSubsystem::UpdatePointLightPools(float DeltaTime, double /*NowSec*/) noexcept
{
	const float BlendAlpha = FMath::Clamp(DeltaTime / 0.25f, 0.15f, 1.0f);

	for (int32 Slot = 0; Slot < MaxTier2; ++Slot)
	{
		UPointLightComponent* Light = Tier2Pool[Slot];
		if (!IsValid(Light))
		{
			continue;
		}

		if (Slot < ActiveTier2Count && ScratchTier2Chosen.IsValidIndex(Slot))
		{
			const FDungeonLightCandidate& Cand = Candidates[ScratchTier2Chosen[Slot]];
			Tier2SmoothedIntensity[Slot] = FMath::Lerp(Tier2SmoothedIntensity[Slot], Cand.BaseIntensity, BlendAlpha);
			Light->SetWorldLocation(Cand.WorldPosition);
			Light->SetLightColor(Cand.Color);
			Light->SetAttenuationRadius(FMath::Clamp(Cand.RadiusUU, 250.0f, 450.0f));
			Light->SetIntensity(Tier2SmoothedIntensity[Slot]);
			Light->SetCastShadows(false);
			Light->SetVolumetricScatteringIntensity(0.0f);
			Light->SetVisibility(Tier2SmoothedIntensity[Slot] > 1.0f);
		}
		else
		{
			Tier2SmoothedIntensity[Slot] = FMath::Lerp(Tier2SmoothedIntensity[Slot], 0.0f, BlendAlpha);
			Light->SetIntensity(Tier2SmoothedIntensity[Slot]);
			if (Tier2SmoothedIntensity[Slot] <= 1.0f)
			{
				Light->SetVisibility(false);
			}
		}
	}

	for (int32 Slot = 0; Slot < MaxTier3World; ++Slot)
	{
		UPointLightComponent* Light = Tier3Pool[Slot];
		if (!IsValid(Light))
		{
			continue;
		}

		if (Slot < ActiveTier3WorldCount && ScratchTier3Chosen.IsValidIndex(Slot))
		{
			const FDungeonLightCandidate& Cand = Candidates[ScratchTier3Chosen[Slot]];
			Tier3SmoothedIntensity[Slot] = FMath::Lerp(Tier3SmoothedIntensity[Slot], Cand.BaseIntensity, BlendAlpha);
			Light->SetWorldLocation(Cand.WorldPosition);
			Light->SetLightColor(Cand.Color);
			Light->SetAttenuationRadius(FMath::Clamp(Cand.RadiusUU, 600.0f, 1000.0f));
			Light->SetIntensity(Tier3SmoothedIntensity[Slot]);
			Light->SetCastShadows(true);
			Light->SetVolumetricScatteringIntensity(2.5f);
			Light->SetVisibility(Tier3SmoothedIntensity[Slot] > 1.0f);
		}
		else
		{
			Tier3SmoothedIntensity[Slot] = FMath::Lerp(Tier3SmoothedIntensity[Slot], 0.0f, BlendAlpha);
			Light->SetIntensity(Tier3SmoothedIntensity[Slot]);
			if (Tier3SmoothedIntensity[Slot] <= 1.0f)
			{
				Light->SetVisibility(false);
			}
		}
	}
}

void UDungeonLightBudgetSubsystem::PushTier1ToMPC() noexcept
{
	UWorld* World = GetWorld();
	if (!World || !CachedSensoryMPC)
	{
		return;
	}

	UMaterialParameterCollectionInstance* MPCInst = World->GetParameterCollectionInstance(CachedSensoryMPC);
	if (!MPCInst)
	{
		return;
	}

	MPCInst->SetScalarParameterValue(FName(TEXT("EcoGlowCount")), static_cast<float>(ActiveTier1Count));
	for (int32 i = 0; i < MaxTier1; ++i)
	{
		const FVector4f& V = PackedTier1[i];
		MPCInst->SetVectorParameterValue(Tier1MPCNames[i], FLinearColor(V.X, V.Y, V.Z, V.W));
	}
}

void UDungeonLightBudgetSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	AssignmentAccumulator += DeltaTime;
	if (bBudgetDirty || AssignmentAccumulator >= 0.1f)
	{
		const float StepDelta = FMath::Max(AssignmentAccumulator, DeltaTime);
		AssignmentAccumulator = 0.0f;
		EvaluateLightBudgetNow(StepDelta);
	}
}
