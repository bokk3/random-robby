#include "Dungeon/MirrorLightBounceComponent.h"
#include "Dungeon/DungeoneerTileMapInstancer.h"
#include "AI/ZombieBase.h"
#include "Dungeon/MirrorMazeActors.h"
#include "Core/DungeoneerCharacter.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace MirrorBounceDetail
{
	inline bool IntersectRayWithMirrorEntry(
		const FVector& RayOrigin,
		const FVector& RayDir,
		const FDungeoneerMirrorInstanceEntry& Entry,
		const float MaxDist,
		float& OutDistance,
		FVector& OutHitPoint,
		FVector& OutNormal) noexcept
	{
		const FVector Normal = Entry.SurfaceNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector(0.0f, 1.0f, 0.0f));
		const double Denom = FVector::DotProduct(RayDir, Normal);
		// Accept rays striking front face (or double-sided phantasm plane)
		if (FMath::Abs(Denom) < 1e-4)
		{
			return false;
		}
		if (!Entry.bIsIllusoryPhantasm && Denom >= -0.05)
		{
			return false;
		}

		const double T = FVector::DotProduct(Entry.WorldCenter - RayOrigin, Normal) / Denom;
		if (T <= 1.0 || T > static_cast<double>(MaxDist))
		{
			return false;
		}

		const FVector CandidateHit = RayOrigin + RayDir * T;
		const FVector Delta = CandidateHit - Entry.WorldCenter;
		const double Lateral = FMath::Abs(FVector::DotProduct(Delta, Entry.MirrorTangent));
		const double Vertical = FMath::Abs(Delta.Z);

		if (Lateral <= static_cast<double>(Entry.HalfWidthUU + 12.0f) &&
			Vertical <= static_cast<double>(Entry.HalfHeightUU + 24.0f))
		{
			OutDistance = static_cast<float>(T);
			OutHitPoint = CandidateHit;
			OutNormal = (Denom < 0.0) ? Normal : -Normal;
			return true;
		}

		return false;
	}
}

UMirrorLightBounceComponent::UMirrorLightBounceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UMirrorLightBounceComponent::EnsureBounceLightInitialized()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!ReflectedSpotLight)
	{
		ReflectedSpotLight = NewObject<USpotLightComponent>(Owner, TEXT("ReflectedMirrorBounceSpotLight"));
		if (ReflectedSpotLight)
		{
			ReflectedSpotLight->SetIntensity(9360.0f);
			ReflectedSpotLight->SetLightColor(FLinearColor(1.0f, 0.94f, 0.82f));
			ReflectedSpotLight->SetInnerConeAngle(ReflectedInnerConeAngleDeg);
			ReflectedSpotLight->SetOuterConeAngle(ReflectedOuterConeAngleDeg);
			ReflectedSpotLight->SetAttenuationRadius(MaxReflectedRangeUU);
			ReflectedSpotLight->bUseInverseSquaredFalloff = true;
			ReflectedSpotLight->VolumetricScatteringIntensity = 2.0f;
			ReflectedSpotLight->SetCastShadows(true);
			ReflectedSpotLight->SetVisibility(false);

			if (USceneComponent* OwnerRoot = Owner->GetRootComponent())
			{
				ReflectedSpotLight->AttachToComponent(OwnerRoot, FAttachmentTransformRules::KeepWorldTransform);
			}
			ReflectedSpotLight->RegisterComponent();
		}
	}
}

void UMirrorLightBounceComponent::SetTargetTileMapInstancer(ADungeoneerTileMapInstancer* InInstancer)
{
	CachedInstancer = InInstancer;
}

void UMirrorLightBounceComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureBounceLightInitialized();

	if (!CachedInstancer.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			for (TActorIterator<ADungeoneerTileMapInstancer> It(World); It; ++It)
			{
				CachedInstancer = *It;
				break;
			}
		}
	}
}

void UMirrorLightBounceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ReflectedSpotLight)
	{
		ReflectedSpotLight->SetVisibility(false);
	}
	Super::EndPlay(EndPlayReason);
}

FVector UMirrorLightBounceComponent::ComputeReflectedVector(
	const FVector& IncidentDirection,
	const FVector& SurfaceNormal) noexcept
{
	const FVector I = IncidentDirection.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	const FVector N = SurfaceNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const FVector Reflected = I - 2.0 * FVector::DotProduct(I, N) * N;
	return Reflected.GetSafeNormal(UE_SMALL_NUMBER, I);
}

FMirrorLightBounceResult UMirrorLightBounceComponent::EvaluateFlashlightBounce(
	const FVector& FlashlightOrigin,
	const FVector& FlashlightDirection,
	const float IncidentIntensity)
{
	FMirrorLightBounceResult Result;
	const FVector I = FlashlightDirection.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	const UWorld* World = GetWorld();

	float BestDist = MaxIncidentRangeUU + 1.0f;
	bool bFoundReflectiveHit = false;
	FVector BestImpact = FVector::ZeroVector;
	FVector BestNormal = FVector::ForwardVector;

	// 1. Physics line trace along flashlight axis looking for MirrorReflective tag or MirrorWall_Solid
	if (World)
	{
		FHitResult HitResult;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MirrorFlashlightBounceTrace), false);
		QueryParams.bTraceComplex = false;
		if (const AActor* Owner = GetOwner())
		{
			QueryParams.AddIgnoredActor(Owner);
		}

		if (World->LineTraceSingleByChannel(
			HitResult,
			FlashlightOrigin,
			FlashlightOrigin + I * MaxIncidentRangeUU,
			ECC_Visibility,
			QueryParams))
		{
			const AActor* HitActor = HitResult.GetActor();
			const UPrimitiveComponent* HitComp = HitResult.GetComponent();
			const bool bIsReflective =
				(HitActor && HitActor->ActorHasTag(TEXT("MirrorReflective"))) ||
				(HitComp && HitComp->ComponentHasTag(TEXT("MirrorReflective"))) ||
				(Cast<AMirrorWall_Solid>(HitActor) != nullptr);

			if (bIsReflective && FVector::DotProduct(I, HitResult.ImpactNormal) < -0.05)
			{
				BestDist = HitResult.Distance;
				BestImpact = HitResult.ImpactPoint;
				BestNormal = HitResult.ImpactNormal.GetSafeNormal();
				bFoundReflectiveHit = true;
			}
		}
	}

	// 2. Deterministic analytical ray-plane check against registered HISM MirrorInstances
	ADungeoneerTileMapInstancer* Instancer = CachedInstancer.Get();
	if (Instancer)
	{
		const TArray<FDungeoneerMirrorInstanceEntry>& Mirrors = Instancer->GetMirrorEntries();
		for (int32 Idx = 0; Idx < Mirrors.Num(); ++Idx)
		{
			const FDungeoneerMirrorInstanceEntry& Entry = Mirrors[Idx];
			if (Entry.bIsIllusoryPhantasm)
			{
				continue;
			}

			float HitDist = 0.0f;
			FVector HitPt = FVector::ZeroVector;
			FVector HitN = FVector::ZeroVector;
			if (MirrorBounceDetail::IntersectRayWithMirrorEntry(FlashlightOrigin, I, Entry, BestDist, HitDist, HitPt, HitN))
			{
				BestDist = HitDist;
				BestImpact = HitPt;
				BestNormal = HitN;
				bFoundReflectiveHit = true;
			}
		}
	}

	if (!bFoundReflectiveHit)
	{
		if (ReflectedSpotLight)
		{
			ReflectedSpotLight->SetVisibility(false);
		}
		LastBounceResult = Result;
		return Result;
	}

	// 3. Apply exact specular law of reflection: R = I - 2 * (I . N) * N
	const FVector ReflectedDir = ComputeReflectedVector(I, BestNormal);
	const FVector BounceSpawnOrigin = BestImpact + BestNormal * 6.0f;
	const float BouncedRange = FMath::Max(250.0f, MaxReflectedRangeUU);

	Result.bDidBounce = true;
	Result.HitLocation = BestImpact;
	Result.SurfaceNormal = BestNormal;
	Result.ReflectedDirection = ReflectedDir;
	Result.BouncedRangeUU = BouncedRange;

	if (ReflectedSpotLight)
	{
		ReflectedSpotLight->SetWorldLocation(BounceSpawnOrigin);
		ReflectedSpotLight->SetWorldRotation(ReflectedDir.Rotation());
		ReflectedSpotLight->SetAttenuationRadius(BouncedRange);
		ReflectedSpotLight->SetIntensity(IncidentIntensity * BounceIntensityMultiplier);
		ReflectedSpotLight->SetInnerConeAngle(ReflectedInnerConeAngleDeg);
		ReflectedSpotLight->SetOuterConeAngle(ReflectedOuterConeAngleDeg);
		ReflectedSpotLight->SetVisibility(true);
	}

	// 4. Secondary raycast along R to reveal Mirror-Palace illusion seams
	if (Instancer)
	{
		float ClosestIllusionDist = BouncedRange + 1.0f;
		int32 BestIllusionIdx = INDEX_NONE;
		const TArray<FDungeoneerMirrorInstanceEntry>& Mirrors = Instancer->GetMirrorEntries();

		for (int32 Idx = 0; Idx < Mirrors.Num(); ++Idx)
		{
			const FDungeoneerMirrorInstanceEntry& Entry = Mirrors[Idx];
			if (!Entry.bIsIllusoryPhantasm)
			{
				continue;
			}

			float Dist = 0.0f;
			FVector UnusedPt = FVector::ZeroVector;
			FVector UnusedN = FVector::ZeroVector;
			if (MirrorBounceDetail::IntersectRayWithMirrorEntry(
				BounceSpawnOrigin,
				ReflectedDir,
				Entry,
				ClosestIllusionDist,
				Dist,
				UnusedPt,
				UnusedN))
			{
				ClosestIllusionDist = Dist;
				BestIllusionIdx = Idx;
			}
		}

		if (BestIllusionIdx != INDEX_NONE)
		{
			Instancer->RevealMirrorIllusionSeam(BestIllusionIdx, true);
			Result.bRevealedIllusionSeam = true;
			Result.RevealedMirrorEntryIndex = BestIllusionIdx;
		}
	}

	if (!Result.bRevealedIllusionSeam && World)
	{
		FHitResult BounceHit;
		FCollisionQueryParams BounceParams(SCENE_QUERY_STAT(MirrorReflectedSeamTrace), false);
		if (const AActor* Owner = GetOwner())
		{
			BounceParams.AddIgnoredActor(Owner);
		}
		if (World->LineTraceSingleByChannel(
			BounceHit,
			BounceSpawnOrigin,
			BounceSpawnOrigin + ReflectedDir * BouncedRange,
			ECC_Visibility,
			BounceParams))
		{
			if (Cast<AMirrorWall_Phantasm>(BounceHit.GetActor()))
			{
				Result.bRevealedIllusionSeam = true;
			}
		}
	}

	LastBounceResult = Result;
	return Result;
}

void UMirrorLightBounceComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!ReflectedSpotLight)
	{
		return;
	}

	if (const ADungeoneerCharacter* Character = Cast<ADungeoneerCharacter>(GetOwner()))
	{
		if (const USpotLightComponent* Flashlight = Character->GetFlashlightComponent())
		{
			EvaluateFlashlightBounce(
				Flashlight->GetComponentLocation(),
				Flashlight->GetForwardVector(),
				Flashlight->Intensity);
		}
	}
}
