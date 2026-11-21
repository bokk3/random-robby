#include "Social/DungeoneerInventoryComponent.h"
#include "Net/UnrealNetwork.h"

UDungeoneerInventoryComponent::UDungeoneerInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UDungeoneerInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME_CONDITION(UDungeoneerInventoryComponent, Items, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDungeoneerInventoryComponent, GoldBalance, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDungeoneerInventoryComponent, ReplicatedRelicMask, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDungeoneerInventoryComponent, OilPhialCount, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UDungeoneerInventoryComponent, AntidoteCount, COND_OwnerOnly);
}

void UDungeoneerInventoryComponent::AuthorityAddItem(const FGuid& ItemGuid, FName ItemId, int32 Quantity)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !ItemGuid.IsValid() || Quantity <= 0)
	{
		return;
	}

	const EDungeoneerRelicId MappedRelic = MapItemIdToRelic(ItemId);
	if (MappedRelic != EDungeoneerRelicId::None)
	{
		AuthorityGrantRelic(MappedRelic);
	}
	else if (ItemId == FName(TEXT("Item_BatteryCell_HighCap")) || ItemId == FName(TEXT("Consumable_OilPhial")))
	{
		AuthorityAddConsumable(EDungeoneerConsumableId::LanternOilPhial, Quantity);
	}
	else if (ItemId == FName(TEXT("Item_MedKit_Sanctuary")) || ItemId == FName(TEXT("Consumable_Antidote")))
	{
		AuthorityAddConsumable(EDungeoneerConsumableId::AntidoteDraught, Quantity);
	}

	FDungeoneerInventoryItem NewItem;
	NewItem.ItemGuid = ItemGuid;
	NewItem.ItemId = ItemId;
	NewItem.Quantity = Quantity;
	NewItem.bLockedForTrade = false;
	Items.Add(NewItem);
}

void UDungeoneerInventoryComponent::AuthorityResetForNewRun() noexcept
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}
	Items.Reset();
	GoldBalance = StartingGold;
	ReplicatedRelicMask = 0;
	OilPhialCount = StartingOilPhials;
	AntidoteCount = StartingAntidotes;
}

bool UDungeoneerInventoryComponent::AuthorityAdjustGold(int32 DeltaGold) noexcept
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	if (GoldBalance + DeltaGold < 0)
	{
		return false;
	}

	GoldBalance += DeltaGold;
	return true;
}

bool UDungeoneerInventoryComponent::AuthorityHasUnlockedItem(const FGuid& ItemGuid) const noexcept
{
	for (const FDungeoneerInventoryItem& Item : Items)
	{
		if (Item.ItemGuid == ItemGuid && !Item.bLockedForTrade && Item.Quantity > 0)
		{
			return true;
		}
	}
	return false;
}

bool UDungeoneerInventoryComponent::AuthorityHasLockedItem(const FGuid& ItemGuid) const noexcept
{
	for (const FDungeoneerInventoryItem& Item : Items)
	{
		if (Item.ItemGuid == ItemGuid && Item.bLockedForTrade && Item.Quantity > 0)
		{
			return true;
		}
	}
	return false;
}

bool UDungeoneerInventoryComponent::AuthoritySetItemTradeLock(const FGuid& ItemGuid, bool bLocked) noexcept
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	for (FDungeoneerInventoryItem& Item : Items)
	{
		if (Item.ItemGuid == ItemGuid)
		{
			if (Item.bLockedForTrade == bLocked)
			{
				return false;
			}
			Item.bLockedForTrade = bLocked;
			return true;
		}
	}
	return false;
}

bool UDungeoneerInventoryComponent::AuthorityExtractLockedItem(const FGuid& ItemGuid, FDungeoneerInventoryItem& OutExtracted)
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return false;
	}

	for (int32 i = 0; i < Items.Num(); ++i)
	{
		if (Items[i].ItemGuid == ItemGuid && Items[i].bLockedForTrade)
		{
			OutExtracted = Items[i];
			OutExtracted.bLockedForTrade = false;
			Items.RemoveAtSwap(i);
			return true;
		}
	}
	return false;
}

bool UDungeoneerInventoryComponent::AuthorityGrantRelic(EDungeoneerRelicId RelicId) noexcept
{
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		return false;
	}
	const uint8 Index = static_cast<uint8>(RelicId);
	if (Index == 0 || Index >= 8)
	{
		return false;
	}
	const uint8 Bit = static_cast<uint8>(1u << (Index - 1u));
	const bool bNewlyAdded = (ReplicatedRelicMask & Bit) == 0;
	ReplicatedRelicMask |= Bit;
	return bNewlyAdded;
}

bool UDungeoneerInventoryComponent::HasRelic(EDungeoneerRelicId RelicId) const noexcept
{
	const uint8 Index = static_cast<uint8>(RelicId);
	if (Index == 0 || Index >= 8)
	{
		return false;
	}
	const uint8 Bit = static_cast<uint8>(1u << (Index - 1u));
	return (ReplicatedRelicMask & Bit) != 0;
}

int32 UDungeoneerInventoryComponent::GetActiveRelicCount() const noexcept
{
	int32 Count = 0;
	for (uint8 i = 1; i <= 5; ++i)
	{
		if (HasRelic(static_cast<EDungeoneerRelicId>(i)))
		{
			++Count;
		}
	}
	return Count;
}

void UDungeoneerInventoryComponent::AuthorityAddConsumable(EDungeoneerConsumableId ConsumableId, int32 DeltaCount) noexcept
{
	if (DeltaCount <= 0 || (GetOwner() && !GetOwner()->HasAuthority()))
	{
		return;
	}
	if (ConsumableId == EDungeoneerConsumableId::LanternOilPhial)
	{
		OilPhialCount = FMath::Min(9, OilPhialCount + DeltaCount);
	}
	else if (ConsumableId == EDungeoneerConsumableId::AntidoteDraught)
	{
		AntidoteCount = FMath::Min(9, AntidoteCount + DeltaCount);
	}
}

bool UDungeoneerInventoryComponent::AuthorityConsumeQuickItem(EDungeoneerConsumableId ConsumableId) noexcept
{
	if (GetOwner() && !GetOwner()->HasAuthority())
	{
		return false;
	}
	if (ConsumableId == EDungeoneerConsumableId::LanternOilPhial && OilPhialCount > 0)
	{
		--OilPhialCount;
		return true;
	}
	if (ConsumableId == EDungeoneerConsumableId::AntidoteDraught && AntidoteCount > 0)
	{
		--AntidoteCount;
		return true;
	}
	return false;
}

int32 UDungeoneerInventoryComponent::GetConsumableCount(EDungeoneerConsumableId ConsumableId) const noexcept
{
	if (ConsumableId == EDungeoneerConsumableId::LanternOilPhial)
	{
		return OilPhialCount;
	}
	if (ConsumableId == EDungeoneerConsumableId::AntidoteDraught)
	{
		return AntidoteCount;
	}
	return 0;
}

const TCHAR* UDungeoneerInventoryComponent::GetRelicDisplayName(EDungeoneerRelicId RelicId) noexcept
{
	switch (RelicId)
	{
	case EDungeoneerRelicId::SunGlassLens:        return TEXT("SHAFT-GLASS LENS");
	case EDungeoneerRelicId::ChronoWardBoots:     return TEXT("RUNNER'S SOLES");
	case EDungeoneerRelicId::EchoTunedBlade:      return TEXT("CHIMING PANE");
	case EDungeoneerRelicId::SolarFlashlightCore: return TEXT("LAST SUNSET");
	case EDungeoneerRelicId::VampireWick:         return TEXT("DRINKING WICK");
	default:                                      return TEXT("NONE");
	}
}

EDungeoneerRelicId UDungeoneerInventoryComponent::MapItemIdToRelic(FName ItemId) noexcept
{
	if (ItemId == FName(TEXT("Relic_SunGlassLens")) || ItemId == FName(TEXT("Item_Lens_DarkVision")))
	{
		return EDungeoneerRelicId::SunGlassLens;
	}
	if (ItemId == FName(TEXT("Relic_ChronoWardBoots")) || ItemId == FName(TEXT("Relic_ChronoAegis_Immunity")))
	{
		return EDungeoneerRelicId::ChronoWardBoots;
	}
	if (ItemId == FName(TEXT("Relic_EchoTunedBlade")) || ItemId == FName(TEXT("Relic_EchoCrown_Tier3")) || ItemId == FName(TEXT("Item_Resonator_AcuteHearing")))
	{
		return EDungeoneerRelicId::EchoTunedBlade;
	}
	if (ItemId == FName(TEXT("Relic_SolarFlashlightCore")))
	{
		return EDungeoneerRelicId::SolarFlashlightCore;
	}
	if (ItemId == FName(TEXT("Relic_VampireWick")) || ItemId == FName(TEXT("Item_Vial_ScentTracer")))
	{
		return EDungeoneerRelicId::VampireWick;
	}
	return EDungeoneerRelicId::None;
}
