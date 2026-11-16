#include "Core/DungeoneerSaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DateTime.h"

namespace DungeoneerSaveConstants
{
	static const FString DefaultMetaSlotName(TEXT("DungeoneerMetaLedger"));
	static constexpr int32 DefaultUserIndex = 0;
}

UDungeoneerSaveGame::UDungeoneerSaveGame()
{
}

const FString& UDungeoneerSaveGame::GetDefaultSlotName() noexcept
{
	return DungeoneerSaveConstants::DefaultMetaSlotName;
}

void UDungeoneerSaveGame::RecordRunOutcome(
	int32 ReachedFloor,
	float DurationSeconds,
	bool bVictory,
	uint8 SessionRelicMask,
	uint8 SessionBestiaryMask,
	int32 GoldEarned,
	int32 DailySeedDayKey) noexcept
{
	HighestFloorReached = FMath::Max(HighestFloorReached, FMath::Max(1, ReachedFloor));
	BestiaryDiscoveredMask |= (SessionBestiaryMask & AllBestiaryBitsMask);
	UnlockedRelicMask |= (SessionRelicMask & AllRelicBitsMask);
	TotalRunsCompleted = FMath::Max(0, TotalRunsCompleted) + 1;
	TotalGoldAccumulated = FMath::Max(0, TotalGoldAccumulated) + FMath::Max(0, GoldEarned);

	if (bVictory)
	{
		TotalRunsCleared = FMath::Max(0, TotalRunsCleared) + 1;
		const float ClampedDuration = FMath::Max(1.0f, DurationSeconds);
		if (BestClearDurationSeconds <= 0.0f || ClampedDuration < BestClearDurationSeconds)
		{
			BestClearDurationSeconds = ClampedDuration;
		}
		if (DailySeedDayKey > 0)
		{
			LastDailySeedCompletedDayKey = FMath::Max(LastDailySeedCompletedDayKey, DailySeedDayKey);
		}
	}
}

int32 UDungeoneerSaveGame::GetBestiaryDiscoveredCount() const noexcept
{
	int32 Count = 0;
	for (int32 Bit = 0; Bit < 4; ++Bit)
	{
		if ((BestiaryDiscoveredMask & (1u << Bit)) != 0u)
		{
			++Count;
		}
	}
	return Count;
}

int32 UDungeoneerSaveGame::GetUnlockedRelicCount() const noexcept
{
	int32 Count = 0;
	for (int32 Bit = 0; Bit < 5; ++Bit)
	{
		if ((UnlockedRelicMask & (1u << Bit)) != 0u)
		{
			++Count;
		}
	}
	return Count;
}

int32 UDungeoneerSaveGame::CycleEquippedLanternTrim() noexcept
{
	EquippedLanternTrimIndex = (EquippedLanternTrimIndex + 1) % LanternTrimCount;
	if (EquippedLanternTrimIndex < 0)
	{
		EquippedLanternTrimIndex += LanternTrimCount;
	}
	return EquippedLanternTrimIndex;
}

UDungeoneerSaveGame* UDungeoneerSaveGame::LoadOrCreateMetaLedger(const FString& SlotNameOverride)
{
	const FString& TargetSlot = SlotNameOverride.IsEmpty()
		? DungeoneerSaveConstants::DefaultMetaSlotName
		: SlotNameOverride;

	if (UGameplayStatics::DoesSaveGameExist(TargetSlot, DungeoneerSaveConstants::DefaultUserIndex))
	{
		if (USaveGame* Loaded = UGameplayStatics::LoadGameFromSlot(TargetSlot, DungeoneerSaveConstants::DefaultUserIndex))
		{
			if (UDungeoneerSaveGame* Typed = Cast<UDungeoneerSaveGame>(Loaded))
			{
				Typed->EquippedLanternTrimIndex = FMath::Clamp(Typed->EquippedLanternTrimIndex, 0, LanternTrimCount - 1);
				return Typed;
			}
		}
	}

	USaveGame* Created = UGameplayStatics::CreateSaveGameObject(UDungeoneerSaveGame::StaticClass());
	return Cast<UDungeoneerSaveGame>(Created);
}

bool UDungeoneerSaveGame::SaveMetaLedger(UDungeoneerSaveGame* Ledger, const FString& SlotNameOverride)
{
	if (!Ledger)
	{
		return false;
	}

	const FString& TargetSlot = SlotNameOverride.IsEmpty()
		? DungeoneerSaveConstants::DefaultMetaSlotName
		: SlotNameOverride;

	return UGameplayStatics::SaveGameToSlot(Ledger, TargetSlot, DungeoneerSaveConstants::DefaultUserIndex);
}

int64 UDungeoneerSaveGame::ComputeDailySeedFromDate(int32 Year, int32 Month, int32 Day) noexcept
{
	const int64 PackedDate = static_cast<int64>(FMath::Max(1970, Year)) * 10000LL
		+ static_cast<int64>(FMath::Clamp(Month, 1, 12)) * 100LL
		+ static_cast<int64>(FMath::Clamp(Day, 1, 31));

	// SplitMix64-inspired avalanche keeping identical UTC calendar dates 100% deterministic.
	uint64 Z = static_cast<uint64>(PackedDate) + 0x9E3779B97F4A7C15ULL;
	Z = (Z ^ (Z >> 30)) * 0xBF58476D1CE4E5B9ULL;
	Z = (Z ^ (Z >> 27)) * 0x94D049BB133111EBULL;
	Z = Z ^ (Z >> 31);
	const int64 PositiveSeed = static_cast<int64>(Z & 0x000000007FFFFFFFULL);
	return (PositiveSeed > 0) ? PositiveSeed : 14032026LL;
}

int64 UDungeoneerSaveGame::GetTodaysDeterministicDailySeed(int32& OutDayKey) noexcept
{
	const FDateTime UtcNow = FDateTime::UtcNow();
	const int32 Year = UtcNow.GetYear();
	const int32 Month = UtcNow.GetMonth();
	const int32 Day = UtcNow.GetDay();
	OutDayKey = Year * 10000 + Month * 100 + Day;
	return ComputeDailySeedFromDate(Year, Month, Day);
}

FLinearColor UDungeoneerSaveGame::GetLanternTrimTintColor(int32 TrimIndex) noexcept
{
	switch (TrimIndex & 3)
	{
	case 0:
		return FLinearColor(1.00f, 0.84f, 0.56f, 1.0f); // Classic Amber
	case 1:
		return FLinearColor(1.00f, 0.94f, 0.72f, 1.0f); // Sun-Glass Gold
	case 2:
		return FLinearColor(0.58f, 0.96f, 0.74f, 1.0f); // Cryptfire Emerald
	case 3:
	default:
		return FLinearColor(0.76f, 0.64f, 1.00f, 1.0f); // Sanctuary Amethyst
	}
}

const TCHAR* UDungeoneerSaveGame::GetLanternTrimDisplayName(int32 TrimIndex) noexcept
{
	switch (TrimIndex & 3)
	{
	case 0:  return TEXT("AMBER");
	case 1:  return TEXT("SUN-GOLD");
	case 2:  return TEXT("CRYPTFIRE");
	case 3:
	default: return TEXT("AMETHYST");
	}
}
