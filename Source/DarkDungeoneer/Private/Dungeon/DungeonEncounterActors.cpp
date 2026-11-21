#include "Dungeon/DungeonEncounterActors.h"
#include "Dungeon/DungeonGeneratorSubsystem.h"
#include "Core/DungeoneerAssetBootstrapSubsystem.h"
#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerGameState.h"
#include "Combat/DungeoneerSkillComponent.h"
#include "Rendering/SensoryPerkComponent.h"
#include "Social/DungeoneerInventoryComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace EncounterVisualHelper
{
	static void ApplyEncounter25DSpriteVisual(
		AActor* OwnerActor,
		UStaticMeshComponent* MeshComp,
		FName DiffuseKey,
		FName NormalKey,
		const FVector& PlaneScale3D,
		const FLinearColor& FallbackTint,
		float RoughnessVal)
	{
		if (!OwnerActor || !MeshComp)
		{
			return;
		}
		if (!MeshComp->GetStaticMesh())
		{
			if (UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
			{
				MeshComp->SetStaticMesh(PlaneMesh);
			}
		}
		MeshComp->SetRelativeScale3D(PlaneScale3D);
		MeshComp->SetCastShadow(false);

		if (const UWorld* World = OwnerActor->GetWorld())
		{
			if (const UGameInstance* GI = World->GetGameInstance())
			{
				if (UDungeoneerAssetBootstrapSubsystem* Bootstrap = GI->GetSubsystem<UDungeoneerAssetBootstrapSubsystem>())
				{
					MeshComp->SetRelativeRotation(Bootstrap->GetCameraFacingSpritePlaneRotation());
					if (UMaterialInstanceDynamic* MID = Bootstrap->CreateDungeonOrRigMaterial(
						OwnerActor, DiffuseKey, NormalKey, FallbackTint, RoughnessVal, false, true))
					{
						MeshComp->SetMaterial(0, MID);
						return;
					}
				}
			}
		}

		MeshComp->SetRelativeRotation(FRotator(48.0, 135.0, 0.0));
		if (!MeshComp->GetMaterial(0))
		{
			if (UMaterialInterface* BaseShapeMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial")))
			{
				if (UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(BaseShapeMat, OwnerActor))
				{
					MID->SetVectorParameterValue(FName(TEXT("Color")), FallbackTint);
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
	BrazierMesh->SetRelativeLocation(FVector(0.0, 0.0, 68.0));

	ShrineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShrineMesh"));
	ShrineMesh->SetupAttachment(RootComponent);
	ShrineMesh->SetRelativeLocation(FVector(-175.0, 145.0, 74.0));

	CampfireLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("CampfireLight"));
	CampfireLight->SetupAttachment(RootComponent);
	CampfireLight->SetRelativeLocation(FVector(0.0, 0.0, 110.0));
	CampfireLight->SetLightColor(FLinearColor(1.0f, 0.62f, 0.24f));
	CampfireLight->SetUseInverseSquaredFalloff(true);
	CampfireLight->SetIntensityUnits(ELightUnits::Lumens);
	CampfireLight->SetSourceRadius(32.0f);
	CampfireLight->SetSoftSourceRadius(48.0f);
	CampfireLight->SetIntensity(680.0f);
	CampfireLight->SetAttenuationRadius(880.0f);
	CampfireLight->VolumetricScatteringIntensity = 1.25f;
	CampfireLight->CastShadows = false;
}

void ARestingGroundActor::BeginPlay()
{
	Super::BeginPlay();

	EncounterVisualHelper::ApplyEncounter25DSpriteVisual(
		this,
		BrazierMesh.Get(),
		FName(TEXT("Sanctuary_Brazier_D")),
		FName(TEXT("Sanctuary_Brazier_N")),
		FVector(1.50, 1.50, 1.50),
		FLinearColor(1.0f, 0.58f, 0.18f),
		0.42f);

	EncounterVisualHelper::ApplyEncounter25DSpriteVisual(
		this,
		ShrineMesh.Get(),
		FName(TEXT("Sanctuary_Shrine_D")),
		FName(TEXT("Sanctuary_Shrine_N")),
		FVector(1.55, 1.55, 1.55),
		FLinearColor(0.24f, 0.88f, 0.98f),
		0.35f);

	if (UWorld* World = GetWorld())
	{
		if (UDungeonGeneratorSubsystem* GenSys = World->GetSubsystem<UDungeonGeneratorSubsystem>())
		{
			GenSys->RegisterRestingGroundBounds(SanctuaryBounds->Bounds.GetBox());
		}
	}
}

bool ARestingGroundActor::TriggerSensoryAltarUpgrade(ADungeoneerCharacter* Player, const int32 GoldCost)
{
	if (!HasAuthority() || !Player)
	{
		return false;
	}

	USensoryPerkComponent* Perks = Player->GetSensoryPerkComponent();
	UDungeoneerInventoryComponent* Inv = Player->GetInventoryComponent();
	if (!Perks || !Inv)
	{
		return false;
	}

	// Pick lowest-tier perk in DarkVision -> AcuteHearing -> ScentThermal order
	static constexpr EDungeoneerSensoryPerk PriorityOrder[3] =
	{
		EDungeoneerSensoryPerk::DarkVision,
		EDungeoneerSensoryPerk::AcuteHearing,
		EDungeoneerSensoryPerk::ScentThermal
	};

	EDungeoneerSensoryPerk BestPerk = EDungeoneerSensoryPerk::DarkVision;
	int32 LowestTier = 99;
	for (EDungeoneerSensoryPerk Candidate : PriorityOrder)
	{
		const int32 Tier = Perks->GetPerkLevel(Candidate);
		if (Tier < 3 && Tier < LowestTier)
		{
			LowestTier = Tier;
			BestPerk = Candidate;
		}
	}

	if (LowestTier >= 3)
	{
		return false;
	}

	if (GoldCost > 0 && !Inv->AuthorityAdjustGold(-GoldCost))
	{
		return false;
	}

	Perks->UpgradePerk(BestPerk);
	Player->PushFloatingCombatPopup(GetActorLocation() + FVector(0.0, 0.0, 120.0), TEXT("THE THIRD EYE OPENS A LITTLE WIDER"), EDungeoneerFloatingTextKind::Relic);
	return true;
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
		CampfireLight->SetIntensity(680.0f + 130.0f * FMath::Sin(CampfirePhase));
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
	StallMesh->SetRelativeLocation(FVector(0.0, 0.0, 74.0));

	MerchantLanternLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("MerchantLanternLight"));
	MerchantLanternLight->SetupAttachment(VendorRoot);
	MerchantLanternLight->SetRelativeLocation(FVector(0.0, 0.0, 165.0));
	MerchantLanternLight->SetLightColor(FLinearColor(0.22f, 0.88f, 0.96f));
	MerchantLanternLight->SetUseInverseSquaredFalloff(true);
	MerchantLanternLight->SetIntensityUnits(ELightUnits::Lumens);
	MerchantLanternLight->SetSourceRadius(24.0f);
	MerchantLanternLight->SetSoftSourceRadius(36.0f);
	MerchantLanternLight->SetIntensity(520.0f);
	MerchantLanternLight->SetAttenuationRadius(700.0f);
	MerchantLanternLight->CastShadows = false;
}

void AVendorActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EncounterVisualHelper::ApplyEncounter25DSpriteVisual(
		this,
		StallMesh.Get(),
		FName(TEXT("Sanctuary_Vendor_D")),
		FName(TEXT("Sanctuary_Vendor_N")),
		FVector(1.75, 1.75, 1.75),
		FLinearColor(0.24f, 0.72f, 0.78f),
		0.45f);
}

void AVendorActor::BeginPlay()
{
	Super::BeginPlay();
	EncounterVisualHelper::ApplyEncounter25DSpriteVisual(
		this,
		StallMesh.Get(),
		FName(TEXT("Sanctuary_Vendor_D")),
		FName(TEXT("Sanctuary_Vendor_N")),
		FVector(1.75, 1.75, 1.75),
		FLinearColor(0.24f, 0.72f, 0.78f),
		0.45f);
}

void AVendorActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (MerchantLanternLight)
	{
		LanternSwayPhase += DeltaSeconds * 3.2f;
		MerchantLanternLight->SetRelativeLocation(FVector(FMath::Sin(LanternSwayPhase) * 14.0, FMath::Cos(LanternSwayPhase * 0.7f) * 8.0, 165.0));
		MerchantLanternLight->SetIntensity(520.0f + 95.0f * FMath::Sin(LanternSwayPhase * 2.1f));
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
	VaultChestMesh->SetRelativeLocation(FVector(0.0, 0.0, 68.0));

	VaultBeaconLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("VaultBeaconLight"));
	VaultBeaconLight->SetupAttachment(VaultRoot);
	VaultBeaconLight->SetRelativeLocation(FVector(0.0, 0.0, 170.0));
	VaultBeaconLight->SetLightColor(FLinearColor(1.0f, 0.82f, 0.24f));
	VaultBeaconLight->SetUseInverseSquaredFalloff(true);
	VaultBeaconLight->SetIntensityUnits(ELightUnits::Lumens);
	VaultBeaconLight->SetSourceRadius(0.0f);
	VaultBeaconLight->SetSoftSourceRadius(0.0f);
	VaultBeaconLight->SetSpecularScale(0.05f);
	VaultBeaconLight->SetIntensity(95.0f);
	VaultBeaconLight->SetAttenuationRadius(720.0f);
	VaultBeaconLight->VolumetricScatteringIntensity = 0.85f;
	VaultBeaconLight->CastShadows = false;
}

void AMajorLootVaultActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EncounterVisualHelper::ApplyEncounter25DSpriteVisual(
		this,
		VaultChestMesh.Get(),
		FName(TEXT("Sanctuary_Chest_D")),
		FName(TEXT("Sanctuary_Chest_N")),
		FVector(1.65, 1.65, 1.65),
		FLinearColor(0.95f, 0.78f, 0.18f),
		0.32f);
}

void AMajorLootVaultActor::BeginPlay()
{
	Super::BeginPlay();
	EncounterVisualHelper::ApplyEncounter25DSpriteVisual(
		this,
		VaultChestMesh.Get(),
		FName(TEXT("Sanctuary_Chest_D")),
		FName(TEXT("Sanctuary_Chest_N")),
		FVector(1.65, 1.65, 1.65),
		FLinearColor(0.95f, 0.78f, 0.18f),
		0.32f);
}

void AMajorLootVaultActor::UnlockVaultWithSeed(int32 RoomSeed)
{
	if (!HasAuthority() || bVaultUnlocked || bLockedByBossGuardian)
	{
		return;
	}

	bVaultUnlocked = true;
	VaultRewards.Reset();
	FRandomStream Stream(RoomSeed);

	// Last Sunset (Relic_SolarFlashlightCore) only comes from the Colossus, so vaults roll from the other four.
	static constexpr const TCHAR* VaultRelicIds[4] =
	{
		TEXT("Relic_SunGlassLens"),
		TEXT("Relic_ChronoWardBoots"),
		TEXT("Relic_EchoTunedBlade"),
		TEXT("Relic_VampireWick")
	};
	const int32 FirstPick = Stream.RandRange(0, 3);

	for (int32 i = 0; i < 3; ++i)
	{
		FVendorCatalogItem Relic;
		Relic.ItemGuid = FGuid(Stream.GetUnsignedInt(), Stream.GetUnsignedInt(), Stream.GetUnsignedInt(), Stream.GetUnsignedInt());
		Relic.ItemId = FName(VaultRelicIds[(FirstPick + i) & 3]);
		Relic.GoldPrice = 350 + Stream.RandRange(0, 150);
		Relic.StockCount = 1;
		VaultRewards.Add(Relic);
	}
}

ADungeoneerLootPickupActor::ADungeoneerLootPickupActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	PickupRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PickupRoot"));
	SetRootComponent(PickupRoot);

	PickupSpriteMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupSpriteMesh"));
	PickupSpriteMesh->SetupAttachment(PickupRoot);
	PickupSpriteMesh->SetRelativeLocation(FVector(0.0, 0.0, BaseZ));

	PickupGlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PickupGlowLight"));
	PickupGlowLight->SetupAttachment(PickupRoot);
	PickupGlowLight->SetRelativeLocation(FVector(0.0, 0.0, 75.0));
	PickupGlowLight->SetUseInverseSquaredFalloff(true);
	PickupGlowLight->SetIntensityUnits(ELightUnits::Lumens);
	PickupGlowLight->SetSourceRadius(16.0f);
	PickupGlowLight->SetSoftSourceRadius(24.0f);
	PickupGlowLight->SetIntensity(220.0f);
	PickupGlowLight->SetAttenuationRadius(360.0f);
	PickupGlowLight->CastShadows = false;
}

void ADungeoneerLootPickupActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	EnsurePickupVisualsInitialized();
}

void ADungeoneerLootPickupActor::BeginPlay()
{
	Super::BeginPlay();
	EnsurePickupVisualsInitialized();
}

void ADungeoneerLootPickupActor::ConfigurePickup(
	EDungeoneerLootDropKind InKind,
	int32 InGoldAmount,
	EDungeoneerConsumableId InConsumableId,
	EDungeoneerRelicId InRelicId)
{
	DropKind = InKind;
	GoldAmount = FMath::Max(0, InGoldAmount);
	ConsumableId = InConsumableId;
	RelicId = InRelicId;
	bVisualsApplied = false;
	EnsurePickupVisualsInitialized();
}

void ADungeoneerLootPickupActor::ConfigureSkillScroll(const EDungeoneerSkillId InSkillId)
{
	DropKind = EDungeoneerLootDropKind::SkillScroll;
	SkillId = InSkillId;
	GoldAmount = 0;
	ConsumableId = EDungeoneerConsumableId::None;
	RelicId = EDungeoneerRelicId::None;
	bVisualsApplied = false;
	EnsurePickupVisualsInitialized();
}

void ADungeoneerLootPickupActor::EnsurePickupVisualsInitialized()
{
	if (bVisualsApplied)
	{
		return;
	}

	FName DiffKey = FName(TEXT("Sanctuary_Chest_D"));
	FName NormKey = FName(TEXT("Sanctuary_Chest_N"));
	FLinearColor GlowColor(1.0f, 0.82f, 0.24f);
	FVector ScaleVec(0.55, 0.55, 0.55);

	if (DropKind == EDungeoneerLootDropKind::Relic)
	{
		DiffKey = FName(TEXT("Sanctuary_Shrine_D"));
		NormKey = FName(TEXT("Sanctuary_Shrine_N"));
		GlowColor = FLinearColor(0.24f, 0.92f, 1.0f);
		ScaleVec = FVector(0.68, 0.68, 0.68);
	}
	else if (DropKind == EDungeoneerLootDropKind::Consumable)
	{
		DiffKey = FName(TEXT("Sanctuary_Brazier_D"));
		NormKey = FName(TEXT("Sanctuary_Brazier_N"));
		GlowColor = (ConsumableId == EDungeoneerConsumableId::AntidoteDraught)
			? FLinearColor(0.32f, 0.96f, 0.52f)
			: FLinearColor(1.0f, 0.68f, 0.22f);
		ScaleVec = FVector(0.58, 0.58, 0.58);
	}
	else if (DropKind == EDungeoneerLootDropKind::SkillScroll)
	{
		DiffKey = FName(TEXT("Sanctuary_Shrine_D"));
		NormKey = FName(TEXT("Sanctuary_Shrine_N"));
		GlowColor = FLinearColor(0.78f, 0.42f, 1.0f);
		ScaleVec = FVector(0.6, 0.6, 0.6);
	}

	EncounterVisualHelper::ApplyEncounter25DSpriteVisual(
		this,
		PickupSpriteMesh.Get(),
		DiffKey,
		NormKey,
		ScaleVec,
		GlowColor,
		0.32f);

	if (PickupGlowLight)
	{
		PickupGlowLight->SetLightColor(GlowColor);
	}
	bVisualsApplied = true;
}

void ADungeoneerLootPickupActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bCollected || !PickupSpriteMesh)
	{
		return;
	}

	HoverPhase += DeltaSeconds * 3.6f;
	PickupSpriteMesh->SetRelativeLocation(FVector(0.0, 0.0, BaseZ + 6.5f * FMath::Sin(HoverPhase)));
}

bool ADungeoneerLootPickupActor::CollectByPlayer(ADungeoneerCharacter* Collector)
{
	if (!HasAuthority() || bCollected || !Collector)
	{
		return false;
	}

	UDungeoneerInventoryComponent* Inv = Collector->GetInventoryComponent();
	if (!Inv)
	{
		return false;
	}

	if (DropKind == EDungeoneerLootDropKind::GoldPouch)
	{
		const int32 Grant = FMath::Max(10, GoldAmount);
		Inv->AuthorityAdjustGold(Grant);
		Collector->PushFloatingCombatPopup(
			GetActorLocation() + FVector(0.0, 0.0, 72.0),
			FString::Printf(TEXT("+%dg"), Grant),
			EDungeoneerFloatingTextKind::Gold);
	}
	else if (DropKind == EDungeoneerLootDropKind::Consumable)
	{
		Inv->AuthorityAddConsumable(ConsumableId, 1);
		const TCHAR* Label = (ConsumableId == EDungeoneerConsumableId::AntidoteDraught)
			? TEXT("+1 ANTIDOTE")
			: TEXT("+1 OIL PHIAL");
		Collector->PushFloatingCombatPopup(GetActorLocation() + FVector(0.0, 0.0, 72.0), Label, EDungeoneerFloatingTextKind::Relic);
	}
	else if (DropKind == EDungeoneerLootDropKind::Relic)
	{
		Inv->AuthorityGrantRelic(RelicId);
		Collector->PushFloatingCombatPopup(
			GetActorLocation() + FVector(0.0, 0.0, 84.0),
			FString::Printf(TEXT("RELIC: %s"), UDungeoneerInventoryComponent::GetRelicDisplayName(RelicId)),
			EDungeoneerFloatingTextKind::Relic);
	}

	else if (DropKind == EDungeoneerLootDropKind::SkillScroll)
	{
		UDungeoneerSkillComponent* Skills = Collector->GetSkillComponent();
		if (!Skills)
		{
			return false;
		}
		const EDungeoneerSkillId Dropped = Skills->EquipFoundSkill(SkillId);
		const TCHAR* Learned = DungeoneerSkills::GetDef(SkillId).DisplayName;
		Collector->PushFloatingCombatPopup(
			GetActorLocation() + FVector(0.0, 0.0, 84.0),
			(Dropped == EDungeoneerSkillId::None)
				? FString::Printf(TEXT("LEARNED: %s"), Learned)
				: FString::Printf(TEXT("LEARNED: %s (forgot %s)"), Learned, DungeoneerSkills::GetDef(Dropped).DisplayName),
			EDungeoneerFloatingTextKind::Relic);
	}

	if (bMarksSecretFound)
	{
		if (ADungeoneerGameState* GS = GetWorld() ? GetWorld()->GetGameState<ADungeoneerGameState>() : nullptr)
		{
			GS->AuthorityRecordSecretFound();
		}
	}

	bCollected = true;
	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
	return true;
}
