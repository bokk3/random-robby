/**
 * SensoryPerkComponent.cpp
 * 
 * Implementation of upgradeable sensory perks and per-viewport sensory state updates.
 */

#include "Rendering/SensoryPerkComponent.h"
#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerLocalPlayerSubsystem.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

USensoryPerkComponent::USensoryPerkComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
}

void USensoryPerkComponent::BeginPlay()
{
	Super::BeginPlay();
	CachedCharacter = Cast<ADungeoneerCharacter>(GetOwner());
}

void USensoryPerkComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USensoryPerkComponent, DarkVisionLevel);
	DOREPLIFETIME(USensoryPerkComponent, AcuteHearingLevel);
	DOREPLIFETIME(USensoryPerkComponent, ScentThermalLevel);
}

int32 USensoryPerkComponent::GetPerkLevel(EDungeoneerSensoryPerk Perk) const noexcept
{
	switch (Perk)
	{
	case EDungeoneerSensoryPerk::DarkVision:
		return DarkVisionLevel;
	case EDungeoneerSensoryPerk::AcuteHearing:
		return AcuteHearingLevel;
	case EDungeoneerSensoryPerk::ScentThermal:
		return ScentThermalLevel;
	default:
		return 0;
	}
}

void USensoryPerkComponent::SetPerkLevel(EDungeoneerSensoryPerk Perk, int32 NewLevel) noexcept
{
	const int32 ClampedLevel = FMath::Clamp(NewLevel, 0, 3);
	switch (Perk)
	{
	case EDungeoneerSensoryPerk::DarkVision:
		DarkVisionLevel = ClampedLevel;
		break;
	case EDungeoneerSensoryPerk::AcuteHearing:
		AcuteHearingLevel = ClampedLevel;
		break;
	case EDungeoneerSensoryPerk::ScentThermal:
		ScentThermalLevel = ClampedLevel;
		break;
	default:
		break;
	}
}

void USensoryPerkComponent::UpgradePerk(EDungeoneerSensoryPerk Perk) noexcept
{
	SetPerkLevel(Perk, GetPerkLevel(Perk) + 1);
}

float USensoryPerkComponent::GetDarkVisionRadius() const noexcept
{
	switch (DarkVisionLevel)
	{
	case 1:
		return 350.0f;
	case 2:
		return 600.0f;
	case 3:
		return 900.0f;
	default:
		return 0.0f;
	}
}

float USensoryPerkComponent::GetDarkVisionFogExtinctionScale() const noexcept
{
	switch (DarkVisionLevel)
	{
	case 1:
		return 0.85f;
	case 2:
		return 0.60f;
	case 3:
		return 0.35f;
	default:
		return 1.0f;
	}
}

float USensoryPerkComponent::GetAcuteHearingIntensity() const noexcept
{
	switch (AcuteHearingLevel)
	{
	case 1:
		return 0.5f;
	case 2:
		return 0.75f;
	case 3:
		return 1.0f;
	default:
		return 0.0f;
	}
}

float USensoryPerkComponent::GetAcuteHearingRangeMultiplier() const noexcept
{
	switch (AcuteHearingLevel)
	{
	case 1:
		return 1.3f;
	case 2:
		return 1.7f;
	case 3:
		return 2.2f;
	default:
		return 1.0f;
	}
}

float USensoryPerkComponent::GetScentThermalIntensity() const noexcept
{
	switch (ScentThermalLevel)
	{
	case 1:
		return 0.4f;
	case 2:
		return 0.7f;
	case 3:
		return 1.0f;
	default:
		return 0.0f;
	}
}

void USensoryPerkComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ADungeoneerCharacter* Character = CachedCharacter.Get();
	if (!Character)
	{
		Character = Cast<ADungeoneerCharacter>(GetOwner());
		CachedCharacter = Character;
	}

	if (!Character)
	{
		return;
	}

	// Strictly query the owning local player (zero GetFirstPlayerController() assumptions)
	ULocalPlayer* LP = Character->GetOwningLocalPlayer();
	if (!LP)
	{
		// Remote proxy or AI pawn; no local viewport to update
		return;
	}

	UDungeoneerLocalPlayerSubsystem* LPSS = CachedLocalPlayerSubsystem.Get();
	if (!LPSS)
	{
		LPSS = LP->GetSubsystem<UDungeoneerLocalPlayerSubsystem>();
		CachedLocalPlayerSubsystem = LPSS;
	}

	if (!LPSS)
	{
		return;
	}

	FDungeoneerNoiseEvent StrongestNoise;
	bool bHasPulse = false;

	if (AcuteHearingLevel > 0)
	{
		if (UWorld* World = GetWorld())
		{
			if (UDungeoneerSensoryWorldSubsystem* SensorySS = World->GetSubsystem<UDungeoneerSensoryWorldSubsystem>())
			{
				bHasPulse = SensorySS->QueryStrongestActivePulse(
					Character->GetActorLocation(),
					GetAcuteHearingRangeMultiplier(),
					StrongestNoise);
			}
		}
	}

	// Zero-allocation stack construct pushed directly to per-viewport MID
	FDungeoneerSensoryViewState ViewState;
	ViewState.DarkVisionRadius = GetDarkVisionRadius();
	ViewState.DarkVisionFogExtinctionScale = GetDarkVisionFogExtinctionScale();
	ViewState.AcuteHearingIntensity = GetAcuteHearingIntensity();
	ViewState.ScentThermalIntensity = GetScentThermalIntensity();
	ViewState.PlayerWorldOrigin = Character->GetActorLocation();
	ViewState.LatestHearingPulseOrigin = bHasPulse ? StrongestNoise.WorldOrigin : FVector::ZeroVector;
	ViewState.LatestHearingPulseRadius = bHasPulse ? StrongestNoise.CurrentRadius : 0.0f;

	LPSS->UpdateViewportSensoryState(ViewState);
}
