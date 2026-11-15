#pragma once

#include "CoreMinimal.h"
#include "Engine/GameViewportClient.h"
#include "DungeoneerGameViewportClient.generated.h"

/**
 * Custom GameViewportClient supporting 2-4 player Local Split-Screen alongside Online Multiplayer.
 * Configures split-screen layouts cleanly and routes per-ULocalPlayer viewport separation.
 */
UCLASS()
class DARKDUNGEONEER_API UDungeoneerGameViewportClient : public UGameViewportClient
{
	GENERATED_BODY()

public:
	UDungeoneerGameViewportClient();

	virtual void Init(struct FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice = true) override;
	virtual void UpdateActiveSplitscreenType() override;

	/** Enables or disables local split-screen without tearing down online session peers. */
	UFUNCTION(BlueprintCallable, Category = "SplitScreen")
	void SetLocalSplitscreenEnabled(bool bEnable);

	/** Adds a local co-op player up to MaxSplitscreenPlayers (4). */
	UFUNCTION(BlueprintCallable, Category = "SplitScreen")
	bool TryJoinLocalCoopPlayer(int32 ControllerId);

	static constexpr int32 MaxLocalSplitscreenPlayers = 4;
};
