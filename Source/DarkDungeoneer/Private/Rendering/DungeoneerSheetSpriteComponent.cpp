#include "Rendering/DungeoneerSheetSpriteComponent.h"
#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

UDungeoneerSheetSpriteComponent::UDungeoneerSheetSpriteComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCastShadow(false);
	SetVisibility(false);
}

bool UDungeoneerSheetSpriteComponent::InitSheet(const TCHAR* CellPrefix)
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UDungeoneerAssetBootstrapSubsystem* Bootstrap = GI ? GI->GetSubsystem<UDungeoneerAssetBootstrapSubsystem>() : nullptr;
	if (!Bootstrap || !CellPrefix || !Bootstrap->GetExtractedCellTexture(FName(*FString::Printf(TEXT("%s_R0_C0_D"), CellPrefix))))
	{
		bHasSheet = false;
		return false;
	}
	if (!GetStaticMesh())
	{
		if (UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
		{
			SetStaticMesh(PlaneMesh);
		}
	}
	SetRelativeRotation(Bootstrap->GetCameraFacingSpritePlaneRotation());

	CellMaterials.Reset();
	CellMaterials.SetNum(16);
	for (int32 R = 0; R < 4; ++R)
	{
		for (int32 C = 0; C < 4; ++C)
		{
			CellMaterials[R * 4 + C] = Bootstrap->CreateDungeonOrRigMaterial(
				GetOwner(),
				FName(*FString::Printf(TEXT("%s_R%d_C%d_D"), CellPrefix, R, C)),
				FName(*FString::Printf(TEXT("%s_R%d_C%d_N"), CellPrefix, R, C)),
				FLinearColor::White, 0.6f, false, true);
		}
	}
	bHasSheet = true;
	SetVisibility(true);
	PlayRow(0, 5.0f, true);
	return true;
}

void UDungeoneerSheetSpriteComponent::PlayRow(const int32 InRow, const float InFps, const bool bInLoop) noexcept
{
	Row = FMath::Clamp(InRow, 0, 3);
	Fps = FMath::Max(InFps, 0.1f);
	bLoop = bInLoop;
	Frame = 0;
	FrameTimer = 0.0f;
	bFinished = false;
	ApplyCell();
}

void UDungeoneerSheetSpriteComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bHasSheet || bFinished)
	{
		return;
	}
	FrameTimer += DeltaTime;
	const float FrameTime = 1.0f / Fps;
	if (FrameTimer < FrameTime)
	{
		return;
	}
	FrameTimer -= FrameTime;
	if (Frame < 3)
	{
		++Frame;
	}
	else if (bLoop)
	{
		Frame = 0;
	}
	else
	{
		bFinished = true;
		return;
	}
	ApplyCell();
}

void UDungeoneerSheetSpriteComponent::ApplyCell() noexcept
{
	const int32 Index = Row * 4 + Frame;
	if (CellMaterials.IsValidIndex(Index) && CellMaterials[Index] && GetMaterial(0) != CellMaterials[Index])
	{
		SetMaterial(0, CellMaterials[Index]);
	}
}
