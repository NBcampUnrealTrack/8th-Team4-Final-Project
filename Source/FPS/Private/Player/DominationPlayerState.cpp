#include "Player/DominationPlayerState.h"

#include "Net/UnrealNetwork.h"

void ADominationPlayerState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADominationPlayerState, CombatClass);
}

EDominationCombatClass ADominationPlayerState::GetCombatClass() const
{
	return CombatClass;
}

bool ADominationPlayerState::HasSelectedCombatClass() const
{
	return CombatClass != EDominationCombatClass::Unselected;
}

bool ADominationPlayerState::TryConfirmCombatClass(
	const EDominationCombatClass RequestedClass)
{
	const bool bIsSelectableClass = RequestedClass == EDominationCombatClass::Trooper
		|| RequestedClass == EDominationCombatClass::Ranger
		|| RequestedClass == EDominationCombatClass::Vanguard;
	if (!HasAuthority()
		|| HasSelectedCombatClass()
		|| !bIsSelectableClass)
	{
		return false;
	}

	CombatClass = RequestedClass;
	OnRep_CombatClass();
	ForceNetUpdate();
	return true;
}

void ADominationPlayerState::OnRep_CombatClass()
{
	OnCombatClassChanged.Broadcast(CombatClass);
}
