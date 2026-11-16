#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "DungeoneerSaveGame.generated.h"

UENUM(BlueprintType)
enum class EDungeoneerBestiaryBit : uint8
{
	Exploder      = 0,
	FastThrower   = 1,
	MirrorStalker = 2,
	BossGuardian  = 3
};

/**
 * TASK-1403: Persistent Meta-Ledger SaveGame (`UDungeoneerSaveGame`).
 * Stores discovered bestiary entries, unlocked relics, best clear records,
 * equipped cosmetic lantern trim, and deterministic Daily Seed completion tracking.
 */
UCLASS(BlueprintType)
class DARKDUNGEONEER_API UDungeoneerSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	static constexpr int32 LanternTrimCount = 4;
	static constexpr uint8 AllBestiaryBitsMask = 0x0F;
	static constexpr uint8 AllRelicBitsMask = 0x1F;

	UDungeoneerSaveGame();

	/** Bitmask of slain enemy archetypes (EDungeoneerBestiaryBit). */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveGame|Ledger")
	uint8 BestiaryDiscoveredMask = 0;

	/** Bitmask of all Relics discovered across runs. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveGame|Ledger")
	uint8 UnlockedRelicMask = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveGame|Ledger")
	int32 HighestFloorReached = 1;

	/** Fastest Floor-3 Guardian victory duration in seconds (0 = none recorded yet). */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveGame|Ledger")
	float BestClearDurationSeconds = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveGame|Ledger")
	int32 TotalRunsCompleted = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveGame|Ledger")
	int32 TotalRunsCleared = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveGame|Ledger")
	int32 TotalGoldAccumulated = 0;

	/** Equipped cosmetic lantern trim tint index (0..3). */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveGame|Cosmetics")
	int32 EquippedLanternTrimIndex = 0;

	/** Last UTC YYYYMMDD key cleared under Daily Seed mode. */
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "SaveGame|Ledger")
	int32 LastDailySeedCompletedDayKey = 0;

	/** Merges run outcomes into the persistent meta-ledger cleanly. */
	UFUNCTION(BlueprintCallable, Category = "SaveGame|Ledger")
	void RecordRunOutcome(
		int32 ReachedFloor,
		float DurationSeconds,
		bool bVictory,
		uint8 SessionRelicMask,
		uint8 SessionBestiaryMask,
		int32 GoldEarned,
		int32 DailySeedDayKey) noexcept;

	UFUNCTION(BlueprintPure, Category = "SaveGame|Ledger")
	int32 GetBestiaryDiscoveredCount() const noexcept;

	UFUNCTION(BlueprintPure, Category = "SaveGame|Ledger")
	int32 GetUnlockedRelicCount() const noexcept;

	UFUNCTION(BlueprintCallable, Category = "SaveGame|Cosmetics")
	int32 CycleEquippedLanternTrim() noexcept;

	UFUNCTION(BlueprintCallable, Category = "SaveGame|Ledger")
	static UDungeoneerSaveGame* LoadOrCreateMetaLedger(const FString& SlotNameOverride = TEXT(""));

	UFUNCTION(BlueprintCallable, Category = "SaveGame|Ledger")
	static bool SaveMetaLedger(UDungeoneerSaveGame* Ledger, const FString& SlotNameOverride = TEXT(""));

	UFUNCTION(BlueprintPure, Category = "SaveGame|DailySeed")
	static int64 ComputeDailySeedFromDate(int32 Year, int32 Month, int32 Day) noexcept;

	static int64 GetTodaysDeterministicDailySeed(int32& OutDayKey) noexcept;
	static FLinearColor GetLanternTrimTintColor(int32 TrimIndex) noexcept;
	static const TCHAR* GetLanternTrimDisplayName(int32 TrimIndex) noexcept;
	static const FString& GetDefaultSlotName() noexcept;
};
