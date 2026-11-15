#include "Core/DungeoneerGameMode.h"
#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerGameState.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Dungeon/DungeonGeneratorSubsystem.h"

ADungeoneerGameMode::ADungeoneerGameMode()
{
	DefaultPawnClass = ADungeoneerCharacter::StaticClass();
	GameStateClass = ADungeoneerGameState::StaticClass();
}

void ADungeoneerGameMode::StartPlay()
{
	Super::StartPlay();

	if (UWorld* World = GetWorld())
	{
		if (UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
		{
			SpatialSys->PreallocateHazardPools(HazardPoolCapacityPerType);
		}

		if (HasAuthority())
		{
			if (ADungeoneerGameState* DungeoneerState = GetGameState<ADungeoneerGameState>())
			{
				DungeoneerState->AuthoritySetFloorSeed(1, InitialFloorSeed, 0);
			}
			else if (UDungeonGeneratorSubsystem* DungeonSys = World->GetSubsystem<UDungeonGeneratorSubsystem>())
			{
				DungeonSys->GenerateFloorGraph(1, InitialFloorSeed);
			}
		}
	}
}
