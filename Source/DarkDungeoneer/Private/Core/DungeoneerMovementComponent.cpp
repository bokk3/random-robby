#include "Core/DungeoneerMovementComponent.h"
#include "Core/DungeoneerMovementConfig.h"
#include "Ecosystem/DungeonEcosystemTypes.h"
#include "Ecosystem/DungeonWaterSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

UDungeoneerMovementComponent::UDungeoneerMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bOrientRotationToMovement = false; // Facing/aim octant is strictly decoupled from movement octant
	bUseControllerDesiredRotation = false;
	MaxAcceleration = 8192.0f;
	BrakingDecelerationWalking = 8192.0f;
	GroundFriction = 12.0f;
	MaxWalkSpeed = DefaultBaseWalkSpeed;
}

bool UDungeoneerMovementComponent::QuantizeInputToOctant(const FVector2D& InVector, float DeadZoneSq, ECompassOctant& InOutOctant) noexcept
{
	if (InVector.SquaredLength() < static_cast<double>(DeadZoneSq))
	{
		return false;
	}

	const float Theta = FMath::Atan2(static_cast<float>(InVector.Y), static_cast<float>(InVector.X));
	constexpr float OctantStepRad = PI / 4.0f;
	const int32 OctantIndex = FMath::RoundToInt(Theta / OctantStepRad) & 7;
	InOutOctant = static_cast<ECompassOctant>(OctantIndex);
	return true;
}

FVector UDungeoneerMovementComponent::GetUnitDirectionForOctant(ECompassOctant Octant) noexcept
{
	return DungeoneerMath::OctantToDirection3D(Octant);
}

void UDungeoneerMovementComponent::SetMoveInputVector(const FVector2D& InMove) noexcept
{
	CachedRawMoveInput = InMove;
	const float DeadZoneSq = MovementConfig ? MovementConfig->InputDeadZoneSq : DefaultDeadZoneSq;
	bHasActiveMoveInput = QuantizeInputToOctant(InMove, DeadZoneSq, CurrentMoveOctant);
}

void UDungeoneerMovementComponent::SetAimInputVector(const FVector2D& InAim) noexcept
{
	CachedRawAimInput = InAim;
	const float DeadZoneSq = MovementConfig ? MovementConfig->InputDeadZoneSq : DefaultDeadZoneSq;
	QuantizeInputToOctant(InAim, DeadZoneSq, CurrentAimOctant);
}

void UDungeoneerMovementComponent::SetAimDirectionWorld(const FVector2D& InAimDelta) noexcept
{
	QuantizeInputToOctant(InAimDelta, 0.0001f, CurrentAimOctant);
}

void UDungeoneerMovementComponent::SetChronoSlowRatio(float InSlowRatio) noexcept
{
	const float MaxSlow = GetMaxChronoSlowConfig();
	ChronoSlowRatio = FMath::Clamp(InSlowRatio, 0.0f, MaxSlow);
}

float UDungeoneerMovementComponent::GetBaseWalkSpeedConfig() const noexcept
{
	return MovementConfig ? MovementConfig->BaseWalkSpeed : DefaultBaseWalkSpeed;
}

float UDungeoneerMovementComponent::GetMaxChronoSlowConfig() const noexcept
{
	return MovementConfig ? MovementConfig->MaxChronoSlowRatio : DefaultMaxSlowRatio;
}

float UDungeoneerMovementComponent::GetMaxSpeed() const
{
	const float BaseSpeed = GetBaseWalkSpeedConfig();
	const float WadingMult = bIsWadingInStream ? EcoCells::WadingSpeedScalar : 1.0f;
	// Chrono-Leech conservation law: V_player = V_base * (1.0f - S) * WadingMult
	return BaseSpeed * FMath::Clamp(1.0f - ChronoSlowRatio, 0.05f, 1.0f) * WadingMult;
}

float UDungeoneerMovementComponent::GetStepPhaseNormalized() const noexcept
{
	const float StepDist = MovementConfig ? MovementConfig->StepCycleDistanceUU : DefaultStepCycleUU;
	return FMath::Clamp(StepDistanceAccumulator / FMath::Max(StepDist, 1.0f), 0.0f, 1.0f);
}

void UDungeoneerMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (!bHasActiveMoveInput)
	{
		Velocity.X = 0.0;
		Velocity.Y = 0.0;
		return;
	}

	const FVector QuantizedDir = GetUnitDirectionForOctant(CurrentMoveOctant);
	const float Speed = GetMaxSpeed();

	// Strictly enforce identical speed along cardinal and diagonal axes
	Velocity.X = QuantizedDir.X * static_cast<double>(Speed);
	Velocity.Y = QuantizedDir.Y * static_cast<double>(Speed);
}

void UDungeoneerMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	const FVector PreMoveLocation = UpdatedComponent ? UpdatedComponent->GetComponentLocation() : FVector::ZeroVector;

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!UpdatedComponent || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector PostMoveLocation = UpdatedComponent->GetComponentLocation();
	const double HorizontalDeltaSq = FVector::DistSquared2D(PreMoveLocation, PostMoveLocation);

	if (HorizontalDeltaSq > static_cast<double>(KINDA_SMALL_NUMBER))
	{
		const float HorizontalDelta = static_cast<float>(FMath::Sqrt(HorizontalDeltaSq));
		const float StepThreshold = MovementConfig ? MovementConfig->StepCycleDistanceUU : DefaultStepCycleUU;
		StepDistanceAccumulator += HorizontalDelta;

		// Deterministic fixed-step distance accumulator synced to the 4-axis step cycle
		while (StepDistanceAccumulator >= StepThreshold)
		{
			StepDistanceAccumulator -= StepThreshold;
			StepSequenceCounter = (StepSequenceCounter + 1) & 0x3FFFFFFF;

			if (OnStepCycleTriggered.IsBound())
			{
				FDungeoneerStepEvent Event;
				Event.WorldLocation = PostMoveLocation;
				Event.StepOctant = CurrentMoveOctant;
				Event.AcousticLoudness = 1.0f - (ChronoSlowRatio * 0.4f);
				Event.StepSequenceIndex = StepSequenceCounter;
				Event.bIsHostileSource = false;
				OnStepCycleTriggered.Broadcast(Event);
			}
		}
	}
	else if (!bHasActiveMoveInput)
	{
		StepDistanceAccumulator = 0.0f;
	}
}

void FSavedMove_Dungeoneer::Clear()
{
	Super::Clear();
	SavedQuantizedChronoSlow = 0;
	bSavedWading = 0;
}

bool FSavedMove_Dungeoneer::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const
{
	if (!Super::CanCombineWith(NewMove, InCharacter, MaxDelta) || !NewMove.IsValid())
	{
		return false;
	}

	const FSavedMove_Dungeoneer* OtherDungeoneerMove = static_cast<const FSavedMove_Dungeoneer*>(NewMove.Get());
	return SavedQuantizedChronoSlow == OtherDungeoneerMove->SavedQuantizedChronoSlow
		&& bSavedWading == OtherDungeoneerMove->bSavedWading;
}

void FSavedMove_Dungeoneer::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (const UDungeoneerMovementComponent* MoveComp = C ? Cast<UDungeoneerMovementComponent>(C->GetCharacterMovement()) : nullptr)
	{
		SavedQuantizedChronoSlow = QuantizeSlowRatioToByte(MoveComp->GetChronoSlowRatio(), MoveComp->GetMaxChronoSlowConfig());
		bSavedWading = MoveComp->IsWadingInStream() ? 1 : 0;
	}
}

void FSavedMove_Dungeoneer::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	if (UDungeoneerMovementComponent* MoveComp = C ? Cast<UDungeoneerMovementComponent>(C->GetCharacterMovement()) : nullptr)
	{
		MoveComp->SetChronoSlowRatio(DequantizeByteToSlowRatio(SavedQuantizedChronoSlow, MoveComp->GetMaxChronoSlowConfig()));
		MoveComp->SetWadingInStream(bSavedWading != 0);
	}
}

FSavedMovePtr FNetworkPredictionData_Client_Dungeoneer::AllocateNewMove()
{
	return FSavedMovePtr(new FSavedMove_Dungeoneer());
}

FNetworkPredictionData_Client* UDungeoneerMovementComponent::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UDungeoneerMovementComponent* MutableThis = const_cast<UDungeoneerMovementComponent*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Dungeoneer(*this);
	}
	return ClientPredictionData;
}

void UDungeoneerMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
	ChronoSlowRatio = FMath::Clamp(ChronoSlowRatio, 0.0f, GetMaxChronoSlowConfig());

	if (const UWorld* World = GetWorld())
	{
		if (const UDungeonWaterSubsystem* WaterSub = World->GetSubsystem<UDungeonWaterSubsystem>())
		{
			if (WaterSub->GetGridWidth() > 0 && UpdatedComponent)
			{
				bIsWadingInStream = WaterSub->IsWadingLocation(UpdatedComponent->GetComponentLocation());
			}
		}
	}
}
