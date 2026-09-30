#include "GameMode/Settings/DominationClassSettings.h"

#include "Character/ShooterCharacter.h"
#include "Tags/GameplayTags.h"

UDominationClassSettings::UDominationClassSettings()
{
	FDominationCombatClassDefinition& Trooper = CombatClasses.AddDefaulted_GetRef();
	Trooper.CombatClass = EDominationCombatClass::Trooper;
	Trooper.PrimaryWeaponTag = RogueTags::Weapon_Type_Rifle;
	Trooper.DisplayName = NSLOCTEXT(
		"DominationClassSelection",
		"TrooperName",
		"Trooper");
	Trooper.Description = NSLOCTEXT(
		"DominationClassSelection",
		"TrooperDescription",
		"Assault Rifle");

	FDominationCombatClassDefinition& Ranger = CombatClasses.AddDefaulted_GetRef();
	Ranger.CombatClass = EDominationCombatClass::Ranger;
	Ranger.PrimaryWeaponTag = RogueTags::Weapon_Type_Sniper;
	Ranger.DisplayName = NSLOCTEXT(
		"DominationClassSelection",
		"RangerName",
		"Ranger");
	Ranger.Description = NSLOCTEXT(
		"DominationClassSelection",
		"RangerDescription",
		"Sniper Rifle");

	FDominationCombatClassDefinition& Vanguard = CombatClasses.AddDefaulted_GetRef();
	Vanguard.CombatClass = EDominationCombatClass::Vanguard;
	Vanguard.PrimaryWeaponTag = RogueTags::Weapon_Type_Shotgun;
	Vanguard.DisplayName = NSLOCTEXT(
		"DominationClassSelection",
		"VanguardName",
		"Vanguard");
	Vanguard.Description = NSLOCTEXT(
		"DominationClassSelection",
		"VanguardDescription",
		"Shotgun");
}

const FDominationCombatClassDefinition* UDominationClassSettings::FindDefinition(
	const EDominationCombatClass CombatClass) const
{
	return CombatClasses.FindByPredicate(
		[CombatClass](const FDominationCombatClassDefinition& Definition)
		{
			return Definition.CombatClass == CombatClass;
		});
}

bool UDominationClassSettings::IsSelectableClass(
	const EDominationCombatClass CombatClass) const
{
	return CombatClass != EDominationCombatClass::Unselected
		&& FindDefinition(CombatClass) != nullptr;
}

bool UDominationClassSettings::HasValidDefinitions() const
{
	if (CombatClasses.IsEmpty() || !IsSelectableClass(DefaultCombatClass))
	{
		return false;
	}

	TSet<EDominationCombatClass> RegisteredClasses;
	for (const FDominationCombatClassDefinition& Definition : CombatClasses)
	{
		if (Definition.CombatClass == EDominationCombatClass::Unselected
			|| RegisteredClasses.Contains(Definition.CombatClass)
			|| !Definition.PrimaryWeaponTag.IsValid()
			|| Definition.PawnClass.IsNull())
		{
			return false;
		}

		RegisteredClasses.Add(Definition.CombatClass);
	}

	return true;
}

UClass* UDominationClassSettings::ResolvePawnClass(
	const EDominationCombatClass CombatClass) const
{
	const FDominationCombatClassDefinition* Definition = FindDefinition(CombatClass);
	if (!Definition || Definition->PawnClass.IsNull())
	{
		return nullptr;
	}

	// Domination에서는 UDominationPreloadSubsystem이 이동 중 미리 읽으므로 즉시 반환된다.
	// Training은 클래스 선택/초기 스폰 준비 구간에서만 이 경로를 사용하므로 startup load로 허용한다.
	return Definition->PawnClass.LoadSynchronous();
}
