#include "Core/DungeoneerGameMode.h"
#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerGameState.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Dungeon/DungeonGeneratorSubsystem.h"
#include "Dungeon/DungeoneerTileMapInstancer.h"
#include "Dungeon/MirrorLightBounceComponent.h"
#include "UI/DungeoneerMainMenuWidget.h"
#include "UI/DungeoneerCoopLobbyWidget.h"
#include "UI/DungeoneerMenuFooterWidget.h"
#include "UI/MenuDioramaStageActor.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace DungeoneerGameModePrivate
{
	static void ApplyLowSpecDynamicLightingCVars() noexcept
	{
		IConsoleManager& ConsoleMgr = IConsoleManager::Get();
		auto SetIntCVar = [&ConsoleMgr](const TCHAR* Name, int32 Value) noexcept
		{
			if (IConsoleVariable* Var = ConsoleMgr.FindConsoleVariable(Name))
			{
				if (!Var->TestFlags(ECVF_ReadOnly))
				{
					Var->Set(Value, ECVF_SetByCode);
				}
			}
		};

		SetIntCVar(TEXT("r.DynamicGlobalIlluminationMethod"), 0);
		SetIntCVar(TEXT("r.Lumen.DiffuseIndirect.Allow"), 0);
		SetIntCVar(TEXT("r.ReflectionMethod"), 0);
		SetIntCVar(TEXT("r.Lumen.Reflections.Allow"), 0);
		SetIntCVar(TEXT("r.Lumen.TraceMeshSDFs.Allow"), 0);
		SetIntCVar(TEXT("r.Shadow.Virtual.Enable"), 0);
		SetIntCVar(TEXT("r.ShadowQuality"), 2);
		SetIntCVar(TEXT("r.Shadow.MaxResolution"), 1024);
		SetIntCVar(TEXT("r.Shadow.MaxCSMResolution"), 1024);
		SetIntCVar(TEXT("r.DistanceFieldShadowing"), 0);
		SetIntCVar(TEXT("r.DistanceFieldAO"), 0);
		SetIntCVar(TEXT("r.CapsuleShadows"), 0);
		SetIntCVar(TEXT("r.VolumetricFog"), 1);
		SetIntCVar(TEXT("r.VolumetricFog.GridPixelSize"), 16);
		SetIntCVar(TEXT("r.VolumetricFog.GridSizeZ"), 64);
		SetIntCVar(TEXT("r.VolumetricFog.HistoryMissSupersampleCount"), 1);
		SetIntCVar(TEXT("r.AntiAliasingMethod"), 1);
		SetIntCVar(TEXT("r.MotionBlurQuality"), 0);
		SetIntCVar(TEXT("r.DepthOfFieldQuality"), 0);
		SetIntCVar(TEXT("r.AmbientOcclusionLevels"), 1);
		SetIntCVar(TEXT("r.BloomQuality"), 2);
		SetIntCVar(TEXT("r.ScreenPercentage"), 75);
		SetIntCVar(TEXT("t.MaxFPS"), 60);
	}
}

ADungeoneerGameMode::ADungeoneerGameMode()
{
	DefaultPawnClass = ADungeoneerCharacter::StaticClass();
	GameStateClass = ADungeoneerGameState::StaticClass();
}

void ADungeoneerGameMode::StartPlay()
{
	Super::StartPlay();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!IsRunningCommandlet() && World->IsGameWorld())
	{
		DungeoneerGameModePrivate::ApplyLowSpecDynamicLightingCVars();
	}

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

	// Build modular HISM Floor 1 geometry immediately so rooms, walls, mirrors, and props exist from frame 1
	if (!ActiveTileMapInstancer)
	{
		ActiveTileMapInstancer = World->SpawnActor<ADungeoneerTileMapInstancer>(
			ADungeoneerTileMapInstancer::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator);
	}
	if (ActiveTileMapInstancer)
	{
		ActiveTileMapInstancer->BuildFromActiveGenerator();
	}

	FVector EntryRoomCenter = FVector::ZeroVector;
	if (const UDungeonGeneratorSubsystem* DungeonSys = World->GetSubsystem<UDungeonGeneratorSubsystem>())
	{
		const FDungeonFloorGraph& Graph = DungeonSys->GetActiveFloorGraph();
		if (Graph.Nodes.Num() > 0)
		{
			EntryRoomCenter = Graph.Nodes[0].WorldCenter;
		}
	}

	if (UGameInstance* GI = World->GetGameInstance())
	{
		const TArray<ULocalPlayer*>& LocalPlayers = GI->GetLocalPlayers();
		for (ULocalPlayer* LP : LocalPlayers)
		{
			if (APlayerController* PC = LP ? LP->GetPlayerController(World) : nullptr)
			{
				if (ADungeoneerCharacter* Hero = Cast<ADungeoneerCharacter>(PC->GetPawn()))
				{
					Hero->SetActorLocation(EntryRoomCenter + FVector(0.0, 0.0, 95.0), false, nullptr, ETeleportType::TeleportPhysics);
					if (UMirrorLightBounceComponent* BounceComp = Hero->GetMirrorBounceComponent())
					{
						BounceComp->SetTargetTileMapInstancer(ActiveTileMapInstancer.Get());
					}
				}
			}
		}

		if (!IsRunningCommandlet() && World->IsGameWorld())
		{
			ActiveDioramaStage = World->SpawnActor<AMenuDioramaStageActor>(
				AMenuDioramaStageActor::StaticClass(),
				EntryRoomCenter + FVector(0.0, 0.0, 10.0),
				FRotator::ZeroRotator);

			if (LocalPlayers.Num() > 0 && LocalPlayers[0])
			{
				if (APlayerController* OwningPC = LocalPlayers[0]->GetPlayerController(World))
				{
					OwningPC->bShowMouseCursor = true;
					OwningPC->bAutoManageActiveCameraTarget = false;
					OwningPC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));
					if (ActiveDioramaStage)
					{
						OwningPC->SetViewTarget(ActiveDioramaStage.Get());
					}

					ActiveMainMenuWidget = CreateWidget<UDungeoneerMainMenuWidget>(OwningPC, UDungeoneerMainMenuWidget::StaticClass());
					ActiveCoopLobbyWidget = CreateWidget<UDungeoneerCoopLobbyWidget>(OwningPC, UDungeoneerCoopLobbyWidget::StaticClass());
					ActiveFooterWidget = CreateWidget<UDungeoneerMenuFooterWidget>(OwningPC, UDungeoneerMenuFooterWidget::StaticClass());

					if (ActiveMainMenuWidget)
					{
						ActiveMainMenuWidget->AddToViewport(10);
						if (ActiveDioramaStage)
						{
							AMenuDioramaStageActor* StagePtr = ActiveDioramaStage.Get();
							ActiveMainMenuWidget->OnMenuSelectionChanged.AddWeakLambda(StagePtr, [StagePtr](EDungeoneerMenuAction Action)
							{
								StagePtr->SetShowcaseModePreview(Action);
							});
						}
					}
					if (ActiveCoopLobbyWidget)
					{
						ActiveCoopLobbyWidget->AddToViewport(11);
					}
					if (ActiveFooterWidget)
					{
						ActiveFooterWidget->BindToGameState(GetGameState<ADungeoneerGameState>());
						ActiveFooterWidget->AddToViewport(12);
					}
				}
			}
		}
	}
}

void ADungeoneerGameMode::EnterLiveDungeonFromMenu(EDungeoneerMenuAction Action)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!ActiveTileMapInstancer)
	{
		ActiveTileMapInstancer = World->SpawnActor<ADungeoneerTileMapInstancer>(
			ADungeoneerTileMapInstancer::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator);
		if (ActiveTileMapInstancer)
		{
			ActiveTileMapInstancer->BuildFromActiveGenerator();
		}
	}

	if (ActiveMainMenuWidget)   { ActiveMainMenuWidget->SetVisibility(ESlateVisibility::Collapsed); }
	if (ActiveCoopLobbyWidget)  { ActiveCoopLobbyWidget->SetVisibility(ESlateVisibility::Collapsed); }
	if (ActiveFooterWidget)     { ActiveFooterWidget->SetVisibility(ESlateVisibility::Collapsed); }

	if (ActiveDioramaStage)
	{
		ActiveDioramaStage->SetActorHiddenInGame(true);
		ActiveDioramaStage->SetActorTickEnabled(false);
	}

	FVector TargetRoomCenter = FVector::ZeroVector;
	if (const UDungeonGeneratorSubsystem* DungeonSys = World->GetSubsystem<UDungeonGeneratorSubsystem>())
	{
		const FDungeonFloorGraph& Graph = DungeonSys->GetActiveFloorGraph();
		if (Graph.Nodes.Num() > 0)
		{
			TargetRoomCenter = Graph.Nodes[0].WorldCenter;
			for (const FDungeonNode& Node : Graph.Nodes)
			{
				if (Action == EDungeoneerMenuAction::MirrorTrial && Node.RoomType == EDungeonRoomType::MirrorPalaceMaze)
				{
					TargetRoomCenter = Node.WorldCenter;
					break;
				}
				if (Action == EDungeoneerMenuAction::RestingGround &&
					(Node.RoomType == EDungeonRoomType::RestingGround || Node.RoomType == EDungeonRoomType::Vendor))
				{
					TargetRoomCenter = Node.WorldCenter;
					break;
				}
				if (Action == EDungeoneerMenuAction::ContinueExpedition && Node.RoomType == EDungeonRoomType::Combat)
				{
					TargetRoomCenter = Node.WorldCenter;
					break;
				}
			}
		}
	}

	if (UGameInstance* GI = World->GetGameInstance())
	{
		for (ULocalPlayer* LP : GI->GetLocalPlayers())
		{
			if (APlayerController* PC = LP ? LP->GetPlayerController(World) : nullptr)
			{
				PC->bShowMouseCursor = true;
				PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));

				if (ADungeoneerCharacter* Character = Cast<ADungeoneerCharacter>(PC->GetPawn()))
				{
					Character->SetActorLocation(TargetRoomCenter + FVector(0.0, 0.0, 95.0), false, nullptr, ETeleportType::TeleportPhysics);
					if (UMirrorLightBounceComponent* BounceComp = Character->GetMirrorBounceComponent())
					{
						BounceComp->SetTargetTileMapInstancer(ActiveTileMapInstancer.Get());
					}
					PC->SetViewTargetWithBlend(Character, 0.35f);
				}
			}
		}
	}
}
