#include "AI/ZombieMirrorStalker.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerMovementComponent.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "Rendering/SensoryPerkComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AZombieMirrorStalker::AZombieMirrorStalker()
{
	BaseWalkSpeed = 310.0f;
	MaxHealth = 95.0f;
	PerceptionHearingRange = 1750.0f;
	StepCycleDistanceUU = 95.0f;
	GoldKillReward = 35;
	BatteryKillReward = 12.0f;
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
}

void AZombieMirrorStalker::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;
	RevealHoldRemaining = 0.0f;
	RevealAlpha = 0.18f;
	bRevealedByLight = false;
	if (ZombieEmissiveLight)
	{
		ZombieEmissiveLight->SetIntensity(0.0f);
	}
}

void AZombieMirrorStalker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!IsZombieAlive() || bIsMirrorDecoy)
	{
		return;
	}

	UWorld* World = GetWorld();
	UZombieSpatialSubsystem* SpatialSys = World ? World->GetSubsystem<UZombieSpatialSubsystem>() : nullptr;
	if (!SpatialSys)
	{
		return;
	}

	const FVector SelfLoc = GetActorLocation();
	bool bLitNow = false;
	const int32 PlayerCount = SpatialSys->GetTrackedPlayerCount();
	for (int32 i = 0; i < PlayerCount; ++i)
	{
		if (const ADungeoneerCharacter* Player = SpatialSys->GetTrackedPlayerAt(i))
		{
			if (Player->IsPointLitByFlashlight(SelfLoc))
			{
				bLitNow = true;
				break;
			}
			if (const USensoryPerkComponent* Perks = Player->GetSensoryPerkComponent())
			{
				if (Perks->GetPerkLevel(EDungeoneerSensoryPerk::DarkVision) >= 2 &&
					FVector::DistSquared2D(Player->GetActorLocation(), SelfLoc) <= FMath::Square(static_cast<double>(DarkVisionRevealRadiusUU)))
				{
					bLitNow = true;
					break;
				}
			}
		}
	}

	if (bLitNow)
	{
		RevealHoldRemaining = RevealFadeOutSeconds;
	}
	else if (RevealHoldRemaining > 0.0f)
	{
		RevealHoldRemaining = FMath::Max(0.0f, RevealHoldRemaining - DeltaSeconds);
	}

	bRevealedByLight = (RevealHoldRemaining > 0.0f);
	const float TargetAlpha = bRevealedByLight ? 1.0f : 0.18f;
	RevealAlpha = FMath::FInterpTo(RevealAlpha, TargetAlpha, DeltaSeconds, 8.0f);

	if (ZombieVisualMesh)
	{
		const FVector BaseScale = GetArchetypeVisualScale();
		ZombieVisualMesh->SetRelativeScale3D(BaseScale * (0.72f + 0.28f * RevealAlpha));
	}
	if (ZombieEmissiveLight)
	{
		ZombieEmissiveLight->SetIntensity(bRevealedByLight ? (420.0f * RevealAlpha) : 0.0f);
	}

	// Pursue nearest lit/audible player cleanly along 8 compass headings
	bool bOutLit = false;
	if (ADungeoneerCharacter* TargetPlayer = SpatialSys->FindNearestTargetPlayer(SelfLoc, PerceptionHearingRange, bOutLit))
	{
		const FVector ToTarget = TargetPlayer->GetActorLocation() - SelfLoc;
		const double DistSq = ToTarget.SizeSquared2D();
		if (DistSq > FMath::Square(68.0))
		{
			ECompassOctant DesiredOctant = CurrentHeadingOctant;
			UDungeoneerMovementComponent::QuantizeInputToOctant(FVector2D(ToTarget.X, ToTarget.Y), 1.0f, DesiredOctant);
			MoveAlongOctant(DesiredOctant, bRevealedByLight ? 0.85f : 1.05f);
		}
		if (DistSq <= FMath::Square(92.0))
		{
			TargetPlayer->ApplyDungeoneerDamage(16.0f * DeltaSeconds, this);
		}
	}
}
