#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DungeonEncounterActors.generated.h"

class UBoxComponent;
class UPointLightComponent;
class UStaticMeshComponent;
class ADungeoneerCharacter;

USTRUCT(BlueprintType)
struct DARKDUNGEONEER_API FVendorCatalogItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor")
	FGuid ItemGuid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor")
	FName ItemId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor")
	int32 GoldPrice = 50;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vendor")
	int32 StockCount = 1;
};

/**
 * Resting Ground Sanctuary (`ARestingGroundActor`).
 * - Disables zombie spawns inside its bounds via UDungeonGeneratorSubsystem.
 * - Provides warm campfire dynamic lighting, restores health/battery, and unlocks
 *   Party Stash / Ability Loadout tuning / Player-to-Player trading.
 */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API ARestingGroundActor : public AActor
{
	GENERATED_BODY()

public:
	ARestingGroundActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category = "RestingGround")
	bool IsTradingAndLoadoutUnlocked() const noexcept { return true; }

protected:
	UFUNCTION()
	void HandleZoneOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void HandleZoneOverlapEnd(UPrimitiveComponent* OverlappedComp, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RestingGround")
	TObjectPtr<UBoxComponent> SanctuaryBounds;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RestingGround|Visual")
	TObjectPtr<UStaticMeshComponent> BrazierMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "RestingGround|Lighting")
	TObjectPtr<UPointLightComponent> CampfireLight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RestingGround")
	float HealthRestorePerSecond = 12.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "RestingGround")
	float BatteryRestorePerSecond = 20.0f;

private:
	TArray<TWeakObjectPtr<ADungeoneerCharacter>, TInlineAllocator<8>> RestingPlayers;
	float CampfirePhase = 0.0f;
};

/**
 * Dungeon Merchant (`AVendorActor`).
 * - Populates a deterministic seeded inventory table from ServerSeed.
 * - Routes purchases through server-authoritative UTradeManagerComponent.
 */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API AVendorActor : public AActor
{
	GENERATED_BODY()

public:
	AVendorActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	void InitializeSeededCatalog(int32 RoomSeed);

	UFUNCTION(BlueprintPure, Category = "Vendor")
	const TArray<FVendorCatalogItem>& GetVendorCatalog() const noexcept { return VendorCatalog; }

	bool FindCatalogItem(const FGuid& ItemGuid, FVendorCatalogItem& OutItem) const noexcept;
	bool ConsumeCatalogStock(const FGuid& ItemGuid) noexcept;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vendor|Visual")
	TObjectPtr<USceneComponent> VendorRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vendor|Visual")
	TObjectPtr<UStaticMeshComponent> StallMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vendor|Lighting")
	TObjectPtr<UPointLightComponent> MerchantLanternLight;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vendor")
	TArray<FVendorCatalogItem> VendorCatalog;
};

/**
 * Terminal Major Loot Vault (`AMajorLootVaultActor`).
 * - Final floor room rewarding cooperative teams with high-tier seeded loot.
 */
UCLASS(Blueprintable)
class DARKDUNGEONEER_API AMajorLootVaultActor : public AActor
{
	GENERATED_BODY()

public:
	AMajorLootVaultActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Vault")
	void UnlockVaultWithSeed(int32 RoomSeed);

	UFUNCTION(BlueprintPure, Category = "Vault")
	bool IsVaultUnlocked() const noexcept { return bVaultUnlocked; }

	UFUNCTION(BlueprintPure, Category = "Vault")
	const TArray<FVendorCatalogItem>& GetVaultLootTable() const noexcept { return VaultRewards; }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vault|Visual")
	TObjectPtr<USceneComponent> VaultRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vault|Visual")
	TObjectPtr<UStaticMeshComponent> VaultChestMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vault|Lighting")
	TObjectPtr<UPointLightComponent> VaultBeaconLight;

private:
	UPROPERTY(VisibleInstanceOnly, Category = "Vault")
	bool bVaultUnlocked = false;

	UPROPERTY(VisibleInstanceOnly, Category = "Vault")
	TArray<FVendorCatalogItem> VaultRewards;
};
