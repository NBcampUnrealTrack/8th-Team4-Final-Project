#include "Weapon/WeaponInventoryComponent.h"

#include "AbilitySystemComponent.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GameplayTags.h"

namespace
{
	constexpr EWeaponType TrackedWeapons[] =
	{
		EWeaponType::AR,
		EWeaponType::Sniper,
		EWeaponType::Shotgun,
		EWeaponType::Pistol,
		EWeaponType::Knife
	};
}

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
	DOREPLIFETIME_CONDITION(UWeaponInventoryComponent, EquippedPrimary, COND_OwnerOnly);
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

bool UWeaponInventoryComponent::IsPrimaryWeapon(const EWeaponType WeaponType)
{
	return WeaponType == EWeaponType::AR
		|| WeaponType == EWeaponType::Sniper
		|| WeaponType == EWeaponType::Shotgun;
}

bool UWeaponInventoryComponent::HasServerAuthority() const
{
	const AActor* Owner = GetOwner();
	return IsValid(Owner) && Owner->HasAuthority();
}

FWeaponOwnershipState* UWeaponInventoryComponent::FindState(const EWeaponType WeaponType)
{
	return WeaponStates.FindByPredicate(
		[WeaponType](const FWeaponOwnershipState& State)
		{
			return State.WeaponType == WeaponType;
		});
}

const FWeaponOwnershipState* UWeaponInventoryComponent::FindState(const EWeaponType WeaponType) const
{
	return const_cast<UWeaponInventoryComponent*>(this)->FindState(WeaponType);
}

bool UWeaponInventoryComponent::IsPossessed(const EWeaponType WeaponType) const
{
	const FWeaponOwnershipState* State = FindState(WeaponType);
	return State && State->bPossessed;
}

bool UWeaponInventoryComponent::IsUnlocked(const EWeaponType WeaponType) const
{
	const FWeaponOwnershipState* State = FindState(WeaponType);
	return State && State->bUnlocked;
}

void UWeaponInventoryComponent::ResetToDefaults()
{
	WeaponStates.Reset(UE_ARRAY_COUNT(TrackedWeapons));

	for (const EWeaponType WeaponType : TrackedWeapons)
	{
		FWeaponOwnershipState& State = WeaponStates.AddDefaulted_GetRef();
		State.WeaponType = WeaponType;

		// 권총/칼은 시작부터 소지하지만 해금은 업그레이드 전까지 닫혀 있다.
		State.bPossessed = (WeaponType == EWeaponType::Pistol || WeaponType == EWeaponType::Knife);
		State.bUnlocked = false;
	}

	EquippedPrimary = EWeaponType::None;
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

bool UWeaponInventoryComponent::ServerConfirmPrimaryPurchase(const EWeaponType WeaponType)
{
	if (!HasServerAuthority() || !IsPrimaryWeapon(WeaponType))
	{
		return false;
	}

	FWeaponOwnershipState* State = FindState(WeaponType);
	if (!State)
	{
		return false;
	}

	State->bPossessed = true;
	State->bUnlocked = true;
	EquippedPrimary = WeaponType;

	NotifyInventoryChanged();
	return true;
}

bool UWeaponInventoryComponent::ServerSetEquippedPrimary(const EWeaponType WeaponType)
{
	if (!HasServerAuthority())
	{
		return false;
	}

	// None은 주무기 미장착을 뜻하므로 허용한다.
	if (WeaponType != EWeaponType::None && !IsPossessed(WeaponType))
	{
		return false;
	}

	EquippedPrimary = WeaponType;
	NotifyInventoryChanged();
	return true;
}

bool UWeaponInventoryComponent::ServerConfirmWeaponUpgrade(const EWeaponType WeaponType)
{
	if (!HasServerAuthority())
	{
		return false;
	}

	FWeaponOwnershipState* State = FindState(WeaponType);
	if (!State || !State->bPossessed)
	{
		return false;
	}

	State->bUnlocked = true;
	NotifyInventoryChanged();
	return true;
}

void UWeaponInventoryComponent::SyncUnlockTags()
{
	if (!IsValid(AbilitySystemComponent))
	{
		return;
	}

	auto SetTag = [this](const FGameplayTag& Tag, const bool bOn)
		{
			// AddLoose가 아니라 개수를 직접 세팅한다. 중복 호출로 카운트가 쌓이지 않게.
			AbilitySystemComponent->SetLooseGameplayTagCount(Tag, bOn ? 1 : 0);
		};

	SetTag(RogueTags::Unlock_Weapon_Rifle_Owned,      IsUnlocked(EWeaponType::AR));
	SetTag(RogueTags::Unlock_Weapon_Sniper_Owned,     IsUnlocked(EWeaponType::Sniper));
	SetTag(RogueTags::Unlock_Weapon_Shotgun_Owned,    IsUnlocked(EWeaponType::Shotgun));
	SetTag(RogueTags::Unlock_Weapon_Pistol_Upgraded,  IsUnlocked(EWeaponType::Pistol));
	SetTag(RogueTags::Unlock_Weapon_Knife_Upgraded,   IsUnlocked(EWeaponType::Knife));

	// RequiredUnlockTags는 HasAll(AND) 판정이라 "총기 중 아무거나 하나"를 표현할 수 없다.
	// 총기_공통 풀 전용 파생 태그를 따로 세운다. 전술 칼은 총기 그룹이 아니다.
	const bool bAnyFirearm =
		IsUnlocked(EWeaponType::AR)
		|| IsUnlocked(EWeaponType::Sniper)
		|| IsUnlocked(EWeaponType::Shotgun)
		|| IsUnlocked(EWeaponType::Pistol);

	SetTag(RogueTags::Unlock_Weapon_Firearm_Any, bAnyFirearm);
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
