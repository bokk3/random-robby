#include "Core/DungeoneerGameViewportClient.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

UDungeoneerGameViewportClient::UDungeoneerGameViewportClient()
{
	MaxSplitscreenPlayers = MaxLocalSplitscreenPlayers;
}

void UDungeoneerGameViewportClient::Init(FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice)
{
	Super::Init(WorldContext, OwningGameInstance, bCreateNewAudioDevice);
	MaxSplitscreenPlayers = MaxLocalSplitscreenPlayers;
	SetForceDisableSplitscreen(true);
}

void UDungeoneerGameViewportClient::UpdateActiveSplitscreenType()
{
	// Local co-op shares one full-screen view through the party camera. Extra local players keep their
	// controllers and pawns but get no view of their own (the engine gives them a zero-size area).
	ActiveSplitscreenType = ESplitScreenType::None;
}

void UDungeoneerGameViewportClient::SetLocalSplitscreenEnabled(bool bEnable)
{
	SetForceDisableSplitscreen(!bEnable);
	UpdateActiveSplitscreenType();
}

bool UDungeoneerGameViewportClient::TryJoinLocalCoopPlayer(int32 ControllerId)
{
	if (!GameInstance || GameInstance->GetNumLocalPlayers() >= MaxLocalSplitscreenPlayers)
	{
		return false;
	}

	FString OutError;
	ULocalPlayer* NewLocalPlayer = GameInstance->CreateLocalPlayer(ControllerId, OutError, true);
	if (NewLocalPlayer)
	{
		UpdateActiveSplitscreenType();
		return true;
	}

	return false;
}
