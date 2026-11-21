#include "Core/DungeoneerPartyCamera.h"
#include "Core/DungeoneerCharacter.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"

namespace DungeoneerParty
{
	FVector ClampToLeash(const FVector& Mover, const FVector& Anchor, const float RadiusUU) noexcept
	{
		const FVector Delta(Mover.X - Anchor.X, Mover.Y - Anchor.Y, 0.0);
		const double DistSq = Delta.SizeSquared();
		const double Radius = static_cast<double>(RadiusUU);
		if (DistSq <= Radius * Radius || DistSq <= KINDA_SMALL_NUMBER)
		{
			return Mover;
		}
		const FVector Dir = Delta / FMath::Sqrt(DistSq);
		return FVector(Anchor.X + Dir.X * Radius, Anchor.Y + Dir.Y * Radius, Mover.Z);
	}

	float ArmLengthForSpread(const float SpreadUU) noexcept
	{
		const float Alpha = FMath::Clamp((SpreadUU - ZoomStartSpreadUU) / FMath::Max(LeashRadiusUU - ZoomStartSpreadUU, 1.0f), 0.0f, 1.0f);
		// Ease-out so the first steps apart barely move the camera.
		const float Eased = Alpha * Alpha * (3.0f - 2.0f * Alpha);
		return FMath::Lerp(ArmLengthNearUU, ArmLengthFarUU, Eased);
	}

	bool IsLocalPartyHero(const ADungeoneerCharacter* Hero) noexcept
	{
		if (!Hero)
		{
			return false;
		}
		const AController* Controller = Hero->GetController();
		return Controller == nullptr || Hero->IsLocallyControlled();
	}
}

ADungeoneerPartyCamera::ADungeoneerPartyCamera()
{
	PrimaryActorTick.bCanEverTick = true;
	// After character movement, so the framing and leash see this frame's positions.
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	SetReplicates(false);

	FocusRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FocusRoot"));
	RootComponent = FocusRoot;

	CameraArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraArm"));
	CameraArm->SetupAttachment(FocusRoot);
	CameraArm->SetUsingAbsoluteRotation(true);
	CameraArm->SetRelativeRotation(FRotator(-42.0f, -45.0f, 0.0f));
	CameraArm->TargetArmLength = DungeoneerParty::ArmLengthNearUU;
	CameraArm->bDoCollisionTest = false;
	CameraArm->bEnableCameraLag = false; // Smoothing is done on the focus point instead.

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("PartyCamera"));
	Camera->SetupAttachment(CameraArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
	Camera->FieldOfView = 35.0f;
	Camera->PostProcessSettings.bOverride_AutoExposureMinBrightness = true;
	Camera->PostProcessSettings.AutoExposureMinBrightness = 0.42f;
	Camera->PostProcessSettings.bOverride_AutoExposureMaxBrightness = true;
	Camera->PostProcessSettings.AutoExposureMaxBrightness = 1.40f;
	Camera->PostProcessSettings.bOverride_AutoExposureBias = true;
	Camera->PostProcessSettings.AutoExposureBias = -0.10f;
	Camera->PostProcessSettings.bOverride_BloomIntensity = true;
	Camera->PostProcessSettings.BloomIntensity = 0.35f;
}

void ADungeoneerPartyCamera::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdatePartyFraming(DeltaSeconds);
}

void ADungeoneerPartyCamera::SnapToParty()
{
	bSnapNextUpdate = true;
	UpdatePartyFraming(0.0f);
}

void ADungeoneerPartyCamera::UpdatePartyFraming(const float DeltaSeconds)
{
	const UWorld* World = GetWorld();
	const UZombieSpatialSubsystem* Spatial = World ? World->GetSubsystem<UZombieSpatialSubsystem>() : nullptr;
	if (!Spatial)
	{
		return;
	}

	ADungeoneerCharacter* Heroes[MaxPartyHeroes] = {};
	int32 Count = 0;
	for (int32 i = 0; i < Spatial->GetTrackedPlayerCount() && Count < MaxPartyHeroes; ++i)
	{
		ADungeoneerCharacter* Hero = Spatial->GetTrackedPlayerAt(i);
		if (DungeoneerParty::IsLocalPartyHero(Hero))
		{
			Heroes[Count++] = Hero;
		}
	}
	FramedHeroCount = Count;
	if (Count == 0)
	{
		return;
	}

	if (bLeashEnabled && Count > 1)
	{
		EnforceLeash(Heroes, Count);
	}

	// Focus: midpoint of the heroes. Solo, lean slightly toward where the hero is aiming.
	FVector Sum = FVector::ZeroVector;
	FVector Impulse = FVector::ZeroVector;
	float Trauma = 0.0f;
	double MaxSpread = 0.0;
	for (int32 i = 0; i < Count; ++i)
	{
		const FVector Loc = Heroes[i]->GetActorLocation();
		Sum += Loc;
		Impulse += Heroes[i]->GetCameraImpulseOffset();
		Trauma = FMath::Max(Trauma, Heroes[i]->GetCameraTrauma());
		for (int32 j = i + 1; j < Count; ++j)
		{
			MaxSpread = FMath::Max(MaxSpread, FVector::Dist2D(Loc, Heroes[j]->GetActorLocation()));
		}
	}
	FVector TargetFocus = Sum / static_cast<double>(Count);
	if (Count == 1)
	{
		TargetFocus += Heroes[0]->GetAimForwardXY() * 70.0;
	}
	const float TargetArm = DungeoneerParty::ArmLengthForSpread(static_cast<float>(MaxSpread));

	// Snap on the first frame and after teleports (new floor, respawn); ease otherwise.
	const bool bJump = !bHasFocus || bSnapNextUpdate || FVector::DistSquared2D(TargetFocus, FocusLocation) > 1500.0 * 1500.0;
	if (bJump || DeltaSeconds <= 0.0f)
	{
		FocusLocation = TargetFocus;
		CurrentArmLength = TargetArm;
	}
	else
	{
		FocusLocation = FMath::VInterpTo(FocusLocation, TargetFocus, DeltaSeconds, 9.0f);
		CurrentArmLength = FMath::FInterpTo(CurrentArmLength, TargetArm, DeltaSeconds, 3.0f);
	}
	bHasFocus = true;
	bSnapNextUpdate = false;

	SetActorLocation(FocusLocation);
	if (CameraArm)
	{
		// Shake grows with the square of trauma so small hits barely move it and blasts really do.
		const float Shake = Trauma * Trauma * 16.0f;
		const double Time = World->GetTimeSeconds();
		const FVector ShakeOffset(
			0.0,
			Shake * (FMath::Sin(Time * 61.0) * 0.6 + FMath::Sin(Time * 37.0) * 0.4),
			Shake * (FMath::Sin(Time * 53.0 + 1.3) * 0.6 + FMath::Sin(Time * 29.0 + 0.4) * 0.4));
		CameraArm->TargetArmLength = CurrentArmLength;
		CameraArm->SocketOffset = Impulse + ShakeOffset;
	}
}

void ADungeoneerPartyCamera::EnforceLeash(ADungeoneerCharacter* const* Heroes, const int32 Count) const
{
	const double Radius = static_cast<double>(DungeoneerParty::LeashRadiusUU);
	for (int32 i = 0; i < Count; ++i)
	{
		for (int32 j = i + 1; j < Count; ++j)
		{
			ADungeoneerCharacter* A = Heroes[i];
			ADungeoneerCharacter* B = Heroes[j];
			if (!A->HasAuthority() || !B->HasAuthority())
			{
				continue;
			}
			const FVector LocA = A->GetActorLocation();
			const FVector LocB = B->GetActorLocation();
			const double Dist = FVector::Dist2D(LocA, LocB);
			if (Dist <= Radius)
			{
				continue;
			}

			// The hero heading away faster is the one held back. If neither is moving (a teleport or a
			// knockback that already ended), it's the one further from where the camera was looking.
			// A downed hero is always the anchor.
			const FVector AwayFromB = FVector(LocA.X - LocB.X, LocA.Y - LocB.Y, 0.0) / FMath::Max(Dist, 1.0);
			const UCharacterMovementComponent* MoveA = A->GetCharacterMovement();
			const UCharacterMovementComponent* MoveB = B->GetCharacterMovement();
			const double OutA = MoveA ? FVector::DotProduct(MoveA->Velocity, AwayFromB) : 0.0;
			const double OutB = MoveB ? FVector::DotProduct(MoveB->Velocity, -AwayFromB) : 0.0;
			bool bMoveA = OutA >= OutB;
			if (FMath::Abs(OutA - OutB) < 10.0 && bHasFocus)
			{
				bMoveA = FVector::DistSquared2D(LocA, FocusLocation) > FVector::DistSquared2D(LocB, FocusLocation);
			}
			if (A->IsDowned() != B->IsDowned())
			{
				bMoveA = B->IsDowned();
			}
			ADungeoneerCharacter* Mover = bMoveA ? A : B;
			const ADungeoneerCharacter* Anchor = bMoveA ? B : A;
			const FVector MoverLoc = Mover->GetActorLocation();
			const FVector AnchorLoc = Anchor->GetActorLocation();

			if (Dist > Radius * DungeoneerParty::LeashSnapFactor)
			{
				// Far past the leash: a swept move could stop on any wall in between, so drop the straggler
				// right beside the anchor, where there is known to be floor.
				const FVector Beside = DungeoneerParty::ClampToLeash(MoverLoc, AnchorLoc, 120.0f);
				Mover->SetActorLocation(FVector(Beside.X, Beside.Y, AnchorLoc.Z), false, nullptr, ETeleportType::TeleportPhysics);
			}
			else
			{
				// Just over the edge: step back along the way they came.
				const FVector Clamped = DungeoneerParty::ClampToLeash(MoverLoc, AnchorLoc, static_cast<float>(Radius));
				Mover->SetActorLocation(Clamped, true, nullptr, ETeleportType::TeleportPhysics);
			}

			// Remove the outward part of the velocity so the hero slides along the edge instead of jittering.
			if (UCharacterMovementComponent* Move = Mover->GetCharacterMovement())
			{
				const FVector Outward = FVector(MoverLoc.X - AnchorLoc.X, MoverLoc.Y - AnchorLoc.Y, 0.0).GetSafeNormal();
				const double OutSpeed = FVector::DotProduct(Move->Velocity, Outward);
				if (OutSpeed > 0.0)
				{
					Move->Velocity -= Outward * OutSpeed;
				}
			}
		}
	}
}
