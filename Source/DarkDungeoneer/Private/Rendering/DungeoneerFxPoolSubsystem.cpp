#include "Rendering/DungeoneerFxPoolSubsystem.h"
#include "Rendering/DungeoneerSpriteMaterialFactory.h"
#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace FxPoolPrivate
{
	/** Cheap deterministic spread for spawn angles and speeds (no RNG state, no allocation). */
	inline float Hash01(uint32 Seed) noexcept
	{
		Seed ^= Seed >> 16;
		Seed *= 0x7feb352dU;
		Seed ^= Seed >> 15;
		Seed *= 0x846ca68bU;
		Seed ^= Seed >> 16;
		return static_cast<float>(Seed & 0xFFFFu) / 65535.0f;
	}

	/** Plane mesh is 100 UU wide, so scale 1 = 100 UU. */
	constexpr float PlaneSizeUU = 100.0f;
}

bool UDungeoneerFxPoolSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UDungeoneerFxPoolSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	const UGameInstance* GI = InWorld.GetGameInstance();
	UDungeoneerAssetBootstrapSubsystem* Bootstrap = GI ? GI->GetSubsystem<UDungeoneerAssetBootstrapSubsystem>() : nullptr;
	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (!Bootstrap || !PlaneMesh || !Bootstrap->EnsureFxAdditiveMaterial())
	{
		UE_LOG(LogTemp, Display, TEXT("FxPool: not started (bootstrap %d, plane %d)"), Bootstrap != nullptr, PlaneMesh != nullptr);
		return; // Headless test worlds have no bootstrap; effects are then simply skipped.
	}

	UprightRotation = Bootstrap->GetCameraFacingSpritePlaneRotation();

	static constexpr const TCHAR* ShapeKeys[static_cast<int32>(EShape::Count)] = { TEXT("FxSpark_D"), TEXT("FxRing_D"), TEXT("SlashArc_D") };
	ShapeMaterials.Reset();
	for (const TCHAR* Key : ShapeKeys)
	{
		ShapeMaterials.Add(Bootstrap->CreateFxMaterial(this, FName(Key), FLinearColor::White, 2.4f));
	}

	FActorSpawnParameters Params;
	Params.ObjectFlags |= RF_Transient;
	PoolOwner = InWorld.SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!PoolOwner)
	{
		return;
	}
	USceneComponent* Root = NewObject<USceneComponent>(PoolOwner, TEXT("FxPoolRoot"));
	PoolOwner->SetRootComponent(Root);
	Root->RegisterComponent();

	Quads.Reset(PoolSize);
	for (int32 i = 0; i < PoolSize; ++i)
	{
		UStaticMeshComponent* Quad = NewObject<UStaticMeshComponent>(PoolOwner);
		Quad->SetStaticMesh(PlaneMesh);
		Quad->SetUsingAbsoluteLocation(true);
		Quad->SetUsingAbsoluteRotation(true);
		Quad->SetUsingAbsoluteScale(true);
		Quad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Quad->SetCastShadow(false);
		Quad->SetVisibility(false);
		Quad->SetupAttachment(Root);
		Quad->RegisterComponent();
		// Size the custom data array now so Tick never grows it.
		Quad->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FxB, 1.0f);
		Quad->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FxG, 1.0f);
		Quad->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FxR, 1.0f);
		Quad->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FxFadeOut, 1.0f);
		Quads.Add(Quad);
	}
	bPoolReady = true;
	UE_LOG(LogTemp, Display, TEXT("FxPool: %d quads ready, materials %d/%d/%d, world %s"), Quads.Num(),
		ShapeMaterials.IsValidIndex(0) && ShapeMaterials[0] != nullptr, ShapeMaterials.IsValidIndex(1) && ShapeMaterials[1] != nullptr,
		ShapeMaterials.IsValidIndex(2) && ShapeMaterials[2] != nullptr, *InWorld.GetName());
}

TStatId UDungeoneerFxPoolSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UDungeoneerFxPoolSubsystem, STATGROUP_Tickables);
}

UDungeoneerFxPoolSubsystem::FParticle* UDungeoneerFxPoolSubsystem::Acquire(const EShape Shape, int32& OutIndex) noexcept
{
	if (!bPoolReady)
	{
		return nullptr;
	}
	// Prefer a free slot near the cursor; if the pool is full, the slot at the cursor is the oldest reuse.
	int32 Index = NextSlot;
	for (int32 Probe = 0; Probe < PoolSize; ++Probe)
	{
		const int32 Candidate = (NextSlot + Probe) % PoolSize;
		if (!Particles[Candidate].bActive)
		{
			Index = Candidate;
			break;
		}
	}
	NextSlot = (Index + 1) % PoolSize;

	FParticle& P = Particles[Index];
	if (!P.bActive)
	{
		++ActiveCount;
	}
	if (SpawnCounter == 0)
	{
		UE_LOG(LogTemp, Display, TEXT("FxPool: first effect spawned (slot %d)"), Index);
	}
	P = FParticle();
	P.bActive = true;
	P.Shape = Shape;
	OutIndex = Index;

	if (UStaticMeshComponent* Quad = Quads[Index].Get())
	{
		UMaterialInstanceDynamic* Mat = ShapeMaterials.IsValidIndex(static_cast<int32>(Shape)) ? ShapeMaterials[static_cast<int32>(Shape)].Get() : nullptr;
		if (Quad->GetMaterial(0) != Mat)
		{
			Quad->SetMaterial(0, Mat);
		}
		Quad->SetVisibility(true);
	}
	++SpawnCounter;
	return &P;
}

void UDungeoneerFxPoolSubsystem::SpawnHitSparks(const FVector& At, const FVector& DirXY, const FLinearColor& Color, const int32 Count, const float Speed)
{
	const FVector Forward = DirXY.GetSafeNormal2D();
	for (int32 i = 0; i < Count; ++i)
	{
		int32 Index = 0;
		FParticle* P = Acquire(EShape::Spark, Index);
		if (!P)
		{
			return;
		}
		const float Spread = (FxPoolPrivate::Hash01(SpawnCounter * 7u + 1u) - 0.5f) * 1.6f; // about +-45 degrees
		const FVector Dir = Forward.IsNearlyZero() ? FVector(1.0, 0.0, 0.0) : Forward.RotateAngleAxis(FMath::RadiansToDegrees(Spread), FVector::UpVector);
		const float SpeedScale = 0.6f + FxPoolPrivate::Hash01(SpawnCounter * 13u + 5u) * 0.8f;
		P->Location = At;
		P->Velocity = Dir * (Speed * SpeedScale) + FVector(0.0, 0.0, 160.0 * SpeedScale);
		P->Gravity = 900.0f;
		P->Drag = 3.0f;
		P->Life = 0.22f + 0.1f * SpeedScale;
		P->StartScale = 0.26f;
		P->EndScale = 0.06f;
		P->Color = Color;
		ApplyToComponent(Index);
	}
}

void UDungeoneerFxPoolSubsystem::SpawnBurst(const FVector& At, const FLinearColor& Color, const int32 Count, const float Speed, const float Size)
{
	const float Offset = FxPoolPrivate::Hash01(SpawnCounter * 3u + 11u) * 360.0f;
	for (int32 i = 0; i < Count; ++i)
	{
		int32 Index = 0;
		FParticle* P = Acquire(EShape::Spark, Index);
		if (!P)
		{
			return;
		}
		const float Yaw = Offset + 360.0f * static_cast<float>(i) / static_cast<float>(FMath::Max(Count, 1));
		const FVector Dir = FRotator(0.0f, Yaw, 0.0f).Vector();
		const float SpeedScale = 0.7f + FxPoolPrivate::Hash01(SpawnCounter * 17u + 3u) * 0.6f;
		P->Location = At;
		P->Velocity = Dir * (Speed * SpeedScale) + FVector(0.0, 0.0, 120.0);
		P->Gravity = 500.0f;
		P->Drag = 2.5f;
		P->Life = 0.45f;
		P->StartScale = Size;
		P->EndScale = Size * 0.2f;
		P->Color = Color;
		ApplyToComponent(Index);
	}
}

void UDungeoneerFxPoolSubsystem::SpawnGroundRing(const FVector& At, const FLinearColor& Color, const float StartRadiusUU, const float EndRadiusUU, const float LifeSec)
{
	int32 Index = 0;
	FParticle* P = Acquire(EShape::Ring, Index);
	if (!P)
	{
		return;
	}
	// The ring texture's band sits at 82% of the quad's half-width, so radius R needs a quad R / 0.82 across each half.
	const float RadiusToScale = 2.0f / (0.82f * FxPoolPrivate::PlaneSizeUU);
	P->Location = FVector(At.X, At.Y, At.Z + 4.0);
	P->bGround = true;
	P->Life = FMath::Max(LifeSec, 0.05f);
	P->StartScale = StartRadiusUU * RadiusToScale;
	P->EndScale = EndRadiusUU * RadiusToScale;
	P->Color = Color;
	ApplyToComponent(Index);
}

void UDungeoneerFxPoolSubsystem::SpawnDust(const FVector& FeetAt)
{
	for (int32 i = 0; i < 2; ++i)
	{
		int32 Index = 0;
		FParticle* P = Acquire(EShape::Spark, Index);
		if (!P)
		{
			return;
		}
		const double Side = (i == 0) ? -1.0 : 1.0;
		P->Location = FeetAt + FVector(0.0, 0.0, 10.0);
		P->Velocity = FVector(Side * 70.0, -Side * 70.0, 40.0);
		P->Drag = 4.0f;
		P->Life = 0.35f;
		P->StartScale = 0.3f;
		P->EndScale = 0.55f;
		P->Color = FLinearColor(0.32f, 0.29f, 0.25f);
		ApplyToComponent(Index);
	}
}

void UDungeoneerFxPoolSubsystem::Tick(const float RawDeltaTime)
{
	if (!bPoolReady || ActiveCount == 0)
	{
		return;
	}
	// A hitch (shader compile, level streaming) must not age effects out before they are ever drawn.
	const float DeltaTime = FMath::Min(RawDeltaTime, 0.05f);
	for (int32 i = 0; i < PoolSize; ++i)
	{
		FParticle& P = Particles[i];
		if (!P.bActive)
		{
			continue;
		}
		P.Age += DeltaTime;
		if (P.Age >= P.Life)
		{
			P.bActive = false;
			--ActiveCount;
			if (UStaticMeshComponent* Quad = Quads[i].Get())
			{
				Quad->SetVisibility(false);
			}
			continue;
		}
		P.Velocity.Z -= P.Gravity * DeltaTime;
		P.Velocity *= FMath::Max(0.0f, 1.0f - P.Drag * DeltaTime);
		P.Location += P.Velocity * DeltaTime;
		ApplyToComponent(i);
	}
}

void UDungeoneerFxPoolSubsystem::ApplyToComponent(const int32 Index) const noexcept
{
	UStaticMeshComponent* Quad = Quads.IsValidIndex(Index) ? Quads[Index].Get() : nullptr;
	if (!Quad)
	{
		return;
	}
	const FParticle& P = Particles[Index];
	const float T = FMath::Clamp(P.Age / FMath::Max(P.Life, 0.01f), 0.0f, 1.0f);
	const float Scale = FMath::Lerp(P.StartScale, P.EndScale, T);
	const float Fade = 1.0f - T * T;
	Quad->SetWorldLocationAndRotation(P.Location, P.bGround ? FRotator::ZeroRotator : UprightRotation);
	Quad->SetWorldScale3D(FVector(Scale, Scale, 1.0f));
	Quad->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FxFadeOut, 1.0f - Fade);
	Quad->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FxR, P.Color.R);
	Quad->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FxG, P.Color.G);
	Quad->SetCustomPrimitiveDataFloat(DungeoneerSpriteCPD::FxB, P.Color.B);
}
