#include "GameMode/Manager/DominationMatchStartManager.h"

#include "Engine/World.h"
#include "GameMode/DominationGameMode.h"
#include "GameMode/State/DominationGameState.h"
#include "GameMode/Manager/DominationLoadoutManager.h"
#include "GameMode/Types/TeamTypes.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Player/DominationPlayerController.h"
#include "Player/DominationPlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogDominationMatchStart, Log, All);

bool UDominationMatchStartManager::Initialize(
	ADominationGameMode* InOwnerGameMode,
	const int32 InMinimumPlayersToStart,
	const int32 InMinimumPlayersPerTeam,
	const float InClassSelectionDuration)
{
	if (!InitializeManager(InOwnerGameMode))
	{
		return false;
	}

	MinimumPlayersToStart = FMath::Max(2, InMinimumPlayersToStart);
	MinimumPlayersPerTeam = FMath::Max(1, InMinimumPlayersPerTeam);
	ClassSelectionDuration = InClassSelectionDuration;

	ADominationGameState* DominationGameState = GetDominationGameState();
	if (!IsValid(DominationGameState))
	{
		Shutdown();
		return false;
	}

	DominationGameState->SetDominationPhase(
		EDominationMatchPhase::WaitingForPlayers);
	return true;
}

void UDominationMatchStartManager::Shutdown()
{
	if (FTimerManager* TimerManager = GetTimerManager())
	{
		TimerManager->ClearTimer(ClassSelectionTimerHandle);
	}

	InitialParticipants.Reset();
	OnInitialPlayersSpawned.Clear();
	Super::Shutdown();
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
	UWorld* World = GetWorld();
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

	const int32 TotalPlayerCount = TeamAPlayers.Num() + TeamBPlayers.Num();
	if (TotalPlayerCount < MinimumPlayersToStart
		|| TeamAPlayers.Num() < MinimumPlayersPerTeam
		|| TeamBPlayers.Num() < MinimumPlayersPerTeam)
	{
		return false;
	}

	InitialParticipants.Reset(TotalPlayerCount);
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
	ADominationGameState* DominationGameState = GetDominationGameState();
	FTimerManager* TimerManager = GetTimerManager();
	if (!IsValid(DominationGameState) || TimerManager == nullptr)
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

	TimerManager->SetTimer(
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
	ADominationGameMode* GameMode = GetOwnerGameMode();
	ADominationGameState* DominationGameState = GetDominationGameState();
	FTimerManager* TimerManager = GetTimerManager();
	if (!IsValid(GameMode)
		|| !IsValid(DominationGameState)
		|| TimerManager == nullptr)
	{
		return;
	}

	TimerManager->ClearTimer(ClassSelectionTimerHandle);
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
	ADominationGameMode* GameMode = GetOwnerGameMode();
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

		if (!LoadoutManager->InitializeCombatClassInventory(PlayerState)
			|| !GameMode->RestartPlayerAtObjective(
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
