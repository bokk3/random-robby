#include "UI/DungeoneerCoopLobbyWidget.h"
#include "Core/DungeoneerGameViewportClient.h"
#include "Core/DungeoneerLocalPlayerSubsystem.h"
#include "Core/DungeoneerCharacter.h"
#include "Rendering/SensoryPerkComponent.h"
#include "Social/DungeoneerSocialSubsystem.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

UDungeoneerCoopLobbyWidget::UDungeoneerCoopLobbyWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	EnsureFixedSquadSlots();
}

void UDungeoneerCoopLobbyWidget::EnsureFixedSquadSlots()
{
	if (SquadSlots.Num() != MaxSquadSlots)
	{
		SquadSlots.SetNum(MaxSquadSlots);
	}

	for (int32 i = 0; i < MaxSquadSlots; ++i)
	{
		SquadSlots[i].SlotIndex = i;
	}
}

void UDungeoneerCoopLobbyWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureFixedSquadSlots();
	RefreshSquadSlotsFromSubsystems();
}

FString UDungeoneerCoopLobbyWidget::GetSplitscreenBadgeText(EDungeoneerSplitscreenMode Mode)
{
	switch (Mode)
	{
	case EDungeoneerSplitscreenMode::Solo1P:       return TEXT("1P SOLO");
	case EDungeoneerSplitscreenMode::SplitHoriz2P: return TEXT("2P HORIZ");
	case EDungeoneerSplitscreenMode::SplitTrio3P:  return TEXT("3P TRIO");
	case EDungeoneerSplitscreenMode::SplitQuad4P:  return TEXT("4P QUAD");
	default:                                       return TEXT("1P SOLO");
	}
}

void UDungeoneerCoopLobbyWidget::SetSplitscreenMode(EDungeoneerSplitscreenMode NewMode)
{
	ActiveSplitscreenMode = NewMode;

	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UDungeoneerGameViewportClient* ViewportClient = Cast<UDungeoneerGameViewportClient>(GI->GetGameViewportClient()))
		{
			ViewportClient->SetLocalSplitscreenEnabled(NewMode != EDungeoneerSplitscreenMode::Solo1P);
			ViewportClient->UpdateActiveSplitscreenType();
		}
	}

	if (Txt_SplitscreenBadge)
	{
		Txt_SplitscreenBadge->SetText(FText::FromString(GetSplitscreenBadgeText(ActiveSplitscreenMode)));
	}
}

void UDungeoneerCoopLobbyWidget::RefreshSquadSlotsFromSubsystems()
{
	EnsureFixedSquadSlots();

	UGameInstance* GI = GetGameInstance();
	UWorld* World = GetWorld();

	int32 FilledSlots = 0;
	if (GI)
	{
		const TArray<ULocalPlayer*>& LocalPlayers = GI->GetLocalPlayers();
		for (int32 i = 0; i < LocalPlayers.Num() && FilledSlots < MaxSquadSlots; ++i)
		{
			ULocalPlayer* LP = LocalPlayers[i];
			if (!LP)
			{
				continue;
			}

			FCoopSquadSlotViewModel& SlotVM = SquadSlots[FilledSlots];
			SlotVM.SlotIndex = FilledSlots;
			SlotVM.bOccupied = true;
			SlotVM.bIsLocalSlot = true;
			SlotVM.LocalControllerId = LP->GetControllerId();
			SlotVM.DisplayLabel = FString::Printf(TEXT("P%d LOCAL #%d"), FilledSlots + 1, LP->GetControllerId());

			int32 DarkTier = 0;
			int32 HearTier = 0;
			int32 ScentTier = 0;

			if (World)
			{
				if (APlayerController* OwningPC = LP->GetPlayerController(World))
				{
					if (const ADungeoneerCharacter* Char = Cast<ADungeoneerCharacter>(OwningPC->GetPawn()))
					{
						if (const USensoryPerkComponent* Perks = Char->GetSensoryPerkComponent())
						{
							DarkTier = Perks->GetPerkLevel(EDungeoneerSensoryPerk::DarkVision);
							HearTier = Perks->GetPerkLevel(EDungeoneerSensoryPerk::AcuteHearing);
							ScentTier = Perks->GetPerkLevel(EDungeoneerSensoryPerk::ScentThermal);
						}
					}
				}
			}

			if (DarkTier == 0 && HearTier == 0 && ScentTier == 0)
			{
				if (const UDungeoneerLocalPlayerSubsystem* LPSub = LP->GetSubsystem<UDungeoneerLocalPlayerSubsystem>())
				{
					const FDungeoneerSensoryViewState& State = LPSub->GetViewportSensoryState();
					DarkTier = (State.DarkVisionRadius > 750.0f) ? 3 : (State.DarkVisionRadius > 450.0f) ? 2 : (State.DarkVisionRadius > 0.0f) ? 1 : 0;
					HearTier = (State.AcuteHearingIntensity > 0.7f) ? 3 : (State.AcuteHearingIntensity > 0.35f) ? 2 : (State.AcuteHearingIntensity > 0.0f) ? 1 : 0;
					ScentTier = (State.ScentThermalIntensity > 0.7f) ? 3 : (State.ScentThermalIntensity > 0.35f) ? 2 : (State.ScentThermalIntensity > 0.0f) ? 1 : 0;
				}
			}

			SlotVM.DarkVisionTier = DarkTier;
			SlotVM.AcuteHearingTier = HearTier;
			SlotVM.ScentThermalTier = ScentTier;
			++FilledSlots;
		}

		if (const UDungeoneerSocialSubsystem* SocialSys = GI->GetSubsystem<UDungeoneerSocialSubsystem>())
		{
			const TArray<FDungeoneerFriendEntry>& Friends = SocialSys->GetCachedFriends();
			int32 FriendIdx = 0;
			while (FilledSlots < MaxSquadSlots)
			{
				FCoopSquadSlotViewModel& SlotVM = SquadSlots[FilledSlots];
				SlotVM.SlotIndex = FilledSlots;
				SlotVM.bIsLocalSlot = false;
				SlotVM.LocalControllerId = -1;
				SlotVM.DarkVisionTier = 0;
				SlotVM.AcuteHearingTier = 0;
				SlotVM.ScentThermalTier = 0;

				if (FriendIdx < Friends.Num() && Friends[FriendIdx].bIsInJoinableDungeoneerSession)
				{
					SlotVM.bOccupied = true;
					SlotVM.DisplayLabel = FString::Printf(TEXT("ONLINE: %s"), *Friends[FriendIdx].DisplayName);
					++FriendIdx;
				}
				else if (SocialSys->IsCoopSessionActive() && FilledSlots == LocalPlayers.Num())
				{
					SlotVM.bOccupied = false;
					SlotVM.DisplayLabel = TEXT("ONLINE PARTY");
				}
				else
				{
					SlotVM.bOccupied = false;
					SlotVM.DisplayLabel = TEXT("OPEN SLOT");
				}

				++FilledSlots;
			}
		}
	}

	for (int32 i = FilledSlots; i < MaxSquadSlots; ++i)
	{
		FCoopSquadSlotViewModel& SlotVM = SquadSlots[i];
		SlotVM.SlotIndex = i;
		SlotVM.bOccupied = false;
		SlotVM.bIsLocalSlot = false;
		SlotVM.LocalControllerId = -1;
		SlotVM.DisplayLabel = TEXT("OPEN SLOT");
		SlotVM.DarkVisionTier = 0;
		SlotVM.AcuteHearingTier = 0;
		SlotVM.ScentThermalTier = 0;
	}

	if (Txt_SplitscreenBadge)
	{
		Txt_SplitscreenBadge->SetText(FText::FromString(GetSplitscreenBadgeText(ActiveSplitscreenMode)));
	}
}

bool UDungeoneerCoopLobbyWidget::TryJoinLocalPlayerSlot(int32 ControllerId)
{
	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return false;
	}

	if (UDungeoneerGameViewportClient* ViewportClient = Cast<UDungeoneerGameViewportClient>(GI->GetGameViewportClient()))
	{
		if (ViewportClient->TryJoinLocalCoopPlayer(ControllerId))
		{
			const int32 Count = GI->GetNumLocalPlayers();
			ActiveSplitscreenMode = (Count >= 4) ? EDungeoneerSplitscreenMode::SplitQuad4P
				: (Count == 3) ? EDungeoneerSplitscreenMode::SplitTrio3P
				: (Count == 2) ? EDungeoneerSplitscreenMode::SplitHoriz2P
				: EDungeoneerSplitscreenMode::Solo1P;
			RefreshSquadSlotsFromSubsystems();
			return true;
		}
	}

	return false;
}

bool UDungeoneerCoopLobbyWidget::InviteFriendToCoopSlot(const FString& FriendNetId)
{
	UGameInstance* GI = GetGameInstance();
	ULocalPlayer* OwningLP = GetOwningLocalPlayer();
	if (!GI || !OwningLP || FriendNetId.IsEmpty())
	{
		return false;
	}

	if (UDungeoneerSocialSubsystem* SocialSys = GI->GetSubsystem<UDungeoneerSocialSubsystem>())
	{
		if (!SocialSys->IsCoopSessionActive())
		{
			SocialSys->HostCoopPartySession(OwningLP, MaxSquadSlots, true);
		}
		return SocialSys->SendPartyInvite(OwningLP, FriendNetId);
	}

	return false;
}
