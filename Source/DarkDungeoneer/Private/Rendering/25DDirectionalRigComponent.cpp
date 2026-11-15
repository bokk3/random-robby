/**
 * 25DDirectionalRigComponent.cpp
 * 
 * Implementation of 2.5D directional cutout multi-plane animation and Custom Primitive Data updates.
 */

#include "Rendering/25DDirectionalRigComponent.h"
#include "Components/PrimitiveComponent.h"

U25DDirectionalRigComponent::U25DDirectionalRigComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	// Establish distinct multi-plane depth layering to prevent Z-fighting in top-down 2.5D
	SocketConfigs[static_cast<int32>(E25DRigSocket::Torso)].DepthLayerY = 0.0f;
	SocketConfigs[static_cast<int32>(E25DRigSocket::Head)].DepthLayerY = 0.5f;
	SocketConfigs[static_cast<int32>(E25DRigSocket::Limbs)].DepthLayerY = -0.5f;
	SocketConfigs[static_cast<int32>(E25DRigSocket::WeaponFlashlight)].DepthLayerY = 1.0f;

	for (int32 i = 0; i < NumSockets; ++i)
	{
		SocketPrimitives[i] = nullptr;
	}
}

void U25DDirectionalRigComponent::BeginPlay()
{
	Super::BeginPlay();
	PushCustomPrimitiveData();
}

void U25DDirectionalRigComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Advance frame timer without dynamic memory allocations
	if (bIsAnimating && GridColumns > 1)
	{
		AnimationTimer += DeltaTime;
		const float FrameDuration = 1.0f / FMath::Max(AnimationFps, 0.01f);
		if (AnimationTimer >= FrameDuration)
		{
			CurrentFrameIndex = (CurrentFrameIndex + 1) % GridColumns;
			AnimationTimer = FMath::Fmod(AnimationTimer, FrameDuration);
		}
	}

	// Decay hit / fuse emissive pulse
	if (EmissivePulseIntensity > 0.0f)
	{
		EmissivePulseIntensity = FMath::Max(0.0f, EmissivePulseIntensity - EmissivePulseDecaySpeed * DeltaTime);
	}

	PushCustomPrimitiveData();
}

void U25DDirectionalRigComponent::RegisterSocketPrimitive(E25DRigSocket Socket, UPrimitiveComponent* PrimitiveComponent)
{
	const int32 Index = static_cast<int32>(Socket);
	if (Index >= 0 && Index < NumSockets)
	{
		SocketPrimitives[Index] = PrimitiveComponent;
		PushCustomPrimitiveData();
	}
}

void U25DDirectionalRigComponent::SetHeadingOctant(ECompassOctant NewOctant) noexcept
{
	if (CurrentOctant != NewOctant)
	{
		CurrentOctant = NewOctant;
		PushCustomPrimitiveData();
	}
}

void U25DDirectionalRigComponent::SetFrameIndex(int32 Frame) noexcept
{
	CurrentFrameIndex = FMath::Clamp(Frame, 0, FMath::Max(GridColumns - 1, 0));
	PushCustomPrimitiveData();
}

void U25DDirectionalRigComponent::TriggerEmissivePulse(float Intensity) noexcept
{
	EmissivePulseIntensity = FMath::Max(EmissivePulseIntensity, Intensity);
}

void U25DDirectionalRigComponent::SetGridDimensions(int32 InColumns, int32 InRows) noexcept
{
	GridColumns = FMath::Max(InColumns, 1);
	GridRows = FMath::Max(InRows, 1);
	CurrentFrameIndex = FMath::Clamp(CurrentFrameIndex, 0, GridColumns - 1);
	PushCustomPrimitiveData();
}

void U25DDirectionalRigComponent::PushCustomPrimitiveData() noexcept
{
	const float CellScaleX = 1.0f / static_cast<float>(FMath::Max(GridColumns, 1));
	const float CellScaleY = 1.0f / static_cast<float>(FMath::Max(GridRows, 1));

	uint8 RowIndex = 0;
	if (bIs8CompassBillboard)
	{
		RowIndex = static_cast<uint8>(CurrentOctant) & 7;
	}
	else
	{
		RowIndex = DungeoneerMath::OctantToCardinalRow(CurrentOctant);
	}

	const float CellOffsetX = static_cast<float>(CurrentFrameIndex) * CellScaleX;
	const float CellOffsetY = static_cast<float>(RowIndex) * CellScaleY;

	// Push parameters to GPU Custom Primitive Data (zero dynamic MID allocation, zero draw-call batch breakage)
	for (int32 i = 0; i < NumSockets; ++i)
	{
		if (UPrimitiveComponent* Prim = SocketPrimitives[i].Get())
		{
			Prim->SetCustomPrimitiveDataFloat(E25DCPDIndex::CellUVOffsetX, CellOffsetX);
			Prim->SetCustomPrimitiveDataFloat(E25DCPDIndex::CellUVOffsetY, CellOffsetY);
			Prim->SetCustomPrimitiveDataFloat(E25DCPDIndex::CellUVScaleX, CellScaleX);
			Prim->SetCustomPrimitiveDataFloat(E25DCPDIndex::CellUVScaleY, CellScaleY);
			Prim->SetCustomPrimitiveDataFloat(E25DCPDIndex::DirectionalRow, static_cast<float>(RowIndex));
			Prim->SetCustomPrimitiveDataFloat(E25DCPDIndex::EmissivePulse, EmissivePulseIntensity);
			Prim->SetCustomPrimitiveDataFloat(E25DCPDIndex::HorizontalFlip, 1.0f);
			Prim->SetCustomPrimitiveDataFloat(E25DCPDIndex::DepthOffset, SocketConfigs[i].DepthLayerY);
		}
	}
}

void U25DDirectionalRigComponent::ConfigureVolumetricShadowProxy(UPrimitiveComponent* ProxyMesh) noexcept
{
	if (!ProxyMesh)
	{
		return;
	}

	ProxyMesh->SetRenderInMainPass(false);
	ProxyMesh->SetCastHiddenShadow(true);
	ProxyMesh->SetCastShadow(true);
}
