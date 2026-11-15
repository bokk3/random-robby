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
