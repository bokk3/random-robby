#include "Core/DungeoneerGameMode.h"
#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerGameState.h"
#include "Core/DungeoneerGameViewportClient.h"
#include "Core/DungeoneerPartyCamera.h"
#include "Core/DungeoneerSaveGame.h"
#include "Combat/DungeoneerSkillComponent.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Social/DungeoneerInventoryComponent.h"
#include "Dungeon/DungeonGeneratorSubsystem.h"
#include "Dungeon/DungeoneerTileMapInstancer.h"
#include "Dungeon/MirrorLightBounceComponent.h"
#include "Rendering/DungeonAtmosphereActor.h"
#include "UI/DungeoneerMainMenuWidget.h"
#include "UI/DungeoneerCoopLobbyWidget.h"
#include "UI/DungeoneerMenuFooterWidget.h"
#include "UI/DungeoneerPlayerHUDWidget.h"
#include "UI/DungeoneerRunSummaryWidget.h"
#include "UI/MenuDioramaStageActor.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
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
		// Pixel art needs full resolution; an upscaled 75% render smears single-texel detail.
		SetIntCVar(TEXT("r.ScreenPercentage"), 100);
		SetIntCVar(TEXT("t.MaxFPS"), 60);
	}
}

ADungeoneerGameMode::ADungeoneerGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
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
		if (const UDungeoneerSaveGame* Ledger = UDungeoneerSaveGame::LoadOrCreateMetaLedger())
		{
			EquippedLanternTrimIndex = Ledger->EquippedLanternTrimIndex;
		}
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("DailySeed")))
	{
		bDailySeedModeActive = true;
		InitialFloorSeed = static_cast<int32>(UDungeoneerSaveGame::GetTodaysDeterministicDailySeed(ActiveDailySeedDayKey) & 0x7FFFFFFFLL);
	}

	if (UZombieSpatialSubsystem* SpatialSys = World->GetSubsystem<UZombieSpatialSubsystem>())
	{
		SpatialSys->PreallocateHazardPools(HazardPoolCapacityPerType);
	}

	if (HasAuthority())
	{
		CurrentFloorIndex = 1;
		int32 ForcedStartFloor = 1;
		if (FParse::Value(FCommandLine::Get(), TEXT("StartFloor="), ForcedStartFloor) && ForcedStartFloor >= 1)
		{
			CurrentFloorIndex = ForcedStartFloor;
		}

		if (ADungeoneerGameState* DungeoneerState = GetGameState<ADungeoneerGameState>())
		{
			DungeoneerState->AuthorityResetRunLedger(bDailySeedModeActive, ActiveDailySeedDayKey);
			DungeoneerState->AuthoritySetFloorSeed(CurrentFloorIndex, InitialFloorSeed, 0);
		}
		else if (UDungeonGeneratorSubsystem* DungeonSys = World->GetSubsystem<UDungeonGeneratorSubsystem>())
		{
			DungeonSys->GenerateFloorGraph(CurrentFloorIndex, InitialFloorSeed);
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
	if (ActiveAtmosphereActor)
	{
		if (const UDungeonGeneratorSubsystem* DungeonSys = World->GetSubsystem<UDungeonGeneratorSubsystem>())
		{
			ActiveAtmosphereActor->SetBiomeAtmosphere(DungeonSys->GetActiveFloorGraph().DominantBiome, false);
		}
	}

	if (!ActiveAtmosphereActor)
	{
		ActiveAtmosphereActor = World->SpawnActor<ADungeonAtmosphereActor>(
			ADungeonAtmosphereActor::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator);
	}
	if (ActiveAtmosphereActor)
	{
		if (const UDungeonGeneratorSubsystem* DungeonSys = World->GetSubsystem<UDungeonGeneratorSubsystem>())
		{
			ActiveAtmosphereActor->SetBiomeAtmosphere(DungeonSys->GetActiveFloorGraph().DominantBiome, false);
		}
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
	EntrySpawnLocation = EntryRoomCenter;

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

	if (FParse::Param(FCommandLine::Get(), TEXT("AutoPlaySolo")))
	{
		const EDungeoneerMenuAction AutoAction = (FParse::Param(FCommandLine::Get(), TEXT("AutoCoop")) || FParse::Param(FCommandLine::Get(), TEXT("AutoSplitScreen")))
			? EDungeoneerMenuAction::CoopSplitScreen
			: EDungeoneerMenuAction::EnterCatacombs;
		EnterLiveDungeonFromMenu(AutoAction);
	}

	float ShotDelaySec = 0.0f;
	if (FParse::Value(FCommandLine::Get(), TEXT("CaptureShotAfterSec="), ShotDelaySec) && ShotDelaySec > 0.0f)
	{
		AutoShotDelayRemaining = ShotDelaySec;
		bPendingAutoShot = true;

		// Lead time per move: how long before the shot it starts, so the frame lands on its most readable pose.
		if (FParse::Value(FCommandLine::Get(), TEXT("CaptureMove="), CaptureMoveName) && !CaptureMoveName.IsEmpty())
		{
			struct FMoveLead { const TCHAR* Name; float LeadSec; };
			static constexpr FMoveLead Leads[] = {
				{ TEXT("Combo"), 0.08f }, { TEXT("Dodge"), 0.08f }, { TEXT("DashStrike"), 0.07f },
				{ TEXT("Parry"), 0.06f }, { TEXT("Flare"), 0.10f }, { TEXT("Kiln"), 0.45f },
				{ TEXT("Hook"), 0.20f }, { TEXT("Shade"), 0.12f }, { TEXT("Whirl"), 0.50f },
				{ TEXT("Decoy"), 1.20f }, { TEXT("Spread"), 1.50f },
			};
			for (const FMoveLead& Lead : Leads)
			{
				if (CaptureMoveName.Equals(Lead.Name, ESearchCase::IgnoreCase))
				{
					CaptureMoveLeadSec = Lead.LeadSec;
					bCaptureMovePending = true;
				}
			}
		}
	}
}

void ADungeoneerGameMode::FireCaptureMove() const
{
	const UZombieSpatialSubsystem* Spatial = GetWorld() ? GetWorld()->GetSubsystem<UZombieSpatialSubsystem>() : nullptr;
	if (!Spatial)
	{
		return;
	}

	// Skill moves are put in slot 0 first so the capture doesn't depend on the starting loadout.
	EDungeoneerSkillId SkillForSlot0 = EDungeoneerSkillId::None;
	if (CaptureMoveName.Equals(TEXT("Flare"), ESearchCase::IgnoreCase)) { SkillForSlot0 = EDungeoneerSkillId::SolarFlare; }
	else if (CaptureMoveName.Equals(TEXT("Kiln"), ESearchCase::IgnoreCase)) { SkillForSlot0 = EDungeoneerSkillId::EmberFlask; }
	else if (CaptureMoveName.Equals(TEXT("Hook"), ESearchCase::IgnoreCase)) { SkillForSlot0 = EDungeoneerSkillId::GrappleChain; }
	else if (CaptureMoveName.Equals(TEXT("Shade"), ESearchCase::IgnoreCase)) { SkillForSlot0 = EDungeoneerSkillId::ShadeStep; }
	else if (CaptureMoveName.Equals(TEXT("Whirl"), ESearchCase::IgnoreCase)) { SkillForSlot0 = EDungeoneerSkillId::WhirlingLantern; }
	else if (CaptureMoveName.Equals(TEXT("Decoy"), ESearchCase::IgnoreCase)) { SkillForSlot0 = EDungeoneerSkillId::MirrorDecoy; }

	for (int32 i = 0; i < Spatial->GetTrackedPlayerCount(); ++i)
	{
		ADungeoneerCharacter* Hero = Spatial->GetTrackedPlayerAt(i);
		if (!Hero)
		{
			continue;
		}
		if (SkillForSlot0 != EDungeoneerSkillId::None)
		{
			if (UDungeoneerSkillComponent* Skills = Hero->GetSkillComponent())
			{
				Skills->EquipSkill(0, SkillForSlot0);
			}
			Hero->TriggerSkillSlot(0);
		}
		else if (CaptureMoveName.Equals(TEXT("Combo"), ESearchCase::IgnoreCase))
		{
			Hero->TriggerPrimaryAttack();
		}
		else if (CaptureMoveName.Equals(TEXT("Dodge"), ESearchCase::IgnoreCase))
		{
			Hero->TriggerDodgeDash();
		}
		else if (CaptureMoveName.Equals(TEXT("DashStrike"), ESearchCase::IgnoreCase))
		{
			Hero->TriggerDodgeDash();
			Hero->TriggerPrimaryAttack();
		}
		else if (CaptureMoveName.Equals(TEXT("Parry"), ESearchCase::IgnoreCase))
		{
			Hero->TriggerParry();
		}
		else if (CaptureMoveName.Equals(TEXT("Spread"), ESearchCase::IgnoreCase) && i > 0)
		{
			// Co-op framing check: push the second hero well past the leash; the party camera pulls them back.
			Hero->SetActorLocation(Hero->GetActorLocation() + FVector(1400.0, 500.0, 0.0), true, nullptr, ETeleportType::TeleportPhysics);
		}
	}
}

void ADungeoneerGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority() && !IsActiveRunMainMenu())
	{
		if (ADungeoneerGameState* DungeoneerState = GetGameState<ADungeoneerGameState>())
		{
			DungeoneerState->AuthorityAdvanceRunClock(DeltaSeconds);
		}
	}

	if (bPendingAutoShot)
	{
		AutoShotDelayRemaining -= DeltaSeconds;
		if (bCaptureMovePending && AutoShotDelayRemaining <= CaptureMoveLeadSec)
		{
			bCaptureMovePending = false;
			FireCaptureMove();
		}
		if (AutoShotDelayRemaining <= 0.0f)
		{
			bPendingAutoShot = false;
			if (UWorld* World = GetWorld())
			{
				const TCHAR* ShotCmd = FParse::Param(FCommandLine::Get(), TEXT("CaptureUI"))
					? TEXT("Shot showui")
					: TEXT("HighResShot 1280x720");
				if (UGameViewportClient* GVC = World->GetGameViewport())
				{
					GVC->ConsoleCommand(ShotCmd);
				}
				else if (GEngine)
				{
					GEngine->Exec(World, ShotCmd);
				}
			}
			if (FParse::Param(FCommandLine::Get(), TEXT("QuitAfterShot")))
			{
				bPendingAutoQuit = true;
				AutoQuitDelayRemaining = 2.5f;
			}
		}
	}
	else if (bPendingAutoQuit)
	{
		AutoQuitDelayRemaining -= DeltaSeconds;
		if (AutoQuitDelayRemaining <= 0.0f)
		{
			bPendingAutoQuit = false;
			FPlatformMisc::RequestExit(false);
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
	if (ActiveRunSummaryWidget) { ActiveRunSummaryWidget->SetVisibility(ESlateVisibility::Collapsed); }

	if (HasAuthority())
	{
		if (ADungeoneerGameState* DungeoneerState = GetGameState<ADungeoneerGameState>())
		{
			DungeoneerState->AuthorityResetRunLedger(bDailySeedModeActive, ActiveDailySeedDayKey);
		}
	}

	const bool bAutoPlaySolo = FParse::Param(FCommandLine::Get(), TEXT("AutoPlaySolo"))
		|| Action == EDungeoneerMenuAction::CoopSplitScreen;
	const bool bForceSplitScreen = (Action == EDungeoneerMenuAction::CoopSplitScreen)
		|| FParse::Param(FCommandLine::Get(), TEXT("AutoCoop"))
		|| FParse::Param(FCommandLine::Get(), TEXT("AutoSplitScreen"));
	if (ActiveDioramaStage)
	{
		if (bAutoPlaySolo)
		{
			ActiveDioramaStage->Destroy();
			ActiveDioramaStage = nullptr;
		}
		else
		{
			ActiveDioramaStage->SetActorHiddenInGame(true);
			ActiveDioramaStage->SetActorTickEnabled(false);
		}
	}

	FVector TargetRoomCenter = FVector::ZeroVector;
	FString ForcedStartRoom;
	FParse::Value(FCommandLine::Get(), TEXT("StartRoom="), ForcedStartRoom);
	if (const UDungeonGeneratorSubsystem* DungeonSys = World->GetSubsystem<UDungeonGeneratorSubsystem>())
	{
		const FDungeonFloorGraph& Graph = DungeonSys->GetActiveFloorGraph();
		if (Graph.Nodes.Num() > 0)
		{
			TargetRoomCenter = Graph.Nodes[0].WorldCenter;
			EntrySpawnLocation = Graph.Nodes[0].WorldCenter;
			for (const FDungeonNode& Node : Graph.Nodes)
			{
				if (!ForcedStartRoom.IsEmpty())
				{
					if (ForcedStartRoom.Equals(TEXT("Combat"), ESearchCase::IgnoreCase) && Node.RoomType == EDungeonRoomType::Combat && !Node.bIsSecretRoom)
					{
						TargetRoomCenter = Node.WorldCenter + FVector(40.0, -20.0, 0.0);
						break;
					}
					if (ForcedStartRoom.Equals(TEXT("Mirror"), ESearchCase::IgnoreCase) && Node.RoomType == EDungeonRoomType::MirrorPalaceMaze)
					{
						TargetRoomCenter = Node.WorldCenter + FVector(-40.0, -40.0, 0.0);
						break;
					}
					if (ForcedStartRoom.Equals(TEXT("Gauntlet"), ESearchCase::IgnoreCase) && Node.RoomType == EDungeonRoomType::DeepFogGauntlet)
					{
						TargetRoomCenter = Node.WorldCenter + FVector(-20.0, -195.0, 0.0);
						break;
					}
					if (ForcedStartRoom.Equals(TEXT("Sanctuary"), ESearchCase::IgnoreCase) && Node.RoomType == EDungeonRoomType::RestingGround)
					{
						TargetRoomCenter = Node.WorldCenter + FVector(-80.0, -60.0, 0.0);
						break;
					}
					if (ForcedStartRoom.Equals(TEXT("Vendor"), ESearchCase::IgnoreCase) && Node.RoomType == EDungeonRoomType::Vendor)
					{
						TargetRoomCenter = Node.WorldCenter + FVector(-80.0, -60.0, 0.0);
						break;
					}
					if ((ForcedStartRoom.Equals(TEXT("Vault"), ESearchCase::IgnoreCase) || ForcedStartRoom.Equals(TEXT("Boss"), ESearchCase::IgnoreCase))
						&& Node.RoomType == EDungeonRoomType::MajorLootVault)
					{
						TargetRoomCenter = Node.WorldCenter + FVector(-175.0, -45.0, 0.0);
						break;
					}
					continue;
				}
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
		const int32 PlayerCount = GI->GetNumLocalPlayers();
		for (int32 LPIdx = 0; LPIdx < PlayerCount; ++LPIdx)
		{
			ULocalPlayer* LP = GI->GetLocalPlayers()[LPIdx];
			if (APlayerController* PC = LP ? LP->GetPlayerController(World) : nullptr)
			{
				PC->bShowMouseCursor = (LPIdx == 0);
				PC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));

				if (ADungeoneerCharacter* Character = Cast<ADungeoneerCharacter>(PC->GetPawn()))
				{
					const FVector OffsetXY = (LPIdx == 0) ? FVector::ZeroVector : ADungeoneerPartyCamera::ScreenRightXY() * (130.0 * static_cast<double>(LPIdx));
					Character->SetActorLocation(TargetRoomCenter + OffsetXY + FVector(0.0, 0.0, 95.0), false, nullptr, ETeleportType::TeleportPhysics);
					Character->ApplyLanternTrimTint((EquippedLanternTrimIndex + LPIdx) & 3);
					// Every way into the dungeon (menu, retry, return from summary) starts a fresh run.
					Character->ResetForNewRun();
					if (UMirrorLightBounceComponent* BounceComp = Character->GetMirrorBounceComponent())
					{
						BounceComp->SetTargetTileMapInstancer(ActiveTileMapInstancer.Get());
					}
					ViewThroughPartyCamera(PC, bAutoPlaySolo ? 0.0f : 0.35f);

					if (!IsRunningCommandlet() && World->IsGameWorld())
					{
						UDungeoneerPlayerHUDWidget* PlayerHUD = nullptr;
						for (UDungeoneerPlayerHUDWidget* ExistingHUD : ActivePlayerHUDWidgets)
						{
							if (ExistingHUD && ExistingHUD->GetOwningLocalPlayer() == LP)
							{
								PlayerHUD = ExistingHUD;
								break;
							}
						}
						if (!PlayerHUD)
						{
							PlayerHUD = CreateWidget<UDungeoneerPlayerHUDWidget>(PC, UDungeoneerPlayerHUDWidget::StaticClass());
							if (PlayerHUD)
							{
								PlayerHUD->AddToPlayerScreen(5);
								ActivePlayerHUDWidgets.Add(PlayerHUD);
							}
						}
						if (PlayerHUD)
						{
							PlayerHUD->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
							PlayerHUD->RefreshHUDFromCharacter(Character);
						}
					}
				}
			}
		}
	}

	if (bForceSplitScreen)
	{
		RequestLocalCoopJoin();
	}
	if (ActivePartyCamera)
	{
		ActivePartyCamera->SnapToParty();
	}
}

ADungeoneerPartyCamera* ADungeoneerGameMode::EnsurePartyCamera()
{
	if (!ActivePartyCamera)
	{
		if (UWorld* World = GetWorld())
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ActivePartyCamera = World->SpawnActor<ADungeoneerPartyCamera>(ADungeoneerPartyCamera::StaticClass(), EntrySpawnLocation, FRotator::ZeroRotator, Params);
		}
	}
	return ActivePartyCamera.Get();
}

void ADungeoneerGameMode::ViewThroughPartyCamera(APlayerController* PC, const float BlendSec)
{
	ADungeoneerPartyCamera* PartyCamera = EnsurePartyCamera();
	if (!PC || !PartyCamera)
	{
		return;
	}
	PC->bAutoManageActiveCameraTarget = false;
	if (BlendSec > 0.0f)
	{
		PC->SetViewTargetWithBlend(PartyCamera, BlendSec);
	}
	else
	{
		PC->SetViewTarget(PartyCamera);
	}
}

int32 ADungeoneerGameMode::AdvanceToNextDungeonFloor()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return CurrentFloorIndex;
	}

	if (CurrentFloorIndex >= 3 && !IsRunningCommandlet())
	{
		ShowRunSummaryOverlay(true);
		return CurrentFloorIndex;
	}

	++CurrentFloorIndex;
	const int64 NextSeed = static_cast<int64>(InitialFloorSeed) + static_cast<int64>(CurrentFloorIndex) * 7919LL;

	if (ADungeoneerGameState* DungeoneerState = GetGameState<ADungeoneerGameState>())
	{
		DungeoneerState->AuthoritySetFloorSeed(CurrentFloorIndex, NextSeed, 0);
	}
	else if (UDungeonGeneratorSubsystem* DungeonSys = World->GetSubsystem<UDungeonGeneratorSubsystem>())
	{
		DungeonSys->GenerateFloorGraph(CurrentFloorIndex, static_cast<int32>(NextSeed));
	}

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
	if (ActiveAtmosphereActor)
	{
		if (const UDungeonGeneratorSubsystem* DungeonSys = World->GetSubsystem<UDungeonGeneratorSubsystem>())
		{
			ActiveAtmosphereActor->SetBiomeAtmosphere(DungeonSys->GetActiveFloorGraph().DominantBiome, false);
		}
	}

	if (!ActiveAtmosphereActor)
	{
		ActiveAtmosphereActor = World->SpawnActor<ADungeonAtmosphereActor>(
			ADungeonAtmosphereActor::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator);
	}
	if (ActiveAtmosphereActor)
	{
		if (const UDungeonGeneratorSubsystem* DungeonSys = World->GetSubsystem<UDungeonGeneratorSubsystem>())
		{
			ActiveAtmosphereActor->SetBiomeAtmosphere(DungeonSys->GetActiveFloorGraph().DominantBiome, false);
		}
	}

	FVector NextEntryCenter = FVector::ZeroVector;
	if (const UDungeonGeneratorSubsystem* DungeonSys = World->GetSubsystem<UDungeonGeneratorSubsystem>())
	{
		const FDungeonFloorGraph& Graph = DungeonSys->GetActiveFloorGraph();
		if (Graph.Nodes.Num() > 0)
		{
			NextEntryCenter = Graph.Nodes[0].WorldCenter;
		}
	}
	EntrySpawnLocation = NextEntryCenter;

	if (UGameInstance* GI = World->GetGameInstance())
	{
		const int32 LPCount = GI->GetNumLocalPlayers();
		for (int32 LPIdx = 0; LPIdx < LPCount; ++LPIdx)
		{
			ULocalPlayer* LP = GI->GetLocalPlayers()[LPIdx];
			if (APlayerController* PC = LP ? LP->GetPlayerController(World) : nullptr)
			{
				if (ADungeoneerCharacter* Character = Cast<ADungeoneerCharacter>(PC->GetPawn()))
				{
					const FVector OffsetXY = (LPIdx == 0) ? FVector::ZeroVector : ADungeoneerPartyCamera::ScreenRightXY() * (130.0 * static_cast<double>(LPIdx));
					Character->SetActorLocation(NextEntryCenter + OffsetXY + FVector(0.0, 0.0, 95.0), false, nullptr, ETeleportType::TeleportPhysics);
					Character->RestoreAtRestingGround(20.0f, 35.0f);
					if (UMirrorLightBounceComponent* BounceComp = Character->GetMirrorBounceComponent())
					{
						BounceComp->SetTargetTileMapInstancer(ActiveTileMapInstancer.Get());
					}
				}
			}
		}
	}

	if (ActivePartyCamera)
	{
		ActivePartyCamera->SnapToParty();
	}

	if (!IsRunningCommandlet())
	{
		if (UDungeoneerSaveGame* Ledger = UDungeoneerSaveGame::LoadOrCreateMetaLedger())
		{
			Ledger->HighestFloorReached = FMath::Max(Ledger->HighestFloorReached, CurrentFloorIndex);
			UDungeoneerSaveGame::SaveMetaLedger(Ledger);
		}
	}

	return CurrentFloorIndex;
}

bool ADungeoneerGameMode::IsActiveRunMainMenu() const noexcept
{
	return (ActiveMainMenuWidget && ActiveMainMenuWidget->GetVisibility() != ESlateVisibility::Collapsed)
		|| (ActiveDioramaStage && !ActiveDioramaStage->IsHidden());
}

bool ADungeoneerGameMode::RequestLocalCoopJoin()
{
	UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	if (!World || !GI)
	{
		return false;
	}

	// One shared screen: the viewport client never splits, player 2 only needs a controller and a pawn.
	if (UDungeoneerGameViewportClient* GVC = Cast<UDungeoneerGameViewportClient>(World->GetGameViewport()))
	{
		GVC->TryJoinLocalCoopPlayer(1);
	}
	else if (GI->GetNumLocalPlayers() < 2)
	{
		FString CreateError;
		GI->CreateLocalPlayer(1, CreateError, true);
	}

	if (GI->GetNumLocalPlayers() < 2)
	{
		return false;
	}

	FVector P1Location = EntrySpawnLocation;
	APlayerController* P1PC = nullptr;
	if (const ULocalPlayer* P1LP = GI->GetLocalPlayers()[0])
	{
		P1PC = P1LP->GetPlayerController(World);
		if (P1PC && P1PC->GetPawn())
		{
			P1Location = P1PC->GetPawn()->GetActorLocation();
			if (ADungeoneerCharacter* P1Char = Cast<ADungeoneerCharacter>(P1PC->GetPawn()))
			{
				P1Char->PushFloatingCombatPopup(P1Location + FVector(0.0, 0.0, 100.0), TEXT("A SECOND LANTERN IS LIT"), EDungeoneerFloatingTextKind::Status);
			}
		}
	}

	ULocalPlayer* P2LP = GI->GetLocalPlayers()[1];
	APlayerController* P2PC = P2LP ? P2LP->GetPlayerController(World) : nullptr;
	if (!P2PC)
	{
		return false;
	}
	if (!P2PC->GetPawn())
	{
		RestartPlayer(P2PC);
	}

	ADungeoneerCharacter* P2Char = Cast<ADungeoneerCharacter>(P2PC->GetPawn());
	if (!P2Char)
	{
		return false;
	}

	P2Char->SetActorLocation(P1Location + ADungeoneerPartyCamera::ScreenRightXY() * 130.0 + FVector(0.0, 0.0, 10.0), false, nullptr, ETeleportType::TeleportPhysics);
	P2Char->ApplyLanternTrimTint((EquippedLanternTrimIndex + 1) & 3);
	if (UMirrorLightBounceComponent* BounceComp = P2Char->GetMirrorBounceComponent())
	{
		BounceComp->SetTargetTileMapInstancer(ActiveTileMapInstancer.Get());
	}

	P2PC->bShowMouseCursor = false;
	P2PC->SetInputMode(FInputModeGameOnly());
	ViewThroughPartyCamera(P2PC, 0.0f);

	// Player 2's vitals card lives on player 1's HUD layer, because only player 1's view is drawn.
	if (!IsRunningCommandlet() && World->IsGameWorld() && P1PC)
	{
		UDungeoneerPlayerHUDWidget* CompanionCard = nullptr;
		for (UDungeoneerPlayerHUDWidget* ExistingHUD : ActivePlayerHUDWidgets)
		{
			if (ExistingHUD && ExistingHUD->IsCompanionCard())
			{
				CompanionCard = ExistingHUD;
				break;
			}
		}
		if (!CompanionCard)
		{
			CompanionCard = CreateWidget<UDungeoneerPlayerHUDWidget>(P1PC, UDungeoneerPlayerHUDWidget::StaticClass());
			if (CompanionCard)
			{
				CompanionCard->ConfigureAsCompanionCard(1);
				CompanionCard->AddToPlayerScreen(6);
				ActivePlayerHUDWidgets.Add(CompanionCard);
			}
		}
		if (CompanionCard)
		{
			CompanionCard->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
			CompanionCard->RefreshHUDFromCharacter(P2Char);
		}
	}

	if (ActivePartyCamera)
	{
		ActivePartyCamera->SnapToParty();
	}
	return true;
}

void ADungeoneerGameMode::ShowRunSummaryOverlay(bool bVictory)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FDungeoneerRunSummaryPayload Payload;
	Payload.bVictory = bVictory;
	Payload.FloorReached = CurrentFloorIndex;
	Payload.bDailySeedRun = bDailySeedModeActive;

	int32 TotalKills = 0;
	int32 TotalGold = 0;
	int32 TotalRelics = 0;
	uint8 CombinedRelicMask = 0;

	if (UGameInstance* GI = World->GetGameInstance())
	{
		for (ULocalPlayer* LP : GI->GetLocalPlayers())
		{
			if (APlayerController* PC = LP ? LP->GetPlayerController(World) : nullptr)
			{
				if (const ADungeoneerCharacter* Hero = Cast<ADungeoneerCharacter>(PC->GetPawn()))
				{
					TotalKills += Hero->GetKillsCount();
					if (const UDungeoneerInventoryComponent* Inv = Hero->GetInventoryComponent())
					{
						TotalGold += Inv->GetGoldBalance();
						CombinedRelicMask |= Inv->GetRelicBitmask();
						TotalRelics = FMath::Max(TotalRelics, Inv->GetActiveRelicCount());
					}
				}
			}
		}
	}

	uint8 SessionBestiaryMask = 0x01;
	if (ADungeoneerGameState* GS = GetGameState<ADungeoneerGameState>())
	{
		GS->AuthorityMarkRunEnded(bVictory);
		const FDungeoneerRunLedgerStats& Stats = GS->GetRunLedgerStats();
		Payload.ElapsedRunSeconds = Stats.ElapsedRunSeconds;
		Payload.RevivesPerformed = Stats.RevivesPerformed;
		Payload.SecretsDiscovered = Stats.SecretsDiscovered;
		Payload.MasterSeed = GS->GetReplicatedFloorSeed().MasterSeed;
		SessionBestiaryMask |= Stats.SessionBestiaryMask;
		TotalKills = FMath::Max(TotalKills, Stats.TotalKills);
		TotalRelics = FMath::Max(TotalRelics, Stats.RelicsCollected);
	}
	else
	{
		Payload.MasterSeed = InitialFloorSeed;
	}

	Payload.KillsCount = TotalKills;
	Payload.GoldBalance = TotalGold;
	Payload.RelicsCount = TotalRelics;

	if (UDungeoneerSaveGame* Ledger = UDungeoneerSaveGame::LoadOrCreateMetaLedger())
	{
		Ledger->RecordRunOutcome(
			CurrentFloorIndex,
			Payload.ElapsedRunSeconds,
			bVictory,
			CombinedRelicMask,
			SessionBestiaryMask,
			TotalGold,
			bDailySeedModeActive ? ActiveDailySeedDayKey : 0);

		Payload.PersistentBestiaryMask = Ledger->BestiaryDiscoveredMask;
		Payload.PersistentRelicMask = Ledger->UnlockedRelicMask;
		Payload.BestFloorRecord = Ledger->HighestFloorReached;
		Payload.BestClearDurationSeconds = Ledger->BestClearDurationSeconds;

		if (!IsRunningCommandlet())
		{
			UDungeoneerSaveGame::SaveMetaLedger(Ledger);
		}
	}

	for (UDungeoneerPlayerHUDWidget* HUD : ActivePlayerHUDWidgets)
	{
		if (HUD)
		{
			HUD->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (!IsRunningCommandlet() && World->IsGameWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance(); GI && GI->GetNumLocalPlayers() > 0)
		{
			if (APlayerController* PrimaryPC = GI->GetLocalPlayers()[0]->GetPlayerController(World))
			{
				PrimaryPC->bShowMouseCursor = true;
				PrimaryPC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));

				if (!ActiveRunSummaryWidget)
				{
					ActiveRunSummaryWidget = CreateWidget<UDungeoneerRunSummaryWidget>(PrimaryPC, UDungeoneerRunSummaryWidget::StaticClass());
					if (ActiveRunSummaryWidget)
					{
						ActiveRunSummaryWidget->AddToViewport(20);
					}
				}
				if (ActiveRunSummaryWidget)
				{
					ActiveRunSummaryWidget->PopulateSummary(Payload);
					ActiveRunSummaryWidget->SetVisibility(ESlateVisibility::Visible);
				}
			}
		}
	}
}

void ADungeoneerGameMode::RetryCurrentSeedRun()
{
	UWorld* World = GetWorld();
	if (!World || !HasAuthority())
	{
		return;
	}

	CurrentFloorIndex = 1;
	if (ADungeoneerGameState* GS = GetGameState<ADungeoneerGameState>())
	{
		GS->AuthorityResetRunLedger(bDailySeedModeActive, ActiveDailySeedDayKey);
		GS->AuthoritySetFloorSeed(1, InitialFloorSeed, 0);
	}
	if (ActiveTileMapInstancer)
	{
		ActiveTileMapInstancer->BuildFromActiveGenerator();
	}
	if (ActiveRunSummaryWidget)
	{
		ActiveRunSummaryWidget->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (UGameInstance* GI = World->GetGameInstance())
	{
		for (ULocalPlayer* LP : GI->GetLocalPlayers())
		{
			if (APlayerController* PC = LP ? LP->GetPlayerController(World) : nullptr)
			{
				if (ADungeoneerCharacter* Character = Cast<ADungeoneerCharacter>(PC->GetPawn()))
				{
					Character->RestoreAtRestingGround(100.0f, 100.0f);
				}
			}
		}
	}

	EnterLiveDungeonFromMenu(EDungeoneerMenuAction::EnterCatacombs);
}

void ADungeoneerGameMode::ReturnToMainMenuFromSummary()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (ActiveRunSummaryWidget)
	{
		ActiveRunSummaryWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
	for (UDungeoneerPlayerHUDWidget* HUD : ActivePlayerHUDWidgets)
	{
		if (HUD)
		{
			HUD->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (ActiveDioramaStage)
	{
		ActiveDioramaStage->SetActorHiddenInGame(false);
		ActiveDioramaStage->SetActorTickEnabled(true);
	}
	if (ActiveMainMenuWidget)  { ActiveMainMenuWidget->SetVisibility(ESlateVisibility::Visible); }
	if (ActiveCoopLobbyWidget) { ActiveCoopLobbyWidget->SetVisibility(ESlateVisibility::Visible); }
	if (ActiveFooterWidget)    { ActiveFooterWidget->SetVisibility(ESlateVisibility::Visible); }

	if (UGameInstance* GI = World->GetGameInstance(); GI && GI->GetNumLocalPlayers() > 0)
	{
		if (APlayerController* PrimaryPC = GI->GetLocalPlayers()[0]->GetPlayerController(World))
		{
			PrimaryPC->bShowMouseCursor = true;
			PrimaryPC->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));
			if (ActiveDioramaStage)
			{
				PrimaryPC->SetViewTargetWithBlend(ActiveDioramaStage.Get(), 0.25f);
			}
		}
	}
}

int32 ADungeoneerGameMode::CycleMenuLanternTrim()
{
	EquippedLanternTrimIndex = (EquippedLanternTrimIndex + 1) % UDungeoneerSaveGame::LanternTrimCount;
	if (UDungeoneerSaveGame* Ledger = UDungeoneerSaveGame::LoadOrCreateMetaLedger())
	{
		Ledger->EquippedLanternTrimIndex = EquippedLanternTrimIndex;
		if (!IsRunningCommandlet())
		{
			UDungeoneerSaveGame::SaveMetaLedger(Ledger);
		}
	}
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			for (ULocalPlayer* LP : GI->GetLocalPlayers())
			{
				if (APlayerController* PC = LP ? LP->GetPlayerController(World) : nullptr)
				{
					if (ADungeoneerCharacter* Character = Cast<ADungeoneerCharacter>(PC->GetPawn()))
					{
						Character->ApplyLanternTrimTint(EquippedLanternTrimIndex);
					}
				}
			}
		}
	}
	return EquippedLanternTrimIndex;
}

bool ADungeoneerGameMode::ToggleDailySeedMode()
{
	bDailySeedModeActive = !bDailySeedModeActive;
	if (bDailySeedModeActive)
	{
		InitialFloorSeed = static_cast<int32>(UDungeoneerSaveGame::GetTodaysDeterministicDailySeed(ActiveDailySeedDayKey) & 0x7FFFFFFFLL);
	}
	else
	{
		ActiveDailySeedDayKey = 0;
		InitialFloorSeed = 133742;
	}

	if (HasAuthority())
	{
		if (ADungeoneerGameState* GS = GetGameState<ADungeoneerGameState>())
		{
			GS->AuthorityResetRunLedger(bDailySeedModeActive, ActiveDailySeedDayKey);
			GS->AuthoritySetFloorSeed(1, InitialFloorSeed, 0);
		}
		if (ActiveTileMapInstancer)
		{
			ActiveTileMapInstancer->BuildFromActiveGenerator();
		}
	}
	return bDailySeedModeActive;
}
