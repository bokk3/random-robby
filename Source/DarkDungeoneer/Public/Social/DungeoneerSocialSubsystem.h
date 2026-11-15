#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "OnlineSubsystem.h"
#include "DungeoneerSocialSubsystem.generated.h"

class ULocalPlayer;

UENUM(BlueprintType)
enum class EDungeoneerChatChannel : uint8
{
	Party          = 0,
	RestingGround  = 1,
	SystemAlert    = 2
};

USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API FDungeoneerFriendEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Social")
	FString UserId;

	UPROPERTY(BlueprintReadOnly, Category = "Social")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Social")
	bool bIsOnline = false;

	UPROPERTY(BlueprintReadOnly, Category = "Social")
	bool bIsInJoinableDungeoneerSession = false;
};

USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API FDungeoneerChatMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Social|Chat")
	int32 SenderLocalControllerId = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, Category = "Social|Chat")
	FString SenderDisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "Social|Chat")
	FString MessageBody;

	UPROPERTY(BlueprintReadOnly, Category = "Social|Chat")
	EDungeoneerChatChannel Channel = EDungeoneerChatChannel::Party;
};

/**
 * Online Social & Combined Local Split-Screen + Online Co-Op Subsystem (`UDungeoneerSocialSubsystem`).
 * - OSSv2 / CommonSession-compatible OnlineSubsystem wrapper for Party creation, invites,
 *   event-driven Friend List presence caching, and per-ULocalPlayer text/voice Chat channels.
 * - Never assumes player index 0; every operation takes explicit ULocalPlayer*.
 */
UCLASS()
class DARKDUNGEONEER_API UDungeoneerSocialSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Creates a co-op session reserving slots for local split-screen players + remote peers. */
	UFUNCTION(BlueprintCallable, Category = "Social|Party")
	bool HostCoopPartySession(ULocalPlayer* HostLocalPlayer, int32 MaxTotalSlots, bool bAllowCrossplay = true);

	/** Sends a party invite to a friend's UniqueNetId string from a specific ULocalPlayer. */
	UFUNCTION(BlueprintCallable, Category = "Social|Party")
	bool SendPartyInvite(ULocalPlayer* SenderLocalPlayer, const FString& TargetUserId);

	/** Queries the OSS friend list asynchronously (event-driven; zero Tick polling). */
	UFUNCTION(BlueprintCallable, Category = "Social|Friends")
	void RequestFriendListRefresh(ULocalPlayer* RequestingLocalPlayer);

	/** Sends a chat message tagged to a specific ULocalPlayer and channel. */
	UFUNCTION(BlueprintCallable, Category = "Social|Chat")
	void BroadcastChatMessage(ULocalPlayer* SenderLocalPlayer, EDungeoneerChatChannel Channel, const FString& Body);

	/** Toggles OSS Voice Chat push-to-talk / mute per ULocalPlayer. */
	UFUNCTION(BlueprintCallable, Category = "Social|Voice")
	void SetVoiceMutedForLocalPlayer(ULocalPlayer* TargetLocalPlayer, bool bMuted);

	UFUNCTION(BlueprintPure, Category = "Social|Friends")
	const TArray<FDungeoneerFriendEntry>& GetCachedFriends() const noexcept { return CachedFriends; }

	UFUNCTION(BlueprintPure, Category = "Social|Party")
	bool IsCoopSessionActive() const noexcept { return bSessionCreated; }

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnFriendListUpdated, const TArray<FDungeoneerFriendEntry>&);
	FOnFriendListUpdated OnFriendListUpdated;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnChatMessageReceived, const FDungeoneerChatMessage&);
	FOnChatMessageReceived OnChatMessageReceived;

private:
	void HandleCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void HandleReadFriendsListComplete(int32 LocalUserNum, bool bWasSuccessful, const FString& ListName, const FString& ErrorStr);

	FDelegateHandle CreateSessionCompleteHandle;
	TArray<FDungeoneerFriendEntry> CachedFriends;
	TArray<FDungeoneerChatMessage> RecentChatHistory;
	bool bSessionCreated = false;
	static constexpr int32 MaxChatHistoryEntries = 64;
};
