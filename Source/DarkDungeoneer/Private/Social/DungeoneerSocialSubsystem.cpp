#include "Social/DungeoneerSocialSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "OnlineSubsystemUtils.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/VoiceInterface.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "GameFramework/PlayerState.h"

void UDungeoneerSocialSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	bSessionCreated = false;
	CachedFriends.Reset();
	RecentChatHistory.Reset();
}

void UDungeoneerSocialSubsystem::Deinitialize()
{
	if (IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld()))
	{
		if (IOnlineSessionPtr SessionInt = OSS->GetSessionInterface())
		{
			SessionInt->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
		}
	}
	Super::Deinitialize();
}

bool UDungeoneerSocialSubsystem::HostCoopPartySession(ULocalPlayer* HostLocalPlayer, int32 MaxTotalSlots, bool bAllowCrossplay)
{
	if (!HostLocalPlayer)
	{
		return false;
	}

	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	if (!OSS)
	{
		return false;
	}

	IOnlineSessionPtr SessionInt = OSS->GetSessionInterface();
	if (!SessionInt.IsValid())
	{
		return false;
	}

	const int32 LocalSplitscreenCount = GetGameInstance() ? GetGameInstance()->GetNumLocalPlayers() : 1;
	const int32 ClampedSlots = FMath::Clamp(MaxTotalSlots, LocalSplitscreenCount, 8);

	FOnlineSessionSettings Settings;
	Settings.NumPublicConnections = ClampedSlots;
	Settings.bShouldAdvertise = true;
	Settings.bAllowJoinInProgress = true;
	Settings.bAllowInvites = true;
	Settings.bAllowJoinViaPresence = true;
	Settings.bIsLANMatch = (OSS->GetSubsystemName() == NULL_SUBSYSTEM);
	Settings.bUsesPresence = true;
	Settings.Set(FName(TEXT("COOP_MODE")), FString(TEXT("25D_DARK_DUNGEONEER")), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	Settings.Set(FName(TEXT("LOCAL_SPLITSCREEN_PLAYERS")), LocalSplitscreenCount, EOnlineDataAdvertisementType::ViaOnlineService);
	Settings.Set(FName(TEXT("ALLOW_CROSSPLAY")), bAllowCrossplay, EOnlineDataAdvertisementType::ViaOnlineService);

	SessionInt->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteHandle);
	CreateSessionCompleteHandle = SessionInt->AddOnCreateSessionCompleteDelegate_Handle(
		FOnCreateSessionCompleteDelegate::CreateUObject(this, &UDungeoneerSocialSubsystem::HandleCreateSessionComplete));

	const FUniqueNetIdRepl NetId = HostLocalPlayer->GetPreferredUniqueNetId();
	if (NetId.IsValid())
	{
		return SessionInt->CreateSession(*NetId, NAME_GameSession, Settings);
	}

	return SessionInt->CreateSession(HostLocalPlayer->GetControllerId(), NAME_GameSession, Settings);
}

void UDungeoneerSocialSubsystem::HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	bSessionCreated = bWasSuccessful;
}

bool UDungeoneerSocialSubsystem::SendPartyInvite(ULocalPlayer* SenderLocalPlayer, const FString& TargetUserId)
{
	if (!SenderLocalPlayer || TargetUserId.IsEmpty())
	{
		return false;
	}

	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	if (!OSS)
	{
		return false;
	}

	IOnlineSessionPtr SessionInt = OSS->GetSessionInterface();
	IOnlineIdentityPtr IdentityInt = OSS->GetIdentityInterface();
	if (!SessionInt.IsValid() || !IdentityInt.IsValid())
	{
		return false;
	}

	const FUniqueNetIdPtr TargetNetId = IdentityInt->CreateUniquePlayerId(TargetUserId);
	if (!TargetNetId.IsValid())
	{
		return false;
	}

	return SessionInt->SendSessionInviteToFriend(SenderLocalPlayer->GetControllerId(), NAME_GameSession, *TargetNetId);
}

void UDungeoneerSocialSubsystem::RequestFriendListRefresh(ULocalPlayer* RequestingLocalPlayer)
{
	if (!RequestingLocalPlayer)
	{
		return;
	}

	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	if (!OSS)
	{
		return;
	}

	IOnlineFriendsPtr FriendsInt = OSS->GetFriendsInterface();
	if (!FriendsInt.IsValid())
	{
		return;
	}

	FriendsInt->ReadFriendsList(
		RequestingLocalPlayer->GetControllerId(),
		EFriendsLists::ToString(EFriendsLists::Default),
		FOnReadFriendsListComplete::CreateUObject(this, &UDungeoneerSocialSubsystem::HandleReadFriendsListComplete));
}

void UDungeoneerSocialSubsystem::HandleReadFriendsListComplete(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr)
{
	if (!bWasSuccessful)
	{
		return;
	}

	IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld());
	IOnlineFriendsPtr FriendsInt = OSS ? OSS->GetFriendsInterface() : nullptr;
	if (!FriendsInt.IsValid())
	{
		return;
	}

	TArray<TSharedRef<FOnlineFriend>> RawFriends;
	if (!FriendsInt->GetFriendsList(LocalUserNum, ListName, RawFriends))
	{
		return;
	}

	CachedFriends.Reset(RawFriends.Num());
	for (const TSharedRef<FOnlineFriend>& Friend : RawFriends)
	{
		FDungeoneerFriendEntry Entry;
		Entry.UserId = Friend->GetUserId()->ToString();
		Entry.DisplayName = Friend->GetDisplayName();
		const FOnlineUserPresence& Presence = Friend->GetPresence();
		Entry.bIsOnline = Presence.bIsOnline;
		Entry.bIsInJoinableDungeoneerSession = Presence.bIsJoinable;
		CachedFriends.Add(MoveTemp(Entry));
	}

	if (OnFriendListUpdated.IsBound())
	{
		OnFriendListUpdated.Broadcast(CachedFriends);
	}
}

void UDungeoneerSocialSubsystem::BroadcastChatMessage(ULocalPlayer* SenderLocalPlayer, EDungeoneerChatChannel Channel, const FString& Body)
{
	if (!SenderLocalPlayer || Body.IsEmpty())
	{
		return;
	}

	FDungeoneerChatMessage Msg;
	Msg.SenderLocalControllerId = SenderLocalPlayer->GetControllerId();
	Msg.Channel = Channel;
	Msg.MessageBody = Body.Left(256);

	if (APlayerController* PC = SenderLocalPlayer->GetPlayerController(GetWorld()))
	{
		if (const APlayerState* PS = PC->GetPlayerState<APlayerState>())
		{
			Msg.SenderDisplayName = PS->GetPlayerName();
		}
	}
	if (Msg.SenderDisplayName.IsEmpty())
	{
		Msg.SenderDisplayName = FString::Printf(TEXT("Dungeoneer_%d"), Msg.SenderLocalControllerId + 1);
	}

	if (RecentChatHistory.Num() >= MaxChatHistoryEntries)
	{
		RecentChatHistory.RemoveAt(0);
	}
	RecentChatHistory.Add(Msg);

	if (OnChatMessageReceived.IsBound())
	{
		OnChatMessageReceived.Broadcast(Msg);
	}
}

void UDungeoneerSocialSubsystem::SetVoiceMutedForLocalPlayer(ULocalPlayer* TargetLocalPlayer, bool bMuted)
{
	if (!TargetLocalPlayer)
	{
		return;
	}

	if (IOnlineSubsystem* OSS = Online::GetSubsystem(GetWorld()))
	{
		if (IOnlineVoicePtr VoiceInt = OSS->GetVoiceInterface())
		{
			const uint8 LocalUserNum = static_cast<uint8>(FMath::Clamp(TargetLocalPlayer->GetControllerId(), 0, 255));
			if (bMuted)
			{
				VoiceInt->StopNetworkedVoice(LocalUserNum);
			}
			else
			{
				VoiceInt->StartNetworkedVoice(LocalUserNum);
			}
		}
	}
}
