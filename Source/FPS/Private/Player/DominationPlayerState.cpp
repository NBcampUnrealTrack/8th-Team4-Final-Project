#include "Player/DominationPlayerState.h"

#include "GameMode/Settings/DominationClassSettings.h"
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
	const UDominationClassSettings* ClassSettings =
		GetDefault<UDominationClassSettings>();
	const bool bIsSelectableClass = IsValid(ClassSettings)
		&& ClassSettings->IsSelectableClass(RequestedClass);
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
