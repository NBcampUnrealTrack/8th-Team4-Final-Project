#include "Data/WeaponData.h"

#include "Animation/AnimMontage.h"

const FPlayerAnims* UWeaponData::FindPlayerAnims(const FGameplayTag WeaponTag, const bool bFirstPerson) const
{
	if (!WeaponTag.IsValid())
	{
		return nullptr;
	}

	const TMap<FGameplayTag, FPlayerAnims>& AnimMap = bFirstPerson ? FirstPersonAnims : ThirdPersonAnims;
	return AnimMap.Find(WeaponTag);
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
