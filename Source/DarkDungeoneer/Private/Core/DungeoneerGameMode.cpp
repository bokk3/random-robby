#include "Core/DungeoneerGameMode.h"
#include "Core/DungeoneerCharacter.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Dungeon/DungeonGeneratorSubsystem.h"

ADungeoneerGameMode::ADungeoneerGameMode()
{
	DefaultPawnClass = ADungeoneerCharacter::StaticClass();
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
			if (UDungeonGeneratorSubsystem* DungeonSys = World->GetSubsystem<UDungeonGeneratorSubsystem>())
			{
				DungeonSys->GenerateFloorGraph(1, InitialFloorSeed);
			}
		}
	}
}
