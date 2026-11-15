#include "Rendering/25DAnimOrchestratorComponent.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace DungeoneerAnim25DPrivate
{
	static uint8 MapOctantToDiagonalRow(ECompassOctant Octant) noexcept
	{
		// Matches TO_GENERATE.md CH_Player_Diagonal_Walk_4x4 row layout (SE=0, SW=1, NE=2, NW=3)
		switch (Octant)
		{
		case ECompassOctant::SE:
		case ECompassOctant::E:
		case ECompassOctant::S:
			return 0;
		case ECompassOctant::SW:
		case ECompassOctant::W:
			return 1;
		case ECompassOctant::NE:
		case ECompassOctant::N:
			return 2;
		case ECompassOctant::NW:
		default:
			return 3;
		}
	}
}

U25DAnimOrchestratorComponent::U25DAnimOrchestratorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	CachedCustomData[E25DAnimCPD::UVCellIndex] = 0.0f;
	CachedCustomData[E25DAnimCPD::EmissiveFusePulse] = 0.0f;
	// Default Alchemical Flask Cyan (#00FFFF)
	CachedCustomData[E25DAnimCPD::FlaskElementR] = 0.0f;
	CachedCustomData[E25DAnimCPD::FlaskElementG] = 1.0f;
	CachedCustomData[E25DAnimCPD::FlaskElementB] = 1.0f;
}

void U25DAnimOrchestratorComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const AActor* OwnerActor = GetOwner())
	{
		LastOwnerLocation = OwnerActor->GetActorLocation();
		bHasLastLocation = true;
	}

	PushCustomPrimitiveData();
}

void U25DAnimOrchestratorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor || DeltaTime <= 0.0f)
	{
		return;
	}

	const FVector CurrentLocation = OwnerActor->GetActorLocation();
	FVector DeltaXY = FVector::ZeroVector;
	if (bHasLastLocation)
	{
		DeltaXY = FVector(CurrentLocation.X - LastOwnerLocation.X, CurrentLocation.Y - LastOwnerLocation.Y, 0.0);
	}
	LastOwnerLocation = CurrentLocation;
	bHasLastLocation = true;

	const float DeltaDistanceUU = static_cast<float>(DeltaXY.Size());
	const FVector VelocityXY = (DeltaTime > SMALL_NUMBER) ? (DeltaXY / DeltaTime) : OwnerActor->GetVelocity();

	AdvanceWalkDistance(DeltaDistanceUU, FVector(VelocityXY.X, VelocityXY.Y, 0.0), DeltaTime);
}

void U25DAnimOrchestratorComponent::BindRigPrimitives(UPrimitiveComponent* InCutoutPlane, UPrimitiveComponent* InShadowProxyCapsule) noexcept
{
	CutoutPlane = InCutoutPlane;
	ShadowProxyCapsule = InShadowProxyCapsule;

	if (InCutoutPlane)
	{
		BasePlaneLocation = InCutoutPlane->GetRelativeLocation();
		BasePlaneRotation = InCutoutPlane->GetRelativeRotation();
		BasePlaneScale = InCutoutPlane->GetRelativeScale3D();
	}

	if (InShadowProxyCapsule)
	{
		InShadowProxyCapsule->SetRenderInMainPass(false);
		InShadowProxyCapsule->SetCastHiddenShadow(true);
		InShadowProxyCapsule->SetCastShadow(true);
	}

	PushCustomPrimitiveData();
}

void U25DAnimOrchestratorComponent::SetDirectionalRow(int32 NewRowIndex) noexcept
{
	const int32 ClampedRow = FMath::Clamp(NewRowIndex, 0, FMath::Max(GridRows - 1, 0));
	if (DirectionalRow != ClampedRow)
	{
		DirectionalRow = ClampedRow;
		PushCustomPrimitiveData();
	}
}

void U25DAnimOrchestratorComponent::SetDirectionFromOctant(ECompassOctant Octant, bool bUseDiagonalSheetOrder) noexcept
{
	const uint8 Row = bUseDiagonalSheetOrder
		? DungeoneerAnim25DPrivate::MapOctantToDiagonalRow(Octant)
		: DungeoneerMath::OctantToCardinalRow(Octant);
	SetDirectionalRow(static_cast<int32>(Row));
}

void U25DAnimOrchestratorComponent::AdvanceWalkDistance(float DeltaDistanceUU, const FVector& CurrentVelocityXY, float DeltaTime)
{
	const float ClampedDist = FMath::Max(DeltaDistanceUU, 0.0f);
	const bool bVelocityIdle = CurrentVelocityXY.IsNearlyZero(1.0f);
	const bool bHasMovement = !bVelocityIdle || (ClampedDist > 0.05f);

	if (bVelocityIdle)
	{
		// Lock UV selection strictly to Column 0 (Contact / Idle Pose A) when standing still
		CurrentFrameColumn = 0;
	}
	else if (ClampedDist > KINDA_SMALL_NUMBER)
	{
		AccumulatedDistanceUU += ClampedDist;
		const float SafeStride = FMath::Max(StrideLengthUU, 1.0f);
		const float RawCycles = AccumulatedDistanceUU / SafeStride;
		NormalizedWalkPhase = FMath::Clamp(RawCycles - FMath::FloorToFloat(RawCycles), 0.0f, 0.99999f);

		const int32 SafeCols = FMath::Max(GridColumns, 1);
		const int32 NewColumn = (SafeCols == 4)
			? (FMath::FloorToInt(RawCycles * 4.0f) & 3)
			: FMath::Clamp(FMath::FloorToInt(NormalizedWalkPhase * static_cast<float>(SafeCols)), 0, SafeCols - 1);
		if (NewColumn != CurrentFrameColumn)
		{
			CurrentFrameColumn = NewColumn;
			if (CurrentFrameColumn == 1 || CurrentFrameColumn == 3)
			{
				DispatchStepContact(CurrentFrameColumn);
			}
		}
	}

	EvaluateSubFrameSprings(DeltaTime, CurrentVelocityXY, bHasMovement);
	ApplyPlaneTransform();
	PushCustomPrimitiveData();
}

void U25DAnimOrchestratorComponent::SetPreExplosionFuseState(float InFuseProgress01, float InEmissivePulseIntensity) noexcept
{
	FuseProgress = FMath::Clamp(InFuseProgress01, 0.0f, 1.0f);
	EmissivePulse = FMath::Max(InEmissivePulseIntensity, 0.0f);
	CachedCustomData[E25DAnimCPD::EmissiveFusePulse] = EmissivePulse;
	PushCustomPrimitiveData();
}

void U25DAnimOrchestratorComponent::SetFlaskElementTint(const FLinearColor& InElementRGB) noexcept
{
	CachedCustomData[E25DAnimCPD::FlaskElementR] = FMath::Clamp(InElementRGB.R, 0.0f, 4.0f);
	CachedCustomData[E25DAnimCPD::FlaskElementG] = FMath::Clamp(InElementRGB.G, 0.0f, 4.0f);
	CachedCustomData[E25DAnimCPD::FlaskElementB] = FMath::Clamp(InElementRGB.B, 0.0f, 4.0f);
	PushCustomPrimitiveData();
}

void U25DAnimOrchestratorComponent::SetFlaskElementByType(uint8 ElementPresetIndex) noexcept
{
	switch (ElementPresetIndex)
	{
	case 0: // Fire Orange
		SetFlaskElementTint(FLinearColor(1.0f, 0.33f, 0.0f, 1.0f));
		break;
	case 1: // Poison Green
		SetFlaskElementTint(FLinearColor(0.18f, 0.95f, 0.22f, 1.0f));
		break;
	case 2: // Slo-Mo Chrono Violet-Blue
		SetFlaskElementTint(FLinearColor(0.48f, 0.22f, 1.0f, 1.0f));
		break;
	default: // Neutral Alchemical Cyan
		SetFlaskElementTint(FLinearColor(0.0f, 1.0f, 1.0f, 1.0f));
		break;
	}
}

void U25DAnimOrchestratorComponent::ResetOrchestratorState() noexcept
{
	DirectionalRow = 0;
	CurrentFrameColumn = 0;
	StepContactCount = 0;
	AccumulatedDistanceUU = 0.0f;
	NormalizedWalkPhase = 0.0f;
	FuseProgress = 0.0f;
	EmissivePulse = 0.0f;
	SwellTremorPhase = 0.0f;
	CurrentStrideHopZ = 0.0f;
	CurrentLeanPitchDeg = 0.0f;
	CurrentLeanRollDeg = 0.0f;
	CurrentBellySwellScale = FVector::OneVector;

	CachedCustomData[E25DAnimCPD::UVCellIndex] = 0.0f;
	CachedCustomData[E25DAnimCPD::EmissiveFusePulse] = 0.0f;
	CachedCustomData[E25DAnimCPD::FlaskElementR] = 0.0f;
	CachedCustomData[E25DAnimCPD::FlaskElementG] = 1.0f;
	CachedCustomData[E25DAnimCPD::FlaskElementB] = 1.0f;

	ApplyPlaneTransform();
	PushCustomPrimitiveData();
}

float U25DAnimOrchestratorComponent::GetCustomPrimitiveDataValue(int32 SlotIndex) const noexcept
{
	if (SlotIndex >= 0 && SlotIndex < E25DAnimCPD::Count)
	{
		return CachedCustomData[SlotIndex];
	}
	return 0.0f;
}

void U25DAnimOrchestratorComponent::EvaluateSubFrameSprings(float DeltaTime, const FVector& VelocityXY, bool bHasMovement) noexcept
{
	if (DeltaTime <= 0.0f)
	{
		return;
	}

	constexpr float FixedSubStep = 1.0f / 120.0f;
	float RemainingDt = FMath::Min(DeltaTime, 0.05f);
	const float InvRefSpeed = 1.0f / FMath::Max(ReferenceWalkSpeedUU, 1.0f);

	const float TargetHopZ = bHasMovement
		? (FMath::Abs(FMath::Sin(NormalizedWalkPhase * TWO_PI)) * MaxStrideHopZ)
		: 0.0f;
	const float VelNormX = FMath::Clamp(static_cast<float>(VelocityXY.X) * InvRefSpeed, -1.25f, 1.25f);
	const float VelNormY = FMath::Clamp(static_cast<float>(VelocityXY.Y) * InvRefSpeed, -1.25f, 1.25f);
	const float TargetLeanPitch = -VelNormX * MaxVelocityLeanDeg;
	const float TargetLeanRoll = VelNormY * MaxVelocityLeanDeg;

	while (RemainingDt > KINDA_SMALL_NUMBER)
	{
		const float SubDt = FMath::Min(RemainingDt, FixedSubStep);
		RemainingDt -= SubDt;

		const float Alpha = FMath::Clamp(1.0f - FMath::Exp(-SpringStiffness * SubDt), 0.0f, 1.0f);
		CurrentStrideHopZ = FMath::Lerp(CurrentStrideHopZ, TargetHopZ, Alpha);
		CurrentLeanPitchDeg = FMath::Lerp(CurrentLeanPitchDeg, TargetLeanPitch, Alpha);
		CurrentLeanRollDeg = FMath::Lerp(CurrentLeanRollDeg, TargetLeanRoll, Alpha);

		if (FuseProgress > 0.001f)
		{
			SwellTremorPhase += SubDt * (18.0f + 42.0f * FuseProgress);
			const float Tremor = FMath::Sin(SwellTremorPhase) * 0.06f * FuseProgress;
			const float TargetScaleXY = 1.0f + (FuseProgress * MaxBellySwellXY) + Tremor;
			const float TargetScaleZ = 1.0f + (FuseProgress * MaxBellySwellZ) - (Tremor * 0.5f);
			CurrentBellySwellScale.X = FMath::Lerp(CurrentBellySwellScale.X, TargetScaleXY, Alpha);
			CurrentBellySwellScale.Y = FMath::Lerp(CurrentBellySwellScale.Y, TargetScaleXY, Alpha);
			CurrentBellySwellScale.Z = FMath::Lerp(CurrentBellySwellScale.Z, TargetScaleZ, Alpha);
		}
		else
		{
			CurrentBellySwellScale = FMath::Lerp(CurrentBellySwellScale, FVector::OneVector, Alpha);
		}
	}
}

void U25DAnimOrchestratorComponent::ApplyPlaneTransform() noexcept
{
	if (UPrimitiveComponent* Plane = CutoutPlane.Get())
	{
		const FVector TargetLoc = BasePlaneLocation + FVector(0.0, 0.0, static_cast<double>(CurrentStrideHopZ));
		const FRotator TargetRot = BasePlaneRotation + FRotator(CurrentLeanPitchDeg, 0.0, CurrentLeanRollDeg);
		const FVector TargetScale = BasePlaneScale * CurrentBellySwellScale;
		Plane->SetRelativeLocationAndRotation(TargetLoc, TargetRot);
		Plane->SetRelativeScale3D(TargetScale);
	}
}

void U25DAnimOrchestratorComponent::PushCustomPrimitiveData() noexcept
{
	const float CellFloat = static_cast<float>(DirectionalRow * FMath::Max(GridColumns, 1) + CurrentFrameColumn);
	CachedCustomData[E25DAnimCPD::UVCellIndex] = CellFloat;
	CachedCustomData[E25DAnimCPD::EmissiveFusePulse] = EmissivePulse;

	if (UPrimitiveComponent* Plane = CutoutPlane.Get())
	{
		for (int32 Slot = 0; Slot < E25DAnimCPD::Count; ++Slot)
		{
			Plane->SetCustomPrimitiveDataFloat(Slot, CachedCustomData[Slot]);
		}
	}
}

void U25DAnimOrchestratorComponent::DispatchStepContact(int32 FrameCol) noexcept
{
	++StepContactCount;

	const EFoot ContactFoot = (FrameCol == 3) ? EFoot::Right : EFoot::Left;

	F25DStepContactEvent Event;
	Event.Foot = ContactFoot;
	Event.FrameIndex = FrameCol;
	Event.CellIndex = GetUVCellIndex();
	Event.AudioLoudness = BaseStepAudioLoudness;
	Event.SurfaceType = SurfaceType_Default;

	const AActor* OwnerActor = GetOwner();
	const UWorld* World = GetWorld();
	Event.WorldLocation = OwnerActor ? OwnerActor->GetActorLocation() : FVector::ZeroVector;

	if (World && OwnerActor)
	{
		FHitResult GroundHit;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(Anim25DStepContact), false, OwnerActor);
		QueryParams.bReturnPhysicalMaterial = true;
		const FVector Start = Event.WorldLocation + FVector(0.0, 0.0, 20.0);
		const FVector End = Event.WorldLocation - FVector(0.0, 0.0, 140.0);
		if (World->LineTraceSingleByChannel(GroundHit, Start, End, ECC_Visibility, QueryParams))
		{
			if (UPhysicalMaterial* PhysMat = GroundHit.PhysMaterial.Get())
			{
				Event.PhysicalMaterial = PhysMat;
				Event.SurfaceType = PhysMat->SurfaceType;
			}
		}

		if (UDungeoneerSensoryWorldSubsystem* SensorySys = World->GetSubsystem<UDungeoneerSensoryWorldSubsystem>())
		{
			SensorySys->BroadcastNoiseEvent(
				Event.WorldLocation,
				Event.AudioLoudness,
				AcousticRippleBaseRadiusUU * FMath::Max(Event.AudioLoudness, 0.25f),
				false);
		}
	}

	OnFootstepContact.Broadcast(ContactFoot);
	OnStepContact.Broadcast(Event);
}
