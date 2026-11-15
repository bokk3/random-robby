#include "Core/DungeoneerGameMode.h"
#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerGameState.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Dungeon/DungeonGeneratorSubsystem.h"
#include "UI/DungeoneerMainMenuWidget.h"
#include "UI/DungeoneerCoopLobbyWidget.h"
#include "UI/DungeoneerMenuFooterWidget.h"
#include "UI/MenuDioramaStageActor.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

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

		if (!IsRunningCommandlet() && World->IsGameWorld())
		{
			if (UGameInstance* GI = World->GetGameInstance())
			{
				AMenuDioramaStageActor* DioramaStage = World->SpawnActor<AMenuDioramaStageActor>(
					AMenuDioramaStageActor::StaticClass(),
					FVector(0.0, 0.0, 120.0),
					FRotator::ZeroRotator);

				const TArray<ULocalPlayer*>& LocalPlayers = GI->GetLocalPlayers();
				if (LocalPlayers.Num() > 0 && LocalPlayers[0])
				{
					if (APlayerController* OwningPC = LocalPlayers[0]->GetPlayerController(World))
					{
						OwningPC->bShowMouseCursor = true;
						if (DioramaStage)
						{
							OwningPC->SetViewTarget(DioramaStage);
						}

						UDungeoneerMainMenuWidget* MainMenu = CreateWidget<UDungeoneerMainMenuWidget>(OwningPC, UDungeoneerMainMenuWidget::StaticClass());
						UDungeoneerCoopLobbyWidget* CoopLobby = CreateWidget<UDungeoneerCoopLobbyWidget>(OwningPC, UDungeoneerCoopLobbyWidget::StaticClass());
						UDungeoneerMenuFooterWidget* Footer = CreateWidget<UDungeoneerMenuFooterWidget>(OwningPC, UDungeoneerMenuFooterWidget::StaticClass());

						if (MainMenu)
						{
							MainMenu->AddToViewport(10);
							if (DioramaStage)
							{
								MainMenu->OnMenuSelectionChanged.AddWeakLambda(DioramaStage, [DioramaStage](EDungeoneerMenuAction Action)
								{
									DioramaStage->SetShowcaseModePreview(Action);
								});
							}
						}
						if (CoopLobby)
						{
							CoopLobby->AddToViewport(11);
						}
						if (Footer)
						{
							Footer->BindToGameState(GetGameState<ADungeoneerGameState>());
							Footer->AddToViewport(12);
						}
					}
				}
			}
		}
	}
}
