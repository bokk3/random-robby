#include "Dungeon/MirrorMazeActors.h"
#include "AI/ZombieBase.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

AMirrorWall_Solid::AMirrorWall_Solid()
{
	PrimaryActorTick.bCanEverTick = false;

	MirrorPlaneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MirrorPlaneMesh"));
	SetRootComponent(MirrorPlaneMesh);
	MirrorPlaneMesh->SetCollisionProfileName(TEXT("BlockAll"));
	MirrorPlaneMesh->SetRenderCustomDepth(true);
	MirrorPlaneMesh->SetCustomDepthStencilValue(MirrorStencilIds::SolidMirrorStencil);

	ReflectedBounceLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("ReflectedBounceLight"));
	ReflectedBounceLight->SetupAttachment(RootComponent);
	ReflectedBounceLight->SetIntensity(6500.0f);
	ReflectedBounceLight->SetInnerConeAngle(10.0f);
	ReflectedBounceLight->SetOuterConeAngle(20.0f);
	ReflectedBounceLight->bUseInverseSquaredFalloff = true;
	ReflectedBounceLight->VolumetricScatteringIntensity = 1.8f;
	ReflectedBounceLight->SetVisibility(false);
}

void AMirrorWall_Solid::BeginPlay()
{
	Super::BeginPlay();
	MirrorPlaneMesh->SetCustomPrimitiveDataFloat(0, 0.05f); // Crisp mirror specular roughness
}

void AMirrorWall_Solid::EvaluateFlashlightBounce(const FVector& BeamOrigin, const FVector& BeamDirection, float BeamRange) noexcept
{
	if (!MirrorPlaneMesh || !ReflectedBounceLight)
	{
		return;
	}

	const FVector WallPos = GetActorLocation();
	const FVector WallNormal = GetActorForwardVector().GetSafeNormal();
	const float Dist = static_cast<float>(FVector::Dist2D(BeamOrigin, WallPos));

	if (Dist > BeamRange || FVector::DotProduct(BeamDirection.GetSafeNormal(), WallNormal) >= -0.1f)
	{
		ReflectedBounceLight->SetVisibility(false);
		return;
	}

	const FVector ReflectedDir = FMath::GetReflectionVector(BeamDirection.GetSafeNormal(), WallNormal);
	ReflectedBounceLight->SetWorldLocation(WallPos + WallNormal * 18.0f);
	ReflectedBounceLight->SetWorldRotation(ReflectedDir.Rotation());
	ReflectedBounceLight->SetAttenuationRadius(FMath::Max(250.0f, BeamRange - Dist));
	ReflectedBounceLight->SetVisibility(true);
}

AMirrorWall_Phantasm::AMirrorWall_Phantasm()
{
	PrimaryActorTick.bCanEverTick = false;

	IllusionMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("IllusionMesh"));
	SetRootComponent(IllusionMesh);
	// Pass-through collision: allows players to step cardinally or diagonally straight through
	IllusionMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	IllusionMesh->SetCollisionResponseToAllChannels(ECR_Overlap);
	IllusionMesh->SetRenderCustomDepth(true);
	IllusionMesh->SetCustomDepthStencilValue(MirrorStencilIds::PhantasmMirrorStencil);

	PortalReflectionCapture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("PortalReflectionCapture"));
	PortalReflectionCapture->SetupAttachment(RootComponent);
	PortalReflectionCapture->bCaptureEveryFrame = false;
	PortalReflectionCapture->bCaptureOnMovement = false;
	PortalReflectionCapture->MaxViewDistanceOverride = 1800.0f; // Frustum/distance culled half-res capture
	PortalReflectionCapture->ShowFlags.SetVolumetricFog(false);
}

void AMirrorWall_Phantasm::BeginPlay()
{
	Super::BeginPlay();

	// Subtle normal-map specular roughness divergence so upgraded senses can detect the illusion
	if (IllusionMesh)
	{
		IllusionMesh->SetCustomPrimitiveDataFloat(0, SubtleRoughnessDivergence);
		IllusionMesh->SetCustomPrimitiveDataFloat(1, 1.0f); // Illusion shader divergence flag
	}

	if (PortalReflectionCapture && !PortalReflectionCapture->TextureTarget)
	{
		// Capped at half-resolution (512x288) per 02-rendering-lighting-25d.md
		UTextureRenderTarget2D* HalfResTarget = NewObject<UTextureRenderTarget2D>(this);
		HalfResTarget->InitAutoFormat(512, 288);
		PortalReflectionCapture->TextureTarget = HalfResTarget;
	}

	if (UWorld* World = GetWorld())
	{
		if (UMirrorCaptureBudgetSubsystem* BudgetSys = World->GetSubsystem<UMirrorCaptureBudgetSubsystem>())
		{
			BudgetSys->RegisterPortal(this);
		}
	}
}

AMirrorDecoyNode::AMirrorDecoyNode()
{
	PrimaryActorTick.bCanEverTick = true;

	DecoyVisualPlane = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DecoyVisualPlane"));
	SetRootComponent(DecoyVisualPlane);
	// Zero collision, zero damage, zero physical shadow (only true zombie casts physical shadow & footsteps)
	DecoyVisualPlane->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DecoyVisualPlane->SetCastShadow(false);
	DecoyVisualPlane->SetRenderCustomDepth(true);
	DecoyVisualPlane->SetCustomDepthStencilValue(MirrorStencilIds::DecoyZombieStencil);
}

void AMirrorDecoyNode::BeginPlay()
{
	Super::BeginPlay();
	if (DecoyVisualPlane)
	{
		DecoyVisualPlane->SetCastShadow(false);
	}
}

void AMirrorDecoyNode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (const AZombieBase* RealZombie = TrueSourceZombie.Get())
	{
		if (!RealZombie->IsZombieAlive())
		{
			SetActorHiddenInGame(true);
			return;
		}

		// Reflect real zombie offset across MirrorPlaneNormal
		const FVector AnchorLoc = GetActorLocation();
		const FVector Delta = RealZombie->GetActorLocation() - AnchorLoc;
		const FVector Normal = MirrorPlaneNormal.GetSafeNormal();
		const FVector ReflectedPos = AnchorLoc + Delta - 2.0 * FVector::DotProduct(Delta, Normal) * Normal;
		DecoyVisualPlane->SetWorldLocation(ReflectedPos);
	}
}

void AMirrorWall_Phantasm::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UMirrorCaptureBudgetSubsystem* BudgetSys = World->GetSubsystem<UMirrorCaptureBudgetSubsystem>())
		{
			BudgetSys->UnregisterPortal(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AMirrorWall_Phantasm::CapturePortalDeferred() noexcept
{
	if (PortalReflectionCapture)
	{
		PortalReflectionCapture->CaptureSceneDeferred();
	}
}

void UMirrorCaptureBudgetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	NumRegisteredPortals = 0;
	RoundRobinCursor = 0;
	FrameTickCounter = 0;
}

void UMirrorCaptureBudgetSubsystem::Deinitialize()
{
	for (int32 i = 0; i < NumRegisteredPortals; ++i)
	{
		RegisteredPortals[i].Reset();
	}
	NumRegisteredPortals = 0;
	Super::Deinitialize();
}

void UMirrorCaptureBudgetSubsystem::RegisterPortal(AMirrorWall_Phantasm* Portal) noexcept
{
	if (!Portal)
	{
		return;
	}

	for (int32 i = 0; i < NumRegisteredPortals; ++i)
	{
		if (RegisteredPortals[i].Get() == Portal)
		{
			return;
		}
	}

	if (NumRegisteredPortals < MaxTrackedMirrorPortals)
	{
		RegisteredPortals[NumRegisteredPortals++] = Portal;
	}
}

void UMirrorCaptureBudgetSubsystem::UnregisterPortal(AMirrorWall_Phantasm* Portal) noexcept
{
	if (!Portal)
	{
		return;
	}

	for (int32 i = 0; i < NumRegisteredPortals; ++i)
	{
		if (RegisteredPortals[i].Get() == Portal)
		{
			RegisteredPortals[i] = RegisteredPortals[NumRegisteredPortals - 1];
			RegisteredPortals[NumRegisteredPortals - 1].Reset();
			--NumRegisteredPortals;
			if (NumRegisteredPortals > 0)
			{
				RoundRobinCursor %= NumRegisteredPortals;
			}
			else
			{
				RoundRobinCursor = 0;
			}
			return;
		}
	}
}

int32 UMirrorCaptureBudgetSubsystem::ComputeFrameCaptureBudget() const noexcept
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	const int32 ActiveLocalPlayers = GI ? FMath::Max(1, GI->GetNumLocalPlayers()) : 1;

	if (ActiveLocalPlayers <= 1)
	{
		return MaxCapturesPerFrameBase;
	}
	if (ActiveLocalPlayers == 2)
	{
		return 1;
	}

	return ((FrameTickCounter & 1U) == 0U) ? 1 : 0;
}

void UMirrorCaptureBudgetSubsystem::ExecuteBudgetedCaptures() noexcept
{
	++FrameTickCounter;

	// Compact stale weak pointers in-place with zero heap allocations
	for (int32 i = NumRegisteredPortals - 1; i >= 0; --i)
	{
		if (!RegisteredPortals[i].IsValid())
		{
			RegisteredPortals[i] = RegisteredPortals[NumRegisteredPortals - 1];
			RegisteredPortals[NumRegisteredPortals - 1].Reset();
			--NumRegisteredPortals;
		}
	}

	if (NumRegisteredPortals <= 0)
	{
		RoundRobinCursor = 0;
		return;
	}

	RoundRobinCursor %= NumRegisteredPortals;

	const int32 FrameBudget = ComputeFrameCaptureBudget();
	const int32 CapturesToRun = FMath::Min(FrameBudget, NumRegisteredPortals);
	for (int32 Step = 0; Step < CapturesToRun; ++Step)
	{
		if (AMirrorWall_Phantasm* Portal = RegisteredPortals[RoundRobinCursor].Get())
		{
			Portal->CapturePortalDeferred();
		}
		RoundRobinCursor = (RoundRobinCursor + 1) % NumRegisteredPortals;
	}
}

void UMirrorCaptureBudgetSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ExecuteBudgetedCaptures();
}
