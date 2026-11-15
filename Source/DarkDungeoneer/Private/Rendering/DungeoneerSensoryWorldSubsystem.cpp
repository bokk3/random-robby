/**
 * DungeoneerSensoryWorldSubsystem.cpp
 * 
 * Implementation of world sensory ring buffers and MPC parameter updates.
 */

#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Engine/World.h"

UDungeoneerSensoryWorldSubsystem::UDungeoneerSensoryWorldSubsystem()
{
	for (int32 i = 0; i < MaxNoiseEvents; ++i)
	{
		NoiseRingBuffer[i] = FDungeoneerNoiseEvent();
		NoiseRingBuffer[i].bActive = false;
	}

	for (int32 i = 0; i < MaxScentNodes; ++i)
	{
		ScentTrailRingBuffer[i] = FDungeoneerScentNode();
		ScentTrailRingBuffer[i].bActive = false;
	}
}

void UDungeoneerSensoryWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	NextNoiseIndex = 0;
	NextScentIndex = 0;

	for (int32 i = 0; i < MaxNoiseEvents; ++i)
	{
		NoiseRingBuffer[i].bActive = false;
	}

	for (int32 i = 0; i < MaxScentNodes; ++i)
	{
		ScentTrailRingBuffer[i].bActive = false;
	}
}

void UDungeoneerSensoryWorldSubsystem::Deinitialize()
{
	GlobalSensoryMPC = nullptr;
	Super::Deinitialize();
}

void UDungeoneerSensoryWorldSubsystem::BroadcastNoiseEvent(
	const FVector& Origin,
	float Loudness,
	float MaxRadius,
	bool bIllusionInverted)
{
	const uint32 Slot = NextNoiseIndex % MaxNoiseEvents;
	++NextNoiseIndex;

	FDungeoneerNoiseEvent& Event = NoiseRingBuffer[Slot];
	Event.WorldOrigin = Origin;
	Event.MaxPulseRadius = FMath::Max(MaxRadius, 50.0f);
	Event.CurrentRadius = 0.0f;
	Event.Loudness = FMath::Clamp(Loudness, 0.0f, 5.0f);
	Event.AgeSeconds = 0.0f;
	Event.LifetimeSeconds = 0.85f;
	Event.bIsIllusionPhaseInverted = bIllusionInverted;
	Event.bActive = true;
}

void UDungeoneerSensoryWorldSubsystem::RecordScentTrailNode(
	const FVector& Location,
	float Intensity,
	float Lifetime,
	bool bHostile)
{
	const uint32 Slot = NextScentIndex % MaxScentNodes;
	++NextScentIndex;

	FDungeoneerScentNode& Node = ScentTrailRingBuffer[Slot];
	Node.WorldLocation = Location;
	Node.Intensity = FMath::Clamp(Intensity, 0.0f, 2.0f);
	Node.AgeSeconds = 0.0f;
	Node.LifetimeSeconds = FMath::Max(Lifetime, 0.5f);
	Node.bIsHostile = bHostile;
	Node.bActive = true;
}

bool UDungeoneerSensoryWorldSubsystem::QueryStrongestActivePulse(
	const FVector& ListenerLocation,
	float HearingPerkMultiplier,
	FDungeoneerNoiseEvent& OutStrongestNoise) const noexcept
{
	float BestAudibility = -1.0f;
	int32 BestIndex = -1;

	for (int32 i = 0; i < MaxNoiseEvents; ++i)
	{
		const FDungeoneerNoiseEvent& Event = NoiseRingBuffer[i];
		if (!Event.bActive)
		{
			continue;
		}

		const float Dist = FVector::Dist(ListenerLocation, Event.WorldOrigin);
		const float AudibleRange = Event.MaxPulseRadius * FMath::Max(HearingPerkMultiplier, 0.1f);
		if (Dist <= AudibleRange)
		{
			const float Falloff = 1.0f - (Dist / AudibleRange);
			const float Audibility = Event.Loudness * Falloff;
			if (Audibility > BestAudibility)
			{
				BestAudibility = Audibility;
				BestIndex = i;
			}
		}
	}

	if (BestIndex >= 0)
	{
		OutStrongestNoise = NoiseRingBuffer[BestIndex];
		return true;
	}

	OutStrongestNoise = FDungeoneerNoiseEvent();
	OutStrongestNoise.bActive = false;
	return false;
}

void UDungeoneerSensoryWorldSubsystem::PushGlobalSensoryMPCParameters(UMaterialParameterCollection* SensoryMPC)
{
	if (!SensoryMPC)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UMaterialParameterCollectionInstance* MPCInst = World->GetParameterCollectionInstance(SensoryMPC);
	if (!MPCInst)
	{
		return;
	}

	float BestLoudness = -1.0f;
	int32 BestIndex = -1;
	for (int32 i = 0; i < MaxNoiseEvents; ++i)
	{
		if (NoiseRingBuffer[i].bActive && NoiseRingBuffer[i].Loudness > BestLoudness)
		{
			BestLoudness = NoiseRingBuffer[i].Loudness;
			BestIndex = i;
		}
	}

	if (BestIndex >= 0)
	{
		const FDungeoneerNoiseEvent& Event = NoiseRingBuffer[BestIndex];
		MPCInst->SetVectorParameterValue(FName(TEXT("HearingPulseOrigin")), FLinearColor(Event.WorldOrigin));
		MPCInst->SetScalarParameterValue(FName(TEXT("HearingPulseRadius")), Event.CurrentRadius);
	}
	else
	{
		MPCInst->SetScalarParameterValue(FName(TEXT("HearingPulseRadius")), 0.0f);
	}
}

void UDungeoneerSensoryWorldSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update acoustic noise expansion in pre-allocated ring buffer (zero allocations)
	for (int32 i = 0; i < MaxNoiseEvents; ++i)
	{
		FDungeoneerNoiseEvent& Event = NoiseRingBuffer[i];
		if (!Event.bActive)
		{
			continue;
		}

		Event.AgeSeconds += DeltaTime;
		if (Event.AgeSeconds >= Event.LifetimeSeconds)
		{
			Event.bActive = false;
			continue;
		}

		const float Progress = Event.AgeSeconds / Event.LifetimeSeconds;
		Event.CurrentRadius = Progress * Event.MaxPulseRadius;
	}

	// Update scent nodes decay in pre-allocated ring buffer (zero allocations)
	for (int32 i = 0; i < MaxScentNodes; ++i)
	{
		FDungeoneerScentNode& Node = ScentTrailRingBuffer[i];
		if (!Node.bActive)
		{
			continue;
		}

		Node.AgeSeconds += DeltaTime;
		if (Node.AgeSeconds >= Node.LifetimeSeconds)
		{
			Node.bActive = false;
		}
	}

	if (GlobalSensoryMPC)
	{
		PushGlobalSensoryMPCParameters(GlobalSensoryMPC);
	}
}
