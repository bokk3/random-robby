#include "Core/DungeoneerLocalPlayerSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "SceneView.h"

FDungeoneerSensoryViewExtension::FDungeoneerSensoryViewExtension(
	const FAutoRegister& AutoRegister,
	ULocalPlayer* InLocalPlayer,
	UDungeoneerLocalPlayerSubsystem* InOwnerSubsystem)
	: FSceneViewExtensionBase(AutoRegister)
	, LocalPlayerPtr(InLocalPlayer)
	, OwnerSubsystem(InOwnerSubsystem)
{
}

bool FDungeoneerSensoryViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	const ULocalPlayer* LocalPlayer = LocalPlayerPtr.Get();
	if (!LocalPlayer || !OwnerSubsystem.IsValid())
	{
		return false;
	}

	const UWorld* ContextWorld = Context.GetWorld();
	return ContextWorld == nullptr || ContextWorld == LocalPlayer->GetWorld();
}

void FDungeoneerSensoryViewExtension::SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
{
	const ULocalPlayer* LocalPlayer = LocalPlayerPtr.Get();
	const UDungeoneerLocalPlayerSubsystem* Subsystem = OwnerSubsystem.Get();
	if (!LocalPlayer || !Subsystem)
	{
		return;
	}

	// Scope post-process material weighting strictly to the owning ULocalPlayer's PlayerIndex
	if (InView.PlayerIndex != LocalPlayer->GetControllerId())
	{
		return;
	}

	if (UMaterialInstanceDynamic* MID = Subsystem->GetViewportSensoryMID())
	{
		FPostProcessSettings& PP = InView.FinalPostProcessSettings;
		PP.AddBlendable(MID, 1.0f);
	}
}

void UDungeoneerLocalPlayerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (ULocalPlayer* LP = GetLocalPlayer<ULocalPlayer>())
	{
		ViewExtension = FSceneViewExtensions::NewExtension<FDungeoneerSensoryViewExtension>(LP, this);
	}
}

void UDungeoneerLocalPlayerSubsystem::Deinitialize()
{
	ViewExtension.Reset();
	SensoryOverlayMID = nullptr;
	Super::Deinitialize();
}

void UDungeoneerLocalPlayerSubsystem::SetSensoryOverlayBaseMaterial(UMaterialInterface* BaseMaterial)
{
	if (!BaseMaterial)
	{
		SensoryOverlayMID = nullptr;
		return;
	}

	if (!SensoryOverlayMID || SensoryOverlayMID->Parent != BaseMaterial)
	{
		SensoryOverlayMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	}

	UpdateViewportSensoryState(CurrentViewState);
}

void UDungeoneerLocalPlayerSubsystem::UpdateViewportSensoryState(const FDungeoneerSensoryViewState& NewState) noexcept
{
	CurrentViewState = NewState;

	if (!SensoryOverlayMID)
	{
		return;
	}

	// Push per-viewport isolated parameters (zero cross-viewport bleed in split-screen)
	SensoryOverlayMID->SetScalarParameterValue(TEXT("DarkVisionRadius"), CurrentViewState.DarkVisionRadius);
	SensoryOverlayMID->SetScalarParameterValue(TEXT("DarkVisionFogExtinctionScale"), CurrentViewState.DarkVisionFogExtinctionScale);
	SensoryOverlayMID->SetScalarParameterValue(TEXT("AcuteHearingIntensity"), CurrentViewState.AcuteHearingIntensity);
	SensoryOverlayMID->SetScalarParameterValue(TEXT("ScentThermalIntensity"), CurrentViewState.ScentThermalIntensity);
	SensoryOverlayMID->SetVectorParameterValue(TEXT("PlayerWorldOrigin"), FLinearColor(CurrentViewState.PlayerWorldOrigin));
	SensoryOverlayMID->SetVectorParameterValue(TEXT("HearingPulseOrigin"), FLinearColor(CurrentViewState.LatestHearingPulseOrigin));
	SensoryOverlayMID->SetScalarParameterValue(TEXT("HearingPulseRadius"), CurrentViewState.LatestHearingPulseRadius);
}
