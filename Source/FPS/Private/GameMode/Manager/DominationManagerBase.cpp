#include "GameMode/Manager/DominationManagerBase.h"

#include "Engine/World.h"
#include "GameMode/DominationGameMode.h"
#include "GameMode/State/DominationGameState.h"
#include "TimerManager.h"

bool UDominationManagerBase::InitializeManager(
	ADominationGameMode* InOwnerGameMode)
{
	if (!IsValid(InOwnerGameMode) || !InOwnerGameMode->HasAuthority())
	{
		return false;
	}

	OwnerGameMode = InOwnerGameMode;
	if (!IsValid(GetWorld()) || !IsValid(GetDominationGameState()))
	{
		OwnerGameMode.Reset();
		return false;
	}

	return true;
}

void UDominationManagerBase::Shutdown()
{
	OwnerGameMode.Reset();
}

UWorld* UDominationManagerBase::GetWorld() const
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return nullptr;
	}

	const ADominationGameMode* GameMode = OwnerGameMode.Get();
	return IsValid(GameMode) ? GameMode->GetWorld() : nullptr;
}

ADominationGameMode* UDominationManagerBase::GetOwnerGameMode() const
{
	return OwnerGameMode.Get();
}

ADominationGameState* UDominationManagerBase::GetDominationGameState() const
{
	const ADominationGameMode* GameMode = OwnerGameMode.Get();
	return IsValid(GameMode)
		? GameMode->GetGameState<ADominationGameState>()
		: nullptr;
}

FTimerManager* UDominationManagerBase::GetTimerManager() const
{
	UWorld* World = GetWorld();
	return IsValid(World) ? &World->GetTimerManager() : nullptr;
}
