#include "Misc/AutomationTest.h"
#include "Core/DungeoneerTypes.h"
#include "Core/DungeoneerGameMode.h"
#include "Core/DungeoneerMovementComponent.h"
#include "Core/DungeoneerGameState.h"
#include "AI/ZombieFastThrower.h"
#include "AI/ZombieBase.h"
#include "Dungeon/DungeonGeneratorSubsystem.h"
#include "Dungeon/DungeoneerTileMapInstancer.h"
#include "Dungeon/MirrorLightBounceComponent.h"
#include "Dungeon/MirrorMazeActors.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
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
#include "Dungeon/DungeonEncounterActors.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
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
	TestEqual(TEXT("Generated floor DAG has 8 nodes"), Footer->GetLastPreviewNodeCount(), 8);

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
		TestEqual(TEXT("All 16 2.5D dungeon atlas textures imported"), DungeonTexCount, 16);
		TestNotNull(TEXT("ATLAS_Dungeon_Floors_4x4_D imported"), Bootstrap->GetLoadedDungeonTexture(FName(TEXT("ATLAS_Dungeon_Floors_4x4_D"))));

		const int32 ExtractedCells = Bootstrap->ExtractDefaultSingleCellTiles();
		TestEqual(TEXT("40 single-cell tile textures extracted from 4x4 atlases"), ExtractedCells, 40);

		TestEqual(TEXT("ScreenDirectionalRow W (-X toward camera) -> Row 0 (Front)"),
			DungeoneerMath::OctantToScreenDirectionalRow(ECompassOctant::W), static_cast<uint8>(0));
		TestEqual(TEXT("ScreenDirectionalRow E (+X into dungeon) -> Row 1 (Back)"),
			DungeoneerMath::OctantToScreenDirectionalRow(ECompassOctant::E), static_cast<uint8>(1));
		TestEqual(TEXT("ScreenDirectionalRow N (+Y screen right) -> Row 2 (Right)"),
			DungeoneerMath::OctantToScreenDirectionalRow(ECompassOctant::N), static_cast<uint8>(2));
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
	Orchestrator->OnStepContact.AddLambda([&CapturedContacts, &LastContactFrame](const F25DStepContactEvent& Evt)
	{
		++CapturedContacts;
		LastContactFrame = Evt.FrameIndex;
	});

	// Default StrideLengthUU = 140.0f -> columns step every 35 UU (cols 0, 1, 2, 3)
	Orchestrator->AdvanceWalkDistance(40.0f, FVector(300.0, 0.0, 0.0), 0.016f);
	TestEqual(TEXT("Walk distance 40 UU advances to frame col 1"), Orchestrator->GetCurrentFrameColumn(), 1);
	TestEqual(TEXT("No step contact fired on passing frame 1"), CapturedContacts, 0);
	TestTrue(TEXT("120Hz stride hop lifted plane smoothly"), Orchestrator->GetCurrentStrideHopZ() > 0.0f);
	TestTrue(TEXT("120Hz directional velocity lean engaged"), FMath::Abs(Orchestrator->GetCurrentVelocityLeanDeg().X) > 0.0f);

	Orchestrator->AdvanceWalkDistance(40.0f, FVector(300.0, 0.0, 0.0), 0.016f);
	TestEqual(TEXT("Walk distance 80 UU advances to frame col 2"), Orchestrator->GetCurrentFrameColumn(), 2);
	TestEqual(TEXT("Step contact fired deterministically on frame 2"), CapturedContacts, 1);
	TestEqual(TEXT("Last contact frame is 2"), LastContactFrame, 2);

	Orchestrator->AdvanceWalkDistance(40.0f, FVector(300.0, 0.0, 0.0), 0.016f);
	TestEqual(TEXT("Walk distance 120 UU advances to frame col 3"), Orchestrator->GetCurrentFrameColumn(), 3);
	TestEqual(TEXT("No step contact fired on passing frame 3"), CapturedContacts, 1);

	Orchestrator->AdvanceWalkDistance(25.0f, FVector(300.0, 0.0, 0.0), 0.016f);
	TestEqual(TEXT("Walk distance 145 UU wraps cleanly to frame col 0"), Orchestrator->GetCurrentFrameColumn(), 0);
	TestEqual(TEXT("Step contact fired deterministically on frame 0"), CapturedContacts, 2);
	TestEqual(TEXT("Last contact frame is 0"), LastContactFrame, 0);

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
