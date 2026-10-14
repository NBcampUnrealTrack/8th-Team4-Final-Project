#include "Player/DominationPlayerState.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "Net/UnrealNetwork.h"

void ADominationPlayerState::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADominationPlayerState, CombatClass);
	DOREPLIFETIME(ADominationPlayerState, PreviewCombatClass);
	DOREPLIFETIME(ADominationPlayerState, RespawnEndServerTime);
	DOREPLIFETIME(ADominationPlayerState, RespawnReductionSeconds);
}

void ADominationPlayerState::SetRespawnTimer(const float DelaySeconds, const float ReductionSeconds)
{
	if (!HasAuthority())
	{
		return;
	}

	RespawnEndServerTime = GetServerTimeSeconds() + FMath::Max(0.0f, DelaySeconds);
	RespawnReductionSeconds = FMath::Max(0.0f, ReductionSeconds);
	OnRep_RespawnTimer();
	ForceNetUpdate();
}

float ADominationPlayerState::GetRespawnSecondsRemaining() const
{
	return IsAlive() ? 0.0f : FMath::Max(0.0f, RespawnEndServerTime - GetServerTimeSeconds());
}

float ADominationPlayerState::GetRespawnReductionSeconds() const
{
	return RespawnReductionSeconds;
}

void ADominationPlayerState::OnRep_RespawnTimer()
{
	OnRespawnTimerChanged.Broadcast();
}

float ADominationPlayerState::GetServerTimeSeconds() const
{
	const UWorld* World = GetWorld();
	const AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr;
	return IsValid(GameState) ? static_cast<float>(GameState->GetServerWorldTimeSeconds()) : 0.0f;
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

bool ADominationPlayerState::TryChangeCombatClass(const EDominationCombatClass RequestedClass)
{
    if (!HasAuthority() || !HasSelectedCombatClass()
        || !GetDefault<UDominationClassSettings>()->IsSelectableClass(RequestedClass))
    {
        return false;
    }
    if (CombatClass != RequestedClass)
    {
        CombatClass = RequestedClass;
        OnRep_CombatClass();
        ForceNetUpdate();
    }
    return true;
}

void ADominationPlayerState::RestoreCombatClassForReconnect(const EDominationCombatClass RestoredClass)
{
	if (!HasAuthority() || RestoredClass == EDominationCombatClass::Unselected)
	{
		return;
	}

	CombatClass = RestoredClass;
	PreviewCombatClass = EDominationCombatClass::Unselected;
	OnRep_CombatClass();
	OnRep_PreviewCombatClass();
	ForceNetUpdate();
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







void ADominationPlayerState::ResetCombatClassSelection()
{
    if (!HasAuthority())
    {
        return;
    }
    const bool bHadSelection = HasSelectedCombatClass();
    const bool bHadPreview = HasPreviewCombatClass();
    CombatClass = EDominationCombatClass::Unselected;
    PreviewCombatClass = EDominationCombatClass::Unselected;
    if (bHadSelection) { OnRep_CombatClass(); }
    if (bHadPreview) { OnRep_PreviewCombatClass(); }
    ForceNetUpdate();
}
