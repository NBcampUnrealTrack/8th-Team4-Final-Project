#include "Data/WeaponData.h"

#include "Animation/AnimMontage.h"

const FPlayerAnims* UWeaponData::FindPlayerAnims(
	const FGameplayTag WeaponTag,
	const bool bFirstPerson,
	const EDominationCombatClass CombatClass) const
{
	if (!WeaponTag.IsValid())
	{
		return nullptr;
	}

	if (CombatClass != EDominationCombatClass::Unselected)
	{
		if (const FCombatClassWeaponAnims* ClassAnims = ClassAnimationOverrides.Find(CombatClass))
		{
			const TMap<FGameplayTag, FPlayerAnims>* ClassAnimMap = &ClassAnims->ThirdPersonAnims;
			if (bFirstPerson)
			{
				ClassAnimMap = &ClassAnims->FirstPersonAnims;
			}

			if (const FPlayerAnims* PlayerAnims = ClassAnimMap->Find(WeaponTag))
			{
				return PlayerAnims;
			}
		}
	}

	const TMap<FGameplayTag, FPlayerAnims>* AnimMap = &ThirdPersonAnims;
	if (bFirstPerson)
	{
		AnimMap = &FirstPersonAnims;
	}

	return AnimMap->Find(WeaponTag);
}

const FWeaponMontageData* UWeaponData::FindWeaponMontages(const FGameplayTag WeaponTag, const bool bFirstPerson) const
{
	if (!WeaponTag.IsValid())
	{
		return nullptr;
	}

	const TMap<FGameplayTag, FWeaponMontageData>& ViewMontages = bFirstPerson ? FirstPersonMontages : ThirdPersonMontages;
	if (const FWeaponMontageData* ViewData = ViewMontages.Find(WeaponTag))
	{
		return ViewData;
	}

	return WeaponMontages.Find(WeaponTag);
}

float UWeaponData::GetEquipDuration(const FGameplayTag WeaponTag) const
{
	float Duration = 0.f;

	if (const FWeaponMontageData* FirstPersonData = FindWeaponMontages(WeaponTag, true))
	{
		if (IsValid(FirstPersonData->EquipMontage))
		{
			Duration = FMath::Max(Duration, FirstPersonData->EquipMontage->GetPlayLength());
		}
	}

	if (const FWeaponMontageData* ThirdPersonData = FindWeaponMontages(WeaponTag, false))
	{
		if (IsValid(ThirdPersonData->EquipMontage))
		{
			Duration = FMath::Max(Duration, ThirdPersonData->EquipMontage->GetPlayLength());
		}
	}

	return Duration;
}

// 재장전 시간의 소스오브트루스를 실제 리로드 몽타주 길이로 삼는다(1인칭/3인칭 중 더 긴 쪽 기준).
// 무기 BP의 ReloadDuration 필드는 몽타주가 없을 때만 쓰이는 폴백이다.
float UWeaponData::GetReloadDuration(const FGameplayTag WeaponTag) const
{
	float Duration = 0.f;

	if (const FWeaponMontageData* FirstPersonData = FindWeaponMontages(WeaponTag, true))
	{
		if (IsValid(FirstPersonData->ReloadMontage))
		{
			Duration = FMath::Max(Duration, FirstPersonData->ReloadMontage->GetPlayLength());
		}
	}

	if (const FWeaponMontageData* ThirdPersonData = FindWeaponMontages(WeaponTag, false))
	{
		if (IsValid(ThirdPersonData->ReloadMontage))
		{
			Duration = FMath::Max(Duration, ThirdPersonData->ReloadMontage->GetPlayLength());
		}
	}

	return Duration;
}
