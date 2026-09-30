#include "GameMode/LobbyGameMode.h"

#include "Algo/RandomShuffle.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameMode/State/LobbyGameState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Player/ShooterPlayerController.h"
#include "Player/ShooterPlayerState.h"
#include "Session/SessionSubSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogLobbyGameMode, Log, All);

ALobbyGameMode::ALobbyGameMode()
{
	GameStateClass = ALobbyGameState::StaticClass();
	PlayerControllerClass = AShooterPlayerController::StaticClass();
	PlayerStateClass = AShooterPlayerState::StaticClass();
}

void ALobbyGameMode::InitGameState()
{
	Super::InitGameState();

	ALobbyGameState* LobbyGameState = GetGameState<ALobbyGameState>();
	const UGameInstance* GameInstance = GetGameInstance();
	if (!IsValid(LobbyGameState) || !IsValid(GameInstance))
	{
		return;
	}

	const USessionSubSystem* SessionSubsystem = GameInstance->GetSubsystem<USessionSubSystem>();
	if (!IsValid(SessionSubsystem))
	{
		return;
	}

	FLobbyRoomSettings NewSettings;
	NewSettings.RoomName = SessionSubsystem->GetRoomName();
	NewSettings.MaxPlayers = SessionSubsystem->GetMaxPlayers();
	NewSettings.MaxPlayersPerTeam = MaxPlayersPerTeam;
	NewSettings.MinimumPlayersToStart = MinimumPlayersToStart;
	LobbyGameState->SetRoomSettings(NewSettings);
}

void ALobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	AssignHostPlayerIfNeeded(NewPlayer);
	NotifyLobbyPlayerListChanged();
}

void ALobbyGameMode::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();
	RebuildLobbyStateAfterSeamlessTravel();
}

void ALobbyGameMode::Logout(AController* Exiting)
{
	const APlayerState* ExitingPlayerState = Exiting ? Exiting->PlayerState : nullptr;
	Super::Logout(Exiting);
	ReassignHostPlayerIfNeeded(ExitingPlayerState);
	NotifyLobbyPlayerListChanged();
}

bool ALobbyGameMode::RequestStartRoomGame(
	APlayerController* RequestingPlayer,
	const FString& GameMapName)
{
	if (!IsHostPlayerState(RequestingPlayer ? RequestingPlayer->PlayerState : nullptr))
	{
		UE_LOG(LogLobbyGameMode, Warning, TEXT("Start room game blocked: requester is not the host."));
		return false;
	}

	if (bStartRoomGamePending || bTravelingToGameMap)
	{
		return false;
	}

	// Preserve existing callers that still provide a map string, then use GameState
	// as the single source of truth for the rest of the start sequence.
	if (!GameMapName.IsEmpty())
	{
		ELobbyMatchMode RequestedMatchMode = ELobbyMatchMode::Domination;
		if (!TryResolveMatchModeFromMapName(GameMapName, RequestedMatchMode)
			|| !RequestLobbyMatchMode(RequestingPlayer, RequestedMatchMode))
		{
			UE_LOG(LogLobbyGameMode, Warning,
				TEXT("Start room game blocked: unsupported map was requested. Map=%s"),
				*GameMapName);
			return false;
		}
	}

	if (!CanStartRoomGame())
	{
		int32 TeamACount = 0;
		int32 TeamBCount = 0;
		int32 UnassignedCount = 0;
		GetTeamAssignmentCounts(TeamACount, TeamBCount, UnassignedCount);

		UE_LOG(LogLobbyGameMode, Warning,
			TEXT("Start room game blocked: readiness or minimum-player rule failed. TeamA=%d TeamB=%d Unassigned=%d MinimumTotal=%d MinimumPerTeam=%d"),
			TeamACount,
			TeamBCount,
			UnassignedCount,
			MinimumPlayersToStart,
			MinimumPlayersPerTeam);
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	USessionSubSystem* SessionSubsystem = GameInstance
		? GameInstance->GetSubsystem<USessionSubSystem>()
		: nullptr;
	if (!SessionSubsystem)
	{
		UE_LOG(LogLobbyGameMode, Warning, TEXT("Start room game blocked: SessionSubsystem is unavailable."));
		return false;
	}

	const ELobbyMatchMode SelectedMatchMode = GetSelectedMatchMode();
	const FString SelectedMapName = GetMapNameForMatchMode(SelectedMatchMode);
	const FString SelectedMatchType = GetMatchTypeForMatchMode(SelectedMatchMode);
	if (SelectedMapName.IsEmpty() || SelectedMatchType.IsEmpty())
	{
		UE_LOG(LogLobbyGameMode, Error,
			TEXT("Start room game blocked: selected mode has no valid map configuration. Mode=%d"),
			static_cast<uint8>(SelectedMatchMode));
		return false;
	}

	// Start-time finalization only fills players who did not select a team.
	// Explicit 2v3, 4v3, and other asymmetric selections are not rebalanced.
	if (!FinalizeTeamsForMatchStart())
	{
		UE_LOG(LogLobbyGameMode, Warning, TEXT("Start room game blocked: teams could not be finalized."));
		return false;
	}

	PendingGameMapPath = ResolveMapPathFromName(SelectedMapName);
	bStartRoomGamePending = true;
	NotifyLobbyPlayerListChanged();
	SessionSubsystem->OnStartRoomGameComplete.AddUniqueDynamic(
		this,
		&ThisClass::HandleStartRoomGameComplete);
	SessionSubsystem->StartRoomGameWithSettings(SelectedMapName, SelectedMatchType);
	return true;
}

bool ALobbyGameMode::CanStartRoomGame() const
{
	return IsSupportedMatchMode(GetSelectedMatchMode())
		&& AreAllClientsReady()
		&& HasMinimumPlayersToStart()
		&& CanFinalizeTeamsForMatchStart();
}

FLobbyStartState ALobbyGameMode::BuildStartState() const
{
	FLobbyStartState Result;
	CountClientReady(Result.ReadyCount, Result.ReadyRequired);

	// 위에서부터 먼저 걸리는 사유 하나만 남겨 순서가 화면 문구의 우선순위
	if (bStartRoomGamePending || bTravelingToGameMap)
	{
		Result.Blocker = ELobbyStartBlocker::Starting;
	}
	else if (!HasMinimumPlayersToStart())
	{
		Result.Blocker = ELobbyStartBlocker::NotEnoughPlayers;
	}
	else if (!CanFinalizeTeamsForMatchStart())
	{
		Result.Blocker = ELobbyStartBlocker::TeamsCannotBeFinalized;
	}
	else if (Result.ReadyCount < Result.ReadyRequired)
	{
		Result.Blocker = ELobbyStartBlocker::WaitingForReady;
	}
	else
	{
		Result.Blocker = ELobbyStartBlocker::None;
	}

	return Result;
}

bool ALobbyGameMode::RequestLobbyMatchMode(
	APlayerController* RequestingPlayer,
	const ELobbyMatchMode RequestedMatchMode)
{
	if (!IsHostPlayerState(RequestingPlayer ? RequestingPlayer->PlayerState : nullptr))
	{
		UE_LOG(LogLobbyGameMode, Warning, TEXT("Game mode selection blocked: requester is not the host."));
		return false;
	}

	if (bStartRoomGamePending
		|| bTravelingToGameMap
		|| !IsSupportedMatchMode(RequestedMatchMode))
	{
		return false;
	}

	ALobbyGameState* LobbyGameState = GetGameState<ALobbyGameState>();
	if (!IsValid(LobbyGameState))
	{
		return false;
	}

	LobbyGameState->SetSelectedMatchMode(RequestedMatchMode);

	// Session advertising is secondary to replicated lobby state. Failure here
	// must not roll back a valid host selection.
	if (!UpdateSessionAdvertisementForMatchMode(RequestedMatchMode))
	{
		UE_LOG(LogLobbyGameMode, Verbose,
			TEXT("Lobby mode changed, but room advertisement was not updated."));
	}

	return true;
}

ELobbyMatchMode ALobbyGameMode::GetSelectedMatchMode() const
{
	const ALobbyGameState* LobbyGameState = GetGameState<ALobbyGameState>();
	return IsValid(LobbyGameState)
		? LobbyGameState->GetSelectedMatchMode()
		: ELobbyMatchMode::Domination;
}

FString ALobbyGameMode::GetSelectedGameMapName() const
{
	return GetMapNameForMatchMode(GetSelectedMatchMode());
}

void ALobbyGameMode::AssignHostPlayerIfNeeded(APlayerController* NewPlayer)
{
	if (HostPlayerState.IsValid() || !IsValid(NewPlayer) || !IsValid(NewPlayer->PlayerState))
	{
		return;
	}

	HostPlayerState = NewPlayer->PlayerState;

	if (ALobbyGameState* LobbyGameState = GetGameState<ALobbyGameState>())
	{
		LobbyGameState->SetHostPlayerState(HostPlayerState.Get());
	}

	// Keep the initial room advertisement aligned with the default Domination selection.
	UpdateSessionAdvertisementForMatchMode(GetSelectedMatchMode());
}

void ALobbyGameMode::RebuildLobbyStateAfterSeamlessTravel()
{
	HostPlayerState.Reset();

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	APlayerState* FallbackHostPlayerState = nullptr;
	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		APlayerController* PlayerController = Iterator->Get();
		AShooterPlayerState* ShooterPlayerState = IsValid(PlayerController)
			? PlayerController->GetPlayerState<AShooterPlayerState>()
			: nullptr;
		if (!IsValid(ShooterPlayerState))
		{
			continue;
		}

		// A new lobby cycle must not inherit ready/load state from the prior match.
		ShooterPlayerState->SetLobbyReady(false);
		ShooterPlayerState->SetLoadedIntoMatch(false);

		if (!IsValid(FallbackHostPlayerState))
		{
			FallbackHostPlayerState = ShooterPlayerState;
		}

		// The local controller on a listen server is the room host.
		if (PlayerController->IsLocalController())
		{
			HostPlayerState = ShooterPlayerState;
		}
	}

	// Dedicated servers do not have a local controller, so retain the existing
	// fallback policy and use the first valid player.
	if (!HostPlayerState.IsValid() && IsValid(FallbackHostPlayerState))
	{
		HostPlayerState = FallbackHostPlayerState;
	}

	if (ALobbyGameState* LobbyGameState = GetGameState<ALobbyGameState>())
	{
		LobbyGameState->SetHostPlayerState(HostPlayerState.Get());
	}

	if (!HostPlayerState.IsValid())
	{
		UE_LOG(LogLobbyGameMode, Warning, TEXT("Lobby seamless travel completed without a valid host player."));
	}
	else
	{
		UpdateSessionAdvertisementForMatchMode(GetSelectedMatchMode());
	}

	NotifyLobbyPlayerListChanged();
}

void ALobbyGameMode::ReassignHostPlayerIfNeeded(const APlayerState* ExitingPlayerState)
{
	if (!HostPlayerState.IsValid() || HostPlayerState.Get() != ExitingPlayerState)
	{
		return;
	}

	HostPlayerState.Reset();

	const AGameStateBase* CurrentGameState = GameState;
	if (!IsValid(CurrentGameState))
	{
		return;
	}

	for (APlayerState* PlayerState : CurrentGameState->PlayerArray)
	{
		if (IsValid(PlayerState) && PlayerState != ExitingPlayerState)
		{
			HostPlayerState = PlayerState;
			break;
		}
	}

	// 새 호스트를 못 찾으면 nullptr 을 넣어 나간 플레이어를 가리키지 않게 함
	if (ALobbyGameState* LobbyGameState = GetGameState<ALobbyGameState>())
	{
		LobbyGameState->SetHostPlayerState(HostPlayerState.Get());
	}
}

bool ALobbyGameMode::RequestLobbyTeam(
	APlayerController* RequestingPlayer,
	const EGameTeam RequestedTeam)
{
	if (bStartRoomGamePending || bTravelingToGameMap)
	{
		return false;
	}

	AShooterPlayerState* ShooterPlayerState = IsValid(RequestingPlayer)
		? RequestingPlayer->GetPlayerState<AShooterPlayerState>()
		: nullptr;
	if (!IsValid(ShooterPlayerState))
	{
		UE_LOG(LogLobbyGameMode, Warning,
			TEXT("Team change blocked: %s has no ShooterPlayerState."),
			*GetNameSafe(RequestingPlayer));
		return false;
	}

	// None and invalid enum values are internal/invalid states, not player-facing choices.
	if (RequestedTeam != EGameTeam::TeamA && RequestedTeam != EGameTeam::TeamB)
	{
		UE_LOG(LogLobbyGameMode, Warning, TEXT("Team change blocked: only TeamA or TeamB is selectable."));
		return false;
	}

	if (ShooterPlayerState->GetTeam() == RequestedTeam)
	{
		return true;
	}

	if (CountTeamPlayers(RequestedTeam) >= MaxPlayersPerTeam)
	{
		UE_LOG(LogLobbyGameMode, Warning,
			TEXT("Team change blocked: requested team is full (%d/%d)."),
			CountTeamPlayers(RequestedTeam),
			MaxPlayersPerTeam);
		return false;
	}

	ShooterPlayerState->SetTeam(RequestedTeam);
	NotifyLobbyPlayerListChanged();
	return true;
}

int32 ALobbyGameMode::CountTeamPlayers(const EGameTeam Team) const
{
	const AGameStateBase* CurrentGameState = GameState;
	if (!IsValid(CurrentGameState))
	{
		return 0;
	}

	int32 PlayerCount = 0;
	for (const APlayerState* PlayerState : CurrentGameState->PlayerArray)
	{
		const AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState);
		if (IsValid(ShooterPlayerState) && ShooterPlayerState->GetTeam() == Team)
		{
			++PlayerCount;
		}
	}

	return PlayerCount;
}

bool ALobbyGameMode::FinalizeTeamsForMatchStart()
{
	if (!CanFinalizeTeamsForMatchStart())
	{
		return false;
	}

	const AGameStateBase* CurrentGameState = GameState;
	if (!IsValid(CurrentGameState))
	{
		return false;
	}

	TArray<AShooterPlayerState*> TeamAPlayers;
	TArray<AShooterPlayerState*> TeamBPlayers;
	TArray<AShooterPlayerState*> UnassignedPlayers;

	for (APlayerState* PlayerState : CurrentGameState->PlayerArray)
	{
		AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState);
		if (!IsValid(ShooterPlayerState))
		{
			continue;
		}

		switch (ShooterPlayerState->GetTeam())
		{
		case EGameTeam::TeamA:
			TeamAPlayers.Add(ShooterPlayerState);
			break;
		case EGameTeam::TeamB:
			TeamBPlayers.Add(ShooterPlayerState);
			break;
		default:
			UnassignedPlayers.Add(ShooterPlayerState);
			break;
		}
	}

	Algo::RandomShuffle(UnassignedPlayers);
	for (AShooterPlayerState* PlayerState : UnassignedPlayers)
	{
		const bool bCanJoinTeamA = TeamAPlayers.Num() < MaxPlayersPerTeam;
		const bool bCanJoinTeamB = TeamBPlayers.Num() < MaxPlayersPerTeam;
		if (!bCanJoinTeamA && !bCanJoinTeamB)
		{
			return false;
		}

		const bool bTeamANeedsMinimum = TeamAPlayers.Num() < MinimumPlayersPerTeam;
		const bool bTeamBNeedsMinimum = TeamBPlayers.Num() < MinimumPlayersPerTeam;

		bool bJoinTeamA = false;
		if (bTeamANeedsMinimum != bTeamBNeedsMinimum)
		{
			bJoinTeamA = bTeamANeedsMinimum;
		}
		else if (!bCanJoinTeamB)
		{
			bJoinTeamA = true;
		}
		else if (!bCanJoinTeamA)
		{
			bJoinTeamA = false;
		}
		else
		{
			bJoinTeamA = TeamAPlayers.Num() == TeamBPlayers.Num()
				? FMath::RandBool()
				: TeamAPlayers.Num() < TeamBPlayers.Num();
		}

		TArray<AShooterPlayerState*>& TargetTeam = bJoinTeamA
			? TeamAPlayers
			: TeamBPlayers;
		TargetTeam.Add(PlayerState);
		PlayerState->SetTeam(bJoinTeamA ? EGameTeam::TeamA : EGameTeam::TeamB);
	}

	const int32 TotalPlayerCount = TeamAPlayers.Num() + TeamBPlayers.Num();
	const bool bTeamsAreValid = TotalPlayerCount >= MinimumPlayersToStart
		&& TeamAPlayers.Num() >= MinimumPlayersPerTeam
		&& TeamBPlayers.Num() >= MinimumPlayersPerTeam
		&& TeamAPlayers.Num() <= MaxPlayersPerTeam
		&& TeamBPlayers.Num() <= MaxPlayersPerTeam;
	if (!bTeamsAreValid)
	{
		return false;
	}

	UE_LOG(LogLobbyGameMode, Log,
		TEXT("Team finalization preserved selected teams. TeamA=%d TeamB=%d"),
		TeamAPlayers.Num(),
		TeamBPlayers.Num());

	if (!UnassignedPlayers.IsEmpty())
	{
		NotifyLobbyPlayerListChanged();
	}

	return true;
}

void ALobbyGameMode::NotifyLobbyPlayerListChanged()
{
	if (ALobbyGameState* LobbyGameState = GetGameState<ALobbyGameState>())
	{
		LobbyGameState->NotifyLobbyPlayerListChanged();
	}
}

bool ALobbyGameMode::AreAllClientsReady() const
{
	int32 ReadyCount = 0;
	int32 TotalCount = 0;
	CountClientReady(ReadyCount, TotalCount);
	return ReadyCount == TotalCount;
}

void ALobbyGameMode::CountClientReady(int32& OutReady, int32& OutTotal) const
{
	OutReady = 0;
	OutTotal = 0;

	const AGameStateBase* CurrentGameState = GameState;
	if (!IsValid(CurrentGameState))
	{
		return;
	}

	for (APlayerState* PlayerState : CurrentGameState->PlayerArray)
	{
		// Pressing Start is the host's readiness confirmation.
		if (!IsValid(PlayerState) || IsHostPlayerState(PlayerState))
		{
			continue;
		}

		++OutTotal;

		const AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState);
		if (IsValid(ShooterPlayerState) && ShooterPlayerState->IsLobbyReady())
		{
			++OutReady;
		}
	}
}

bool ALobbyGameMode::HasMinimumPlayersToStart() const
{
	int32 TeamACount = 0;
	int32 TeamBCount = 0;
	int32 UnassignedCount = 0;
	GetTeamAssignmentCounts(TeamACount, TeamBCount, UnassignedCount);

	return TeamACount + TeamBCount + UnassignedCount >= MinimumPlayersToStart;
}

bool ALobbyGameMode::CanFinalizeTeamsForMatchStart() const
{
	if (MinimumPlayersToStart < 2
		|| MinimumPlayersPerTeam < 1
		|| MinimumPlayersPerTeam > MaxPlayersPerTeam)
	{
		return false;
	}

	int32 TeamACount = 0;
	int32 TeamBCount = 0;
	int32 UnassignedCount = 0;
	GetTeamAssignmentCounts(TeamACount, TeamBCount, UnassignedCount);

	const int32 TotalPlayerCount = TeamACount + TeamBCount + UnassignedCount;
	if (TotalPlayerCount < MinimumPlayersToStart
		|| TotalPlayerCount > MaxPlayersPerTeam * 2
		|| TeamACount > MaxPlayersPerTeam
		|| TeamBCount > MaxPlayersPerTeam)
	{
		return false;
	}

	const int32 AvailableSlots = (MaxPlayersPerTeam - TeamACount)
		+ (MaxPlayersPerTeam - TeamBCount);
	if (UnassignedCount > AvailableSlots)
	{
		return false;
	}

	const int32 PlayersNeededForTeamMinimum =
		FMath::Max(0, MinimumPlayersPerTeam - TeamACount)
		+ FMath::Max(0, MinimumPlayersPerTeam - TeamBCount);

	return PlayersNeededForTeamMinimum <= UnassignedCount;
}

bool ALobbyGameMode::IsHostPlayerState(const APlayerState* PlayerState) const
{
	return IsValid(PlayerState)
		&& HostPlayerState.IsValid()
		&& HostPlayerState.Get() == PlayerState;
}

bool ALobbyGameMode::IsSupportedMatchMode(const ELobbyMatchMode MatchMode) const
{
	return !GetMapNameForMatchMode(MatchMode).IsEmpty()
		&& !GetMatchTypeForMatchMode(MatchMode).IsEmpty();
}

bool ALobbyGameMode::TryResolveMatchModeFromMapName(
	const FString& MapName,
	ELobbyMatchMode& OutMatchMode) const
{
	const FString RequestedPath = ResolveMapPathFromName(MapName);
	if (RequestedPath.Equals(
		ResolveMapPathFromName(BombMapName),
		ESearchCase::IgnoreCase))
	{
		OutMatchMode = ELobbyMatchMode::Bomb;
		return true;
	}

	if (RequestedPath.Equals(
		ResolveMapPathFromName(DominationMapName),
		ESearchCase::IgnoreCase))
	{
		OutMatchMode = ELobbyMatchMode::Domination;
		return true;
	}

	return false;
}

void ALobbyGameMode::GetTeamAssignmentCounts(
	int32& OutTeamA,
	int32& OutTeamB,
	int32& OutUnassigned) const
{
	OutTeamA = 0;
	OutTeamB = 0;
	OutUnassigned = 0;

	const AGameStateBase* CurrentGameState = GameState;
	if (!IsValid(CurrentGameState))
	{
		return;
	}

	for (const APlayerState* PlayerState : CurrentGameState->PlayerArray)
	{
		const AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState);
		if (!IsValid(ShooterPlayerState))
		{
			continue;
		}

		switch (ShooterPlayerState->GetTeam())
		{
		case EGameTeam::TeamA:
			++OutTeamA;
			break;
		case EGameTeam::TeamB:
			++OutTeamB;
			break;
		default:
			++OutUnassigned;
			break;
		}
	}
}

FString ALobbyGameMode::GetMapNameForMatchMode(const ELobbyMatchMode MatchMode) const
{
	switch (MatchMode)
	{
	case ELobbyMatchMode::Bomb:
		return BombMapName;
	case ELobbyMatchMode::Domination:
		return DominationMapName;
	default:
		return TEXT("");
	}
}

FString ALobbyGameMode::GetMatchTypeForMatchMode(const ELobbyMatchMode MatchMode) const
{
	switch (MatchMode)
	{
	case ELobbyMatchMode::Bomb:
		return TEXT("Bomb");
	case ELobbyMatchMode::Domination:
		return TEXT("Domination");
	default:
		return TEXT("");
	}
}

bool ALobbyGameMode::UpdateSessionAdvertisementForMatchMode(
	const ELobbyMatchMode MatchMode)
{
	const FString MapName = GetMapNameForMatchMode(MatchMode);
	const FString MatchType = GetMatchTypeForMatchMode(MatchMode);
	if (MapName.IsEmpty() || MatchType.IsEmpty())
	{
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	USessionSubSystem* SessionSubsystem = GameInstance
		? GameInstance->GetSubsystem<USessionSubSystem>()
		: nullptr;

	return SessionSubsystem
		&& SessionSubsystem->UpdateRoomGameMode(MapName, MatchType);
}

FString ALobbyGameMode::ResolveMapPathFromName(const FString& MapName) const
{
	if (MapName.StartsWith(TEXT("/Game/")))
	{
		return MapName;
	}

	if (MapName.IsEmpty())
	{
		return DominationMapName.IsEmpty()
			? TEXT("/Game/Maps/DominationMap")
			: FString::Printf(TEXT("/Game/Maps/%s"), *DominationMapName);
	}

	return FString::Printf(TEXT("/Game/Maps/%s"), *MapName);
}

void ALobbyGameMode::TravelToPendingGameMap()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client || bTravelingToGameMap)
	{
		return;
	}

	bTravelingToGameMap = true;

	for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
	{
		if (AShooterPlayerController* ShooterPlayerController =
			Cast<AShooterPlayerController>(Iterator->Get()))
		{
			ShooterPlayerController->ClientShowLoadingScreen(0.2f);
		}
	}

	bUseSeamlessTravel = true;
	UE_LOG(LogTemp, Log, TEXT("[DominationTravel] Stage=ServerTravelRequested Target=%s Players=%d"),
		*PendingGameMapPath, World->GetNumPlayerControllers());
	const bool bTravelAccepted = World->ServerTravel(PendingGameMapPath + TEXT("?listen"));
	UE_LOG(LogTemp, Log, TEXT("[DominationTravel] Stage=ServerTravelAccepted Accepted=%d"), bTravelAccepted);
}

void ALobbyGameMode::HandleStartRoomGameComplete(const bool bWasSuccessful)
{
	UGameInstance* GameInstance = GetGameInstance();
	USessionSubSystem* SessionSubsystem = GameInstance
		? GameInstance->GetSubsystem<USessionSubSystem>()
		: nullptr;
	if (SessionSubsystem)
	{
		SessionSubsystem->OnStartRoomGameComplete.RemoveDynamic(
			this,
			&ThisClass::HandleStartRoomGameComplete);
	}

	bStartRoomGamePending = false;

	if (bWasSuccessful)
	{
		TravelToPendingGameMap();
		return;
	}

	NotifyLobbyPlayerListChanged();

	if (const APlayerState* HostState = HostPlayerState.Get())
	{
		if (AShooterPlayerController* HostController =
			Cast<AShooterPlayerController>(HostState->GetOwningController()))
		{
			HostController->ClientNotifyLobbyStartFailed();
		}
	}
}
