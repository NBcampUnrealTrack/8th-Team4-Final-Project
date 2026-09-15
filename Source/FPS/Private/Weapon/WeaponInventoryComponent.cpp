#include "Weapon/WeaponInventoryComponent.h"

#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GameplayTags.h"
#include "Templates/Function.h"

UWeaponInventoryComponent::UWeaponInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UWeaponInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 상대 팀에게 로드아웃을 알릴 이유가 없다.
	DOREPLIFETIME_CONDITION(UWeaponInventoryComponent, WeaponStates, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UWeaponInventoryComponent, EquippedPrimaryTag, COND_OwnerOnly);
}

void UWeaponInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	if (HasServerAuthority())
	{
		ResetToDefaults();
	}
}

void UWeaponInventoryComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	AbilitySystemComponent = InASC;

	if (HasServerAuthority())
	{
		SyncUnlockTags();
	}
}

bool UWeaponInventoryComponent::IsPrimaryWeaponTag(const FGameplayTag WeaponTag)
{
	return WeaponTag == RogueTags::Weapon_Type_Rifle
		|| WeaponTag == RogueTags::Weapon_Type_Sniper
		|| WeaponTag == RogueTags::Weapon_Type_Shotgun;
}

bool UWeaponInventoryComponent::HasServerAuthority() const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner) && Owner->HasAuthority();
}

FWeaponOwnershipState* UWeaponInventoryComponent::FindState(const FGameplayTag WeaponTag)
{
	return WeaponStates.FindByPredicate(
		[WeaponTag](const FWeaponOwnershipState& State)
		{
			return State.WeaponTag == WeaponTag;
		});
}

const FWeaponOwnershipState* UWeaponInventoryComponent::FindState(const FGameplayTag WeaponTag) const
{
	return WeaponStates.FindByPredicate(
		[WeaponTag](const FWeaponOwnershipState& State)
		{
			return State.WeaponTag == WeaponTag;
		});
}

bool UWeaponInventoryComponent::IsPossessed(const FGameplayTag WeaponTag) const
{
	const FWeaponOwnershipState* State = FindState(WeaponTag);
	return State && State->bPossessed;
}

bool UWeaponInventoryComponent::IsUnlocked(const FGameplayTag WeaponTag) const
{
	const FWeaponOwnershipState* State = FindState(WeaponTag);
	return State && State->bUnlocked;
}

void UWeaponInventoryComponent::ResetToDefaults()
{
	const FGameplayTag TrackedWeaponTags[] =
	{
		RogueTags::Weapon_Type_Rifle,
		RogueTags::Weapon_Type_Sniper,
		RogueTags::Weapon_Type_Shotgun,
		RogueTags::Weapon_Type_Pistol,
		RogueTags::Weapon_Type_Knife
	};

	WeaponStates.Reset(UE_ARRAY_COUNT(TrackedWeaponTags));

	for (const FGameplayTag WeaponTag : TrackedWeaponTags)
	{
		FWeaponOwnershipState& State = WeaponStates.AddDefaulted_GetRef();
		State.WeaponTag = WeaponTag;

		// 권총/칼은 시작부터 소지하지만 해금은 업그레이드 전까지 닫혀 있다.
		State.bPossessed = (WeaponTag == RogueTags::Weapon_Type_Pistol
			|| WeaponTag == RogueTags::Weapon_Type_Knife);
		State.bUnlocked = false;
	}

	EquippedPrimaryTag = FGameplayTag();
}

void UWeaponInventoryComponent::ServerInitializeForNewMatch()
{
	if (!HasServerAuthority())
	{
		return;
	}

	ResetToDefaults();
	NotifyInventoryChanged();
}

bool UWeaponInventoryComponent::ServerConfirmPrimaryPurchase(const FGameplayTag WeaponTag)
{
	if (!HasServerAuthority() || !IsPrimaryWeaponTag(WeaponTag))
	{
		return false;
	}

	FWeaponOwnershipState* State = FindState(WeaponTag);
	if (!State)
	{
		return false;
	}

	State->bPossessed = true;
	State->bUnlocked = true;
	EquippedPrimaryTag = WeaponTag;

	NotifyInventoryChanged();
	return true;
}

bool UWeaponInventoryComponent::ServerSetEquippedPrimary(const FGameplayTag WeaponTag)
{
	if (!HasServerAuthority())
	{
		return false;
	}

	// 빈 태그는 주무기 미장착을 뜻하므로 허용한다.
	if (WeaponTag.IsValid() && !IsPossessed(WeaponTag))
	{
		return false;
	}

	EquippedPrimaryTag = WeaponTag;
	NotifyInventoryChanged();
	return true;
}

bool UWeaponInventoryComponent::ServerConfirmWeaponUpgrade(const FGameplayTag WeaponTag)
{
	if (!HasServerAuthority())
	{
		return false;
	}

	FWeaponOwnershipState* State = FindState(WeaponTag);
	if (!State || !State->bPossessed)
	{
		return false;
	}

	State->bUnlocked = true;
	NotifyInventoryChanged();
	return true;
}

namespace
{
    // The mapping lives here once: both GAS synchronization and read-only UI queries use it.
    void VisitAugmentUnlocks(const UWeaponInventoryComponent& Inventory,
        TFunctionRef<void(FGameplayTag, bool)> Visit)
    {
        struct FUnlockRule { FGameplayTag Weapon; FGameplayTag Unlock; bool bFirearm; };
        const FUnlockRule Rules[] = {
            {RogueTags::Weapon_Type_Rifle,   RogueTags::Unlock_Weapon_Rifle_Owned, true},
            {RogueTags::Weapon_Type_Sniper,  RogueTags::Unlock_Weapon_Sniper_Owned, true},
            {RogueTags::Weapon_Type_Shotgun, RogueTags::Unlock_Weapon_Shotgun_Owned, true},
            {RogueTags::Weapon_Type_Pistol,  RogueTags::Unlock_Weapon_Pistol_Upgraded, true},
            {RogueTags::Weapon_Type_Knife,   RogueTags::Unlock_Weapon_Knife_Upgraded, false}
        };
        bool bAnyFirearm = false;
        for (const FUnlockRule& Rule : Rules)
        {
            const bool bUnlocked = Inventory.IsUnlocked(Rule.Weapon);
            Visit(Rule.Unlock, bUnlocked);
            bAnyFirearm |= bUnlocked && Rule.bFirearm;
        }
        Visit(RogueTags::Unlock_Weapon_Firearm_Any, bAnyFirearm);
    }
}

void UWeaponInventoryComponent::GetAugmentUnlockTags(FGameplayTagContainer& OutTags) const
{
    OutTags.Reset();
    VisitAugmentUnlocks(*this, [&OutTags](const FGameplayTag Tag, const bool bUnlocked)
    {
        if (bUnlocked) { OutTags.AddTag(Tag); }
    });
}

void UWeaponInventoryComponent::SyncUnlockTags()
{
    if (!IsValid(AbilitySystemComponent)) { return; }
    VisitAugmentUnlocks(*this, [this](const FGameplayTag Tag, const bool bUnlocked)
    {
        // Keep the original server-only, idempotent tag-count semantics.
        AbilitySystemComponent->SetLooseGameplayTagCount(Tag, bUnlocked ? 1 : 0);
    });
}

void UWeaponInventoryComponent::NotifyInventoryChanged()
{
	if (HasServerAuthority())
	{
		SyncUnlockTags();
	}

	OnWeaponInventoryChanged.Broadcast();
}

void UWeaponInventoryComponent::OnRep_Inventory()
{
	OnWeaponInventoryChanged.Broadcast();
}
