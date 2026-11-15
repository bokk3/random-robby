#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UI/DungeoneerMainMenuWidget.h"
#include "DungeoneerGameMode.generated.h"

class ADungeonAtmosphereActor;
class ADungeoneerTileMapInstancer;
class AMenuDioramaStageActor;
class UDungeoneerCoopLobbyWidget;
class UDungeoneerMenuFooterWidget;
class UDungeoneerPlayerHUDWidget;

/**
 * Primary Cooperative GameMode (`ADungeoneerGameMode`).
 * - Configures ADungeoneerCharacter as DefaultPawnClass.
 * - Pre-allocates zombie hazard & scorch decal ring pools in UZombieSpatialSubsystem.
 * - Generates deterministic Floor 1..N DAG via UDungeonGeneratorSubsystem.
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

	/** Advances all co-op players to the next procedural floor (Floor N -> N+1) with deterministic seed. */
	UFUNCTION(BlueprintCallable, Category = "Dungeon|Loop")
	int32 AdvanceToNextDungeonFloor();

	UFUNCTION(BlueprintPure, Category = "Dungeon|Loop")
	int32 GetCurrentFloorIndex() const noexcept { return CurrentFloorIndex; }

	UFUNCTION(BlueprintPure, Category = "Dungeon|Loop")
	FVector GetEntrySpawnLocation() const noexcept { return EntrySpawnLocation; }

	UFUNCTION(BlueprintPure, Category = "Dungeon")
	ADungeoneerTileMapInstancer* GetActiveTileMapInstancer() const noexcept { return ActiveTileMapInstancer.Get(); }

	UFUNCTION(BlueprintPure, Category = "UI|Diorama")
	AMenuDioramaStageActor* GetActiveDioramaStage() const noexcept { return ActiveDioramaStage.Get(); }

	UFUNCTION(BlueprintPure, Category = "Atmosphere")
	ADungeonAtmosphereActor* GetActiveAtmosphereActor() const noexcept { return ActiveAtmosphereActor.Get(); }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dungeon")
	int32 InitialFloorSeed = 424242;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dungeon|Loop")
	int32 CurrentFloorIndex = 1;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Dungeon|Loop")
	FVector EntrySpawnLocation = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AI|Pooling")
	int32 HazardPoolCapacityPerType = 16;

	UPROPERTY(Transient)
	TObjectPtr<ADungeoneerTileMapInstancer> ActiveTileMapInstancer;

	UPROPERTY(Transient)
	TObjectPtr<ADungeonAtmosphereActor> ActiveAtmosphereActor;

	UPROPERTY(Transient)
	TObjectPtr<AMenuDioramaStageActor> ActiveDioramaStage;

	UPROPERTY(Transient)
	TObjectPtr<UDungeoneerMainMenuWidget> ActiveMainMenuWidget;

	UPROPERTY(Transient)
	TObjectPtr<UDungeoneerCoopLobbyWidget> ActiveCoopLobbyWidget;

	UPROPERTY(Transient)
	TObjectPtr<UDungeoneerMenuFooterWidget> ActiveFooterWidget;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDungeoneerPlayerHUDWidget>> ActivePlayerHUDWidgets;
};
