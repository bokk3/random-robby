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
}

void UDungeoneerInventoryComponent::AuthorityAddItem(const FGuid& ItemGuid, FName ItemId, int32 Quantity)
{
	if (!GetOwner() || !GetOwner()->HasAuthority() || !ItemGuid.IsValid() || Quantity <= 0)
	{
		return;
	}

	FDungeoneerInventoryItem NewItem;
	NewItem.ItemGuid = ItemGuid;
	NewItem.ItemId = ItemId;
	NewItem.Quantity = Quantity;
	NewItem.bLockedForTrade = false;
	Items.Add(NewItem);
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
