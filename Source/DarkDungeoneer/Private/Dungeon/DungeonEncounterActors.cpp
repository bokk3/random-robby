#include "Dungeon/DungeonEncounterActors.h"
#include "Dungeon/DungeonGeneratorSubsystem.h"
#include "Core/DungeoneerCharacter.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace EncounterVisualHelper
{
	static void ApplyEncounterPrimitiveVisual(
		AActor* OwnerActor,
		UStaticMeshComponent* MeshComp,
		const TCHAR* MeshAssetPath,
		const FVector& Scale3D,
		const FLinearColor& TintColor,
		float RoughnessVal)
	{
		if (!OwnerActor || !MeshComp)
		{
			return;
		}
		if (!MeshComp->GetStaticMesh())
		{
			if (UStaticMesh* MeshAsset = LoadObject<UStaticMesh>(nullptr, MeshAssetPath))
			{
				MeshComp->SetStaticMesh(MeshAsset);
			}
		}
		MeshComp->SetRelativeScale3D(Scale3D);
		if (!MeshComp->GetMaterial(0))
		{
			if (UMaterialInterface* BaseShapeMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
			{
				if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseShapeMat, OwnerActor))
				{
					MID->SetVectorParameterValue(FName(TEXT("Color")), TintColor);
					MID->SetScalarParameterValue(FName(TEXT("Roughness")), RoughnessVal);
					MeshComp->SetMaterial(0, MID);
				}
			}
		}
	}
}

ARestingGroundActor::ARestingGroundActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SanctuaryBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("SanctuaryBounds"));
	SetRootComponent(SanctuaryBounds);
	SanctuaryBounds->SetBoxExtent(FVector(750.0f, 750.0f, 300.0f));
	SanctuaryBounds->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	SanctuaryBounds->OnComponentBeginOverlap.AddDynamic(this, &ARestingGroundActor::HandleZoneOverlapBegin);
	SanctuaryBounds->OnComponentEndOverlap.AddDynamic(this, &ARestingGroundActor::HandleZoneOverlapEnd);

	BrazierMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BrazierMesh"));
	BrazierMesh->SetupAttachment(RootComponent);
	BrazierMesh->SetRelativeLocation(FVector(0.0, 0.0, 45.0));

	CampfireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("CampfireLight"));
	CampfireLight->SetupAttachment(RootComponent);
	CampfireLight->SetRelativeLocation(FVector(0.0, 0.0, 120.0));
	CampfireLight->SetLightColor(FLinearColor(1.0f, 0.58f, 0.24f));
	CampfireLight->SetIntensity(9000.0f);
	CampfireLight->SetAttenuationRadius(1100.0f);
	CampfireLight->VolumetricScatteringIntensity = 1.6f;
	CampfireLight->CastShadows = false;
}

void ARestingGroundActor::BeginPlay()
{
	Super::BeginPlay();

	EncounterVisualHelper::ApplyEncounterPrimitiveVisual(
		this,
		BrazierMesh.Get(),
		TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),
		FVector(1.15, 1.15, 0.90),
		FLinearColor(1.0f, 0.55f, 0.16f),
		0.35f);

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
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	VendorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VendorRoot"));
	SetRootComponent(VendorRoot);

	StallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StallMesh"));
	StallMesh->SetupAttachment(VendorRoot);
	StallMesh->SetRelativeLocation(FVector(0.0, 0.0, 70.0));

	MerchantLanternLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MerchantLanternLight"));
	MerchantLanternLight->SetupAttachment(VendorRoot);
	MerchantLanternLight->SetRelativeLocation(FVector(0.0, 0.0, 175.0));
	MerchantLanternLight->SetLightColor(FLinearColor(0.22f, 0.88f, 0.96f));
	MerchantLanternLight->SetIntensity(3600.0f);
	MerchantLanternLight->SetAttenuationRadius(850.0f);
	MerchantLanternLight->CastShadows = false;
}

void AVendorActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EncounterVisualHelper::ApplyEncounterPrimitiveVisual(
		this,
		StallMesh.Get(),
		TEXT("/Engine/BasicShapes/Cube.Cube"),
		FVector(1.80, 1.10, 1.40),
		FLinearColor(0.24f, 0.72f, 0.78f),
		0.42f);
}

void AVendorActor::BeginPlay()
{
	Super::BeginPlay();
	EncounterVisualHelper::ApplyEncounterPrimitiveVisual(
		this,
		StallMesh.Get(),
		TEXT("/Engine/BasicShapes/Cube.Cube"),
		FVector(1.80, 1.10, 1.40),
		FLinearColor(0.24f, 0.72f, 0.78f),
		0.42f);
}

void AVendorActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (MerchantLanternLight)
	{
		LanternSwayPhase += DeltaSeconds * 3.2f;
		MerchantLanternLight->SetRelativeLocation(FVector(FMath::Sin(LanternSwayPhase) * 14.0, FMath::Cos(LanternSwayPhase * 0.7f) * 8.0, 175.0));
		MerchantLanternLight->SetIntensity(3450.0f + 450.0f * FMath::Sin(LanternSwayPhase * 2.1f));
	}
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

	VaultRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VaultRoot"));
	SetRootComponent(VaultRoot);

	VaultChestMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VaultChestMesh"));
	VaultChestMesh->SetupAttachment(VaultRoot);
	VaultChestMesh->SetRelativeLocation(FVector(0.0, 0.0, 65.0));

	VaultBeaconLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("VaultBeaconLight"));
	VaultBeaconLight->SetupAttachment(VaultRoot);
	VaultBeaconLight->SetRelativeLocation(FVector(0.0, 0.0, 190.0));
	VaultBeaconLight->SetLightColor(FLinearColor(1.0f, 0.82f, 0.22f));
	VaultBeaconLight->SetIntensity(4800.0f);
	VaultBeaconLight->SetAttenuationRadius(1050.0f);
	VaultBeaconLight->CastShadows = false;
}

void AMajorLootVaultActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EncounterVisualHelper::ApplyEncounterPrimitiveVisual(
		this,
		VaultChestMesh.Get(),
		TEXT("/Engine/BasicShapes/Cube.Cube"),
		FVector(1.60, 1.20, 1.30),
		FLinearColor(0.95f, 0.78f, 0.18f),
		0.22f);
}

void AMajorLootVaultActor::BeginPlay()
{
	Super::BeginPlay();
	EncounterVisualHelper::ApplyEncounterPrimitiveVisual(
		this,
		VaultChestMesh.Get(),
		TEXT("/Engine/BasicShapes/Cube.Cube"),
		FVector(1.60, 1.20, 1.30),
		FLinearColor(0.95f, 0.78f, 0.18f),
		0.22f);
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
