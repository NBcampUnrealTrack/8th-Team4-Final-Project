#include "GameMode/Settings/DominationClassSettings.h"

#include "Character/ShooterCharacter.h"
#include "Customization/GameplayAppearanceConfig.h"
#include "Tags/GameplayTags.h"

UDominationClassSettings::UDominationClassSettings()
{
	GameplayAppearanceConfigs =
	{
		{TEXT("Trooper"), TSoftObjectPtr<UGameplayAppearanceConfig>(FSoftObjectPath(TEXT("/Game/FPS/Customizable/Character/Gameplay/DA_GameplayTrooper.DA_GameplayTrooper")))},
		{TEXT("Ranger"), TSoftObjectPtr<UGameplayAppearanceConfig>(FSoftObjectPath(TEXT("/Game/FPS/Customizable/Character/Gameplay/DA_GameplayRanger.DA_GameplayRanger")))},
		{TEXT("Vanguard"), TSoftObjectPtr<UGameplayAppearanceConfig>(FSoftObjectPath(TEXT("/Game/FPS/Customizable/Character/Gameplay/DA_GameplayVanguard.DA_GameplayVanguard")))}
	};

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

	// 단독 Training의 초기 클래스 선택 경로에서만 사용한다.
	// Domination은 LoadoutManager의 비동기 준비 후 PawnClass.Get()으로 조회한다.
	return Definition->PawnClass.LoadSynchronous();
}
