#include "Combat/DungeoneerSkillComponent.h"
#include "AI/ZombieExploder.h"
#include "AI/ZombieSpatialSubsystem.h"
#include "Audio/DungeonAcousticSubsystem.h"
#include "Core/DungeoneerCharacter.h"
#include "Core/DungeoneerGameMode.h"
#include "Dungeon/DungeoneerTileMapInstancer.h"
#include "Ecosystem/DungeonFloraManagerComponent.h"
#include "Rendering/DungeoneerSensoryWorldSubsystem.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

namespace
{
	// Index = static_cast<int32>(EDungeoneerSkillId).
	const FDungeoneerSkillDef GSkillDefs[] =
	{
		{ EDungeoneerSkillId::None, TEXT("Empty"), TEXT("--"), TEXT(""), 0.0f, 0.0f },
		{ EDungeoneerSkillId::EmberFlask, TEXT("Bottled Kiln"), TEXT("FLASK"),
			TEXT("Lob a flask of lamp oil that bursts into flame and sets enemies burning. Lights Exploder fuses."),
			7.0f, 0.0f },
		{ EDungeoneerSkillId::GrappleChain, TEXT("Lamplighter's Hook"), TEXT("CHAIN"),
			TEXT("Hook the nearest enemy in front and drag it to you, stunned. Heavy foes drag you to them instead."),
			6.0f, 0.0f },
		{ EDungeoneerSkillId::ShadeStep, TEXT("Their Own Shadow"), TEXT("STEP"),
			TEXT("Blink behind the nearest enemy. Your next strike within a second is a backstab for 2.5x damage."),
			8.0f, 0.0f },
		{ EDungeoneerSkillId::WhirlingLantern, TEXT("Censer's Wheel"), TEXT("WHIRL"),
			TEXT("Spin the lantern on its chain for 1.4s, hitting everything around you. You can keep moving."),
			9.0f, 10.0f },
		{ EDungeoneerSkillId::SolarFlare, TEXT("Borrowed Noon"), TEXT("FLARE"),
			TEXT("Burn 25% oil in one blinding flash that damages and stuns everything nearby."),
			6.0f, 0.0f },
		{ EDungeoneerSkillId::MirrorDecoy, TEXT("Slipped Self"), TEXT("DECOY"),
			TEXT("Leave a ringing reflection that draws enemies for 5s, then shatters, hurting and stunning anything close."),
			12.0f, 0.0f },
	};
	static_assert(static_cast<int32>(UE_ARRAY_COUNT(GSkillDefs)) == static_cast<int32>(EDungeoneerSkillId::Count), "Skill table must cover every skill id");

	constexpr float WhirlTickInterval = 0.2f;
	constexpr float WhirlRadius = 230.0f;
	constexpr float WhirlTickDamage = 16.0f;
	constexpr float DecoyPulseInterval = 0.5f;
}

const FDungeoneerSkillDef& DungeoneerSkills::GetDef(const EDungeoneerSkillId Id) noexcept
{
	const int32 Index = static_cast<int32>(Id);
	return GSkillDefs[(Index >= 0 && Index < static_cast<int32>(UE_ARRAY_COUNT(GSkillDefs))) ? Index : 0];
}

EDungeoneerSkillId DungeoneerSkills::SkillFromRoll(const uint32 Roll) noexcept
{
	return static_cast<EDungeoneerSkillId>(1 + static_cast<int32>(Roll % static_cast<uint32>(NumRollableSkills)));
}

AZombieBase* DungeoneerCombat::FindBestTargetInCone(const UWorld* World, const FVector& Origin, const FVector& ForwardXY, const float Range, const float MinCos) noexcept
{
	const FVector Fwd = ForwardXY.GetSafeNormal2D();
	AZombieBase* Best = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();
	ForEachZombieInRadius(World, Origin, Range, [&](AZombieBase& Zombie, const double DistSq)
	{
		const FVector Dir = (Zombie.GetActorLocation() - Origin).GetSafeNormal2D();
		const bool bInCone = DistSq < FMath::Square(80.0) || FVector::DotProduct(Fwd, Dir) >= MinCos;
		if (bInCone && DistSq < BestDistSq)
		{
			BestDistSq = DistSq;
			Best = &Zombie;
		}
	});
	return Best;
}

UDungeoneerSkillComponent::UDungeoneerSkillComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
	EquippedSkills[0] = EDungeoneerSkillId::EmberFlask;
	EquippedSkills[1] = EDungeoneerSkillId::SolarFlare;
}

void UDungeoneerSkillComponent::ResetLoadoutForNewRun()
{
	EquippedSkills[0] = EDungeoneerSkillId::EmberFlask;
	EquippedSkills[1] = EDungeoneerSkillId::SolarFlare;
	for (int32 Slot = 0; Slot < NumSlots; ++Slot)
	{
		CooldownRemaining[Slot] = 0.0f;
		CooldownTotal[Slot] = FMath::Max(DungeoneerSkills::GetDef(EquippedSkills[Slot]).CooldownSec, 0.01f);
	}
	NextSlotToReplace = 0;
	WhirlRemaining = 0.0f;
	WhirlElapsed = 0.0f;
	if (DecoyRemaining > 0.0f)
	{
		DecoyRemaining = 0.0f;
		if (ADungeoneerCharacter* Hero = GetHero())
		{
			Hero->ShowDecoy(DecoyLocation, false);
		}
	}
}

void UDungeoneerSkillComponent::BeginPlay()
{
	Super::BeginPlay();
	for (int32 Slot = 0; Slot < NumSlots; ++Slot)
	{
		CooldownRemaining[Slot] = 0.0f;
		CooldownTotal[Slot] = FMath::Max(DungeoneerSkills::GetDef(EquippedSkills[Slot]).CooldownSec, 0.01f);
	}
}

void UDungeoneerSkillComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UDungeoneerSkillComponent, EquippedSkills);
}

ADungeoneerCharacter* UDungeoneerSkillComponent::GetHero() const noexcept
{
	return Cast<ADungeoneerCharacter>(GetOwner());
}

EDungeoneerSkillId UDungeoneerSkillComponent::GetEquippedSkill(const int32 SlotIndex) const noexcept
{
	return (SlotIndex >= 0 && SlotIndex < NumSlots) ? EquippedSkills[SlotIndex] : EDungeoneerSkillId::None;
}

float UDungeoneerSkillComponent::GetCooldownRemaining(const int32 SlotIndex) const noexcept
{
	return (SlotIndex >= 0 && SlotIndex < NumSlots) ? CooldownRemaining[SlotIndex] : 0.0f;
}

float UDungeoneerSkillComponent::GetCooldownFraction(const int32 SlotIndex) const noexcept
{
	if (SlotIndex < 0 || SlotIndex >= NumSlots)
	{
		return 0.0f;
	}
	return FMath::Clamp(CooldownRemaining[SlotIndex] / FMath::Max(CooldownTotal[SlotIndex], 0.01f), 0.0f, 1.0f);
}

void UDungeoneerSkillComponent::EquipSkill(const int32 SlotIndex, const EDungeoneerSkillId SkillId)
{
	if (SlotIndex < 0 || SlotIndex >= NumSlots)
	{
		return;
	}
	EquippedSkills[SlotIndex] = SkillId;
	CooldownRemaining[SlotIndex] = 0.0f;
	CooldownTotal[SlotIndex] = FMath::Max(DungeoneerSkills::GetDef(SkillId).CooldownSec, 0.01f);
}

EDungeoneerSkillId UDungeoneerSkillComponent::EquipFoundSkill(const EDungeoneerSkillId SkillId)
{
	if (SkillId == EDungeoneerSkillId::None)
	{
		return EDungeoneerSkillId::None;
	}
	for (int32 Slot = 0; Slot < NumSlots; ++Slot)
	{
		if (EquippedSkills[Slot] == SkillId)
		{
			CooldownRemaining[Slot] = 0.0f; // Duplicate pickup: refresh instead of swapping.
			return EDungeoneerSkillId::None;
		}
	}
	for (int32 Slot = 0; Slot < NumSlots; ++Slot)
	{
		if (EquippedSkills[Slot] == EDungeoneerSkillId::None)
		{
			EquipSkill(Slot, SkillId);
			return EDungeoneerSkillId::None;
		}
	}
	const int32 Slot = NextSlotToReplace;
	NextSlotToReplace = (NextSlotToReplace + 1) % NumSlots;
	const EDungeoneerSkillId Dropped = EquippedSkills[Slot];
	EquipSkill(Slot, SkillId);
	return Dropped;
}

bool UDungeoneerSkillComponent::TryActivateSlot(const int32 SlotIndex)
{
	ADungeoneerCharacter* Hero = GetHero();
	if (!Hero || SlotIndex < 0 || SlotIndex >= NumSlots || !Hero->CanStartAction())
	{
		return false;
	}

	const EDungeoneerSkillId SkillId = EquippedSkills[SlotIndex];
	const FDungeoneerSkillDef& Def = DungeoneerSkills::GetDef(SkillId);
	if (SkillId == EDungeoneerSkillId::None || CooldownRemaining[SlotIndex] > 0.0f || Hero->GetFlashlightBattery() < Def.OilCost)
	{
		return false;
	}

	if (GetOwner()->HasAuthority())
	{
		if (!ExecuteSkillAuthority(SkillId))
		{
			return false;
		}
	}
	else
	{
		Server_ActivateSlot(SlotIndex);
	}

	CooldownTotal[SlotIndex] = FMath::Max(Def.CooldownSec, 0.01f);
	CooldownRemaining[SlotIndex] = Def.CooldownSec;
	return true;
}

void UDungeoneerSkillComponent::Server_ActivateSlot_Implementation(const int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= NumSlots || CooldownRemaining[SlotIndex] > 0.0f)
	{
		return;
	}
	const EDungeoneerSkillId SkillId = EquippedSkills[SlotIndex];
	if (ExecuteSkillAuthority(SkillId))
	{
		const float Cooldown = DungeoneerSkills::GetDef(SkillId).CooldownSec;
		CooldownTotal[SlotIndex] = FMath::Max(Cooldown, 0.01f);
		CooldownRemaining[SlotIndex] = Cooldown;
	}
}

bool UDungeoneerSkillComponent::ExecuteSkillAuthority(const EDungeoneerSkillId SkillId)
{
	ADungeoneerCharacter* Hero = GetHero();
	if (!Hero)
	{
		return false;
	}
	const float OilCost = DungeoneerSkills::GetDef(SkillId).OilCost;
	if (OilCost > 0.0f && Hero->GetFlashlightBattery() < OilCost)
	{
		return false;
	}

	bool bFired = false;
	switch (SkillId)
	{
	case EDungeoneerSkillId::EmberFlask:      bFired = ExecuteEmberFlask(); break;
	case EDungeoneerSkillId::GrappleChain:    bFired = ExecuteGrappleChain(); break;
	case EDungeoneerSkillId::ShadeStep:       bFired = ExecuteShadeStep(); break;
	case EDungeoneerSkillId::WhirlingLantern: bFired = ExecuteWhirlingLantern(); break;
	case EDungeoneerSkillId::SolarFlare:      bFired = ExecuteSolarFlare(); break;
	case EDungeoneerSkillId::MirrorDecoy:     bFired = ExecuteMirrorDecoy(); break;
	default: break;
	}

	if (bFired)
	{
		const TCHAR* Cue = nullptr;
		switch (SkillId)
		{
		case EDungeoneerSkillId::EmberFlask:      Cue = TEXT("Combat_Skill_Kiln"); break;
		case EDungeoneerSkillId::GrappleChain:    Cue = TEXT("Combat_Skill_Hook"); break;
		case EDungeoneerSkillId::ShadeStep:       Cue = TEXT("Combat_Skill_Shade"); break;
		case EDungeoneerSkillId::WhirlingLantern: Cue = TEXT("Combat_Skill_Wheel"); break;
		case EDungeoneerSkillId::SolarFlare:      Cue = TEXT("Combat_Skill_Noon"); break;
		case EDungeoneerSkillId::MirrorDecoy:     Cue = TEXT("Combat_Skill_Slipped"); break;
		default: break;
		}
		if (Cue)
		{
			UDungeonAcousticSubsystem::PlayCombatCueAt(GetWorld(), FName(Cue), Hero->GetActorLocation(), 0.9f, 0.02f);
		}
	}

	if (bFired && OilCost > 0.0f)
	{
		Hero->SpendLanternOil(OilCost);
	}
	return bFired;
}

bool UDungeoneerSkillComponent::ExecuteEmberFlask()
{
	ADungeoneerCharacter* Hero = GetHero();
	UWorld* World = GetWorld();
	if (!Hero || !World)
	{
		return false;
	}

	const FVector Origin = Hero->GetActorLocation();
	const FVector Fwd = Hero->GetAimForwardXY();
	const AZombieBase* AutoTarget = DungeoneerCombat::FindBestTargetInCone(World, Origin, Fwd, 620.0f, 0.6f);
	const FVector Impact = AutoTarget ? AutoTarget->GetActorLocation() : Origin + Fwd * 380.0;

	DungeoneerCombat::ForEachZombieInRadius(World, Impact, 190.0f, [Hero, &Impact](AZombieBase& Zombie, double)
	{
		Zombie.TriggerHitFeedback(Zombie.GetActorLocation() - Impact, 140.0f);
		Zombie.ApplyZombieDamage(30.0f, Hero);
		Zombie.ApplyBurn(16.0f, 4.0f, Hero);
		if (AZombieExploder* Exploder = Cast<AZombieExploder>(&Zombie))
		{
			Exploder->IgniteOrAccelerateFuse(0.6f);
		}
	});

	if (UZombieSpatialSubsystem* Spatial = World->GetSubsystem<UZombieSpatialSubsystem>())
	{
		Spatial->ActivatePooledScorchDecal(Impact);
	}
	if (UDungeoneerSensoryWorldSubsystem* Sensory = World->GetSubsystem<UDungeoneerSensoryWorldSubsystem>())
	{
		Sensory->BroadcastNoiseEvent(Impact, 1.4f, 900.0f, false);
	}
	Hero->ShowSkillBeam(Origin, Impact, 0.16f, FLinearColor(1.0f, 0.45f, 0.08f));
	Hero->PushFloatingCombatPopup(Impact + FVector(0.0, 0.0, 70.0), TEXT("EMBER FLASK"), EDungeoneerFloatingTextKind::Relic);
	Hero->SetSkillCastFrame(EHeroPoseSheet::Skills, 0); // overhand flask throw
	Hero->BeginSkillPose(EDungeoneerCombatActionState::SkillCast, 0.22f);
	return true;
}

bool UDungeoneerSkillComponent::ExecuteGrappleChain()
{
	ADungeoneerCharacter* Hero = GetHero();
	UWorld* World = GetWorld();
	if (!Hero || !World)
	{
		return false;
	}

	const FVector Origin = Hero->GetActorLocation();
	AZombieBase* Target = DungeoneerCombat::FindBestTargetInCone(World, Origin, Hero->GetAimForwardXY(), 760.0f, 0.72f);
	if (!Target)
	{
		Hero->PushFloatingCombatPopup(Origin + FVector(0.0, 0.0, 90.0), TEXT("NO TARGET"), EDungeoneerFloatingTextKind::Status);
		return false;
	}

	const FVector TargetLoc = Target->GetActorLocation();
	const FVector DirToTarget = (TargetLoc - Origin).GetSafeNormal2D();
	Hero->ShowSkillBeam(Origin, TargetLoc, 0.2f, FLinearColor(0.75f, 0.78f, 0.85f));

	if (Target->GetStunResistance() >= 0.5f)
	{
		// Too heavy to move: reel the hero in instead.
		const FVector Dest = TargetLoc - DirToTarget * 150.0;
		Hero->SetActorLocation(FVector(Dest.X, Dest.Y, Origin.Z), true, nullptr, ETeleportType::TeleportPhysics);
		Hero->GrantInvulnerability(0.2f);
	}
	else
	{
		const FVector Dest = Origin + DirToTarget * 120.0;
		Target->SetActorLocation(FVector(Dest.X, Dest.Y, TargetLoc.Z), true, nullptr, ETeleportType::TeleportPhysics);
	}
	Target->ApplyStun(0.9f);
	Target->TriggerHitFeedback(-DirToTarget, 0.0f);
	Target->ApplyZombieDamage(20.0f, Hero);
	Hero->TriggerCombatCameraImpulse(DirToTarget, 14.0f, 0.03f);
	Hero->SetSkillCastFrame(EHeroPoseSheet::Skills, 1); // chain cast
	Hero->BeginSkillPose(EDungeoneerCombatActionState::SkillCast, 0.2f);
	return true;
}

bool UDungeoneerSkillComponent::ExecuteShadeStep()
{
	ADungeoneerCharacter* Hero = GetHero();
	UWorld* World = GetWorld();
	if (!Hero || !World)
	{
		return false;
	}

	const FVector Origin = Hero->GetActorLocation();
	const FVector Fwd = Hero->GetAimForwardXY();
	AZombieBase* Target = DungeoneerCombat::FindBestTargetInCone(World, Origin, Fwd, 650.0f, 0.4f);
	if (!Target)
	{
		Target = DungeoneerCombat::FindBestTargetInCone(World, Origin, Fwd, 480.0f, -1.0f);
	}

	Hero->ShowAfterimage(Origin, 0.35f);
	Hero->GrantInvulnerability(0.25f);

	if (!Target)
	{
		const FVector Dest = Origin + Fwd * 320.0;
		Hero->SetActorLocation(FVector(Dest.X, Dest.Y, Origin.Z), true, nullptr, ETeleportType::TeleportPhysics);
		return true;
	}

	const FVector TargetLoc = Target->GetActorLocation();
	const FVector Through = (TargetLoc - Origin).GetSafeNormal2D();
	const FVector Dest = TargetLoc + Through * 115.0;
	Hero->SetActorLocation(FVector(Dest.X, Dest.Y, Origin.Z), true, nullptr, ETeleportType::TeleportPhysics);
	Hero->FaceTowards(TargetLoc);
	Target->ApplyStun(0.35f);
	Hero->GrantEmpoweredStrike(2.5f, 1.3f, TEXT("BACKSTAB READY"));
	return true;
}

bool UDungeoneerSkillComponent::ExecuteWhirlingLantern()
{
	ADungeoneerCharacter* Hero = GetHero();
	if (!Hero)
	{
		return false;
	}
	WhirlRemaining = WhirlDurationSec;
	WhirlElapsed = 0.0f;
	WhirlTickAccumulator = WhirlTickInterval; // First tick lands immediately.
	Hero->BeginSkillPose(EDungeoneerCombatActionState::Whirl, WhirlDurationSec);
	return true;
}

bool UDungeoneerSkillComponent::ExecuteSolarFlare()
{
	ADungeoneerCharacter* Hero = GetHero();
	return Hero && Hero->TriggerChargedFlare();
}

bool UDungeoneerSkillComponent::ExecuteMirrorDecoy()
{
	ADungeoneerCharacter* Hero = GetHero();
	if (!Hero)
	{
		return false;
	}
	DecoyLocation = Hero->GetActorLocation();
	DecoyRemaining = DecoyDurationSec;
	DecoyPulseAccumulator = DecoyPulseInterval;
	Hero->ShowDecoy(DecoyLocation, true);
	Hero->PushFloatingCombatPopup(DecoyLocation + FVector(0.0, 0.0, 95.0), TEXT("SLIPPED SELF"), EDungeoneerFloatingTextKind::Relic);

	// The hero steps backward out of the copy (swept, so walls stop it), leaving the copy where they stood.
	const FVector Back = DecoyLocation - Hero->GetAimForwardXY() * DecoyHopBackUU;
	Hero->SetActorLocation(FVector(Back.X, Back.Y, DecoyLocation.Z), true, nullptr, ETeleportType::TeleportPhysics);
	Hero->GrantInvulnerability(0.2f);
	return true;
}

void UDungeoneerSkillComponent::TickWhirl(const float DeltaTime) noexcept
{
	if (WhirlRemaining <= 0.0f)
	{
		return;
	}
	ADungeoneerCharacter* Hero = GetHero();
	UWorld* World = GetWorld();
	WhirlRemaining = FMath::Max(0.0f, WhirlRemaining - DeltaTime);
	WhirlElapsed += DeltaTime;
	WhirlTickAccumulator += DeltaTime;

	if (!Hero || !World || !GetOwner()->HasAuthority())
	{
		return;
	}
	if (Hero->IsDowned())
	{
		WhirlRemaining = 0.0f;
		return;
	}
	while (WhirlTickAccumulator >= WhirlTickInterval)
	{
		WhirlTickAccumulator -= WhirlTickInterval;
		if (UZombieSpatialSubsystem* Spatial = World->GetSubsystem<UZombieSpatialSubsystem>())
		{
			const int32 Hits = Spatial->ApplyMeleeArcDamage(Hero->GetActorLocation(), Hero->GetAimForwardXY(), WhirlRadius, -1.0f, WhirlTickDamage, Hero, 160.0f);
			if (Hits > 0)
			{
				Hero->TriggerCombatCameraImpulse(Hero->GetAimForwardXY(), 5.0f, 0.0f);
			}
		}
	}
}

void UDungeoneerSkillComponent::TickDecoy(const float DeltaTime) noexcept
{
	if (DecoyRemaining <= 0.0f)
	{
		return;
	}
	ADungeoneerCharacter* Hero = GetHero();
	UWorld* World = GetWorld();
	DecoyRemaining = FMath::Max(0.0f, DecoyRemaining - DeltaTime);
	DecoyPulseAccumulator += DeltaTime;

	if (World && DecoyPulseAccumulator >= DecoyPulseInterval)
	{
		DecoyPulseAccumulator = 0.0f;
		if (UDungeoneerSensoryWorldSubsystem* Sensory = World->GetSubsystem<UDungeoneerSensoryWorldSubsystem>())
		{
			Sensory->BroadcastNoiseEvent(DecoyLocation, 1.6f, 1100.0f, false);
		}
	}

	if (DecoyRemaining > 0.0f)
	{
		return;
	}

	// Shatter.
	if (Hero)
	{
		Hero->ShowDecoy(DecoyLocation, false);
		if (GetOwner()->HasAuthority())
		{
			const FVector Center = DecoyLocation;
			DungeoneerCombat::ForEachZombieInRadius(World, Center, 260.0f, [Hero, &Center](AZombieBase& Zombie, double)
			{
				Zombie.TriggerHitFeedback(Zombie.GetActorLocation() - Center, 320.0f);
				Zombie.ApplyZombieDamage(45.0f, Hero);
				Zombie.ApplyStun(0.6f);
			});
		}
		Hero->PushFloatingCombatPopup(DecoyLocation + FVector(0.0, 0.0, 80.0), TEXT("SHATTER"), EDungeoneerFloatingTextKind::CritDamage);
	}
}

void UDungeoneerSkillComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	for (int32 Slot = 0; Slot < NumSlots; ++Slot)
	{
		CooldownRemaining[Slot] = FMath::Max(0.0f, CooldownRemaining[Slot] - DeltaTime);
	}
	TickWhirl(DeltaTime);
	TickDecoy(DeltaTime);
}
