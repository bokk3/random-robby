#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "UI/DungeoneerMainMenuWidget.h"
#include "DungeoneerGameMode.generated.h"

class ADungeonAtmosphereActor;
class ADungeoneerPartyCamera;
class ADungeoneerTileMapInstancer;
class AMenuDioramaStageActor;
class UDungeoneerCoopLobbyWidget;
class UDungeoneerMenuFooterWidget;
class UDungeoneerPlayerHUDWidget;
class UDungeoneerRunSummaryWidget;

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
	virtual void Tick(float DeltaSeconds) override;

	/** Hides the Main Menu overlay and transitions players in-place into the active procedural dungeon. */
	UFUNCTION(BlueprintCallable, Category = "Game|Menu")
	void EnterLiveDungeonFromMenu(EDungeoneerMenuAction Action);

	/** Advances all co-op players to the next procedural floor (Floor N -> N+1) with deterministic seed. */
	UFUNCTION(BlueprintCallable, Category = "Dungeon|Loop")
	int32 AdvanceToNextDungeonFloor();

	/** Adds a second local player on the same screen: spawns their hero beside player 1, points their
	 *  view at the shared party camera and adds their vitals card to player 1's HUD. */
	UFUNCTION(BlueprintCallable, Category = "CoOp")
	bool RequestLocalCoopJoin();

	/** Spawns the shared party camera on first use. */
	ADungeoneerPartyCamera* EnsurePartyCamera();

	ADungeoneerPartyCamera* GetPartyCamera() const noexcept { return ActivePartyCamera.Get(); }

	/** TASK-1403: Records Meta-Ledger SaveGame outcome and displays the Crypt Ledger Summary modal. */
	UFUNCTION(BlueprintCallable, Category = "Dungeon|Ledger")
	void ShowRunSummaryOverlay(bool bVictory);

	UFUNCTION(BlueprintCallable, Category = "Dungeon|Ledger")
	void RetryCurrentSeedRun();

	UFUNCTION(BlueprintCallable, Category = "Dungeon|Ledger")
	void ReturnToMainMenuFromSummary();

	UFUNCTION(BlueprintCallable, Category = "UI|Cosmetics")
	int32 CycleMenuLanternTrim();

	UFUNCTION(BlueprintCallable, Category = "Dungeon|DailySeed")
	bool ToggleDailySeedMode();

	UFUNCTION(BlueprintPure, Category = "Dungeon|DailySeed")
	bool IsDailySeedModeActive() const noexcept { return bDailySeedModeActive; }

	UFUNCTION(BlueprintPure, Category = "Game|Menu")
	bool IsActiveRunMainMenu() const noexcept;

	UFUNCTION(BlueprintPure, Category = "UI|Summary")
	UDungeoneerRunSummaryWidget* GetActiveRunSummaryWidget() const noexcept { return ActiveRunSummaryWidget.Get(); }

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

	UPROPERTY(Transient)
	TObjectPtr<UDungeoneerRunSummaryWidget> ActiveRunSummaryWidget;

	UPROPERTY(Transient)
	TObjectPtr<ADungeoneerPartyCamera> ActivePartyCamera;

private:
	/** Makes PC look through the party camera and stops the engine from snapping it back to its pawn. */
	void ViewThroughPartyCamera(APlayerController* PC, float BlendSec);

	/** -CaptureMove=<name>: makes every local hero perform a move just before the auto screenshot. */
	void FireCaptureMove() const;

	FString CaptureMoveName;
	float CaptureMoveLeadSec = 0.0f;
	bool bCaptureMovePending = false;

	float AutoShotDelayRemaining = -1.0f;
	float AutoQuitDelayRemaining = -1.0f;
	int32 ActiveDailySeedDayKey = 0;
	int32 EquippedLanternTrimIndex = 0;
	bool bDailySeedModeActive = false;
	bool bPendingAutoShot = false;
	bool bPendingAutoQuit = false;
};
