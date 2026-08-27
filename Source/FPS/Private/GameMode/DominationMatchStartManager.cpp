#include "GameMode/DominationMatchStartManager.h"

#include "Engine/World.h"
#include "GameMode/DominationGameMode.h"
#include "GameMode/DominationGameState.h"
#include "GameMode/DominationLoadoutManager.h"
#include "GameMode/TeamTypes.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Player/DominationPlayerController.h"
#include "Player/DominationPlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogDominationMatchStart, Log, All);

void UDominationMatchStartManager::Initialize(
	ADominationGameMode* InOwnerGameMode,
	const int32 InRequiredPlayersPerTeam,
	const float InClassSelectionDuration)
{
	OwnerGameMode = InOwnerGameMode;
	RequiredPlayersPerTeam = InRequiredPlayersPerTeam;
	ClassSelectionDuration = InClassSelectionDuration;

	if (ADominationGameState* DominationGameState = GetDominationGameState())
	{
		DominationGameState->SetDominationPhase(EDominationMatchPhase::WaitingForPlayers);
	}
}

void UDominationMatchStartManager::Shutdown()
{
	if (ADominationGameMode* GameMode = OwnerGameMode.Get())
	{
		GameMode->GetWorldTimerManager().ClearTimer(ClassSelectionTimerHandle);
	}

	InitialParticipants.Reset();
	OwnerGameMode.Reset();
	OnInitialPlayersSpawned.Clear();
}

void UDominationMatchStartManager::NotifyPlayerLoaded()
{
	const ADominationGameState* DominationGameState = GetDominationGameState();
	if (!IsValid(DominationGameState)
		|| DominationGameState->GetDominationPhase() != EDominationMatchPhase::WaitingForPlayers
		|| !CaptureInitialParticipants())
	{
		return;
	}

	BeginClassSelection();
}

void UDominationMatchStartManager::RequestCombatClass(
	ADominationPlayerController* PlayerController,
	const EDominationCombatClass RequestedClass)
{
	const ADominationGameState* DominationGameState = GetDominationGameState();
	if (!IsValid(DominationGameState)
		|| DominationGameState->GetDominationPhase() != EDominationMatchPhase::ClassSelecting
		|| !IsInitialParticipant(PlayerController))
	{
		return;
	}

	ADominationPlayerState* DominationPlayerState =
		PlayerController->GetPlayerState<ADominationPlayerState>();
	if (!IsValid(DominationPlayerState)
		|| !DominationPlayerState->TryConfirmCombatClass(RequestedClass))
	{
		return;
	}

	if (AreAllParticipantsClassSelected())
	{
		CompleteClassSelection();
	}
}

void UDominationMatchStartManager::GetInitialParticipantPlayerStates(
	TArray<ADominationPlayerState*>& OutPlayerStates) const
{
	OutPlayerStates.Reset();
	for (const TWeakObjectPtr<ADominationPlayerController>& Participant : InitialParticipants)
	{
		if (ADominationPlayerController* PlayerController = Participant.Get())
		{
			if (ADominationPlayerState* PlayerState =
				PlayerController->GetPlayerState<ADominationPlayerState>())
			{
				OutPlayerStates.Add(PlayerState);
			}
		}
	}
}

bool UDominationMatchStartManager::CaptureInitialParticipants()
{
	ADominationGameMode* GameMode = OwnerGameMode.Get();
	UWorld* World = IsValid(GameMode) ? GameMode->GetWorld() : nullptr;
	if (!IsValid(World))
	{
		return false;
	}

	TArray<ADominationPlayerController*> TeamAPlayers;
	TArray<ADominationPlayerController*> TeamBPlayers;
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		ADominationPlayerController* PlayerController =
			Cast<ADominationPlayerController>(Iterator->Get());
		const ADominationPlayerState* PlayerState = IsValid(PlayerController)
			? PlayerController->GetPlayerState<ADominationPlayerState>()
			: nullptr;
		if (!IsValid(PlayerState) || !PlayerState->IsLoadedIntoMatch())
		{
			return false;
		}

		switch (PlayerState->GetTeam())
		{
		case EGameTeam::TeamA:
			TeamAPlayers.Add(PlayerController);
			break;
		case EGameTeam::TeamB:
			TeamBPlayers.Add(PlayerController);
			break;
		default:
			return false;
		}
	}

	if (TeamAPlayers.Num() != RequiredPlayersPerTeam
		|| TeamBPlayers.Num() != RequiredPlayersPerTeam)
	{
		return false;
	}

	InitialParticipants.Reset(RequiredPlayersPerTeam * 2);
	for (ADominationPlayerController* PlayerController : TeamAPlayers)
	{
		InitialParticipants.Add(PlayerController);
	}
	for (ADominationPlayerController* PlayerController : TeamBPlayers)
	{
		InitialParticipants.Add(PlayerController);
	}

	return true;
}

bool UDominationMatchStartManager::IsInitialParticipant(
	const ADominationPlayerController* PlayerController) const
{
	if (!IsValid(PlayerController))
	{
		return false;
	}

	for (const TWeakObjectPtr<ADominationPlayerController>& Participant : InitialParticipants)
	{
		if (Participant.Get() == PlayerController)
		{
			return true;
		}
	}

	return false;
}

bool UDominationMatchStartManager::AreAllParticipantsClassSelected() const
{
	for (const TWeakObjectPtr<ADominationPlayerController>& Participant : InitialParticipants)
	{
		const ADominationPlayerController* PlayerController = Participant.Get();
		const ADominationPlayerState* PlayerState = IsValid(PlayerController)
			? PlayerController->GetPlayerState<ADominationPlayerState>()
			: nullptr;
		if (!IsValid(PlayerState) || !PlayerState->HasSelectedCombatClass())
		{
			return false;
		}
	}

	return !InitialParticipants.IsEmpty();
}

void UDominationMatchStartManager::BeginClassSelection()
{
	ADominationGameMode* GameMode = OwnerGameMode.Get();
	ADominationGameState* DominationGameState = GetDominationGameState();
	if (!IsValid(GameMode) || !IsValid(DominationGameState))
	{
		return;
	}

	DominationGameState->SetDominationPhase(
		EDominationMatchPhase::ClassSelecting,
		ClassSelectionDuration);

	for (const TWeakObjectPtr<ADominationPlayerController>& Participant : InitialParticipants)
	{
		if (ADominationPlayerController* PlayerController = Participant.Get())
		{
			PlayerController->ClientHideLoadingScreen();
		}
	}

	GameMode->GetWorldTimerManager().SetTimer(
		ClassSelectionTimerHandle,
		this,
		&ThisClass::HandleClassSelectionTimeout,
		ClassSelectionDuration,
		false);
}

void UDominationMatchStartManager::HandleClassSelectionTimeout()
{
	for (const TWeakObjectPtr<ADominationPlayerController>& Participant : InitialParticipants)
	{
		ADominationPlayerController* PlayerController = Participant.Get();
		ADominationPlayerState* PlayerState = IsValid(PlayerController)
			? PlayerController->GetPlayerState<ADominationPlayerState>()
			: nullptr;
		if (IsValid(PlayerState) && !PlayerState->HasSelectedCombatClass())
		{
			PlayerState->TryConfirmCombatClass(EDominationCombatClass::Trooper);
		}
	}

	CompleteClassSelection();
}

void UDominationMatchStartManager::CompleteClassSelection()
{
	ADominationGameMode* GameMode = OwnerGameMode.Get();
	ADominationGameState* DominationGameState = GetDominationGameState();
	if (!IsValid(GameMode) || !IsValid(DominationGameState))
	{
		return;
	}

	GameMode->GetWorldTimerManager().ClearTimer(ClassSelectionTimerHandle);
	if (!SpawnInitialParticipants())
	{
		UE_LOG(LogDominationMatchStart, Error,
			TEXT("Domination match start stopped because an initial player could not be spawned with its class loadout."));
		return;
	}

	GameMode->StartMatch();
	DominationGameState->SetDominationPhase(EDominationMatchPhase::InProgress);
	OnInitialPlayersSpawned.Broadcast();
}

bool UDominationMatchStartManager::SpawnInitialParticipants()
{
	ADominationGameMode* GameMode = OwnerGameMode.Get();
	UDominationLoadoutManager* LoadoutManager = IsValid(GameMode)
		? GameMode->GetLoadoutManager()
		: nullptr;
	if (!IsValid(GameMode) || !IsValid(LoadoutManager))
	{
		return false;
	}

	for (const TWeakObjectPtr<ADominationPlayerController>& Participant : InitialParticipants)
	{
		ADominationPlayerController* PlayerController = Participant.Get();
		ADominationPlayerState* PlayerState = IsValid(PlayerController)
			? PlayerController->GetPlayerState<ADominationPlayerState>()
			: nullptr;
		if (!IsValid(PlayerController) || !IsValid(PlayerState))
		{
			return false;
		}

		if (!GameMode->RestartPlayerAtObjective(
			PlayerController,
			EDominationPointId::C))
		{
			return false;
		}

		APawn* SpawnedPawn = PlayerController->GetPawn();
		if (!IsValid(SpawnedPawn)
			|| SpawnedPawn->GetController() != PlayerController
			|| !LoadoutManager->ApplyCombatClassLoadout(PlayerState, SpawnedPawn))
		{
			return false;
		}
	}

	return true;
}

ADominationGameState* UDominationMatchStartManager::GetDominationGameState() const
{
	const ADominationGameMode* GameMode = OwnerGameMode.Get();
	return IsValid(GameMode) ? GameMode->GetGameState<ADominationGameState>() : nullptr;
}
