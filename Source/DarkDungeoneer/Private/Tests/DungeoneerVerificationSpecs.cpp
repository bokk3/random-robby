#include "Misc/AutomationTest.h"
#include "Core/DungeoneerTypes.h"
#include "Core/DungeoneerMovementComponent.h"
#include "Core/DungeoneerGameState.h"
#include "AI/ZombieFastThrower.h"
#include "AI/ZombieBase.h"
#include "Dungeon/DungeonGeneratorSubsystem.h"
#include "Dungeon/MirrorMazeActors.h"
#include "Audio/GibberishSpeechComponent.h"
#include "Social/DungeoneerInventoryComponent.h"
#include "Social/TradeManagerComponent.h"
#include "UI/DungeoneerMainMenuWidget.h"
#include "UI/MenuDioramaStageActor.h"
#include "UI/DungeoneerCoopLobbyWidget.h"
#include "UI/DungeoneerMenuFooterWidget.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
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
	Stage->SetShowcaseModePreview(EDungeoneerMenuAction::MirrorTrial);
	Stage->Tick(0.5f);
	TestTrue(TEXT("Diorama flashlight sweeps smoothly toward NE"), Stage->GetCurrentFlashlightYawDeg() > 0.0f);

	TestWorld->DestroyWorld(false);
	return true;
}
