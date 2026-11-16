#include "Core/DungeoneerGameState.h"
#include "Dungeon/DungeonGeneratorSubsystem.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

ADungeoneerGameState::ADungeoneerGameState()
{
	bReplicates = true;
}

void ADungeoneerGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADungeoneerGameState, ReplicatedFloorSeed);
	DOREPLIFETIME(ADungeoneerGameState, ReplicatedPartyStashGold);
	DOREPLIFETIME(ADungeoneerGameState, PartyStashItems);
	DOREPLIFETIME(ADungeoneerGameState, RunLedgerStats);
}

void ADungeoneerGameState::AuthoritySetFloorSeed(int32 NewFloorIndex, int64 NewMasterSeed, int32 NewMutatedBitmask)
{
	if (!HasAuthority())
	{
		return;
	}

	ReplicatedFloorSeed.FloorIndex = FMath::Max(NewFloorIndex, 1);
	ReplicatedFloorSeed.MasterSeed = NewMasterSeed;
	ReplicatedFloorSeed.MutatedRoomBitmask = NewMutatedBitmask;
	ApplyFloorSeedLocally();
}

FDungeonFloorSeed ADungeoneerGameState::AuthorityRerollSeedDeterministically()
{
	if (HasAuthority())
	{
		// Deterministic LCG permutation so repeated [R] rerolls are reproducible from prior seed
		const uint64 Prev = static_cast<uint64>(ReplicatedFloorSeed.MasterSeed);
		const uint64 Next = (Prev * 6364136223846793005ULL + 1442695040888963407ULL) & 0x7FFFFFFFFFFFFFFFULL;
		ReplicatedFloorSeed.MasterSeed = static_cast<int64>(Next);
		++ReplicatedFloorSeed.RerollCount;
		ApplyFloorSeedLocally();
	}

	return ReplicatedFloorSeed;
}

void ADungeoneerGameState::OnRep_DungeonFloorSeed()
{
	ApplyFloorSeedLocally();
}

void ADungeoneerGameState::ApplyFloorSeedLocally() noexcept
{
	if (UWorld* World = GetWorld())
	{
		if (UDungeonGeneratorSubsystem* DungeonSys = World->GetSubsystem<UDungeonGeneratorSubsystem>())
		{
			const int32 StreamSeed = static_cast<int32>(ReplicatedFloorSeed.MasterSeed & 0x7FFFFFFFLL);
			DungeonSys->GenerateFloorGraph(ReplicatedFloorSeed.FloorIndex, StreamSeed);
		}
	}

	if (OnFloorSeedChanged.IsBound())
	{
		OnFloorSeedChanged.Broadcast(ReplicatedFloorSeed);
	}
}

void ADungeoneerGameState::OnRep_PartyStashChanged()
{
	if (OnPartyStashChangedEvent.IsBound())
	{
		OnPartyStashChangedEvent.Broadcast();
	}
}

bool ADungeoneerGameState::AuthorityDepositGoldToStash(UDungeoneerInventoryComponent* PlayerInv, int32 Amount) noexcept
{
	if (!HasAuthority() || !PlayerInv || Amount <= 0)
	{
		return false;
	}
	if (!PlayerInv->AuthorityAdjustGold(-Amount))
	{
		return false;
	}
	ReplicatedPartyStashGold += Amount;
	OnRep_PartyStashChanged();
	return true;
}

bool ADungeoneerGameState::AuthorityWithdrawGoldFromStash(UDungeoneerInventoryComponent* PlayerInv, int32 Amount) noexcept
{
	if (!HasAuthority() || !PlayerInv || Amount <= 0 || ReplicatedPartyStashGold < Amount)
	{
		return false;
	}
	ReplicatedPartyStashGold -= Amount;
	PlayerInv->AuthorityAdjustGold(Amount);
	OnRep_PartyStashChanged();
	return true;
}

bool ADungeoneerGameState::AuthorityDepositItemToStash(UDungeoneerInventoryComponent* PlayerInv, const FGuid& ItemGuid)
{
	if (!HasAuthority() || !PlayerInv || !ItemGuid.IsValid() || PartyStashItems.Num() >= MaxPartyStashItems)
	{
		return false;
	}
	if (!PlayerInv->AuthorityHasUnlockedItem(ItemGuid))
	{
		return false;
	}
	if (!PlayerInv->AuthoritySetItemTradeLock(ItemGuid, true))
	{
		return false;
	}

	FDungeoneerInventoryItem Extracted;
	if (!PlayerInv->AuthorityExtractLockedItem(ItemGuid, Extracted))
	{
		PlayerInv->AuthoritySetItemTradeLock(ItemGuid, false);
		return false;
	}

	Extracted.bLockedForTrade = false;
	PartyStashItems.Add(Extracted);
	OnRep_PartyStashChanged();
	return true;
}

bool ADungeoneerGameState::AuthorityWithdrawItemFromStash(UDungeoneerInventoryComponent* PlayerInv, const FGuid& ItemGuid)
{
	if (!HasAuthority() || !PlayerInv || !ItemGuid.IsValid())
	{
		return false;
	}

	for (int32 Index = 0; Index < PartyStashItems.Num(); ++Index)
	{
		if (PartyStashItems[Index].ItemGuid == ItemGuid)
		{
			const FDungeoneerInventoryItem Stashed = PartyStashItems[Index];
			PlayerInv->AuthorityAddItem(Stashed.ItemGuid, Stashed.ItemId, Stashed.Quantity);
			PartyStashItems.RemoveAtSwap(Index);
			OnRep_PartyStashChanged();
			return true;
		}
	}
	return false;
}

void ADungeoneerGameState::AuthorityResetRunLedger(bool bDailySeed, int32 DailyDayKey) noexcept
{
	if (!HasAuthority())
	{
		return;
	}
	RunLedgerStats = FDungeoneerRunLedgerStats{};
	RunLedgerStats.bDailySeedRun = bDailySeed;
	RunLedgerStats.DailySeedDayKey = DailyDayKey;
}

void ADungeoneerGameState::AuthorityRecordKillInLedger(uint8 BestiaryBitMask) noexcept
{
	if (!HasAuthority())
	{
		return;
	}
	++RunLedgerStats.TotalKills;
	RunLedgerStats.SessionBestiaryMask |= (BestiaryBitMask & 0x0F);
}

void ADungeoneerGameState::AuthorityRecordSecretFound() noexcept
{
	if (HasAuthority())
	{
		++RunLedgerStats.SecretsDiscovered;
	}
}

void ADungeoneerGameState::AuthorityRecordRelicCollected() noexcept
{
	if (HasAuthority())
	{
		++RunLedgerStats.RelicsCollected;
	}
}

void ADungeoneerGameState::AuthorityRecordRevivePerformed() noexcept
{
	if (HasAuthority())
	{
		++RunLedgerStats.RevivesPerformed;
	}
}

void ADungeoneerGameState::AuthorityAdvanceRunClock(float DeltaSeconds) noexcept
{
	if (HasAuthority() && !RunLedgerStats.bRunEnded && DeltaSeconds > 0.0f)
	{
		RunLedgerStats.ElapsedRunSeconds += DeltaSeconds;
	}
}

void ADungeoneerGameState::AuthorityMarkRunEnded(bool bVictory) noexcept
{
	if (!HasAuthority())
	{
		return;
	}
	RunLedgerStats.bRunEnded = true;
	RunLedgerStats.bRunVictory = bVictory;
}
