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
	SetForceDisableSplitscreen(false);
}

void UDungeoneerGameViewportClient::UpdateActiveSplitscreenType()
{
	const int32 NumLocalPlayers = GameInstance ? GameInstance->GetNumLocalPlayers() : 1;
	if (NumLocalPlayers <= 1)
	{
		ActiveSplitscreenType = ESplitScreenType::None;
	}
	else if (NumLocalPlayers == 2)
	{
		ActiveSplitscreenType = ESplitScreenType::TwoPlayer_Vertical;
	}
	else if (NumLocalPlayers == 3)
	{
		ActiveSplitscreenType = ESplitScreenType::ThreePlayer_FavorTop;
	}
	else
	{
		ActiveSplitscreenType = ESplitScreenType::FourPlayer_Grid;
	}
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
