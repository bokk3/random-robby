#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DungeoneerGameMode.generated.h"

/**
 * Primary Cooperative GameMode (`ADungeoneerGameMode`).
 * - Configures ADungeoneerCharacter as DefaultPawnClass.
 * - Pre-allocates zombie hazard & scorch decal ring pools in UZombieSpatialSubsystem.
 * - Generates deterministic Floor 1 DAG via UDungeonGeneratorSubsystem.
 */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API ADungeoneerGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ADungeoneerGameMode();

	virtual void StartPlay() override;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon")
	int32 InitialFloorSeed = 424242;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Pooling")
	int32 HazardPoolCapacityPerType = 16;
};
