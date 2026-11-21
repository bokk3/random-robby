#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/World.h"
#include "AI/ZombieBase.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Core/DungeoneerTypes.h"
#include "DungeoneerSkillComponent.generated.h"

class ADungeoneerCharacter;

/** Static tuning for one skill. Kept in code so balance changes show up in diffs. */
struct FDungeoneerSkillDef
{
	EDungeoneerSkillId Id = EDungeoneerSkillId::None;
	const TCHAR* DisplayName = TEXT("Empty");
	const TCHAR* HudLabel = TEXT("--");
	const TCHAR* Description = TEXT("");
	float CooldownSec = 0.0f;
	float OilCost = 0.0f;
};

namespace DungeoneerSkills
{
	DARKDUNGEONEER_API const FDungeoneerSkillDef& GetDef(EDungeoneerSkillId Id) noexcept;

	/** Every skill except None, in enum order. Used for loot rolls. */
	inline constexpr int32 NumRollableSkills = static_cast<int32>(EDungeoneerSkillId::Count) - 1;
	DARKDUNGEONEER_API EDungeoneerSkillId SkillFromRoll(uint32 Roll) noexcept;
}

namespace DungeoneerCombat
{
	/** Calls Fn(AZombieBase&, DistSq2D) for every living, real zombie within Radius of Origin. No allocations. */
	template <typename FnType>
	int32 ForEachZombieInRadius(const UWorld* World, const FVector& Origin, float Radius, FnType&& Fn)
	{
		const UZombieSpatialSubsystem* Spatial = World ? World->GetSubsystem<UZombieSpatialSubsystem>() : nullptr;
		if (!Spatial)
		{
			return 0;
		}
		const double RadiusSq = static_cast<double>(Radius) * static_cast<double>(Radius);
		int32 Visited = 0;
		const int32 Count = Spatial->GetTrackedZombieCount();
		for (int32 i = 0; i < Count; ++i)
		{
			AZombieBase* Zombie = Spatial->GetTrackedZombieAt(i);
			if (!Zombie || !Zombie->IsZombieAlive() || !Zombie->IsRealPhysicalZombie())
			{
				continue;
			}
			const double DistSq = FVector::DistSquared2D(Origin, Zombie->GetActorLocation());
			if (DistSq <= RadiusSq)
			{
				Fn(*Zombie, DistSq);
				++Visited;
			}
		}
		return Visited;
	}

	/**
	 * Picks the living zombie closest to Origin inside a forward cone (MinCos against ForwardXY)
	 * and range. Returns nullptr when nothing qualifies.
	 */
	DARKDUNGEONEER_API AZombieBase* FindBestTargetInCone(const UWorld* World, const FVector& Origin, const FVector& ForwardXY, float Range, float MinCos) noexcept;
}

/** Number of skill slots. Namespace scope so GENERATED_BODY's replication code can see it. */
inline constexpr int32 GDungeoneerSkillSlotCount = 2;

/**
 * Two equippable, cooldown-gated skills plus the timed effects some of them leave running
 * (the whirl's damage ticks, the decoy's lure pulses). The owning character handles input and
 * pose; this component owns rules, timers and hit resolution.
 */
UCLASS(ClassGroup = (Dungeoneer), meta = (BlueprintSpawnableComponent))
class DARKDUNGEONEER_API UDungeoneerSkillComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDungeoneerSkillComponent();

	static constexpr int32 NumSlots = GDungeoneerSkillSlotCount;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Fires the skill in a slot if it is off cooldown and affordable. Client calls are forwarded to the server. */
	UFUNCTION(BlueprintCallable, Category = "Skills")
	bool TryActivateSlot(int32 SlotIndex);

	/** Puts a skill in a specific slot and resets that slot's cooldown. Authority only. */
	UFUNCTION(BlueprintCallable, Category = "Skills")
	void EquipSkill(int32 SlotIndex, EDungeoneerSkillId SkillId);

	/**
	 * Equips a skill found in the dungeon. Fills an empty slot first, otherwise replaces the slot
	 * that was replaced least recently. Returns the skill that was dropped (None if a slot was empty
	 * or the skill was already equipped).
	 */
	EDungeoneerSkillId EquipFoundSkill(EDungeoneerSkillId SkillId);

	/** Back to the starting pair (Bottled Kiln, Borrowed Noon), cooldowns cleared, running effects stopped. */
	void ResetLoadoutForNewRun();

	UFUNCTION(BlueprintPure, Category = "Skills")
	EDungeoneerSkillId GetEquippedSkill(int32 SlotIndex) const noexcept;

	UFUNCTION(BlueprintPure, Category = "Skills")
	float GetCooldownRemaining(int32 SlotIndex) const noexcept;

	/** 0 = ready, 1 = just used. */
	UFUNCTION(BlueprintPure, Category = "Skills")
	float GetCooldownFraction(int32 SlotIndex) const noexcept;

	UFUNCTION(BlueprintPure, Category = "Skills")
	bool IsWhirling() const noexcept { return WhirlRemaining > 0.0f; }

	UFUNCTION(BlueprintPure, Category = "Skills")
	float GetWhirlElapsed() const noexcept { return WhirlElapsed; }

	UFUNCTION(BlueprintPure, Category = "Skills")
	bool IsDecoyActive() const noexcept { return DecoyRemaining > 0.0f; }

	FVector GetDecoyLocation() const noexcept { return DecoyLocation; }

	static constexpr float WhirlDurationSec = 1.4f;
	static constexpr float DecoyDurationSec = 5.0f;
	static constexpr float DecoyHopBackUU = 160.0f;

protected:
	UFUNCTION(Server, Reliable)
	void Server_ActivateSlot(int32 SlotIndex);

	/** Applies the skill's gameplay effects. Returns false if it could not fire (no oil, no target...). */
	bool ExecuteSkillAuthority(EDungeoneerSkillId SkillId);

	bool ExecuteEmberFlask();
	bool ExecuteGrappleChain();
	bool ExecuteShadeStep();
	bool ExecuteWhirlingLantern();
	bool ExecuteSolarFlare();
	bool ExecuteMirrorDecoy();

	void TickWhirl(float DeltaTime) noexcept;
	void TickDecoy(float DeltaTime) noexcept;

	ADungeoneerCharacter* GetHero() const noexcept;

	/** Static arrays can't be Blueprint-exposed; use GetEquippedSkill. Defaults are set in the constructor. */
	UPROPERTY(Replicated, VisibleInstanceOnly, Category = "Skills")
	EDungeoneerSkillId EquippedSkills[GDungeoneerSkillSlotCount];

private:
	float CooldownRemaining[NumSlots] = { 0.0f, 0.0f };
	float CooldownTotal[NumSlots] = { 1.0f, 1.0f };
	int32 NextSlotToReplace = 0;

	float WhirlRemaining = 0.0f;
	float WhirlElapsed = 0.0f;
	float WhirlTickAccumulator = 0.0f;

	FVector DecoyLocation = FVector::ZeroVector;
	float DecoyRemaining = 0.0f;
	float DecoyPulseAccumulator = 0.0f;
};
