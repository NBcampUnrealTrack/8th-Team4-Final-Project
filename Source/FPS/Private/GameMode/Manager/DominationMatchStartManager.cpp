#include "GameMode/Manager/DominationMatchStartManager.h"

#include "Engine/World.h"
#include "GameMode/DominationGameMode.h"
#include "GameMode/State/DominationGameState.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "GameMode/Manager/DominationLoadoutManager.h"
#include "GameMode/Manager/TeamPlayerManager.h"
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
	InOwnerGameMode->GetLoadoutManager()->OnCombatAssetsReady.AddUObject(
		this, &ThisClass::HandleCombatAssetsReady);
	return true;
}

void UDominationMatchStartManager::Shutdown()
{
	if (ADominationGameMode* GameMode = GetOwnerGameMode())
	{
		GameMode->GetLoadoutManager()->OnCombatAssetsReady.RemoveAll(this);
	}
	if (FTimerManager* TimerManager = GetTimerManager())
	{
		TimerManager->ClearTimer(ClassSelectionTimerHandle);
	}

	ResetClientCombatAssetPreparation();
	InitialParticipants.Reset();
	SpawnRetryCount = 0;
	bStartSuspended = false;
	bWaitingForCombatAssets = false;
	OnInitialPlayersSpawned.Clear();
	Super::Shutdown();
}

void UDominationMatchStartManager::NotifyPlayerLoaded()
{
	const ADominationGameState* DominationGameState = GetDominationGameState();
	if (bStartSuspended || !IsValid(DominationGameState)
		|| DominationGameState->GetDominationPhase() != EDominationMatchPhase::WaitingForPlayers
		|| !CaptureInitialParticipants())
	{
		return;
	}

	BeginClassSelection();
}

void UDominationMatchStartManager::NotifyPlayerLeaving(APlayerController* PlayerController)
{
	ADominationGameState* GameState = GetDominationGameState();
	if (!IsValid(GameState) || bStartSuspended)
	{
		return;
	}

	const EDominationMatchPhase Phase = GameState->GetDominationPhase();
	if (Phase != EDominationMatchPhase::WaitingForPlayers
		&& Phase != EDominationMatchPhase::ClassSelecting)
	{
		return;
	}

	InitialParticipants.RemoveAll(
		[PlayerController](const TWeakObjectPtr<ADominationPlayerController>& Participant)
		{
			return !Participant.IsValid() || Participant.Get() == PlayerController;
		});
	PendingClientCombatAssets.Remove(Cast<ADominationPlayerController>(PlayerController));

	if (Phase == EDominationMatchPhase::ClassSelecting && HasRequiredParticipants())
	{
		if (AreAllParticipantsClassSelected())
		{
			CompleteClassSelection();
		}
		return;
	}

	ResetStartAttempt();
	// Logout 이전에는 World의 컨트롤러 목록에 이탈자가 아직 남아 있다.
	if (CaptureInitialParticipants(PlayerController))
	{
		BeginClassSelection();
	}
}

void UDominationMatchStartManager::RetryMatchStart()
{
	const ADominationGameState* GameState = GetDominationGameState();
	if (IsValid(GameState)
		&& GameState->GetDominationPhase() == EDominationMatchPhase::WaitingForPlayers)
	{
		bStartSuspended = false;
		NotifyPlayerLoaded();
	}
}

void UDominationMatchStartManager::RequestCombatClass(
	ADominationPlayerController* PlayerController,
	const EDominationCombatClass)
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
		|| !DominationPlayerState->HasPreviewCombatClass()
		|| !DominationPlayerState->TryConfirmCombatClass(
			DominationPlayerState->GetPreviewCombatClass()))
	{
		return;
	}

	GetOwnerGameMode()->GetLoadoutManager()->EnsureCombatAssets(DominationPlayerState->GetCombatClass());

	if (AreAllParticipantsClassSelected())
	{
		CompleteClassSelection();
	}
}

void UDominationMatchStartManager::RequestPreviewCombatClass(
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
	if (!IsValid(DominationPlayerState))
	{
		return;
	}

	DominationPlayerState->TrySetPreviewCombatClass(RequestedClass);
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

bool UDominationMatchStartManager::CaptureInitialParticipants(
	const APlayerController* ExcludedPlayer)
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
		if (Iterator->Get() == ExcludedPlayer)
		{
			continue;
		}
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

bool UDominationMatchStartManager::HasRequiredParticipants() const
{
	int32 TeamACount = 0;
	int32 TeamBCount = 0;
	for (const TWeakObjectPtr<ADominationPlayerController>& Participant : InitialParticipants)
	{
		const ADominationPlayerState* PlayerState = Participant.IsValid()
			? Participant->GetPlayerState<ADominationPlayerState>() : nullptr;
		if (!IsValid(PlayerState))
		{
			return false;
		}
		TeamACount += PlayerState->GetTeam() == EGameTeam::TeamA ? 1 : 0;
		TeamBCount += PlayerState->GetTeam() == EGameTeam::TeamB ? 1 : 0;
	}
	return TeamACount + TeamBCount >= MinimumPlayersToStart
		&& TeamACount >= MinimumPlayersPerTeam && TeamBCount >= MinimumPlayersPerTeam;
}

void UDominationMatchStartManager::CleanupInitialSpawns()
{
	for (const TWeakObjectPtr<ADominationPlayerController>& Participant : InitialParticipants)
	{
		if (ADominationPlayerController* PlayerController = Participant.Get())
		{
			UTeamPlayerManager::RemovePlayerPawn(PlayerController);
			if (ADominationPlayerState* PlayerState = PlayerController->GetPlayerState<ADominationPlayerState>())
			{
				PlayerState->SetParticipationState(EPlayerParticipationState::WaitingForMatchStart);
			}
		}
	}
}

void UDominationMatchStartManager::ResetStartAttempt()
{
	GetTimerManager()->ClearTimer(ClassSelectionTimerHandle);
	ResetClientCombatAssetPreparation();
	CleanupInitialSpawns();
	InitialParticipants.Reset();
	SpawnRetryCount = 0;
	bWaitingForCombatAssets = false;
	GetDominationGameState()->SetDominationPhase(EDominationMatchPhase::WaitingForPlayers);
}

void UDominationMatchStartManager::SuspendMatchStart(const TCHAR* Reason)
{
	ResetStartAttempt();
	bStartSuspended = true;
	UE_LOG(LogDominationMatchStart, Error,
		TEXT("Domination start suspended: %s Fix the cause and call RetryDominationMatchStart()."), Reason);
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
			ADominationPlayerState* PlayerState = PlayerController->GetPlayerState<ADominationPlayerState>();
			PlayerState->ResetCombatClassSelection();
			PlayerState->SetParticipationState(EPlayerParticipationState::WaitingForMatchStart);
			PlayerController->ClientFinishLoadingScreen();
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
			const UDominationClassSettings* ClassSettings =
				GetDefault<UDominationClassSettings>();
			if (IsValid(ClassSettings))
			{
				// LOCK IN을 누르지 않아도 임시 선택이 그대로 확정된다.
				const EDominationCombatClass FallbackClass =
					PlayerState->HasPreviewCombatClass()
						? PlayerState->GetPreviewCombatClass()
						: ClassSettings->DefaultCombatClass;
				PlayerState->TryConfirmCombatClass(FallbackClass);
			}
		}
	}

	CompleteClassSelection();
}

void UDominationMatchStartManager::CompleteClassSelection()
{
	ADominationGameMode* GameMode = GetOwnerGameMode();
	ADominationGameState* GameState = GetDominationGameState();
	if (bStartSuspended || !IsValid(GameMode) || !IsValid(GameState)
		|| GameState->GetDominationPhase() != EDominationMatchPhase::ClassSelecting)
	{
		return;
	}

	GetTimerManager()->ClearTimer(ClassSelectionTimerHandle);
	const EInitialSpawnResult Result = SpawnInitialParticipants();
	bWaitingForCombatAssets = Result == EInitialSpawnResult::AssetsLoading;
	if (bWaitingForCombatAssets)
	{
		return;
	}
	if (Result == EInitialSpawnResult::InvalidConfiguration)
	{
		SuspendMatchStart(TEXT("An initial participant's class or loadout is invalid."));
		return;
	}
	if (Result == EInitialSpawnResult::TemporarilyBlocked)
	{
		if (SpawnRetryCount >= MaxSpawnRetries)
		{
			SuspendMatchStart(TEXT("Initial spawns remained blocked after the retry limit."));
			return;
		}
		CleanupInitialSpawns();
		++SpawnRetryCount;
		GetTimerManager()->SetTimer(ClassSelectionTimerHandle, this,
			&ThisClass::CompleteClassSelection, SpawnRetryDelay, false);
		UE_LOG(LogDominationMatchStart, Warning, TEXT("Retrying initial spawns (%d/%d)."),
			SpawnRetryCount, MaxSpawnRetries);
		return;
	}

	SpawnRetryCount = 0;
	GameMode->StartMatch();
	GameState->SetDominationPhase(EDominationMatchPhase::InProgress);
	OnInitialPlayersSpawned.Broadcast();
}

void UDominationMatchStartManager::HandleCombatAssetsReady(EDominationCombatClass, bool)
{
	if (bWaitingForCombatAssets)
	{
		CompleteClassSelection();
	}
}

bool UDominationMatchStartManager::EnsureClientCombatAssets()
{
	if (!bClientCombatAssetsRequested)
	{
		bClientCombatAssetsRequested = true;
		ClientCombatAssetGeneration = FGuid::NewGuid();
		TArray<EDominationCombatClass> CombatClasses;
		for (const TWeakObjectPtr<ADominationPlayerController>& Participant : InitialParticipants)
		{
			ADominationPlayerController* PlayerController = Participant.Get();
			CombatClasses.AddUnique(PlayerController->GetPlayerState<ADominationPlayerState>()->GetCombatClass());
			if (!PlayerController->IsLocalController())
			{
				PendingClientCombatAssets.Add(Participant);
			}
		}

		UE_LOG(LogDominationMatchStart, Log,
			TEXT("[DominationTravel] Selected roster client preparation: Generation=%s Classes=%d RemoteClients=%d"),
			*ClientCombatAssetGeneration.ToString(), CombatClasses.Num(), PendingClientCombatAssets.Num());
		// 참가자 집합을 먼저 확정한 뒤 RPC를 보낸다. 리슨 호스트는 서버 로드 핸들을 공유한다.
		for (const TWeakObjectPtr<ADominationPlayerController>& Participant : PendingClientCombatAssets)
		{
			Participant->ClientPrepareInitialCombatAssets(ClientCombatAssetGeneration, CombatClasses);
		}
	}
	return PendingClientCombatAssets.IsEmpty();
}

void UDominationMatchStartManager::NotifyClientInitialCombatAssetsReady(
	ADominationPlayerController* PlayerController, const FGuid Generation, const bool bSucceeded)
{
	const ADominationGameState* GameState = GetDominationGameState();
	if (bStartSuspended || !bClientCombatAssetsRequested || Generation != ClientCombatAssetGeneration
		|| !IsValid(GameState) || GameState->GetDominationPhase() != EDominationMatchPhase::ClassSelecting
		|| !IsInitialParticipant(PlayerController) || PendingClientCombatAssets.Remove(PlayerController) == 0)
	{
		return;
	}
	UE_LOG(LogDominationMatchStart, Log,
		TEXT("[DominationTravel] Client selected roster acknowledgement: Player=%s Success=%d Remaining=%d"),
		*GetNameSafe(PlayerController), bSucceeded, PendingClientCombatAssets.Num());
	if (!bSucceeded)
	{
		SuspendMatchStart(TEXT("An initial client could not load the selected combat roster."));
		return;
	}
	if (PendingClientCombatAssets.IsEmpty())
	{
		CompleteClassSelection();
	}
}

void UDominationMatchStartManager::ResetClientCombatAssetPreparation()
{
	const FGuid CancelledGeneration = ClientCombatAssetGeneration;
	ClientCombatAssetGeneration.Invalidate();
	bClientCombatAssetsRequested = false;
	PendingClientCombatAssets.Reset();
	if (!CancelledGeneration.IsValid())
	{
		return;
	}
	for (const TWeakObjectPtr<ADominationPlayerController>& Participant : InitialParticipants)
	{
		if (Participant.IsValid() && !Participant->IsLocalController())
		{
			Participant->ClientCancelInitialCombatAssets(CancelledGeneration);
		}
	}
}

UDominationMatchStartManager::EInitialSpawnResult UDominationMatchStartManager::SpawnInitialParticipants()
{
	ADominationGameMode* GameMode = GetOwnerGameMode();
	UDominationLoadoutManager* LoadoutManager = GameMode->GetLoadoutManager();
	if (!IsValid(LoadoutManager) || !AreAllParticipantsClassSelected()
		|| !GetDefault<UDominationClassSettings>()->HasValidDefinitions()
		|| !LoadoutManager->HasRequiredWeaponRegistryEntries())
	{
		return EInitialSpawnResult::InvalidConfiguration;
	}

	bool bAssetsLoading = false;
	for (const TWeakObjectPtr<ADominationPlayerController>& Participant : InitialParticipants)
	{
		const EDominationCombatAssetStatus Status = LoadoutManager->EnsureCombatAssets(
			Participant->GetPlayerState<ADominationPlayerState>()->GetCombatClass());
		if (Status == EDominationCombatAssetStatus::Failed)
		{
			return EInitialSpawnResult::InvalidConfiguration;
		}
		bAssetsLoading |= Status == EDominationCombatAssetStatus::Loading;
	}
	const bool bClientsReady = EnsureClientCombatAssets();
	if (bAssetsLoading || !bClientsReady)
	{
		return EInitialSpawnResult::AssetsLoading;
	}

	// 모든 인벤토리를 먼저 준비하고, 모든 Pawn이 성공한 뒤 참가 상태를 확정한다.
	for (const TWeakObjectPtr<ADominationPlayerController>& Participant : InitialParticipants)
	{
		if (!LoadoutManager->InitializeCombatClassInventory(
			Participant->GetPlayerState<ADominationPlayerState>()))
		{
			return EInitialSpawnResult::InvalidConfiguration;
		}
	}
	for (const TWeakObjectPtr<ADominationPlayerController>& Participant : InitialParticipants)
	{
		ADominationPlayerController* PlayerController = Participant.Get();
		if (!GameMode->RestartPlayerAtObjective(PlayerController, DominationRules::InitialPointId))
		{
			return EInitialSpawnResult::TemporarilyBlocked;
		}
		if (!GameMode->PrepareCombatPawn(PlayerController))
		{
			return EInitialSpawnResult::InvalidConfiguration;
		}
	}
	for (const TWeakObjectPtr<ADominationPlayerController>& Participant : InitialParticipants)
	{
		GameMode->ActivateCombatPlayer(Participant.Get());
	}
	return EInitialSpawnResult::Succeeded;
}
