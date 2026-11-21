#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "DungeoneerSheetSpriteComponent.generated.h"

class UMaterialInstanceDynamic;

/**
 * A camera-facing sprite that plays one row of a sliced 4x4 sheet as a flipbook (cells
 * "<Prefix>_R<row>_C<col>_D" from the asset bootstrap). Used for NPCs, whose sheets are laid out
 * as one action per row rather than one facing per row.
 */
UCLASS(ClassGroup = (Dungeoneer), meta = (BlueprintSpawnableComponent))
class DARKDUNGEONEER_API UDungeoneerSheetSpriteComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UDungeoneerSheetSpriteComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Builds the 16 cell materials. Returns false (and stays hidden) if the sheet isn't on disk. */
	bool InitSheet(const TCHAR* CellPrefix);

	bool HasSheet() const noexcept { return bHasSheet; }

	/** Plays a row's four frames at Fps. Without looping it holds the last frame and IsFinished() becomes true. */
	void PlayRow(int32 InRow, float InFps, bool bInLoop) noexcept;

	int32 GetRow() const noexcept { return Row; }
	int32 GetFrame() const noexcept { return Frame; }
	bool IsFinished() const noexcept { return bFinished; }

private:
	void ApplyCell() noexcept;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> CellMaterials;

	int32 Row = 0;
	int32 Frame = 0;
	float FrameTimer = 0.0f;
	float Fps = 5.0f;
	bool bLoop = true;
	bool bFinished = false;
	bool bHasSheet = false;
};
