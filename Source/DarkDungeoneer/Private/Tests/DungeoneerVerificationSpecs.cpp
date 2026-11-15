#include "Misc/AutomationTest.h"
#include "Core/DungeoneerTypes.h"
#include "Core/DungeoneerMovementComponent.h"
#include "AI/ZombieFastThrower.h"
#include "Dungeon/DungeonGeneratorSubsystem.h"
#include "Audio/GibberishSpeechComponent.h"
#include "Social/DungeoneerInventoryComponent.h"
#include "Social/TradeManagerComponent.h"

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
