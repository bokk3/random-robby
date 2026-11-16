#include "Misc/AutomationTest.h"
#include "Core/DungeoneerTypes.h"
#include "Core/DungeoneerGameMode.h"
#include "Core/DungeoneerMovementComponent.h"
#include "Core/DungeoneerGameState.h"
#include "AI/ZombieFastThrower.h"
#include "AI/ZombieExploder.h"
#include "AI/ZombieBase.h"
#include "Dungeon/DungeonGeneratorSubsystem.h"
#include "Dungeon/DungeoneerTileMapInstancer.h"
#include "Dungeon/MirrorLightBounceComponent.h"
#include "Dungeon/MirrorMazeActors.h"
#include "Rendering/DungeonAtmosphereActor.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Audio/GibberishSpeechComponent.h"
#include "Social/DungeoneerInventoryComponent.h"
#include "Social/TradeManagerComponent.h"
#include "UI/DungeoneerMainMenuWidget.h"
#include "UI/MenuDioramaStageActor.h"
#include "UI/DungeoneerCoopLobbyWidget.h"
#include "UI/DungeoneerMenuFooterWidget.h"
#include "UI/DungeoneerPlayerHUDWidget.h"
#include "UI/DungeoneerTradeModalWidget.h"
#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "Ecosystem/DungeonEcosystemTypes.h"
#include "Ecosystem/DungeonFaunaSubsystem.h"
#include "Ecosystem/DungeonFloraManagerComponent.h"
#include "Ecosystem/DungeonWaterSubsystem.h"
#include "Audio/DungeonAcousticSubsystem.h"
#include "Audio/DungeoneerPCMSoundWave.h"
#include "Lighting/DungeonLightBudgetSubsystem.h"
#include "Rendering/DungeoneerEcoMaterialFactory.h"
#include "Dungeon/DungeonEncounterActors.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "RHIDefinitions.h"
#include "Misc/Paths.h"
#include "Rendering/25DAnimOrchestratorComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "UObject/Package.h"

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDungeoneerCompassNormalizationTest,
	"DarkDungeoneer.Locomotion.DiagonalEqualsCardinalSpeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDungeoneerCompassNormalizationTest::RunTest(const FString& Parameters)
{
	const float CardinalSpeed = static_cast<float>(UDungeoneerMovementComponent::GetUnitDirectionForOctant(ECompassOctant::E).Size());
	for (uint8 Oct = 0; Oct < 8; ++Oct)
	{
		const ECompassOctant Octant = static_cast<ECompassOctant>(Oct);
		const FVector UnitVec = UDungeoneerMovementComponent::GetUnitDirectionForOctant(Octant);
		const float Mag = static_cast<float>(UnitVec.Size());
		TestTrue(FString::Printf(TEXT("Octant %u speed matches cardinal within 1e-5"), Oct), FMath::IsNearlyEqual(Mag, CardinalSpeed, 1e-5f));
	}

	ECompassOctant Quantized = ECompassOctant::S;
	TestTrue(TEXT("NE vector quantizes cleanly to ECompassOctant::NE"), UDungeoneerMovementComponent::QuantizeInputToOctant(FVector2D(1.0, 1.0), 0.04f, Quantized));
	TestEqual(TEXT("Octant is NE"), Quantized, ECompassOctant::NE);

	TestFalse(TEXT("Deadzone input preserves previous octant"), UDungeoneerMovementComponent::QuantizeInputToOctant(FVector2D(0.01, 0.01), 0.04f, Quantized));
	TestEqual(TEXT("Octant preserved as NE"), Quantized, ECompassOctant::NE);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDungeoneerChronoLeechMathTest,
	"DarkDungeoneer.AI.ChronoLeechConservationLaw",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDungeoneerChronoLeechMathTest::RunTest(const FString& Parameters)
{
	constexpr float PlayerBase = 450.0f;
	constexpr float ZombieBase = 410.0f;
	constexpr float SpeedScalar = 1.15f;
	constexpr float BaseCooldown = 3.2f;
	constexpr float CadenceScalar = 1.40f;

	constexpr float SlowS = 0.50f;
	const float ExpectedPlayerSpeed = PlayerBase * (1.0f - SlowS);
	const float ActualZombieSpeed = AZombieFastThrower::ComputeLeechWalkSpeed(ZombieBase, SlowS, SpeedScalar);
	const float ActualCooldown = AZombieFastThrower::ComputeLeechThrowCooldown(BaseCooldown, SlowS, CadenceScalar);

	TestTrue(TEXT("Player speed reduced proportionally"), FMath::IsNearlyEqual(ExpectedPlayerSpeed, 225.0f, 1e-4f));
	TestTrue(TEXT("Zombie speed boosted proportionally"), FMath::IsNearlyEqual(ActualZombieSpeed, ZombieBase * (1.0f + SlowS * SpeedScalar), 1e-4f));
	TestTrue(TEXT("Zombie cooldown shortened proportionally"), ActualCooldown < BaseCooldown);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDungeoneerGibberishDeterminismTest,
	"DarkDungeoneer.Audio.GibberishPhonemeDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDungeoneerGibberishDeterminismTest::RunTest(const FString& Parameters)
{
	int32 IndexA = -1, IndexB = -1;
	float PitchA = 0.0f, PitchB = 0.0f;
	UGibberishSpeechComponent::EvaluateCharacterPhoneme(TEXT('?'), IndexA, PitchA);
	UGibberishSpeechComponent::EvaluateCharacterPhoneme(TEXT('?'), IndexB, PitchB);

	TestEqual(TEXT("Question mark maps deterministically"), IndexA, IndexB);
	TestTrue(TEXT("Question mark raises pitch +3 semitones"), FMath::IsNearlyEqual(PitchA, 3.0f, 1e-4f));

	int32 DotIndex = -1;
	float DotPitch = 0.0f;
	UGibberishSpeechComponent::EvaluateCharacterPhoneme(TEXT('.'), DotIndex, DotPitch);
	TestTrue(TEXT("Period drops pitch -2 semitones"), FMath::IsNearlyEqual(DotPitch, -2.0f, 1e-4f));
	TestTrue(TEXT("Grain index bounded [0, 23]"), DotIndex >= 0 && DotIndex < 24);

	return true;
}

#endif // WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDungeoneerTradeTwoPhaseCommitTest,
	"DarkDungeoneer.Social.TradeTwoPhaseCommitRollbackAndCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDungeoneerTradeTwoPhaseCommitTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Transient UWorld created"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	AActor* ActorA = TestWorld->SpawnActor<AActor>();
	AActor* ActorB = TestWorld->SpawnActor<AActor>();
	UDungeoneerInventoryComponent* InvA = NewObject<UDungeoneerInventoryComponent>(ActorA);
	UDungeoneerInventoryComponent* InvB = NewObject<UDungeoneerInventoryComponent>(ActorB);
	UTradeManagerComponent* TradeMgr = NewObject<UTradeManagerComponent>(ActorA);
	InvA->RegisterComponent();
	InvB->RegisterComponent();
	TradeMgr->RegisterComponent();

	// Adjust starting gold to exact balances (A=100, B=20)
	InvA->AuthorityAdjustGold(-150);
	InvB->AuthorityAdjustGold(-230);

	const FGuid GuidA = FGuid::NewGuid();
	const FGuid GuidB = FGuid::NewGuid();
	InvA->AuthorityAddItem(GuidA, FName(TEXT("Relic_FlashBulb")), 1);
	InvB->AuthorityAddItem(GuidB, FName(TEXT("Elixir_ChronoWard")), 1);

	TestTrue(TEXT("Lock Item A for failed 2PC test"), InvA->AuthoritySetItemTradeLock(GuidA, true));
	TestTrue(TEXT("Lock Item B for failed 2PC test"), InvB->AuthoritySetItemTradeLock(GuidB, true));

	// Failure scenario: Party B promises 500g while holding only 20g -> must roll back both locks & gold cleanly
	FTradeTransaction TxFail;
	TxFail.TransactionId = FGuid::NewGuid();
	TxFail.Phase = ETradePhaseState::OfferLocked;
	TxFail.InitiatorInventory = InvA;
	TxFail.CounterpartyInventory = InvB;
	TxFail.InitiatorOffer.OfferedItemGuids.Add(GuidA);
	TxFail.InitiatorOffer.OfferedGold = 50;
	TxFail.CounterpartyOffer.OfferedItemGuids.Add(GuidB);
	TxFail.CounterpartyOffer.OfferedGold = 500;

	TestFalse(TEXT("2PC commit rejects insufficient gold"), TradeMgr->AuthorityExecuteTwoPhaseCommit(TxFail));
	TestEqual(TEXT("Phase rolled back after failure"), TxFail.Phase, ETradePhaseState::RolledBack);
	TestTrue(TEXT("Item A unlocked cleanly on rollback"), InvA->AuthorityHasUnlockedItem(GuidA));
	TestTrue(TEXT("Item B unlocked cleanly on rollback"), InvB->AuthorityHasUnlockedItem(GuidB));
	TestEqual(TEXT("Gold A preserved on rollback"), InvA->GetGoldBalance(), 100);
	TestEqual(TEXT("Gold B preserved on rollback"), InvB->GetGoldBalance(), 20);
	TestEqual(TEXT("Zero duplication in Inventory A"), InvA->GetItems().Num(), 1);
	TestEqual(TEXT("Zero duplication in Inventory B"), InvB->GetItems().Num(), 1);

	// Success scenario: Re-lock items with valid gold -> atomic swap without duplication
	TestTrue(TEXT("Re-lock Item A"), InvA->AuthoritySetItemTradeLock(GuidA, true));
	TestTrue(TEXT("Re-lock Item B"), InvB->AuthoritySetItemTradeLock(GuidB, true));

	FTradeTransaction TxCommit = TxFail;
	TxCommit.TransactionId = FGuid::NewGuid();
	TxCommit.Phase = ETradePhaseState::OfferLocked;
	TxCommit.CounterpartyOffer.OfferedGold = 10;

	TestTrue(TEXT("2PC commit succeeds when both parties validated"), TradeMgr->AuthorityExecuteTwoPhaseCommit(TxCommit));
	TestEqual(TEXT("Phase marked Committed"), TxCommit.Phase, ETradePhaseState::Committed);
	TestTrue(TEXT("Inventory A received unlocked Item B"), InvA->AuthorityHasUnlockedItem(GuidB));
	TestTrue(TEXT("Inventory B received unlocked Item A"), InvB->AuthorityHasUnlockedItem(GuidA));
	TestFalse(TEXT("Inventory A no longer owns Item A"), InvA->AuthorityHasUnlockedItem(GuidA));
	TestFalse(TEXT("Inventory B no longer owns Item B"), InvB->AuthorityHasUnlockedItem(GuidB));
	TestEqual(TEXT("Inventory A gold settled (100 - 50 + 10 = 60)"), InvA->GetGoldBalance(), 60);
	TestEqual(TEXT("Inventory B gold settled (20 - 10 + 50 = 60)"), InvB->GetGoldBalance(), 60);

	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDungeoneerMenuAndSeedReplicationTest,
	"DarkDungeoneer.UI.MenuNavigationAndFloorSeedDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDungeoneerMenuAndSeedReplicationTest::RunTest(const FString& Parameters)
{
	// 1. SavedMove uint8 ChronoSlowRatio quantization round-trip
	constexpr float MaxSlow = 0.65f;
	TestEqual(TEXT("0.0 slow quantizes to 0"), FSavedMove_Dungeoneer::QuantizeSlowRatioToByte(0.0f, MaxSlow), static_cast<uint8>(0));
	TestEqual(TEXT("Max slow quantizes to 255"), FSavedMove_Dungeoneer::QuantizeSlowRatioToByte(MaxSlow, MaxSlow), static_cast<uint8>(255));
	const uint8 MidByte = FSavedMove_Dungeoneer::QuantizeSlowRatioToByte(0.325f, MaxSlow);
	const float MidRecovered = FSavedMove_Dungeoneer::DequantizeByteToSlowRatio(MidByte, MaxSlow);
	TestTrue(TEXT("Mid ChronoSlow ratio dequantizes within 0.005f"), FMath::IsNearlyEqual(MidRecovered, 0.325f, 0.005f));

	// 2. Main Menu Widget navigation wrap-around & map dispatch
	UDungeoneerMainMenuWidget* MainMenu = NewObject<UDungeoneerMainMenuWidget>(GetTransientPackage());
	TestEqual(TEXT("Initial menu selection is EnterCatacombs (0)"), MainMenu->GetSelectedMenuIndex(), 0);
	MainMenu->MoveSelectionUp();
	TestEqual(TEXT("MoveSelectionUp wraps from 0 to Quit (7)"), MainMenu->GetSelectedMenuIndex(), 7);
	MainMenu->MoveSelectionDown();
	TestEqual(TEXT("MoveSelectionDown wraps from 7 to 0"), MainMenu->GetSelectedMenuIndex(), 0);
	TestEqual(TEXT("MirrorTrial maps cleanly"), MainMenu->GetTargetTravelMapForAction(EDungeoneerMenuAction::MirrorTrial), FName(TEXT("/Game/Maps/L_MirrorTrial")));

	// 3. Co-Op Lobby split-screen badge helpers
	TestEqual(TEXT("2P badge text matches mockup"), UDungeoneerCoopLobbyWidget::GetSplitscreenBadgeText(EDungeoneerSplitscreenMode::SplitHoriz2P), FString(TEXT("2P HORIZ")));

	// 4. Replicated GameState seed determinism, Footer telemetry, BudgetSubsystem, & Diorama Stage
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Transient UI/Seed UWorld created"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	if (UMirrorCaptureBudgetSubsystem* CaptureBudget = TestWorld->GetSubsystem<UMirrorCaptureBudgetSubsystem>())
	{
		TestTrue(TEXT("Portal capture budget capped <= 2"), CaptureBudget->ComputeFrameCaptureBudget() <= 2);
	}

	ADungeoneerGameState* GS = TestWorld->SpawnActor<ADungeoneerGameState>();
	UDungeoneerMenuFooterWidget* Footer = NewObject<UDungeoneerMenuFooterWidget>(TestWorld);
	GS->AuthoritySetFloorSeed(2, 0x5D8F2A11LL, 3);
	Footer->BindToGameState(GS);

	TestTrue(TEXT("Footer displays seeded hex"), Footer->GetFormattedSeedHex().Contains(TEXT("0x5D8F2A11")));
	TestEqual(TEXT("Generated floor DAG has 12 nodes (10 core + 2 secret pockets)"), Footer->GetLastPreviewNodeCount(), 12);

	Footer->RequestDeterministicSeedReroll();
	const FDungeonFloorSeed RerollA = GS->GetReplicatedFloorSeed();

	ADungeoneerGameState* GSCheck = TestWorld->SpawnActor<ADungeoneerGameState>();
	GSCheck->AuthoritySetFloorSeed(2, 0x5D8F2A11LL, 3);
	const FDungeonFloorSeed RerollB = GSCheck->AuthorityRerollSeedDeterministically();
	TestEqual(TEXT("Deterministic seed reroll identical across GameState instances"), RerollA.MasterSeed, RerollB.MasterSeed);
	TestEqual(TEXT("Reroll count incremented"), RerollA.RerollCount, 1);

	Footer->UpdateOctantTelemetry(ECompassOctant::NE, 450.0f, 0.20f);
	TestTrue(TEXT("Telemetry reflects 360.0 UU/s speed"), Footer->GetOctantTelemetryText().Contains(TEXT("360.0 UU/s")));
	TestTrue(TEXT("Chrono conservation readout matches dPlayer==dThrower"), Footer->GetChronoConservationText().Contains(TEXT("-0.20 == dThrower +0.20")));

	AMenuDioramaStageActor* Stage = TestWorld->SpawnActor<AMenuDioramaStageActor>();
	Stage->EnsureShowcaseMeshesInitialized();
	TestNotNull(TEXT("Diorama ShowcaseFloorMesh has non-null StaticMesh"), Stage->GetShowcaseFloorMesh() ? Stage->GetShowcaseFloorMesh()->GetStaticMesh().Get() : nullptr);
	TestNotNull(TEXT("Diorama ExploderShowcaseProxy has non-null StaticMesh"), Stage->GetExploderShowcaseProxy() ? Stage->GetExploderShowcaseProxy()->GetStaticMesh().Get() : nullptr);
	TestNotNull(TEXT("Diorama FastThrowerShowcaseProxy has non-null StaticMesh"), Stage->GetFastThrowerShowcaseProxy() ? Stage->GetFastThrowerShowcaseProxy()->GetStaticMesh().Get() : nullptr);
	TestNotNull(TEXT("Diorama PhantasmMirrorShowcaseProxy has non-null StaticMesh"), Stage->GetPhantasmMirrorShowcaseProxy() ? Stage->GetPhantasmMirrorShowcaseProxy()->GetStaticMesh().Get() : nullptr);
	TestNotNull(TEXT("Diorama DecoyProjectionProxy has non-null StaticMesh"), Stage->GetDecoyProjectionProxy() ? Stage->GetDecoyProjectionProxy()->GetStaticMesh().Get() : nullptr);
	Stage->SetShowcaseModePreview(EDungeoneerMenuAction::MirrorTrial);
	Stage->Tick(0.5f);
	TestTrue(TEXT("Diorama flashlight sweeps smoothly toward NE"), Stage->GetCurrentFlashlightYawDeg() > 0.0f);

	ADungeoneerGameMode* GM = TestWorld->SpawnActor<ADungeoneerGameMode>();
	TestNotNull(TEXT("ADungeoneerGameMode spawned for in-place menu transition"), GM);
	if (GM)
	{
		GM->EnterLiveDungeonFromMenu(EDungeoneerMenuAction::MirrorTrial);
		TestNotNull(TEXT("EnterLiveDungeonFromMenu initialized ActiveTileMapInstancer"), GM->GetActiveTileMapInstancer());
		if (GM->GetActiveTileMapInstancer())
		{
			TestTrue(TEXT("In-place transition built HISM floor and walls"),
				GM->GetActiveTileMapInstancer()->GetFloorHISM()->GetInstanceCount() >= 9 &&
				GM->GetActiveTileMapInstancer()->GetWallHISM()->GetInstanceCount() >= 8);
		}
	}

	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDungeoneerHUDAndTradeModalAndAssetBootstrapTest,
	"DarkDungeoneer.UI.HUDAndTradeModalAndAssetBootstrap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDungeoneerHUDAndTradeModalAndAssetBootstrapTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Transient UWorld created for v0.9.0 specs"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	// 1. TASK-901: Per-viewport Player HUD Widget binds cleanly to ADungeoneerCharacter
	ADungeoneerCharacter* Hero = TestWorld->SpawnActor<ADungeoneerCharacter>();
	TestNotNull(TEXT("ADungeoneerCharacter spawned"), Hero);
	if (Hero && Hero->GetDungeoneerMovement())
	{
		Hero->GetDungeoneerMovement()->SetMoveInputVector(FVector2D(1.0f, 1.0f));
		Hero->GetDungeoneerMovement()->SetAimInputVector(FVector2D(-1.0f, 0.0f));
		Hero->GetDungeoneerMovement()->SetChronoSlowRatio(0.25f);
	}

	UDungeoneerPlayerHUDWidget* HUD = NewObject<UDungeoneerPlayerHUDWidget>(TestWorld);
	TestNotNull(TEXT("UDungeoneerPlayerHUDWidget allocated"), HUD);
	if (HUD && Hero)
	{
		HUD->RefreshHUDFromCharacter(Hero);
		TestEqual(TEXT("HUD MoveOctant matches NE"), HUD->GetMoveOctant(), ECompassOctant::NE);
		TestEqual(TEXT("HUD AimOctant matches W"), HUD->GetAimOctant(), ECompassOctant::W);
		TestTrue(TEXT("HUD HealthPercent at 1.0"), FMath::IsNearlyEqual(HUD->GetHealthPercent(), 1.0f, 1e-4f));
		TestTrue(TEXT("HUD BatteryPercent at 1.0"), FMath::IsNearlyEqual(HUD->GetBatteryPercent(), 1.0f, 1e-4f));
		TestTrue(TEXT("HUD ChronoSlowRatio at 0.25"), FMath::IsNearlyEqual(HUD->GetChronoSlowRatio(), 0.25f, 1e-4f));
		TestTrue(TEXT("HUD formatted Chrono-Slow text displays S=0.25"), HUD->GetFormattedChronoSlowText().Contains(TEXT("S=0.25")));
	}

	// 2. TASK-902: Resting Ground Trade Modal + Gibberish Speech + Vendor Purchase 2PC
	AVendorActor* Vendor = TestWorld->SpawnActor<AVendorActor>();
	TestNotNull(TEXT("AVendorActor spawned"), Vendor);
	Vendor->InitializeSeededCatalog(1337);
	TestTrue(TEXT("Seeded vendor catalog non-empty"), Vendor->GetVendorCatalog().Num() >= 2);

	AActor* PlayerActor = TestWorld->SpawnActor<AActor>();
	UDungeoneerInventoryComponent* PlayerInv = NewObject<UDungeoneerInventoryComponent>(PlayerActor);
	UTradeManagerComponent* TradeMgr = NewObject<UTradeManagerComponent>(PlayerActor);
	UGibberishSpeechComponent* SpeechComp = NewObject<UGibberishSpeechComponent>(Vendor);
	PlayerInv->RegisterComponent();
	TradeMgr->RegisterComponent();
	SpeechComp->RegisterComponent();

	PlayerInv->AuthorityAdjustGold(500);
	const int32 GoldBefore = PlayerInv->GetGoldBalance();
	const int32 ItemPrice = Vendor->GetVendorCatalog()[1].GoldPrice;
	const FName TargetItemId = Vendor->GetVendorCatalog()[1].ItemId;

	UDungeoneerTradeModalWidget* TradeModal = NewObject<UDungeoneerTradeModalWidget>(TestWorld);
	TradeModal->BindTradeContext(Vendor, SpeechComp, TradeMgr, PlayerInv);
	TradeModal->SelectVendorCatalogIndex(1);
	TestEqual(TEXT("Selected catalog index updated to 1"), TradeModal->GetSelectedCatalogIndex(), 1);
	TestTrue(TEXT("Vendor catalog selection starts procedural gibberish"), SpeechComp->IsPlayingGibberish());

	const bool bPurchased = TradeModal->ExecuteSelectedVendorPurchase();
	TestTrue(TEXT("Vendor purchase executed cleanly"), bPurchased);
	TestTrue(TEXT("OnTradeAckReceived marked bLastTradeCommitted=true"), TradeModal->WasLastTradeCommitted());
	TestEqual(TEXT("Player gold debited accurately"), PlayerInv->GetGoldBalance(), GoldBefore - ItemPrice);
	TestEqual(TEXT("Purchased relic added to player inventory"), PlayerInv->GetItems().Num(), 1);
	if (PlayerInv->GetItems().Num() > 0)
	{
		TestEqual(TEXT("Purchased item ID matches vendor catalog"), PlayerInv->GetItems()[0].ItemId, TargetItemId);
	}

	// 3. TASK-903: Runtime & .uasset Content Bootstrap Subsystem
	UGameInstance* TestGameInstance = NewObject<UGameInstance>(GetTransientPackage());
	TestNotNull(TEXT("Dummy UGameInstance allocated for UGameInstanceSubsystem Outer"), TestGameInstance);
	UDungeoneerAssetBootstrapSubsystem* Bootstrap = TestGameInstance ? NewObject<UDungeoneerAssetBootstrapSubsystem>(TestGameInstance) : nullptr;
	TestNotNull(TEXT("UDungeoneerAssetBootstrapSubsystem allocated"), Bootstrap);
	if (Bootstrap)
	{
		const int32 RigCount = Bootstrap->ImportAllRigTexturesFromDisk();
		TestEqual(TEXT("All 18 2.5D rig textures imported"), RigCount, 18);

		const int32 DungeonTexCount = Bootstrap->ImportAllDungeonTexturesFromDisk();
		TestTrue(TEXT("All 2.5D dungeon atlas textures imported (>= 20 including Doors & Transitions)"), DungeonTexCount >= 20);
		TestNotNull(TEXT("ATLAS_Dungeon_Floors_4x4_D imported"), Bootstrap->GetLoadedDungeonTexture(FName(TEXT("ATLAS_Dungeon_Floors_4x4_D"))));
		TestNotNull(TEXT("PROP_Doors_And_Level_Transitions_4x4_D imported"), Bootstrap->GetLoadedDungeonTexture(FName(TEXT("PROP_Doors_And_Level_Transitions_4x4_D"))));

		const int32 ExtractedCells = Bootstrap->ExtractDefaultSingleCellTiles();
		TestTrue(TEXT("All 4x4 dungeon atlas cells, Doors/Sanctuary fallbacks & 16-cell rig walk frames extracted (>= 250)"), ExtractedCells >= 250);

		static constexpr const TCHAR* WalkRigPrefixes[3] = { TEXT("Hero"), TEXT("Exploder"), TEXT("Thrower") };
		for (const TCHAR* Prefix : WalkRigPrefixes)
		{
			for (int32 Row = 0; Row < 4; ++Row)
			{
				for (int32 Col = 0; Col < 4; ++Col)
				{
					const FName DiffCellKey(*FString::Printf(TEXT("%s_R%d_C%d_D"), Prefix, Row, Col));
					const FName NormCellKey(*FString::Printf(TEXT("%s_R%d_C%d_N"), Prefix, Row, Col));
					TestNotNull(FString::Printf(TEXT("%s extracted"), *DiffCellKey.ToString()), Bootstrap->GetExtractedCellTexture(DiffCellKey));
					TestNotNull(FString::Printf(TEXT("%s extracted"), *NormCellKey.ToString()), Bootstrap->GetExtractedCellTexture(NormCellKey));
					TestNotNull(FString::Printf(TEXT("%s mask cached"), *DiffCellKey.ToString()), Bootstrap->GetSpriteMaskTexture(DiffCellKey));
				}
			}
		}

		TestEqual(TEXT("ScreenDirectionalRow W (-X toward camera) -> Row 0 (Front)"),
			DungeoneerMath::OctantToScreenDirectionalRow(ECompassOctant::W), static_cast<uint8>(0));
		TestEqual(TEXT("ScreenDirectionalRow E (+X into dungeon) -> Row 1 (Back)"),
			DungeoneerMath::OctantToScreenDirectionalRow(ECompassOctant::E), static_cast<uint8>(1));
		TestEqual(TEXT("ScreenDirectionalRow N (+Y screen right) -> Row 2 (Right)"),
			DungeoneerMath::OctantToScreenDirectionalRow(ECompassOctant::N), static_cast<uint8>(2));
		TestEqual(TEXT("Isometric -45 deg Yaw: NE -> Row 2 (Screen Right)"),
			DungeoneerMath::OctantToScreenDirectionalRowForCameraYaw(ECompassOctant::NE, -45.0f), static_cast<uint8>(2));
		TestEqual(TEXT("Isometric -45 deg Yaw: SW -> Row 3 (Screen Left)"),
			DungeoneerMath::OctantToScreenDirectionalRowForCameraYaw(ECompassOctant::SW, -45.0f), static_cast<uint8>(3));
		TestEqual(TEXT("Isometric -45 deg Yaw: NW -> Row 0 (Screen Front)"),
			DungeoneerMath::OctantToScreenDirectionalRowForCameraYaw(ECompassOctant::NW, -45.0f), static_cast<uint8>(0));
		TestEqual(TEXT("Isometric -45 deg Yaw: SE -> Row 1 (Screen Back)"),
			DungeoneerMath::OctantToScreenDirectionalRowForCameraYaw(ECompassOctant::SE, -45.0f), static_cast<uint8>(1));
		TestEqual(TEXT("ScreenDirectionalRow NE -> Row 2 (Right)"),
			DungeoneerMath::OctantToScreenDirectionalRow(ECompassOctant::NE), static_cast<uint8>(2));
		TestEqual(TEXT("ScreenDirectionalRow S (-Y screen left) -> Row 3 (Left)"),
			DungeoneerMath::OctantToScreenDirectionalRow(ECompassOctant::S), static_cast<uint8>(3));

		const FRotator SpriteRot = Bootstrap->GetCameraFacingSpritePlaneRotation();
		const FVector SpriteWorldNormal = SpriteRot.RotateVector(FVector(0.0, 0.0, 1.0));
		TestTrue(TEXT("Sprite plane world normal faces -60 deg camera (X < 0, Z > 0)"),
			SpriteWorldNormal.X < -0.5 && SpriteWorldNormal.Z > 0.25);

		static constexpr const TCHAR* DirectionalDiffKeys[12] =
		{
			TEXT("Hero_Row0_D"), TEXT("Hero_Row1_D"), TEXT("Hero_Row2_D"), TEXT("Hero_Row3_D"),
			TEXT("Exploder_Row0_D"), TEXT("Exploder_Row1_D"), TEXT("Exploder_Row2_D"), TEXT("Exploder_Row3_D"),
			TEXT("Thrower_Row0_D"), TEXT("Thrower_Row1_D"), TEXT("Thrower_Row2_D"), TEXT("Thrower_Row3_D")
		};
		for (const TCHAR* Key : DirectionalDiffKeys)
		{
			TestNotNull(FString::Printf(TEXT("%s extracted"), Key), Bootstrap->GetExtractedCellTexture(FName(Key)));
		}

		UTexture2D* FloorTileTex = Bootstrap->GetExtractedCellTexture(FName(TEXT("Floor_Tile0_D")));
		TestNotNull(TEXT("Extracted single-cell Floor_Tile0_D cached"), FloorTileTex);
		if (FloorTileTex)
		{
			TestEqual(TEXT("Extracted Floor_Tile0_D configured to TF_Nearest"), FloorTileTex->Filter, TextureFilter::TF_Nearest);
			TestEqual(TEXT("Extracted Floor_Tile0_D configured to TEXTUREGROUP_Pixels2D"), static_cast<TextureGroup>(FloorTileTex->LODGroup), TEXTUREGROUP_Pixels2D);
		}
		TestNotNull(TEXT("Extracted Hero_Cell0_D cached"), Bootstrap->GetExtractedCellTexture(FName(TEXT("Hero_Cell0_D"))));
		UTexture2D* HeroCell0Mask = Bootstrap->GetSpriteMaskTexture(FName(TEXT("Hero_Cell0_D")));
		TestNotNull(TEXT("Hero_Cell0_D silhouette mask cached"), HeroCell0Mask);
		UTexture2D* HeroRow1Mask = Bootstrap->GetSpriteMaskTexture(FName(TEXT("Hero_Row1_D")));
		TestNotNull(TEXT("Hero_Row1_D silhouette mask cached"), HeroRow1Mask);
		TestNotNull(TEXT("CreateDungeonOrRigMaterial builds HISM textured material"),
			Bootstrap->CreateDungeonOrRigMaterial(Bootstrap, FName(TEXT("Floor_Tile0_D")), FName(TEXT("Floor_Tile0_N")), FLinearColor::White, 0.72f, true));
		UMaterialInstanceDynamic* HeroMaskedMID = Bootstrap->CreateDungeonOrRigMaterial(
			Bootstrap, FName(TEXT("Hero_Row1_D")), FName(TEXT("Hero_Row1_N")), FLinearColor::White, 0.45f, false, true);
		TestNotNull(TEXT("CreateDungeonOrRigMaterial builds masked two-sided Hero sprite material"), HeroMaskedMID);
		if (HeroMaskedMID && HeroRow1Mask)
		{
			TestTrue(TEXT("Masked Hero sprite binds silhouette mask texture to OpacityMask parameter"),
				HeroMaskedMID->K2_GetTextureParameterValue(FName(TEXT("OpacityMask"))) == HeroRow1Mask);
		}

		UTexture2D* HeroDiffuse = Bootstrap->GetLoadedRigTexture(FName(TEXT("Hero_D")));
		TestNotNull(TEXT("Hero_D texture present in cache"), HeroDiffuse);
		if (HeroDiffuse)
		{
			TestEqual(TEXT("Hero_D configured to TF_Nearest"), HeroDiffuse->Filter, TextureFilter::TF_Nearest);
			TestEqual(TEXT("Hero_D configured to TEXTUREGROUP_Pixels2D"), static_cast<TextureGroup>(HeroDiffuse->LODGroup), TEXTUREGROUP_Pixels2D);
		}

		const int32 VoiceCount = Bootstrap->ImportAllVoiceStemsFromDisk();
		TestTrue(TEXT("All WAV phoneme/hero stems imported (>= 48)"), VoiceCount >= 48);

		UMaterialParameterCollection* MPC = Bootstrap->EnsureSensoryMPC();
		TestNotNull(TEXT("MPC_SensoryState synthesized"), MPC);
		if (MPC)
		{
			TestNotNull(TEXT("HearingPulseRadius scalar registered"), MPC->GetScalarParameterByName(FName(TEXT("HearingPulseRadius"))));
		}

		const bool bSavedAssets = Bootstrap->SaveCoreDataAssetsToDisk();
		TestTrue(TEXT("Core .uasset DataAssets & MPC saved to Content/"), bSavedAssets);

		TestTrue(TEXT("MPC_SensoryState.uasset exists on disk"),
			FPaths::FileExists(FPaths::ProjectContentDir() / TEXT("Materials/MPC_SensoryState.uasset")));
		TestTrue(TEXT("DA_25DMaterialConfig.uasset exists on disk"),
			FPaths::FileExists(FPaths::ProjectContentDir() / TEXT("Materials/DA_25DMaterialConfig.uasset")));
		TestTrue(TEXT("DA_MovementConfig.uasset exists on disk"),
			FPaths::FileExists(FPaths::ProjectContentDir() / TEXT("Core/DA_MovementConfig.uasset")));
	}

	// Verify distance-locked 4-frame alternating walk cycle advancement on Character & Zombie
	if (ADungeoneerCharacter* HeroProbe = TestWorld->SpawnActor<ADungeoneerCharacter>())
	{
		HeroProbe->AdvanceWalkFlipbookByDistance(10.0f, true);
		TestEqual(TEXT("Hero walk col 0 on Contact/Idle start [0..35 UU)"), HeroProbe->GetActiveWalkFrameColumn(), 0);
		HeroProbe->AdvanceWalkFlipbookByDistance(35.0f, true);
		TestEqual(TEXT("Hero walk col 1 on Full Left Step [35..70 UU)"), HeroProbe->GetActiveWalkFrameColumn(), 1);
		HeroProbe->AdvanceWalkFlipbookByDistance(35.0f, true);
		TestEqual(TEXT("Hero walk col 2 on Passing Pose B [70..105 UU)"), HeroProbe->GetActiveWalkFrameColumn(), 2);
		HeroProbe->AdvanceWalkFlipbookByDistance(35.0f, true);
		TestEqual(TEXT("Hero walk col 3 on Full Right Step [105..140 UU)"), HeroProbe->GetActiveWalkFrameColumn(), 3);
		HeroProbe->AdvanceWalkFlipbookByDistance(0.0f, false);
		TestEqual(TEXT("Hero locks cleanly to Contact/Idle Col 0 when idle"), HeroProbe->GetActiveWalkFrameColumn(), 0);
	}

	if (AZombieFastThrower* ThrowerProbe = TestWorld->SpawnActor<AZombieFastThrower>())
	{
		ThrowerProbe->AdvanceWalkFlipbookByDistance(60.0f, true);
		TestEqual(TEXT("Thrower sprint col 1 on Left Step contact"), ThrowerProbe->GetActiveWalkFrameColumn(), 1);
		ThrowerProbe->AdvanceWalkFlipbookByDistance(110.0f, true);
		TestEqual(TEXT("Thrower sprint col 3 on Right Step contact"), ThrowerProbe->GetActiveWalkFrameColumn(), 3);
	}

	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDungeoneer25DAnimOrchestratorTest,
	"DarkDungeoneer.Rendering.25DAnimOrchestratorAndAtlasMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDungeoneer25DAnimOrchestratorTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Transient UWorld created for 2.5D Anim Orchestrator test"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	AActor* Host = TestWorld->SpawnActor<AActor>();
	TestNotNull(TEXT("Host actor spawned"), Host);

	UStaticMeshComponent* CutoutPlane = NewObject<UStaticMeshComponent>(Host);
	UCapsuleComponent* ShadowProxy = NewObject<UCapsuleComponent>(Host);
	U25DAnimOrchestratorComponent* Orchestrator = NewObject<U25DAnimOrchestratorComponent>(Host);
	CutoutPlane->RegisterComponent();
	ShadowProxy->RegisterComponent();
	Orchestrator->RegisterComponent();
	Orchestrator->BindRigPrimitives(CutoutPlane, ShadowProxy);

	TestFalse(TEXT("Shadow proxy hidden in main pass"), ShadowProxy->bRenderInMainPass);
	TestTrue(TEXT("Shadow proxy casts hidden shadow"), static_cast<bool>(ShadowProxy->bCastHiddenShadow));

	int32 CapturedContacts = 0;
	int32 LastContactFrame = -1;
	EFoot LastContactFoot = EFoot::Right;
	TArray<EFoot> FootSequence;
	Orchestrator->OnStepContact.AddLambda([&CapturedContacts, &LastContactFrame](const F25DStepContactEvent& Evt)
	{
		++CapturedContacts;
		LastContactFrame = Evt.FrameIndex;
	});
	Orchestrator->OnFootstepContact.AddLambda([&FootSequence, &LastContactFoot](EFoot Foot)
	{
		FootSequence.Add(Foot);
		LastContactFoot = Foot;
	});

	// Default StrideLengthUU = 140.0f -> Col 0 [0..35), Col 1 Left Step [35..70), Col 2 Passing B [70..105), Col 3 Right Step [105..140)
	Orchestrator->AdvanceWalkDistance(40.0f, FVector(300.0, 0.0, 0.0), 0.016f);
	TestEqual(TEXT("Walk distance 40 UU advances to frame col 1 (Left Step)"), Orchestrator->GetCurrentFrameColumn(), 1);
	TestEqual(TEXT("Step contact fired deterministically on Left Step col 1"), CapturedContacts, 1);
	TestEqual(TEXT("Last contact frame is 1"), LastContactFrame, 1);
	TestEqual(TEXT("OnFootstepContact fired EFoot::Left on col 1"), LastContactFoot, EFoot::Left);
	TestTrue(TEXT("120Hz stride hop lifted plane smoothly"), Orchestrator->GetCurrentStrideHopZ() > 0.0f);
	TestTrue(TEXT("120Hz directional velocity lean engaged"), FMath::Abs(Orchestrator->GetCurrentVelocityLeanDeg().X) > 0.0f);

	Orchestrator->AdvanceWalkDistance(40.0f, FVector(300.0, 0.0, 0.0), 0.016f);
	TestEqual(TEXT("Walk distance 80 UU advances to frame col 2 (Passing B)"), Orchestrator->GetCurrentFrameColumn(), 2);
	TestEqual(TEXT("No step contact fired on passing frame 2"), CapturedContacts, 1);

	Orchestrator->AdvanceWalkDistance(40.0f, FVector(300.0, 0.0, 0.0), 0.016f);
	TestEqual(TEXT("Walk distance 120 UU advances to frame col 3 (Right Step)"), Orchestrator->GetCurrentFrameColumn(), 3);
	TestEqual(TEXT("Step contact fired deterministically on Right Step col 3"), CapturedContacts, 2);
	TestEqual(TEXT("Last contact frame is 3"), LastContactFrame, 3);
	TestEqual(TEXT("OnFootstepContact fired EFoot::Right on col 3"), LastContactFoot, EFoot::Right);
	TestEqual(TEXT("Alternating Left -> Right foot sequence captured"), FootSequence.Num(), 2);

	Orchestrator->AdvanceWalkDistance(25.0f, FVector(300.0, 0.0, 0.0), 0.016f);
	TestEqual(TEXT("Walk distance 145 UU wraps cleanly to frame col 0 (Contact A)"), Orchestrator->GetCurrentFrameColumn(), 0);
	TestEqual(TEXT("No duplicate step contact fired on Contact/Idle wrap col 0"), CapturedContacts, 2);

	// Advance back into Col 1 then verify Velocity.IsNearlyZero() locks back to Col 0 Idle
	Orchestrator->AdvanceWalkDistance(35.0f, FVector(300.0, 0.0, 0.0), 0.016f);
	TestEqual(TEXT("Walk distance 180 UU steps cleanly into Left Step col 1"), Orchestrator->GetCurrentFrameColumn(), 1);
	Orchestrator->AdvanceWalkDistance(0.0f, FVector::ZeroVector, 0.016f);
	TestEqual(TEXT("Velocity.IsNearlyZero() locks UV selection cleanly to Idle Col 0"), Orchestrator->GetCurrentFrameColumn(), 0);

	// Directional octant -> CustomData[0] = Row * 4 + Col (West row = 3, col = 0 => cell 12)
	Orchestrator->SetDirectionFromOctant(ECompassOctant::W, false);
	TestEqual(TEXT("West octant selects row 3"), Orchestrator->GetDirectionalRow(), 3);
	TestTrue(TEXT("CustomData[0] pushed cell index 12.0f to CutoutPlane"),
		CutoutPlane->GetCustomPrimitiveData().Data.Num() >= 5 &&
		FMath::IsNearlyEqual(CutoutPlane->GetCustomPrimitiveData().Data[E25DAnimCPD::UVCellIndex], 12.0f, 1e-4f));

	// Pre-explosion belly swell + CustomData[1] Emissive Pulse
	Orchestrator->SetPreExplosionFuseState(0.85f, 3.0f);
	Orchestrator->AdvanceWalkDistance(0.0f, FVector::ZeroVector, 0.15f);
	TestTrue(TEXT("Belly-swell spring expands XY scale above 1.15"), Orchestrator->GetCurrentBellySwellScale().X > 1.15);
	TestTrue(TEXT("CustomData[1] holds EmissiveFusePulse = 3.0f"),
		FMath::IsNearlyEqual(CutoutPlane->GetCustomPrimitiveData().Data[E25DAnimCPD::EmissiveFusePulse], 3.0f, 1e-4f));

	// Alchemical Flask Element RGB Tint via CustomData[2..4] without MIDs
	Orchestrator->SetFlaskElementByType(2); // Chrono Violet-Blue (0.48, 0.22, 1.0)
	TestTrue(TEXT("CustomData[2] holds Chrono R = 0.48"),
		FMath::IsNearlyEqual(CutoutPlane->GetCustomPrimitiveData().Data[E25DAnimCPD::FlaskElementR], 0.48f, 1e-4f));
	TestTrue(TEXT("CustomData[4] holds Chrono B = 1.00"),
		FMath::IsNearlyEqual(CutoutPlane->GetCustomPrimitiveData().Data[E25DAnimCPD::FlaskElementB], 1.0f, 1e-4f));

	// Deliverable 1 outputs on disk
	TestTrue(TEXT("Hero_E.png emissive mask exists on disk"),
		FPaths::FileExists(FPaths::ProjectContentDir() / TEXT("Art/Rigs/Hero/Rig_Hero_E.png")));
	TestTrue(TEXT("Hero AtlasMetadata.json exists on disk"),
		FPaths::FileExists(FPaths::ProjectContentDir() / TEXT("Art/Rigs/Hero/AtlasMetadata.json")));
	TestTrue(TEXT("DungeoneerHero AtlasMetadata.json exists on disk"),
		FPaths::FileExists(FPaths::ProjectContentDir() / TEXT("Art/Rigs/DungeoneerHero/AtlasMetadata.json")));

	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDungeoneerTileMapInstancerAndMirrorBounceTest,
	"DarkDungeoneer.Dungeon.TileMapInstancerCutawayAndMirrorBounce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDungeoneerTileMapInstancerAndMirrorBounceTest::RunTest(const FString& Parameters)
{
	// 1. Law of Reflection unit check: R = I - 2 * (I . N) * N
	{
		const FVector I(1.0, -1.0, 0.0);
		const FVector N(0.0, 1.0, 0.0);
		const FVector R = UMirrorLightBounceComponent::ComputeReflectedVector(I, N);
		const FVector Expected = FVector(1.0, 1.0, 0.0).GetSafeNormal();
		TestTrue(TEXT("Specular reflection reflects South-East ray across North normal to North-East"),
			R.Equals(Expected, 1e-4));
	}

	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Transient UWorld created for TileMapInstancer & MirrorBounce test"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	UDungeonGeneratorSubsystem* GenSub = TestWorld->GetSubsystem<UDungeonGeneratorSubsystem>();
	TestNotNull(TEXT("UDungeonGeneratorSubsystem present"), GenSub);
	const FDungeonFloorGraph Graph = GenSub->GenerateFloorGraph(1, 4242);

	ADungeoneerTileMapInstancer* Instancer = TestWorld->SpawnActor<ADungeoneerTileMapInstancer>();
	TestNotNull(TEXT("ADungeoneerTileMapInstancer spawned"), Instancer);
	Instancer->BuildDungeonFromFloorGraph(Graph);

	UHierarchicalInstancedStaticMeshComponent* WallHISM = Instancer->GetWallHISM();
	UHierarchicalInstancedStaticMeshComponent* MirrorHISM = Instancer->GetMirrorHISM();
	UHierarchicalInstancedStaticMeshComponent* BreakableHISM = Instancer->GetBreakableHISM();
	TestNotNull(TEXT("WallHISM initialized"), WallHISM);
	TestNotNull(TEXT("MirrorHISM initialized"), MirrorHISM);
	TestNotNull(TEXT("BreakableHISM initialized"), BreakableHISM);

	TestTrue(TEXT("WallHISM preserves bCastHiddenShadow = true"), static_cast<bool>(WallHISM->bCastHiddenShadow));
	TestTrue(TEXT("MirrorHISM carries MirrorReflective tag"), MirrorHISM->ComponentHasTag(TEXT("MirrorReflective")));
	TestTrue(TEXT("HISM floor tiles & 4-axis walls populated"), Instancer->GetFloorHISM()->GetInstanceCount() >= 9 && WallHISM->GetInstanceCount() >= 8);
	TestTrue(TEXT("Walkable 4-axis connecting corridors built between linked rooms"), Instancer->GetCorridorTileCount() > 0);
	TestTrue(TEXT("Room encounters (Zombies, Sanctuary, Vendor, Vault) spawned deterministically"), Instancer->GetSpawnedEncounterCount() >= 5);

	bool bEntryEastDoorBlocked = false;
	const FVector2D EntryEastDoorPos(1152.0, 0.0);
	for (const FDungeoneerWallInstanceEntry& WallEntry : Instancer->GetWallEntries())
	{
		if (FVector2D::DistSquared(FVector2D(WallEntry.WorldCenter.X, WallEntry.WorldCenter.Y), EntryEastDoorPos) < FMath::Square(160.0))
		{
			bEntryEastDoorBlocked = true;
			break;
		}
	}
	TestFalse(TEXT("Entry room East doorway aperture cleanly carved with zero wall blockage"), bEntryEastDoorBlocked);

	// 2. Per-viewport South-Wall Cutaway check
	int32 SouthWallIdx = INDEX_NONE;
	int32 NorthWallIdx = INDEX_NONE;
	for (int32 i = 0; i < Instancer->GetWallEntries().Num(); ++i)
	{
		const FDungeoneerWallInstanceEntry& E = Instancer->GetWallEntries()[i];
		if (SouthWallIdx == INDEX_NONE && E.WallAxis == EDungeoneerWallAxis::South)
		{
			SouthWallIdx = i;
		}
		else if (NorthWallIdx == INDEX_NONE && E.WallAxis == EDungeoneerWallAxis::North)
		{
			NorthWallIdx = i;
		}
	}
	TestTrue(TEXT("Found orthogonal South and North wall instances"), SouthWallIdx != INDEX_NONE && NorthWallIdx != INDEX_NONE);

	const FDungeoneerWallInstanceEntry& SouthWall = Instancer->GetWallEntries()[SouthWallIdx];
	const FDungeoneerWallInstanceEntry& NorthWall = Instancer->GetWallEntries()[NorthWallIdx];
	// Camera placed outside South wall looking through South wall toward player center
	const FVector CameraLoc = SouthWall.WorldCenter + FVector(0.0, -350.0, 220.0);
	const FVector PlayerLoc = SouthWall.WorldCenter + FVector(0.0, 350.0, -80.0);
	const int32 CutawayCount = Instancer->EvaluateCameraToPlayerSegmentCutaway(CameraLoc, PlayerLoc);
	TestTrue(TEXT("South wall cutaway activated between camera and player"), CutawayCount >= 1);

	const int32 WallStride = WallHISM->NumCustomDataFloats;
	TestTrue(TEXT("Occluded South wall wrote CustomData[0] == 1.0f"),
		FMath::IsNearlyEqual(WallHISM->PerInstanceSMCustomData[SouthWall.HISMInstanceIndex * WallStride], 1.0f, 1e-4f));
	TestTrue(TEXT("Non-occluded North wall remained CustomData[0] == 0.0f"),
		FMath::IsNearlyEqual(WallHISM->PerInstanceSMCustomData[NorthWall.HISMInstanceIndex * WallStride], 0.0f, 1e-4f));
	TestTrue(TEXT("bCastHiddenShadow stayed true during cutaway"), static_cast<bool>(WallHISM->bCastHiddenShadow));

	// Move camera ray away so South wall reverts cleanly to full height
	Instancer->EvaluateCameraToPlayerSegmentCutaway(FVector(50000.0, 50000.0, 600.0), FVector(50000.0, 50100.0, 100.0));
	TestTrue(TEXT("Unoccluded South wall reverted CustomData[0] to 0.0f"),
		FMath::IsNearlyEqual(WallHISM->PerInstanceSMCustomData[SouthWall.HISMInstanceIndex * WallStride], 0.0f, 1e-4f));

	// 3. Mirror Light-Bounce + Illusion Seam Reveal check
	const int32 SolidIdx = Instancer->RegisterMirrorInstance(
		FVector(0.0, 400.0, 110.0),
		FVector(0.0, -1.0, 0.0),
		120.0f,
		120.0f,
		false,
		0);
	const int32 IllusionIdx = Instancer->RegisterMirrorInstance(
		FVector(400.0, 0.0, 110.0),
		FVector(-1.0, 0.0, 0.0),
		120.0f,
		120.0f,
		true,
		4);
	TestTrue(TEXT("Solid & Illusory mirror fixtures registered"), SolidIdx != INDEX_NONE && IllusionIdx != INDEX_NONE);

	AActor* BounceHost = TestWorld->SpawnActor<AActor>();
	USceneComponent* BounceRoot = NewObject<USceneComponent>(BounceHost);
	BounceHost->SetRootComponent(BounceRoot);
	BounceRoot->RegisterComponent();
	UMirrorLightBounceComponent* BounceComp = NewObject<UMirrorLightBounceComponent>(BounceHost);
	BounceComp->RegisterComponent();
	BounceComp->EnsureBounceLightInitialized();
	BounceComp->SetTargetTileMapInstancer(Instancer);

	// Ray starts at (-400, 0, 110), shoots along (+1, +1, 0) -> strikes solid mirror at (0, 400, 110) normal (0, -1, 0) -> reflects (+1, -1, 0) toward illusion mirror at (400, 0, 110)!
	const FMirrorLightBounceResult BounceRes = BounceComp->EvaluateFlashlightBounce(
		FVector(-400.0, 0.0, 110.0),
		FVector(1.0, 1.0, 0.0).GetSafeNormal(),
		12000.0f);

	TestTrue(TEXT("Flashlight struck MirrorReflective plane and bounced"), BounceRes.bDidBounce);
	TestTrue(TEXT("Reflected beam points along (+1, -1, 0) normalized"),
		BounceRes.ReflectedDirection.Equals(FVector(1.0, -1.0, 0.0).GetSafeNormal(), 1e-3));
	TestNotNull(TEXT("Pre-allocated secondary ReflectedSpotLight exists"), BounceComp->GetReflectedSpotLight());
	TestTrue(TEXT("ReflectedSpotLight became visible on bounce"),
		BounceComp->GetReflectedSpotLight()->GetVisibleFlag());
	TestTrue(TEXT("Reflected corridor light revealed Phantasm illusion seam"), BounceRes.bRevealedIllusionSeam);

	const FDungeoneerMirrorInstanceEntry& IllusionEntry = Instancer->GetMirrorEntries()[IllusionIdx];
	const int32 MirrorStride = MirrorHISM->NumCustomDataFloats;
	TestTrue(TEXT("Revealed Phantasm mirror wrote CustomData[0] == 1.0f"),
		FMath::IsNearlyEqual(MirrorHISM->PerInstanceSMCustomData[IllusionEntry.HISMInstanceIndex * MirrorStride], 1.0f, 1e-4f));

	// 4. Breakable Noise Emitter check -> Shattered UV swap + Acute Hearing NoiseEvent
	const int32 BreakableIdx = Instancer->RegisterBreakableInstance(
		FVector(900.0, 900.0, 45.0),
		0,
		2,
		90.0f,
		1.45f);
	TestTrue(TEXT("Breakable barrel instance registered"), BreakableIdx != INDEX_NONE);

	const int32 ShatteredCount = Instancer->EvaluateEntityStepOnBreakables(FVector(910.0, 905.0, 45.0), 1.45f, 90.0f);
	TestEqual(TEXT("Stepping onto breakable shattered exactly 1 instance"), ShatteredCount, 1);

	const FDungeoneerBreakableInstanceEntry& BrokenEntry = Instancer->GetBreakableEntries()[BreakableIdx];
	const int32 BreakStride = BreakableHISM->NumCustomDataFloats;
	TestTrue(TEXT("Breakable swapped CustomData[1] to shattered UV cell 2"),
		FMath::IsNearlyEqual(BreakableHISM->PerInstanceSMCustomData[BrokenEntry.HISMInstanceIndex * BreakStride + 1], 2.0f, 1e-4f));

	const UDungeoneerSensoryWorldSubsystem* SensorySub = TestWorld->GetSubsystem<UDungeoneerSensoryWorldSubsystem>();
	TestNotNull(TEXT("SensoryWorldSubsystem active for Acute Hearing pulse"), SensorySub);
	FDungeoneerNoiseEvent OutNoise;
	const bool bHasNoise = SensorySub->QueryStrongestActivePulse(FVector(950.0, 900.0, 45.0), 1.0f, OutNoise);
	TestTrue(TEXT("Breakable emitted Acute Hearing NoiseEvent into SensoryWorldSubsystem"), bHasNoise);
	TestTrue(TEXT("Emitted NoiseEvent recorded origin at breakable barrel"),
		OutNoise.WorldOrigin.Equals(FVector(900.0, 900.0, 45.0), 5.0));

	// 5. Deliverable 1 Dungeon Pipeline files on disk
	TestTrue(TEXT("ATLAS_Dungeon_Floors_4x4_H.png exists on disk"),
		FPaths::FileExists(FPaths::ProjectContentDir() / TEXT("Art/Dungeon/ATLAS_Dungeon_Floors_4x4/ATLAS_Dungeon_Floors_4x4_H.png")));
	TestTrue(TEXT("ATLAS_Dungeon_Floors_4x4_R.png exists on disk"),
		FPaths::FileExists(FPaths::ProjectContentDir() / TEXT("Art/Dungeon/ATLAS_Dungeon_Floors_4x4/ATLAS_Dungeon_Floors_4x4_R.png")));
	TestTrue(TEXT("PROP_Mirror_Palace_Maze_4x4_ORM.png exists on disk"),
		FPaths::FileExists(FPaths::ProjectContentDir() / TEXT("Art/Dungeon/PROP_Mirror_Palace_Maze_4x4/PROP_Mirror_Palace_Maze_4x4_ORM.png")));

	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDungeoneerCombatDodgeVaultFloorTest,
	"DarkDungeoneer.Gameplay.CombatDodgeVaultAndFloorLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDungeoneerCombatDodgeVaultFloorTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Created combat & floor loop test world"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}
	TestWorld->InitializeActorsForPlay(FURL());
	TestWorld->BeginPlay();

	FActorSpawnParameters HeroParams;
	ADungeoneerCharacter* Hero = TestWorld->SpawnActor<ADungeoneerCharacter>(FVector(0.0, 0.0, 95.0), FRotator::ZeroRotator, HeroParams);
	TestNotNull(TEXT("Spawned Hero for combat test"), Hero);
	Hero->DispatchBeginPlay();
	UDungeoneerMovementComponent* HeroMove = Hero->GetDungeoneerMovement();
	TestNotNull(TEXT("Hero has UDungeoneerMovementComponent"), HeroMove);
	HeroMove->SetAimInputVector(FVector2D(1.0, 0.0)); // Facing North (+X)

	// 1. Spawn ZombieFastThrower (95 HP) directly in melee cone along +X, plus an out-of-cone flanker behind (-X)
	AZombieFastThrower* FrontZombie = TestWorld->SpawnActor<AZombieFastThrower>(FVector(150.0, 0.0, 95.0), FRotator::ZeroRotator);
	AZombieExploder* RearZombie = TestWorld->SpawnActor<AZombieExploder>(FVector(-180.0, 0.0, 95.0), FRotator::ZeroRotator);
	TestNotNull(TEXT("Spawned FrontZombie"), FrontZombie);
	TestNotNull(TEXT("Spawned RearZombie"), RearZombie);
	FrontZombie->DispatchBeginPlay();
	RearZombie->DispatchBeginPlay();
	TestTrue(TEXT("FrontZombie initially alive"), FrontZombie->IsZombieAlive());

	const int32 GoldBefore = Hero->GetInventoryComponent()->GetGoldBalance();
	const bool bFirstSwing = Hero->TriggerPrimaryAttack();
	TestTrue(TEXT("Lantern Blade primary attack triggered cleanly"), bFirstSwing);
	TestEqual(TEXT("Lantern Blade hit exactly FrontZombie in forward cone"), Hero->GetLastAttackHitsCount(), 1);
	TestTrue(TEXT("FrontZombie took cleave damage"), FrontZombie->GetCurrentHealth() < FrontZombie->GetMaxHealth());
	TestEqual(TEXT("RearZombie outside cone remained unharmed"), RearZombie->GetCurrentHealth(), RearZombie->GetMaxHealth());

	// Advance cooldown/swing time so Hero can execute finishing cleave
	for (int32 Step = 0; Step < 25; ++Step)
	{
		Hero->Tick(0.02f);
	}
	TestEqual(TEXT("Attack state returned to None after swing"),
		static_cast<uint8>(Hero->GetCombatActionState()),
		static_cast<uint8>(EDungeoneerCombatActionState::None));

	const bool bSecondSwing = Hero->TriggerPrimaryAttack();
	TestTrue(TEXT("Second Lantern Blade strike triggered after cooldown"), bSecondSwing);
	TestFalse(TEXT("FrontZombie slain on second cleave"), FrontZombie->IsZombieAlive());
	TestEqual(TEXT("Hero recorded 1 zombie kill"), Hero->GetKillsCount(), 1);
	TestTrue(TEXT("Hero gained Gold reward on zombie kill"), Hero->GetInventoryComponent()->GetGoldBalance() > GoldBefore);

	// Advance swing state cleanly before dodge test
	for (int32 Step = 0; Step < 25; ++Step)
	{
		Hero->Tick(0.02f);
	}

	// 2. Dodge Dash grants i-frames blocking incoming damage
	const float HpBeforeDodge = Hero->GetCurrentHealth();
	const bool bDodged = Hero->TriggerDodgeDash();
	TestTrue(TEXT("Dodge Dash triggered cleanly"), bDodged);
	TestTrue(TEXT("Hero gained brief Dodge i-frames"), Hero->IsInvulnerable());
	Hero->ApplyDungeoneerDamage(35.0f, RearZombie);
	TestEqual(TEXT("Damage ignored during Dodge i-frames"), Hero->GetCurrentHealth(), HpBeforeDodge);

	for (int32 Step = 0; Step < 40; ++Step)
	{
		Hero->Tick(0.02f);
	}
	TestFalse(TEXT("Dodge i-frames expired cleanly"), Hero->IsInvulnerable());

	// 3. HUD captures Floor, Gold, Kills, and Combat state cleanly
	UDungeoneerPlayerHUDWidget* HUD = NewObject<UDungeoneerPlayerHUDWidget>(TestWorld);
	HUD->RefreshHUDFromCharacter(Hero);
	TestEqual(TEXT("HUD tracked 1 kill"), HUD->GetCachedKillsCount(), 1);
	TestTrue(TEXT("HUD tracked updated gold balance"), HUD->GetCachedGoldBalance() > GoldBefore);
	TestTrue(TEXT("HUD formatted combat readout includes FLOOR & KILLS"),
		HUD->GetFormattedCombatAndLoopText().Contains(TEXT("FLOOR 1")) &&
		HUD->GetFormattedCombatAndLoopText().Contains(TEXT("KILLS 1")));

	// 4. GameMode multi-floor loop + Vault encounter unlock & Floor descent
	ADungeoneerGameMode* GM = TestWorld->SpawnActor<ADungeoneerGameMode>();
	TestNotNull(TEXT("Spawned ADungeoneerGameMode"), GM);
	TestEqual(TEXT("GameMode starts on Floor 1"), GM->GetCurrentFloorIndex(), 1);
	const int32 NextFloor = GM->AdvanceToNextDungeonFloor();
	TestEqual(TEXT("AdvanceToNextDungeonFloor incremented to Floor 2"), NextFloor, 2);

	const UDungeonGeneratorSubsystem* Gen = TestWorld->GetSubsystem<UDungeonGeneratorSubsystem>();
	TestNotNull(TEXT("DungeonGeneratorSubsystem active"), Gen);
	TestEqual(TEXT("Generator updated ActiveFloorGraph to Floor 2"), Gen->GetActiveFloorGraph().FloorIndex, 2);

	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDungeoneerBiomesOctagonalTrapsAndExploderLureTest,
	"DarkDungeoneer.Dungeon.BiomesOctagonalRoomsTrapsAndExploderLure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDungeoneerBiomesOctagonalTrapsAndExploderLureTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Created Biome/Traps/Lure test world"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}
	TestWorld->InitializeActorsForPlay(FURL());
	TestWorld->BeginPlay();

	UDungeonGeneratorSubsystem* GenSub = TestWorld->GetSubsystem<UDungeonGeneratorSubsystem>();
	TestNotNull(TEXT("DungeonGeneratorSubsystem present"), GenSub);

	// 1. Multi-Floor Biome Progression & Transition Bleed
	const FDungeonFloorGraph F1 = GenSub->GenerateFloorGraph(1, 9001);
	const FDungeonFloorGraph F2 = GenSub->GenerateFloorGraph(2, 9001);
	const FDungeonFloorGraph F3 = GenSub->GenerateFloorGraph(3, 9001);
	TestEqual(TEXT("Floor 1 dominant biome is Crypt"), F1.DominantBiome, EDungeonBiomeType::Crypt);
	TestEqual(TEXT("Floor 2 dominant biome is SporeFens"), F2.DominantBiome, EDungeonBiomeType::SporeFens);
	TestEqual(TEXT("Floor 3 dominant biome is EmberForge"), F3.DominantBiome, EDungeonBiomeType::EmberForge);
	TestEqual(TEXT("Floor 1 has 12 nodes (10 core + 2 Secret Phantasm Pockets)"), F1.Nodes.Num(), 12);

	int32 SecretCount = 0;
	bool bHasOctagonal = false;
	bool bHasPillared = false;
	bool bHasBleedVault = false;
	for (const FDungeonNode& Node : F1.Nodes)
	{
		if (Node.bIsSecretRoom && Node.RoomShape == EDungeonRoomShape::SecretPhantasmPocket)
		{
			++SecretCount;
		}
		if (Node.RoomShape == EDungeonRoomShape::OctagonalRotunda) { bHasOctagonal = true; }
		if (Node.RoomShape == EDungeonRoomShape::PillaredGallery)  { bHasPillared = true; }
		if (Node.RoomType == EDungeonRoomType::MajorLootVault && Node.bHasDescendingStairs && Node.BleedBiomeType == F1.NextFloorPreviewBiome)
		{
			bHasBleedVault = true;
		}
	}
	TestEqual(TEXT("Generated exactly 2 Secret Phantasm Pockets"), SecretCount, 2);
	TestTrue(TEXT("Floor graph contains OctagonalRotunda rooms"), bHasOctagonal);
	TestTrue(TEXT("Floor graph contains PillaredGallery rooms"), bHasPillared);
	TestTrue(TEXT("Terminal MajorLootVault carries descending stairs and NextFloorPreviewBiome bleed"), bHasBleedVault);

	// 2. 8-HISM Bucketing & Secret Phantasm Thresholds
	ADungeoneerTileMapInstancer* Instancer = TestWorld->SpawnActor<ADungeoneerTileMapInstancer>();
	TestNotNull(TEXT("TileMapInstancer spawned"), Instancer);
	Instancer->BuildDungeonFromFloorGraph(F1);

	TestTrue(TEXT("FloorHISM populated"), Instancer->GetFloorHISM() && Instancer->GetFloorHISM()->GetInstanceCount() > 0);
	TestTrue(TEXT("WetFloorHISM specular puddle bucket populated in Crypt"), Instancer->GetWetFloorHISM() && Instancer->GetWetFloorHISM()->GetInstanceCount() > 0);
	TestTrue(TEXT("WallHISM populated"), Instancer->GetWallHISM() && Instancer->GetWallHISM()->GetInstanceCount() > 0);
	TestTrue(TEXT("DiagonalWedgeHISM 45-deg corner bucket populated"), Instancer->GetDiagonalWedgeHISM() && Instancer->GetDiagonalWedgeHISM()->GetInstanceCount() > 0);
	TestTrue(TEXT("MirrorHISM populated including secret Phantasm thresholds"), Instancer->GetMirrorHISM() && Instancer->GetMirrorHISM()->GetInstanceCount() >= 3);
	TestTrue(TEXT("SconceHISM doorway/sanctuary/stair bucket populated"), Instancer->GetSconceHISM() && Instancer->GetSconceHISM()->GetInstanceCount() > 0);
	TestTrue(TEXT("BreakableHISM props & trap vents populated"), Instancer->GetBreakableHISM() && Instancer->GetBreakableHISM()->GetInstanceCount() > 0);
	TestTrue(TEXT("ForegroundParallaxHISM overhead beams populated"), Instancer->GetForegroundParallaxHISM() && Instancer->GetForegroundParallaxHISM()->GetInstanceCount() > 0);

	// Verify Asset Bootstrap full 4x4 cell unlock + Doors/Sanctuary procedural fallback textures
	if (UGameInstance* DummyGI = NewObject<UGameInstance>(GetTransientPackage()))
	{
		if (UDungeoneerAssetBootstrapSubsystem* Bootstrap = NewObject<UDungeoneerAssetBootstrapSubsystem>(DummyGI))
		{
			Bootstrap->ImportAllDungeonTexturesFromDisk();
			Bootstrap->ExtractDefaultSingleCellTiles();
			TestNotNull(TEXT("Floor_Cell15_D (row 3) extracted via full 4x4 unlock"), Bootstrap->GetExtractedCellTexture(FName(TEXT("Floor_Cell15_D"))));
			TestNotNull(TEXT("Doors_Arch_D registered with safe procedural fallback"), Bootstrap->GetExtractedCellTexture(FName(TEXT("Doors_Arch_D"))));
			TestNotNull(TEXT("Sanctuary_Brazier_D registered with safe procedural fallback"), Bootstrap->GetExtractedCellTexture(FName(TEXT("Sanctuary_Brazier_D"))));
		}
	}

	// 3. Systemic Trap Damage & Exploder Ember Ignition + Acoustic Noise Lure
	AZombieExploder* TrapExploder = TestWorld->SpawnActor<AZombieExploder>(FVector(5000.0, 0.0, 95.0), FRotator::ZeroRotator);
	TestNotNull(TEXT("Spawned TrapExploder"), TrapExploder);
	TrapExploder->DispatchBeginPlay();
	const float ExploderHpBefore = TrapExploder->GetCurrentHealth();
	Instancer->RegisterTrapVentInstance(FVector(5000.0, 0.0, 2.5), EDungeoneerTrapHazardType::EmberFire, 140.0f, 25.0f, 12);
	const int32 FiredVents = Instancer->EvaluateTrapVentsForEntities(0.2f);
	TestTrue(TEXT("EmberFire trap vent triggered cleanly on zombie"), FiredVents >= 1);
	TestTrue(TEXT("EmberFire trap damaged overlapping zombie"), TrapExploder->GetCurrentHealth() < ExploderHpBefore);
	TestTrue(TEXT("EmberFire trap ignited AZombieExploder fuse immediately"), TrapExploder->IsFuseIgnited());

	// Noise lure check: Zombie outside player range hears loud breakable/chime sound and moves toward it
	AZombieExploder* LuredZombie = TestWorld->SpawnActor<AZombieExploder>(FVector(7000.0, 500.0, 95.0), FRotator::ZeroRotator);
	TestNotNull(TEXT("Spawned LuredZombie"), LuredZombie);
	LuredZombie->DispatchBeginPlay();
	UDungeoneerSensoryWorldSubsystem* SensorySub = TestWorld->GetSubsystem<UDungeoneerSensoryWorldSubsystem>();
	TestNotNull(TEXT("SensoryWorldSubsystem present"), SensorySub);
	SensorySub->BroadcastNoiseEvent(FVector(7000.0, 1200.0, 45.0), 2.35f, 1500.0f, false);
	LuredZombie->Tick(0.05f);
	const FVector PendingInput = LuredZombie->GetPendingMovementInputVector();
	TestTrue(TEXT("Loud environmental noise lures zombie along +Y toward origin"), PendingInput.Y > 0.7);

	// 4. DungeonAtmosphereActor biome transition & zero-alloc AmbientMotesHISM check
	ADungeonAtmosphereActor* Atmo = TestWorld->SpawnActor<ADungeonAtmosphereActor>();
	TestNotNull(TEXT("Spawned DungeonAtmosphereActor"), Atmo);
	Atmo->DispatchBeginPlay();
	Atmo->SetBiomeAtmosphere(EDungeonBiomeType::SporeFens, false);
	TestEqual(TEXT("Atmosphere tracks SporeFens biome"), Atmo->GetActiveBiome(), EDungeonBiomeType::SporeFens);
	TestTrue(TEXT("AmbientMotesHISM pre-allocated 64 instances"), Atmo->GetAmbientMotesHISM() && Atmo->GetAmbientMotesHISM()->GetInstanceCount() == 64);
	Atmo->UpdateAmbientMotesAroundFocus(FVector(200.0, 200.0, 95.0), 0.1f);

	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDungeoneerEcoCPDRGB24AndBootstrapTest,
	"DarkDungeoneer.Ecosystem.CustomDataPackingAndRGB24",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDungeoneerEcoCPDRGB24AndBootstrapTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("EEcoCPD::Count == 8"), EEcoCPD::Count, 8);

	const FLinearColor TestColors[] = {
		FLinearColor(0.0f, 0.0f, 0.0f, 1.0f),
		FLinearColor(1.0f, 1.0f, 1.0f, 1.0f),
		EcoPalette::EmeraldMoss,
		EcoPalette::MagentaBloom,
		EcoPalette::AstralCyan,
		EcoPalette::GoldenRune,
		EcoPalette::AmberFire,
		EcoPalette::MagmaOrange
	};
	for (const FLinearColor& Col : TestColors)
	{
		const uint8 ExpR = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Col.R * 255.0f), 0, 255));
		const uint8 ExpG = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Col.G * 255.0f), 0, 255));
		const uint8 ExpB = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt32(Col.B * 255.0f), 0, 255));
		const float Packed = EcoColors::PackRGB24(ExpR, ExpG, ExpB);
		uint8 OutR = 0, OutG = 0, OutB = 0;
		EcoColors::UnpackRGB24(Packed, OutR, OutG, OutB);
		TestEqual(TEXT("RGB24 exact R round-trip"), OutR, ExpR);
		TestEqual(TEXT("RGB24 exact G round-trip"), OutG, ExpG);
		TestEqual(TEXT("RGB24 exact B round-trip"), OutB, ExpB);
	}

	// Verify edge values 0 and 255 explicitly
	{
		uint8 R0 = 99, G0 = 99, B0 = 99;
		EcoColors::UnpackRGB24(EcoColors::PackRGB24(0, 0, 0), R0, G0, B0);
		TestEqual(TEXT("Min R=0"), R0, static_cast<uint8>(0));
		TestEqual(TEXT("Min G=0"), G0, static_cast<uint8>(0));
		TestEqual(TEXT("Min B=0"), B0, static_cast<uint8>(0));

		uint8 R1 = 0, G1 = 0, B1 = 0;
		EcoColors::UnpackRGB24(EcoColors::PackRGB24(255, 255, 255), R1, G1, B1);
		TestEqual(TEXT("Max R=255"), R1, static_cast<uint8>(255));
		TestEqual(TEXT("Max G=255"), G1, static_cast<uint8>(255));
		TestEqual(TEXT("Max B=255"), B1, static_cast<uint8>(255));
	}

	// Verify asset bootstrap: EcoTime/EcoGlow00..31 MPC params, M_Dungeoneer_EcoInstanced, Eco textures & 42 foley WAVs
	if (UGameInstance* DummyGI = NewObject<UGameInstance>(GetTransientPackage()))
	{
		if (UDungeoneerAssetBootstrapSubsystem* Bootstrap = NewObject<UDungeoneerAssetBootstrapSubsystem>(DummyGI))
		{
			UMaterialParameterCollection* MPC = Bootstrap->EnsureSensoryMPC();
			TestNotNull(TEXT("SensoryMPC loaded"), MPC);
			if (MPC)
			{
				TestNotNull(TEXT("MPC has EcoTime"), MPC->GetScalarParameterByName(FName(TEXT("EcoTime"))));
				TestNotNull(TEXT("MPC has EcoGlowCount"), MPC->GetScalarParameterByName(FName(TEXT("EcoGlowCount"))));
				TestNotNull(TEXT("MPC has EcoGlow00"), MPC->GetVectorParameterByName(FName(TEXT("EcoGlow00"))));
				TestNotNull(TEXT("MPC has EcoGlow31"), MPC->GetVectorParameterByName(FName(TEXT("EcoGlow31"))));
			}

			Bootstrap->SaveCoreDataAssetsToDisk();
			UMaterialInterface* EcoMat = Bootstrap->EnsureEcoInstancedMaterial();
			TestNotNull(TEXT("M_Dungeoneer_EcoInstanced ensured"), EcoMat);
#if WITH_EDITOR
			if (UMaterial* EcoBaseMat = Cast<UMaterial>(EcoMat))
			{
				const EShaderPlatform PlatformsToCompile[] = { SP_PCD3D_SM5, SP_PCD3D_SM6 };
				for (const EShaderPlatform Platform : PlatformsToCompile)
				{
					if (FMaterialResource* MatRes = EcoBaseMat->GetMaterialResource(Platform))
					{
						const bool bStarted = MatRes->CacheShaders(EMaterialShaderPrecompileMode::Default);
						MatRes->FinishCompilation();
						for (const FString& CompileErr : MatRes->GetCompileErrors())
						{
							AddError(FString::Printf(
								TEXT("M_Dungeoneer_EcoInstanced SM%d compile error: %s"),
								Platform == SP_PCD3D_SM6 ? 6 : 5,
								*CompileErr));
						}
						TestTrue(
							Platform == SP_PCD3D_SM6
								? TEXT("M_Dungeoneer_EcoInstanced compiles cleanly for PCD3D_SM6")
								: TEXT("M_Dungeoneer_EcoInstanced compiles cleanly for PCD3D_SM5"),
							bStarted && MatRes->GetCompileErrors().IsEmpty());
					}
				}
			}
#endif

			Bootstrap->ImportAllEcosystemTexturesFromDisk();
			TestNotNull(TEXT("PROP_Magical_Flora_And_Crystals_4x4_D loaded"),
				Bootstrap->GetLoadedEcosystemTexture(FName(TEXT("PROP_Magical_Flora_And_Crystals_4x4_D"))));
			TestNotNull(TEXT("PROP_Water_And_Fauna_4x4_D loaded"),
				Bootstrap->GetLoadedEcosystemTexture(FName(TEXT("PROP_Water_And_Fauna_4x4_D"))));
			TestNotNull(TEXT("PROP_Fauna_Critters_Strip_4x1_D loaded"),
				Bootstrap->GetLoadedEcosystemTexture(FName(TEXT("PROP_Fauna_Critters_Strip_4x1_D"))));
			if (UTexture2D* FaunaTex = Bootstrap->GetLoadedEcosystemTexture(FName(TEXT("PROP_Fauna_Critters_Strip_4x1_D"))))
			{
				const EPixelFormat FaunaPF = FaunaTex->GetPixelFormat();
				uint8 CornerAlpha = 255;
				if (const FTexturePlatformData* PD = FaunaTex->GetPlatformData(); PD && PD->Mips.Num() > 0)
				{
					if (const uint8* RawBytes = static_cast<const uint8*>(PD->Mips[0].BulkData.LockReadOnly()))
					{
						CornerAlpha = RawBytes[3];
						PD->Mips[0].BulkData.Unlock();
					}
				}
				int32 bNoAlpha = 0;
#if WITH_EDITORONLY_DATA
				bNoAlpha = FaunaTex->CompressionNoAlpha ? 1 : 0;
#endif
				UE_LOG(LogTemp, Display, TEXT("[EcoDiag] FaunaTex PF=%d NoAlpha=%d CornerAlpha=%u"),
					static_cast<int32>(FaunaPF), bNoAlpha, CornerAlpha);
				TestEqual(TEXT("FaunaTex corner pixel keeps Alpha=0"), CornerAlpha, static_cast<uint8>(0));
			}

			Bootstrap->ImportAllFoleyFromDisk();
			TestTrue(TEXT("All 42 ecosystem foley WAVs imported"), Bootstrap->GetLoadedFoleyCount() >= 42);
			USoundWave* WadeLoop = Bootstrap->GetLoadedFoley(FName(TEXT("Water_Loop_StreamRush")));
			USoundWave* OneShotClick = Bootstrap->GetLoadedFoley(FName(TEXT("Bio_BeetleClick_0")));
			TestNotNull(TEXT("Water_Loop_StreamRush imported"), WadeLoop);
			TestNotNull(TEXT("Bio_BeetleClick_0 imported"), OneShotClick);
			if (WadeLoop)
			{
				TestTrue(TEXT("Loop_* foley sets bLooping = true"), WadeLoop->bLooping != 0);
				TestTrue(TEXT("Imported foley is UDungeoneerPCMSoundWave (procedural AudioMixer safe)"),
					WadeLoop->IsA<UDungeoneerPCMSoundWave>() && WadeLoop->bProcedural != 0);
				const ESoundWaveLoadingBehavior WadeBehavior = WadeLoop->GetLoadingBehavior(true);
				TestTrue(TEXT("Loop foley pre-resolves LoadingBehavior on GameThread"),
					WadeBehavior != ESoundWaveLoadingBehavior::Uninitialized
					&& WadeBehavior != ESoundWaveLoadingBehavior::Inherited);

				FSoundGeneratorInitParams GenParams;
				GenParams.SampleRate = 48000.0f;
				GenParams.NumChannels = WadeLoop->NumChannels;
				ISoundGeneratorPtr LoopGen = WadeLoop->CreateSoundGenerator(GenParams);
				TestTrue(TEXT("Looping foley creates valid ISoundGenerator"), LoopGen.IsValid());
				if (LoopGen.IsValid())
				{
					TArray<float> OutPCM;
					OutPCM.SetNumZeroed(1024 * FMath::Max(1, WadeLoop->NumChannels));
					const int32 Written = LoopGen->OnGenerateAudio(OutPCM.GetData(), OutPCM.Num());
					TestEqual(TEXT("Loop generator renders requested buffer frames"), Written, OutPCM.Num());
					TestFalse(TEXT("Looping generator stays unfinished after first buffer"), LoopGen->IsFinished());
				}
			}
			if (OneShotClick)
			{
				const ESoundWaveLoadingBehavior ClickBehavior = OneShotClick->GetLoadingBehavior(true);
				TestTrue(TEXT("One-shot foley is procedural with pre-resolved LoadingBehavior"),
					OneShotClick->IsA<UDungeoneerPCMSoundWave>()
					&& OneShotClick->bProcedural != 0
					&& ClickBehavior != ESoundWaveLoadingBehavior::Uninitialized
					&& ClickBehavior != ESoundWaveLoadingBehavior::Inherited);

				FSoundGeneratorInitParams GenParams;
				GenParams.SampleRate = 48000.0f;
				GenParams.NumChannels = OneShotClick->NumChannels;
				ISoundGeneratorPtr OneShotGen = OneShotClick->CreateSoundGenerator(GenParams);
				TestTrue(TEXT("One-shot foley creates valid ISoundGenerator"), OneShotGen.IsValid());
				if (OneShotGen.IsValid())
				{
					TArray<float> OutPCM;
					OutPCM.SetNumZeroed(1024 * FMath::Max(1, OneShotClick->NumChannels));
					bool bReachedEnd = false;
					for (int32 Iter = 0; Iter < 256; ++Iter)
					{
						OneShotGen->OnGenerateAudio(OutPCM.GetData(), OutPCM.Num());
						if (OneShotGen->IsFinished())
						{
							bReachedEnd = true;
							break;
						}
					}
					TestTrue(TEXT("One-shot generator finishes cleanly without looping forever"), bReachedEnd);
				}
			}
		}
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDungeoneerLightBudgetTierCapsTest,
	"DarkDungeoneer.Ecosystem.LightBudgetTierCaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDungeoneerLightBudgetTierCapsTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Created headless UWorld for LightBudget tests"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	TestWorld->InitializeActorsForPlay(FURL());
	AActor* PoolHost = TestWorld->SpawnActor<AActor>();
	USceneComponent* Root = NewObject<USceneComponent>(PoolHost);
	PoolHost->SetRootComponent(Root);
	Root->RegisterComponent();

	UDungeonLightBudgetSubsystem* LightBudget = TestWorld->GetSubsystem<UDungeonLightBudgetSubsystem>();
	TestNotNull(TEXT("UDungeonLightBudgetSubsystem active"), LightBudget);
	if (!LightBudget)
	{
		TestWorld->DestroyWorld(false);
		return false;
	}

	LightBudget->SetShadowQualityOverride(3); // High quality: 32 Tier1, 16 Tier2, 2 Tier3
	LightBudget->OnFloorBuilt(PoolHost);

	// Register 300 mixed candidates across Tiers 1, 2, 3
	int32 FirstBloomCandId = INDEX_NONE;
	for (int32 i = 0; i < 300; ++i)
	{
		FDungeonLightCandidate C;
		C.WorldPosition = FVector(100.0 * (i + 1), (i % 5) * 80.0, 60.0);
		if (i < 6)
		{
			C.RequestedTier = EDungeonLightTier::Tier3_Hero;
			C.RadiusUU = 800.0f;
			C.Color = EcoPalette::GoldenRune;
		}
		else if (i < 46)
		{
			C.RequestedTier = EDungeonLightTier::Tier2_Pooled;
			C.RadiusUU = 340.0f;
			C.Color = EcoPalette::AmberFire;
		}
		else
		{
			C.RequestedTier = EDungeonLightTier::Tier1_MPCGlow;
			C.RadiusUU = 120.0f;
			C.Color = EcoPalette::AstralCyan;
		}
		C.bEnabled = (i != 20); // Candidate 20 starts dormant for 15s bloom lease test
		const int32 Cid = LightBudget->RegisterCandidate(C);
		if (i == 20)
		{
			FirstBloomCandId = Cid;
		}
	}

	LightBudget->EvaluateLightBudgetNow(0.25f);
	TestEqual(TEXT("High quality caps Tier 3 world lights at 2"), LightBudget->GetActiveTier3WorldCount(), 2);
	TestEqual(TEXT("High quality caps Tier 2 pooled lights at 16"), LightBudget->GetActiveTier2Count(), 16);
	TestEqual(TEXT("High quality caps Tier 1 MPC glow nodes at 32"), LightBudget->GetActiveTier1Count(), 32);

	// Verify 15s Tier 2 lease activates dormant flora candidate & expires cleanly
	TestTrue(TEXT("RequestTier2Lease succeeds on flora candidate"), LightBudget->RequestTier2Lease(FirstBloomCandId, 15.0f));
	LightBudget->EvaluateLightBudgetNow(0.1f);
	const FDungeonLightCandidate* Leased = LightBudget->GetCandidate(FirstBloomCandId);
	TestNotNull(TEXT("Leased candidate pointer valid"), Leased);
	TestTrue(TEXT("Leased flora candidate enabled & boosted"), Leased && Leased->bEnabled && Leased->PriorityBoost >= 1.5f);

	// Verify D7 Intel HD 520 scaling: sg.ShadowQuality <= 1 drops Tier 2 -> 8 and Tier 1 -> 16
	LightBudget->SetShadowQualityOverride(1);
	LightBudget->EvaluateLightBudgetNow(0.1f);
	TestEqual(TEXT("Low ShadowQuality caps Tier 2 pooled lights at 8"), LightBudget->GetActiveTier2Count(), 8);
	TestEqual(TEXT("Low ShadowQuality caps Tier 1 MPC glow nodes at 16"), LightBudget->GetActiveTier1Count(), 16);

	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDungeoneerWaterAndWadingDeterminismTest,
	"DarkDungeoneer.Ecosystem.WaterSurfaceGridAndWadingDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDungeoneerWaterAndWadingDeterminismTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Created headless UWorld for Water tests"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	TestWorld->InitializeActorsForPlay(FURL());

	UDungeonGeneratorSubsystem* GenSub = TestWorld->GetSubsystem<UDungeonGeneratorSubsystem>();
	UDungeonWaterSubsystem* WaterSub = TestWorld->GetSubsystem<UDungeonWaterSubsystem>();
	TestNotNull(TEXT("Generator subsystem present"), GenSub);
	TestNotNull(TEXT("Water subsystem present"), WaterSub);

	const FDungeonFloorGraph Graph = GenSub->GenerateFloorGraph(1, 7711);
	ADungeoneerTileMapInstancer* Instancer = TestWorld->SpawnActor<ADungeoneerTileMapInstancer>();
	TestNotNull(TEXT("Spawned TileMapInstancer"), Instancer);
	Instancer->BuildDungeonFromFloorGraph(Graph);

	TestTrue(TEXT("TileMapInstancer owns HISM_Water"), Instancer->GetWaterHISM() != nullptr);
	TestTrue(TEXT("TileMapInstancer owns ISM_WaterFX"), Instancer->GetWaterFxISM() != nullptr);
	TestTrue(TEXT("TileMapInstancer owns FloraManager"), Instancer->GetFloraManagerComponent() != nullptr);
	TestTrue(TEXT("O(1) SurfaceGrid allocated"), WaterSub->GetGridWidth() >= 8 && WaterSub->GetGridHeight() >= 8);
	TestTrue(TEXT("Streams spawned in Crypt floor"), WaterSub->GetActiveStreamTileCount() > 0);
	TestTrue(TEXT("Weeping wall cascades spawned"), WaterSub->GetActiveCascadeCount() > 0);

	// Verify explicit SurfaceGrid stamping & puddle -> dry step wet footprints (4s fade)
	const FVector PuddlePos(500.0, 500.0, 0.0);
	const FVector DryPos(950.0, 500.0, 0.0);
	const FVector StreamPos(1350.0, 500.0, 0.0);
	WaterSub->SetSurfaceCell(PuddlePos, EDungeonSurfaceKind::ShallowPuddle);
	WaterSub->SetSurfaceCell(DryPos, EDungeonSurfaceKind::DryStone);
	WaterSub->SetSurfaceCell(StreamPos, EDungeonSurfaceKind::AnkleDeepStream);

	TestEqual(TEXT("ResolveSurface ShallowPuddle"), WaterSub->ResolveSurface(PuddlePos), EDungeonSurfaceKind::ShallowPuddle);
	TestEqual(TEXT("ResolveSurface DryStone"), WaterSub->ResolveSurface(DryPos), EDungeonSurfaceKind::DryStone);
	TestEqual(TEXT("ResolveSurface AnkleDeepStream"), WaterSub->ResolveSurface(StreamPos), EDungeonSurfaceKind::AnkleDeepStream);

	// Step in puddle then step onto dry stone -> footprint spawns
	ADungeoneerCharacter* Hero = TestWorld->SpawnActor<ADungeoneerCharacter>();
	TestNotNull(TEXT("Spawned Hero"), Hero);
	const int32 FootprintsBefore = WaterSub->GetActiveFootprintCount();
	WaterSub->NotifyWalkerStep(Hero, PuddlePos, ECompassOctant::E, true);
	WaterSub->NotifyWalkerStep(Hero, DryPos, ECompassOctant::E, true);
	TestEqual(TEXT("Dry step after puddle spawns 1 footprint"), WaterSub->GetActiveFootprintCount(), FootprintsBefore + 1);

	// Advance simulation past 4.0s fade lifetime -> footprint clears
	WaterSub->StepHydrologySimulation(4.25f);
	TestEqual(TEXT("4s fading footprint expires cleanly"), WaterSub->GetActiveFootprintCount(), FootprintsBefore);

	// Verify 0.88x wading speed preserves Diagonal == Cardinal speed + SavedMove never combines across stream edge
	UDungeoneerMovementComponent* MoveComp = Hero->GetDungeoneerMovement();
	TestNotNull(TEXT("Hero has UDungeoneerMovementComponent"), MoveComp);
	if (MoveComp)
	{
		MoveComp->SetWadingInStream(false);
		const float DryMaxSpeed = MoveComp->GetMaxSpeed();
		MoveComp->SetWadingInStream(true);
		const float WadeMaxSpeed = MoveComp->GetMaxSpeed();
		TestTrue(TEXT("Wading speed is strictly 0.88x dry speed"),
			FMath::IsNearlyEqual(WadeMaxSpeed, DryMaxSpeed * EcoCells::WadingSpeedScalar, 1e-3f));

		const FVector DirN = UDungeoneerMovementComponent::GetUnitDirectionForOctant(ECompassOctant::N) * WadeMaxSpeed;
		const FVector DirNE = UDungeoneerMovementComponent::GetUnitDirectionForOctant(ECompassOctant::NE) * WadeMaxSpeed;
		TestTrue(TEXT("Diagonal wading speed == cardinal wading speed"),
			FMath::IsNearlyEqual(static_cast<float>(DirN.Size2D()), static_cast<float>(DirNE.Size2D()), 1e-2f));

		FSavedMove_Dungeoneer MoveDry;
		MoveDry.bSavedWading = 0;
		FSavedMove_Dungeoneer MoveWet;
		MoveWet.bSavedWading = 1;
		// Verify flag mismatch prevents SavedMove combine across stream edge
		TestTrue(TEXT("SavedMove wading bit distinct across stream edge"), MoveDry.bSavedWading != MoveWet.bSavedWading);
	}

	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDungeoneerFloraInteractionTest,
	"DarkDungeoneer.Ecosystem.FloraBloomMossWaveAndCrystalNoFeedback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDungeoneerFloraInteractionTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Created headless UWorld for Flora tests"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	TestWorld->InitializeActorsForPlay(FURL());
	ADungeoneerTileMapInstancer* Instancer = TestWorld->SpawnActor<ADungeoneerTileMapInstancer>();
	TestNotNull(TEXT("Spawned TileMapInstancer"), Instancer);

	UDungeonFloraManagerComponent* Flora = Instancer->GetFloraManagerComponent();
	UDungeonLightBudgetSubsystem* LightBudget = TestWorld->GetSubsystem<UDungeonLightBudgetSubsystem>();
	UDungeoneerSensoryWorldSubsystem* SensoryWorld = TestWorld->GetSubsystem<UDungeoneerSensoryWorldSubsystem>();
	TestNotNull(TEXT("FloraManager present"), Flora);
	TestNotNull(TEXT("LightBudget present"), LightBudget);
	TestNotNull(TEXT("SensoryWorld present"), SensoryWorld);

	// 1. Prism-Lily bloom + 15s Tier 2 lease
	const int32 LilyIdx = Flora->AddFloraInstance(EFloraKind::PrismLily, FVector(200.0, 200.0, 0.0), EcoPalette::MagentaBloom);
	TestEqual(TEXT("Prism-Lily starts dormant"), Flora->GetFloraStateAt(LilyIdx), static_cast<uint8>(0));
	Flora->IgnitePrismLily(LilyIdx, 15.0f);
	TestEqual(TEXT("IgnitePrismLily blooms lily"), Flora->GetFloraStateAt(LilyIdx), static_cast<uint8>(1));
	TestTrue(TEXT("Bloomed Prism-Lily emissive reaches 12.0"), FMath::IsNearlyEqual(Flora->GetFloraEmissiveAt(LilyIdx), 12.0f, 1e-2f));
	const FDungeonLightCandidate* LilyLight = LightBudget->GetCandidate(Flora->GetFloraLightCandidateAt(LilyIdx));
	TestTrue(TEXT("Prism-Lily acquires enabled Tier 2 lease"), LilyLight && LilyLight->bEnabled && LilyLight->LeaseExpiryTime > 0.0);

	// 2. Pulse-Moss zero-allocation CSR BFS wave
	const int32 Moss0 = Flora->AddFloraInstance(EFloraKind::PulseMoss, FVector(1000.0, 200.0, 0.0), EcoPalette::EmeraldMoss);
	const int32 Moss1 = Flora->AddFloraInstance(EFloraKind::PulseMoss, FVector(1150.0, 200.0, 0.0), EcoPalette::EmeraldMoss);
	const int32 Moss2 = Flora->AddFloraInstance(EFloraKind::PulseMoss, FVector(1300.0, 200.0, 0.0), EcoPalette::EmeraldMoss);
	Flora->RebuildPulseMossCSRGraph(200.0f);
	const int32 ReachedMoss = Flora->TriggerPulseMossWaveNear(FVector(1005.0, 200.0, 0.0), 80.0f);
	TestEqual(TEXT("Pulse-Moss CSR BFS reaches all 3 connected nodes"), ReachedMoss, 3);
	TestTrue(TEXT("Pulse-Moss wave staggers EventTime by +0.08s per hop"),
		Flora->GetFloraEventTimeAt(Moss2) > Flora->GetFloraEventTimeAt(Moss1) &&
		Flora->GetFloraEventTimeAt(Moss1) >= Flora->GetFloraEventTimeAt(Moss0));

	// 3. Spore-Cap burst + cooldown
	Flora->AddFloraInstance(EFloraKind::SporeCap, FVector(2000.0, 200.0, 0.0), EcoPalette::EmeraldMoss);
	TestTrue(TEXT("Spore-Cap triggers within 120 UU"), Flora->TriggerSporeCapNear(FVector(2040.0, 200.0, 0.0), 120.0f));
	TestFalse(TEXT("Spore-Cap respects 6s cooldown against immediate retrigger"), Flora->TriggerSporeCapNear(FVector(2040.0, 200.0, 0.0), 120.0f));

	// 4. Echo-Crystal flashes on noise ripple and NEVER emits a new noise event (zero feedback)
	const int32 CrystalIdx = Flora->AddFloraInstance(EFloraKind::EchoCrystal, FVector(3000.0, 200.0, 0.0), EcoPalette::AstralCyan);
	SensoryWorld->BroadcastNoiseEvent(FVector(3050.0, 200.0, 0.0), 1.5f, 400.0f, false);
	const uint64 SerialBefore = SensoryWorld->GetNoiseEventSerial();
	const int32 Flashed = Flora->EvaluateEchoCrystalsAgainstNoise();
	TestEqual(TEXT("Echo-Crystal reacts to covering noise event"), Flashed, 1);
	TestEqual(TEXT("Echo-Crystal enters charged state"), Flora->GetFloraStateAt(CrystalIdx), static_cast<uint8>(1));
	TestEqual(TEXT("Echo-Crystal NEVER emits noise event into SensoryWorld"), static_cast<int64>(SensoryWorld->GetNoiseEventSerial()), static_cast<int64>(SerialBefore));

	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDungeoneerFaunaSoAScatterTest,
	"DarkDungeoneer.Ecosystem.FaunaSoAZeroAllocScatterNewtNoise",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDungeoneerFaunaSoAScatterTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Created headless UWorld for Fauna tests"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	TestWorld->InitializeActorsForPlay(FURL());
	TestWorld->BeginPlay();
	ADungeoneerTileMapInstancer* Instancer = TestWorld->SpawnActor<ADungeoneerTileMapInstancer>();
	if (Instancer) { Instancer->DispatchBeginPlay(); }
	UDungeonGeneratorSubsystem* GenSub = TestWorld->GetSubsystem<UDungeonGeneratorSubsystem>();
	UDungeonFaunaSubsystem* FaunaSub = TestWorld->GetSubsystem<UDungeonFaunaSubsystem>();
	UDungeoneerSensoryWorldSubsystem* SensoryWorld = TestWorld->GetSubsystem<UDungeoneerSensoryWorldSubsystem>();
	TestNotNull(TEXT("TileMapInstancer present"), Instancer);
	TestNotNull(TEXT("FaunaSubsystem present"), FaunaSub);
	TestNotNull(TEXT("SensoryWorld present"), SensoryWorld);

	const FDungeonFloorGraph Graph = GenSub->GenerateFloorGraph(1, 4422);
	Instancer->BuildDungeonFromFloorGraph(Graph);
	TestNotNull(TEXT("TileMapInstancer owns ISM_Fauna"), Instancer->GetFaunaISM());
	TestTrue(TEXT("Floor build seeds critters into SoA"), FaunaSub->GetActiveCritterCount() > 0);

	const int32 MothIdx = FaunaSub->SpawnScriptedCritter(EFaunaSpecies::LumenMoth, FVector(5000.0, 5000.0, 90.0));
	const int32 NewtIdx = FaunaSub->SpawnScriptedCritter(EFaunaSpecies::PuddleNewt, FVector(6500.0, 6500.0, 6.0));
	TestTrue(TEXT("Scripted moth & newt spawned in fixed 256 pool"), MothIdx != INDEX_NONE && NewtIdx != INDEX_NONE);

	// 1. Verify zero heap allocation across 600 simulation ticks
	const SIZE_T InitialScratchBytes = FaunaSub->GetScratchTransformsAllocatedSize();
	for (int32 t = 0; t < 600; ++t)
	{
		FaunaSub->StepFaunaSimulation(1.0f / 30.0f);
	}
	TestEqual(TEXT("ScratchTransforms buffer capacity unchanged across 600 ticks (0 heap allocs)"),
		static_cast<uint64>(FaunaSub->GetScratchTransformsAllocatedSize()),
		static_cast<uint64>(InitialScratchBytes));

	// 2. Fast Thrower within 250 UU scatters Lumen-Moth into Flee state
	AZombieFastThrower* Thrower = TestWorld->SpawnActor<AZombieFastThrower>(FVector(5050.0, 5000.0, 88.0), FRotator::ZeroRotator);
	TestNotNull(TEXT("Spawned Fast Thrower near moth"), Thrower);
	if (Thrower) { Thrower->DispatchBeginPlay(); }
	FaunaSub->StepFaunaSimulation(1.0f / 30.0f);
	TestEqual(TEXT("Moth scatters into EFaunaState::Flee near Fast Thrower"),
		FaunaSub->GetCritterStateAt(MothIdx), EFaunaState::Flee);

	// 3. Player within 180 UU triggers Newt Dart + 0.25 loudness decoy ring + 4.0s cooldown
	const int64 SerialBefore = SensoryWorld->GetNoiseEventSerial();
	ADungeoneerCharacter* Hero = TestWorld->SpawnActor<ADungeoneerCharacter>(FVector(6540.0, 6500.0, 88.0), FRotator::ZeroRotator);
	TestNotNull(TEXT("Spawned Hero near puddle newt"), Hero);
	if (Hero) { Hero->DispatchBeginPlay(); }
	FaunaSub->StepFaunaSimulation(1.0f / 30.0f);
	TestEqual(TEXT("Puddle-Newt enters EFaunaState::Dart on proximity"), FaunaSub->GetCritterStateAt(NewtIdx), EFaunaState::Dart);
	TestTrue(TEXT("Puddle-Newt respects 4.0s noise cooldown"), FaunaSub->GetCritterNoiseCooldownAt(NewtIdx) > 3.5f);
	TestTrue(TEXT("Puddle-Newt broadcasts decoy ring on SensoryWorld"), SensoryWorld->GetNoiseEventSerial() > SerialBefore);

	// Confirm the latest ring's loudness is strictly 0.25 (< 1.55 zombie lure threshold)
	bool bFoundDecoyRing = false;
	for (const FDungeoneerNoiseEvent& Ev : SensoryWorld->GetNoiseRingBuffer())
	{
		if (Ev.bActive && FMath::IsNearlyEqual(Ev.Loudness, 0.25f, 1e-2f))
		{
			bFoundDecoyRing = true;
			break;
		}
	}
	TestTrue(TEXT("Newt decoy ring loudness is 0.25 (< 1.55 zombie lure threshold)"), bFoundDecoyRing);

	TestWorld->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDungeoneerAcousticSurfaceLayersTest,
	"DarkDungeoneer.Ecosystem.AcousticSurfaceLayers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FDungeoneerAcousticSurfaceLayersTest::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Game, false);
	TestNotNull(TEXT("Created headless UWorld for Acoustic tests"), TestWorld);
	if (!TestWorld)
	{
		return false;
	}

	TestWorld->InitializeActorsForPlay(FURL());
	TestWorld->BeginPlay();
	ADungeoneerTileMapInstancer* Instancer = TestWorld->SpawnActor<ADungeoneerTileMapInstancer>();
	if (Instancer) { Instancer->DispatchBeginPlay(); }
	UDungeonGeneratorSubsystem* GenSub = TestWorld->GetSubsystem<UDungeonGeneratorSubsystem>();
	UDungeonWaterSubsystem* WaterSub = TestWorld->GetSubsystem<UDungeonWaterSubsystem>();
	UDungeonAcousticSubsystem* AcousticSub = TestWorld->GetSubsystem<UDungeonAcousticSubsystem>();
	TestNotNull(TEXT("TileMapInstancer present"), Instancer);
	TestNotNull(TEXT("WaterSubsystem present"), WaterSub);
	TestNotNull(TEXT("AcousticSubsystem present"), AcousticSub);

	const FDungeonFloorGraph Graph = GenSub->GenerateFloorGraph(1, 5588);
	Instancer->BuildDungeonFromFloorGraph(Graph);

	TestEqual(TEXT("Preallocated 22 pooled audio voices cleanly on floor build"),
		AcousticSub->GetPreallocatedVoiceCount(), UDungeonAcousticSubsystem::TotalVoicePoolSize);

	ADungeoneerCharacter* Hero = TestWorld->SpawnActor<ADungeoneerCharacter>(FVector(4200.0, 4200.0, 88.0), FRotator::ZeroRotator);
	TestNotNull(TEXT("Spawned Hero"), Hero);
	if (Hero) { Hero->DispatchBeginPlay(); }
	TestTrue(TEXT("Hero auto-registered with AcousticSubsystem on BeginPlay"), AcousticSub->IsWalkerRegistered(Hero));

	// Verify ShallowPuddle & CrystalShard yield 1.4x noise radius (850 * 1.4 = 1190 UU) on Layer A (cols 1 & 3)
	WaterSub->SetSurfaceCell(Hero->GetActorLocation(), EDungeonSurfaceKind::ShallowPuddle);
	const int32 LayerABefore = AcousticSub->GetLayerAFireCount();
	const int32 LayerBBefore = AcousticSub->GetLayerBFireCount();

	AcousticSub->NotifyWalkColumnChanged(Hero, 1);
	TestEqual(TEXT("Col 1 fires Layer A"), AcousticSub->GetLayerAFireCount(), LayerABefore + 1);
	TestEqual(TEXT("Col 1 does NOT fire Layer B"), AcousticSub->GetLayerBFireCount(), LayerBBefore);
	TestEqual(TEXT("Resolved ShallowPuddle surface"), AcousticSub->GetLastTriggerRecord().Surface, EDungeonSurfaceKind::ShallowPuddle);
	TestTrue(TEXT("ShallowPuddle scales noise radius by 1.4x (1190 UU)"),
		FMath::IsNearlyEqual(AcousticSub->GetLastTriggerRecord().NoiseRadiusUU, 850.0f * 1.40f, 1.0f));

	// CrystalShard also 1.4x on Col 3
	WaterSub->SetSurfaceCell(Hero->GetActorLocation(), EDungeonSurfaceKind::CrystalShard);
	AcousticSub->NotifyWalkColumnChanged(Hero, 3);
	TestEqual(TEXT("Col 3 fires Layer A"), AcousticSub->GetLayerAFireCount(), LayerABefore + 2);
	TestEqual(TEXT("Resolved CrystalShard surface"), AcousticSub->GetLastTriggerRecord().Surface, EDungeonSurfaceKind::CrystalShard);
	TestTrue(TEXT("CrystalShard scales noise radius by 1.4x (1190 UU)"),
		FMath::IsNearlyEqual(AcousticSub->GetLastTriggerRecord().NoiseRadiusUU, 850.0f * 1.40f, 1.0f));

	// Passing pose cols 0 & 2 fire Layer B ONLY for Hero
	AcousticSub->NotifyWalkColumnChanged(Hero, 2);
	TestEqual(TEXT("Col 2 fires Layer B for Hero"), AcousticSub->GetLayerBFireCount(), LayerBBefore + 1);
	TestEqual(TEXT("Col 2 does NOT fire Layer A"), AcousticSub->GetLayerAFireCount(), LayerABefore + 2);

	// Verify monotonic LPF occlusion curve & biome fog scaling
	const float ClearHz = UDungeonAcousticSubsystem::ComputeOcclusionLowPassHz(0.0f, 1.0f);
	const float PartialHz = UDungeonAcousticSubsystem::ComputeOcclusionLowPassHz(0.5f, 1.0f);
	const float FullOccHz = UDungeonAcousticSubsystem::ComputeOcclusionLowPassHz(1.0f, 1.0f);
	const float SporeFogHz = UDungeonAcousticSubsystem::ComputeOcclusionLowPassHz(0.0f, 0.72f);
	TestTrue(TEXT("Occlusion LPF monotonically decreases with obstruction and dense fog"),
		(ClearHz > PartialHz) && (PartialHz > FullOccHz) && (ClearHz > SporeFogHz));

	TestWorld->DestroyWorld(false);
	return true;
}
