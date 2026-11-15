#include "UI/DungeoneerMenuFooterWidget.h"
#include "Dungeon/DungeonGeneratorSubsystem.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"

UDungeoneerMenuFooterWidget::UDungeoneerMenuFooterWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UDungeoneerMenuFooterWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (const UWorld* World = GetWorld())
	{
		if (ADungeoneerGameState* GS = World->GetGameState<ADungeoneerGameState>())
		{
			BindToGameState(GS);
		}
	}

	RefreshTelemetryTextBlocks();
}

void UDungeoneerMenuFooterWidget::NativeDestruct()
{
	if (BoundGameState.IsValid() && SeedChangedDelegateHandle.IsValid())
	{
		BoundGameState->OnFloorSeedChanged.Remove(SeedChangedDelegateHandle);
	}
	SeedChangedDelegateHandle.Reset();
	BoundGameState.Reset();

	Super::NativeDestruct();
}

void UDungeoneerMenuFooterWidget::BindToGameState(ADungeoneerGameState* InGameState)
{
	if (BoundGameState.IsValid() && SeedChangedDelegateHandle.IsValid())
	{
		BoundGameState->OnFloorSeedChanged.Remove(SeedChangedDelegateHandle);
	}
	SeedChangedDelegateHandle.Reset();
	BoundGameState = InGameState;

	if (InGameState)
	{
		SeedChangedDelegateHandle = InGameState->OnFloorSeedChanged.AddUObject(this, &UDungeoneerMenuFooterWidget::HandleFloorSeedUpdated);
		HandleFloorSeedUpdated(InGameState->GetReplicatedFloorSeed());
	}
}

void UDungeoneerMenuFooterWidget::RequestDeterministicSeedReroll()
{
	if (ADungeoneerGameState* GS = BoundGameState.Get())
	{
		if (GS->HasAuthority())
		{
			GS->AuthorityRerollSeedDeterministically();
			return;
		}
	}

	// Offline / standalone preview deterministic LCG fallback synced to UDungeonGeneratorSubsystem
	const uint64 Prev = static_cast<uint64>(CachedFloorSeed.MasterSeed);
	const uint64 Next = (Prev * 6364136223846793005ULL + 1442695040888963407ULL) & 0x7FFFFFFFFFFFFFFFULL;
	CachedFloorSeed.MasterSeed = static_cast<int64>(Next);
	++CachedFloorSeed.RerollCount;

	if (UWorld* World = GetWorld())
	{
		if (UDungeonGeneratorSubsystem* DungeonSys = World->GetSubsystem<UDungeonGeneratorSubsystem>())
		{
			const FDungeonFloorGraph Graph = DungeonSys->GenerateFloorGraph(
				CachedFloorSeed.FloorIndex,
				static_cast<int32>(CachedFloorSeed.MasterSeed & 0x7FFFFFFFLL));
			LastPreviewNodeCount = Graph.Nodes.Num();
		}
	}

	RefreshTelemetryTextBlocks();
}

void UDungeoneerMenuFooterWidget::UpdateOctantTelemetry(ECompassOctant Octant, float PlayerBaseSpeedUU, float PlayerSlowRatio) noexcept
{
	CurrentOctant = Octant;
	CachedPlayerBaseSpeedUU = FMath::Max(0.0f, PlayerBaseSpeedUU);
	CachedPlayerSlowRatio = FMath::Clamp(PlayerSlowRatio, 0.0f, 0.65f);
	RefreshTelemetryTextBlocks();
}

void UDungeoneerMenuFooterWidget::HandleFloorSeedUpdated(const FDungeonFloorSeed& NewSeed)
{
	CachedFloorSeed = NewSeed;

	if (const UWorld* World = GetWorld())
	{
		if (const UDungeonGeneratorSubsystem* DungeonSys = World->GetSubsystem<UDungeonGeneratorSubsystem>())
		{
			LastPreviewNodeCount = DungeonSys->GetActiveFloorGraph().Nodes.Num();
		}
	}

	RefreshTelemetryTextBlocks();
}

FString UDungeoneerMenuFooterWidget::GetFormattedSeedHex() const
{
	const uint32 Seed32 = static_cast<uint32>(CachedFloorSeed.MasterSeed & 0xFFFFFFFFLL);
	return FString::Printf(TEXT("SEED 0x%08X [F%d #%d]"), Seed32, CachedFloorSeed.FloorIndex, CachedFloorSeed.RerollCount);
}

FString UDungeoneerMenuFooterWidget::GetOctantTelemetryText() const
{
	const int32 AngleDeg = static_cast<int32>(DungeoneerMath::GCompassYawDegrees[static_cast<uint8>(CurrentOctant) & 7]);
	const FVector Dir = DungeoneerMath::OctantToDirection3D(CurrentOctant);
	const float EffectiveScalarSpeed = CachedPlayerBaseSpeedUU * FMath::Clamp(1.0f - CachedPlayerSlowRatio, 0.05f, 1.0f);
	return FString::Printf(
		TEXT("OCTANT %d DEG (%.3f, %.3f) | %.1f UU/s"),
		AngleDeg,
		Dir.X,
		Dir.Y,
		EffectiveScalarSpeed);
}

FString UDungeoneerMenuFooterWidget::GetChronoConservationText() const
{
	return FString::Printf(
		TEXT("CHRONO-LEECH dPlayer -%.2f == dThrower +%.2f"),
		CachedPlayerSlowRatio,
		CachedPlayerSlowRatio);
}

void UDungeoneerMenuFooterWidget::RefreshTelemetryTextBlocks() noexcept
{
	if (Txt_FloorSeedHex)
	{
		Txt_FloorSeedHex->SetText(FText::FromString(GetFormattedSeedHex()));
	}
	if (Txt_OctantTelemetry)
	{
		Txt_OctantTelemetry->SetText(FText::FromString(GetOctantTelemetryText()));
	}
	if (Txt_ChronoConservation)
	{
		Txt_ChronoConservation->SetText(FText::FromString(GetChronoConservationText()));
	}
}
