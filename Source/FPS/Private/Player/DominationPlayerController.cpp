#include "Player/DominationPlayerController.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameMode/DominationGameMode.h"
#include "GameMode/DominationGameState.h"
#include "Player/DominationPlayerState.h"
#include "UI/Frontend/DominationUIInitComponent.h"

ADominationPlayerController::ADominationPlayerController()
{
	UIInitComponent = CreateDefaultSubobject<UDominationUIInitComponent>(
		TEXT("DominationUIInitComponent"));
}

bool ADominationPlayerController::CanProcessGameplayInput() const
{
	const ADominationGameState* DominationGameState = GetWorld()
		? GetWorld()->GetGameState<ADominationGameState>()
		: nullptr;
	const ADominationPlayerState* DominationPlayerState =
		GetPlayerState<ADominationPlayerState>();

	return Super::CanProcessGameplayInput()
		&& IsValid(DominationGameState)
		&& DominationGameState->GetDominationPhase() == EDominationMatchPhase::InProgress
		&& IsValid(GetPawn())
		&& IsValid(DominationPlayerState)
		&& DominationPlayerState->IsAlive();
}

void ADominationPlayerController::SelectCombatClass(
	const EDominationCombatClass RequestedClass)
{
	if (HasAuthority())
	{
		ServerSelectCombatClass_Implementation(RequestedClass);
		return;
	}

	ServerSelectCombatClass(RequestedClass);
}

void ADominationPlayerController::ServerSelectCombatClass_Implementation(
	const EDominationCombatClass RequestedClass)
{
	ADominationGameMode* DominationGameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<ADominationGameMode>()
		: nullptr;
	if (IsValid(DominationGameMode))
	{
		DominationGameMode->RequestCombatClass(this, RequestedClass);
	}
}
