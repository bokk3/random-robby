#include "Dungeon/DungeonEncounterActors.h"
#include "Dungeon/DungeonGeneratorSubsystem.h"
#include "Core/DungeoneerCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"

ARestingGroundActor::ARestingGroundActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SanctuaryBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("SanctuaryBounds"));
	SetRootComponent(SanctuaryBounds);
	SanctuaryBounds->SetBoxExtent(FVector(750.0f, 750.0f, 300.0f));
	SanctuaryBounds->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	SanctuaryBounds->OnComponentBeginOverlap.AddDynamic(this, &ARestingGroundActor::HandleZoneOverlapBegin);
	SanctuaryBounds->OnComponentEndOverlap.AddDynamic(this, &ARestingGroundActor::HandleZoneOverlapEnd);

	CampfireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("CampfireLight"));
	CampfireLight->SetupAttachment(RootComponent);
	CampfireLight->SetLightColor(FLinearColor(1.0f, 0.58f, 0.24f));
	CampfireLight->SetIntensity(9000.0f);
	CampfireLight->SetAttenuationRadius(1100.0f);
	CampfireLight->VolumetricScatteringIntensity = 1.6f;
}

void ARestingGroundActor::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (UDungeonGeneratorSubsystem* GenSys = World->GetSubsystem<UDungeonGeneratorSubsystem>())
		{
			GenSys->RegisterRestingGroundBounds(SanctuaryBounds->Bounds.GetBox());
		}
	}
}

void ARestingGroundActor::HandleZoneOverlapBegin(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	if (ADungeoneerCharacter* Player = Cast<ADungeoneerCharacter>(OtherActor))
	{
		if (RestingPlayers.Num() < 8)
		{
			RestingPlayers.AddUnique(Player);
		}
	}
}

void ARestingGroundActor::HandleZoneOverlapEnd(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	if (ADungeoneerCharacter* Player = Cast<ADungeoneerCharacter>(OtherActor))
	{
		RestingPlayers.RemoveAll([Player](const TWeakObjectPtr<ADungeoneerCharacter>& Ptr)
		{
			return !Ptr.IsValid() || Ptr.Get() == Player;
		});
	}
}

void ARestingGroundActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (CampfireLight)
	{
		CampfirePhase += DeltaSeconds * 7.5f;
		CampfireLight->SetIntensity(8600.0f + 1100.0f * FMath::Sin(CampfirePhase));
	}

	if (!HasAuthority())
	{
		return;
	}

	for (const TWeakObjectPtr<ADungeoneerCharacter>& WeakPlayer : RestingPlayers)
	{
		if (ADungeoneerCharacter* Player = WeakPlayer.Get())
		{
			Player->RestoreAtRestingGround(HealthRestorePerSecond * DeltaSeconds, BatteryRestorePerSecond * DeltaSeconds);
		}
	}
}

AVendorActor::AVendorActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void AVendorActor::InitializeSeededCatalog(int32 RoomSeed)
{
	VendorCatalog.Reset();
	FRandomStream Stream(RoomSeed);

	static constexpr const TCHAR* PoolIds[5] =
	{
		TEXT("Item_BatteryCell_HighCap"),
		TEXT("Item_Lens_DarkVision"),
		TEXT("Item_Resonator_AcuteHearing"),
		TEXT("Item_Vial_ScentTracer"),
		TEXT("Item_MedKit_Sanctuary")
	};

	for (int32 i = 0; i < 5; ++i)
	{
		FVendorCatalogItem Item;
		Item.ItemGuid = FGuid(Stream.GetUnsignedInt(), Stream.GetUnsignedInt(), Stream.GetUnsignedInt(), Stream.GetUnsignedInt());
		Item.ItemId = FName(PoolIds[i]);
		Item.GoldPrice = 40 + Stream.RandRange(0, 6) * 15;
		Item.StockCount = Stream.RandRange(1, 3);
		VendorCatalog.Add(Item);
	}
}

bool AVendorActor::FindCatalogItem(const FGuid& ItemGuid, FVendorCatalogItem& OutItem) const noexcept
{
	for (const FVendorCatalogItem& Entry : VendorCatalog)
	{
		if (Entry.ItemGuid == ItemGuid && Entry.StockCount > 0)
		{
			OutItem = Entry;
			return true;
		}
	}
	return false;
}

bool AVendorActor::ConsumeCatalogStock(const FGuid& ItemGuid) noexcept
{
	for (FVendorCatalogItem& Entry : VendorCatalog)
	{
		if (Entry.ItemGuid == ItemGuid && Entry.StockCount > 0)
		{
			--Entry.StockCount;
			return true;
		}
	}
	return false;
}

AMajorLootVaultActor::AMajorLootVaultActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
}

void AMajorLootVaultActor::UnlockVaultWithSeed(int32 RoomSeed)
{
	if (!HasAuthority() || bVaultUnlocked)
	{
		return;
	}

	bVaultUnlocked = true;
	VaultRewards.Reset();
	FRandomStream Stream(RoomSeed);

	static constexpr const TCHAR* HighTierIds[3] =
	{
		TEXT("Relic_SolarFlashlightCore"),
		TEXT("Relic_EchoCrown_Tier3"),
		TEXT("Relic_ChronoAegis_Immunity")
	};

	for (int32 i = 0; i < 3; ++i)
	{
		FVendorCatalogItem Relic;
		Relic.ItemGuid = FGuid(Stream.GetUnsignedInt(), Stream.GetUnsignedInt(), Stream.GetUnsignedInt(), Stream.GetUnsignedInt());
		Relic.ItemId = FName(HighTierIds[i]);
		Relic.GoldPrice = 350 + Stream.RandRange(0, 150);
		Relic.StockCount = 1;
		VaultRewards.Add(Relic);
	}
}
