#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UI/DungeoneerMainMenuWidget.h"
#include "DungeoneerGameMode.generated.h"

class ADungeoneerTileMapInstancer;
class AMenuDioramaStageActor;
class UDungeoneerCoopLobbyWidget;
class UDungeoneerMenuFooterWidget;

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

	/** Hides the Main Menu overlay and transitions players in-place into the active procedural dungeon. */
	UFUNCTION(BlueprintCallable, Category = "Game|Menu")
	void EnterLiveDungeonFromMenu(EDungeoneerMenuAction Action);

	UFUNCTION(BlueprintPure, Category = "Dungeon")
	ADungeoneerTileMapInstancer* GetActiveTileMapInstancer() const noexcept { return ActiveTileMapInstancer.Get(); }

	UFUNCTION(BlueprintPure, Category = "UI|Diorama")
	AMenuDioramaStageActor* GetActiveDioramaStage() const noexcept { return ActiveDioramaStage.Get(); }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon")
	int32 InitialFloorSeed = 424242;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Pooling")
	int32 HazardPoolCapacityPerType = 16;

	UPROPERTY(Transient)
	TObjectPtr<ADungeoneerTileMapInstancer> ActiveTileMapInstancer;

	UPROPERTY(Transient)
	TObjectPtr<AMenuDioramaStageActor> ActiveDioramaStage;

	UPROPERTY(Transient)
	TObjectPtr<UDungeoneerMainMenuWidget> ActiveMainMenuWidget;

	UPROPERTY(Transient)
	TObjectPtr<UDungeoneerCoopLobbyWidget> ActiveCoopLobbyWidget;

	UPROPERTY(Transient)
	TObjectPtr<UDungeoneerMenuFooterWidget> ActiveFooterWidget;
};
