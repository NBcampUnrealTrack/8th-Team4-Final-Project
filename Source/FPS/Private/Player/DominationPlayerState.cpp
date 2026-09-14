#include "Player/DominationPlayerState.h"

#include "GameMode/Settings/DominationClassSettings.h"
#include "Net/UnrealNetwork.h"

void ADominationPlayerState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADominationPlayerState, CombatClass);
	DOREPLIFETIME(ADominationPlayerState, PreviewCombatClass);
}

EDominationCombatClass ADominationPlayerState::GetCombatClass() const
{
	return CombatClass;
}

EDominationCombatClass ADominationPlayerState::GetPreviewCombatClass() const
{
	return PreviewCombatClass;
}

bool ADominationPlayerState::HasSelectedCombatClass() const
{
	return CombatClass != EDominationCombatClass::Unselected;
}

bool ADominationPlayerState::HasPreviewCombatClass() const
{
	return PreviewCombatClass != EDominationCombatClass::Unselected;
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

bool ADominationPlayerState::TrySetPreviewCombatClass(const EDominationCombatClass RequestedClass)
{
	const UDominationClassSettings* ClassSettings = GetDefault<UDominationClassSettings>();
	const bool bIsSelectableClass = IsValid(ClassSettings) && ClassSettings->IsSelectableClass(RequestedClass);
	if (!HasAuthority()
		|| HasSelectedCombatClass()
		|| PreviewCombatClass == RequestedClass
		|| !bIsSelectableClass)
	{
		return false;
	}

	PreviewCombatClass = RequestedClass;
	OnRep_PreviewCombatClass();
	ForceNetUpdate();
	return true;
}

void ADominationPlayerState::OnRep_CombatClass()
{
	OnCombatClassChanged.Broadcast(CombatClass);
}

void ADominationPlayerState::OnRep_PreviewCombatClass()
{
	OnPreviewCombatClassChanged.Broadcast(PreviewCombatClass);
}






