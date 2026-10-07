#include "GAS/RogueAmmoAttributeSet.h"

#include "Actor/FieldPickup.h"
#include "Combat/CombatComponent.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectExtension.h"
#include "Tags/GameplayTags.h"
#include "Weapon/WeaponBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogAmmoPickup, Log, All);

URogueAmmoAttributeSet::URogueAmmoAttributeSet()
{
	InitIncomingReserveAmmo(0.f);
}

AWeaponBase* URogueAmmoAttributeSet::ResolveReserveAmmoTarget(
	const UAbilitySystemComponent& AbilitySystem, const FGameplayEffectContextHandle& EffectContext,
	float Magnitude, int32& OutAmount)
{
	OutAmount = 0;
	if (!AbilitySystem.IsOwnerActorAuthoritative()
		|| !AbilitySystem.GetSet<URogueAmmoAttributeSet>()
		|| AbilitySystem.HasMatchingGameplayTag(RogueTags::State_Combat_IsDead))
	{
		return nullptr;
	}

	AActor* Avatar = AbilitySystem.GetAvatarActor();
	if (!IsValid(Avatar) || !Avatar->HasAuthority() || Avatar->IsActorBeingDestroyed())
	{
		return nullptr;
	}

	const UCombatComponent* Combat = Avatar->FindComponentByClass<UCombatComponent>();
	AWeaponBase* Weapon = IsValid(Combat) ? Combat->GetEquippedWeapon() : nullptr;
	if (!IsValid(Weapon) || Weapon->IsActorBeingDestroyed() || Weapon->GetOwner() != Avatar)
	{
		return nullptr;
	}

	// 픽업 BP의 부모나 저장된 지급 수치와 무관하게 한 탄창 분량을 지급한다.
	// 아이템 외의 탄약 GE는 전달받은 수치를 그대로 사용한다.
	if (Cast<AFieldPickup>(EffectContext.GetSourceObject()))
	{
		Magnitude = static_cast<float>(Weapon->GetEffectiveMagazineSize());
	}
	if (!FMath::IsFinite(Magnitude) || Magnitude < 1.f)
	{
		return nullptr;
	}

	// double로 상한을 계산해 float의 MAX_int32 반올림에 따른 변환 오버플로를 막는다.
	const int32 Amount = static_cast<int32>(FMath::Min(
		static_cast<double>(Magnitude), static_cast<double>(MAX_int32)));
	if (!Weapon->CanReceiveReserveAmmo(Amount))
	{
		return nullptr;
	}

	OutAmount = Amount;
	return Weapon;
}

void URogueAmmoAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	if (Data.EvaluatedData.Attribute != GetIncomingReserveAmmoAttribute())
	{
		return;
	}

	const float Magnitude = GetIncomingReserveAmmo();
	// 무기 변경 이벤트가 다른 효과를 실행하더라도 이전 지급량이 남지 않게 한다.
	SetIncomingReserveAmmo(0.f);

	int32 Amount = 0;
	const FGameplayEffectContextHandle& EffectContext = Data.EffectSpec.GetContext();
	if (AWeaponBase* Weapon = ResolveReserveAmmoTarget(Data.Target, EffectContext, Magnitude, Amount))
	{
		const int32 PreviousReserveAmmo = Weapon->GetCurrentReserveAmmo();
		const int32 AppliedAmount = Weapon->AddReserveAmmo(Amount);
		if (const AFieldPickup* Pickup = Cast<AFieldPickup>(EffectContext.GetSourceObject()))
		{
			UE_LOG(LogAmmoPickup, Log,
				TEXT("[AmmoPickup] Pickup=%s Weapon=%s Tag=%s BaseMagazine=%d MaxMagazine=%d EffectMagnitude=%.2f Granted=%d Reserve=%d->%d"),
				*GetNameSafe(Pickup), *GetNameSafe(Weapon), *Weapon->GetWeaponTag().ToString(),
				Weapon->GetMagazineSize(), Weapon->GetEffectiveMagazineSize(), Magnitude,
				AppliedAmount, PreviousReserveAmmo, Weapon->GetCurrentReserveAmmo());
		}
	}
}
