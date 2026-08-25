#include "GameMode/LobbyGameMode.h"

#include "Algo/RandomShuffle.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameMode/LobbyGameState.h"
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

bool ALobbyGameMode::RequestStartRoomGame(APlayerController* RequestingPlayer, const FString& GameMapName)
{
	if (!IsHostPlayerState(RequestingPlayer ? RequestingPlayer->PlayerState : nullptr))
	{
		UE_LOG(LogLobbyGameMode, Warning, TEXT("Start room game blocked: requester is not the host."));
		return false;
	}

	if (!CanStartRoomGame())
	{
		UE_LOG(LogLobbyGameMode, Warning, TEXT("Start room game blocked: not every client is ready."));
		return false;
	}

	UGameInstance* GameInstance = GetGameInstance();
	USessionSubSystem* SessionSubsystem = GameInstance ? GameInstance->GetSubsystem<USessionSubSystem>() : nullptr;
	if (!SessionSubsystem)
	{
		UE_LOG(LogLobbyGameMode, Warning, TEXT("Start room game blocked: SessionSubsystem is unavailable."));
		return false;
	}

	if (bStartRoomGamePending || bTravelingToGameMap)
	{
		return false;
	}

	// 게임 시작이 확정된 시점에만 팀을 확정
	// 로비에서 실시간으로 강제 이동시키면 직접 고른 팀이 예고 없이 바뀌므로 여기서 한 번만 정리
	BalanceTeamsForMatchStart();

	PendingGameMapPath = ResolveMapPathFromName(GameMapName.IsEmpty() ? TEXT("BombMap") : GameMapName);
	bStartRoomGamePending = true;
	SessionSubsystem->OnStartRoomGameComplete.AddUniqueDynamic(this, &ThisClass::HandleStartRoomGameComplete);
	SessionSubsystem->StartRoomGame(GameMapName);
	return true;
}

bool ALobbyGameMode::CanStartRoomGame() const
{
	return AreAllClientsReady();
}

void ALobbyGameMode::AssignHostPlayerIfNeeded(APlayerController* NewPlayer)
{
	if (HostPlayerState.IsValid() || !IsValid(NewPlayer) || !IsValid(NewPlayer->PlayerState))
	{
		return;
	}

	HostPlayerState = NewPlayer->PlayerState;
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

		// 새 로비 사이클의 Ready는 이전 로비/매치 상태를 이어받지 않는다.
		ShooterPlayerState->SetLobbyReady(false);

		if (!IsValid(FallbackHostPlayerState))
		{
			FallbackHostPlayerState = ShooterPlayerState;
		}

		// 리슨 서버의 로컬 컨트롤러가 실제 방장이다.
		if (PlayerController->IsLocalController())
		{
			HostPlayerState = ShooterPlayerState;
		}
	}

	// Dedicated Server 등 로컬 컨트롤러가 없는 환경에서는 기존 정책처럼 첫 플레이어를 사용한다.
	if (!HostPlayerState.IsValid() && IsValid(FallbackHostPlayerState))
	{
		HostPlayerState = FallbackHostPlayerState;
	}

	if (!HostPlayerState.IsValid())
	{
		UE_LOG(LogLobbyGameMode, Warning, TEXT("Lobby seamless travel completed without a valid host player."));
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
			return;
		}
	}
}

bool ALobbyGameMode::RequestLobbyTeam(APlayerController* RequestingPlayer, const EGameTeam RequestedTeam)
{
	AShooterPlayerState* ShooterPlayerState = IsValid(RequestingPlayer)
		? RequestingPlayer->GetPlayerState<AShooterPlayerState>()
		: nullptr;
	if (!IsValid(ShooterPlayerState))
	{
		UE_LOG(LogLobbyGameMode, Warning, TEXT("Team change blocked: %s has no ShooterPlayerState."), *GetNameSafe(RequestingPlayer));
		return false;
	}

	// None 은 "아직 고르지 않음"을 뜻하는 내부 상태이므로 선택 대상이 아님
	if (RequestedTeam == EGameTeam::None)
	{
		UE_LOG(LogLobbyGameMode, Warning, TEXT("Team change blocked: None is not selectable."));
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
	if (!CurrentGameState)
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

void ALobbyGameMode::BalanceTeamsForMatchStart()
{
	const AGameStateBase* CurrentGameState = GameState;
	if (!CurrentGameState)
	{
		UE_LOG(LogLobbyGameMode, Warning, TEXT("Team balancing skipped: GameState is invalid."));
		return;
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

	// 팀을 고르지 않은 플레이어는 무작위 순서로 인원이 적은 팀에 한 명씩 채움
	Algo::RandomShuffle(UnassignedPlayers);
	for (AShooterPlayerState* PlayerState : UnassignedPlayers)
	{
		const bool bJoinTeamA = TeamAPlayers.Num() == TeamBPlayers.Num()
			? FMath::RandBool()
			: TeamAPlayers.Num() < TeamBPlayers.Num();

		TArray<AShooterPlayerState*>& TargetTeam = bJoinTeamA ? TeamAPlayers : TeamBPlayers;
		TargetTeam.Add(PlayerState);
		PlayerState->SetTeam(bJoinTeamA ? EGameTeam::TeamA : EGameTeam::TeamB);
	}

	// 직접 고른 인원이 한쪽에 쏠려 2명 이상 차이나면 마지막에 합류한 순서대로 반대 팀으로 옮김
	while (FMath::Abs(TeamAPlayers.Num() - TeamBPlayers.Num()) > 1)
	{
		const bool bMoveFromTeamA = TeamAPlayers.Num() > TeamBPlayers.Num();
		TArray<AShooterPlayerState*>& SourceTeam = bMoveFromTeamA ? TeamAPlayers : TeamBPlayers;
		TArray<AShooterPlayerState*>& TargetTeam = bMoveFromTeamA ? TeamBPlayers : TeamAPlayers;
		const EGameTeam TargetTeamId = bMoveFromTeamA ? EGameTeam::TeamB : EGameTeam::TeamA;

		AShooterPlayerState* MovedPlayerState = SourceTeam.Pop();
		TargetTeam.Add(MovedPlayerState);
		MovedPlayerState->SetTeam(TargetTeamId);

		UE_LOG(LogLobbyGameMode, Log,
			TEXT("Team balancing moved %s to the other team."),
			*GetNameSafe(MovedPlayerState));
	}

	UE_LOG(LogLobbyGameMode, Log,
		TEXT("Team balancing finished. TeamA=%d TeamB=%d"),
		TeamAPlayers.Num(),
		TeamBPlayers.Num());

	NotifyLobbyPlayerListChanged();
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
	const AGameStateBase* CurrentGameState = GameState;
	if (!CurrentGameState)
	{
		UE_LOG(LogLobbyGameMode, Warning, TEXT("Ready check failed: GameState is invalid."));
		return false;
	}

	bool bHasClientPlayer = false;
	for (APlayerState* PlayerState : CurrentGameState->PlayerArray)
	{
		if (!IsValid(PlayerState))
		{
			UE_LOG(LogLobbyGameMode, Warning, TEXT("Ready check: invalid PlayerState in PlayerArray."));
			continue;
		}

		if (IsHostPlayerState(PlayerState))
		{
			continue;
		}

		bHasClientPlayer = true;
		const AShooterPlayerState* ShooterPlayerState = Cast<AShooterPlayerState>(PlayerState);
		if (!IsValid(ShooterPlayerState) || !ShooterPlayerState->IsLobbyReady())
		{
			UE_LOG(LogLobbyGameMode, Warning, TEXT("Ready check failed: Player=%s Ready=false"), *GetNameSafe(PlayerState));
			return false;
		}
	}

	return bHasClientPlayer;
}

bool ALobbyGameMode::IsHostPlayerState(const APlayerState* PlayerState) const
{
	return IsValid(PlayerState) && HostPlayerState.IsValid() && HostPlayerState.Get() == PlayerState;
}

FString ALobbyGameMode::ResolveMapPathFromName(const FString& MapName) const
{
	if (MapName.StartsWith(TEXT("/Game/")))
	{
		return MapName;
	}

	if (MapName.IsEmpty())
	{
		return TEXT("/Game/Maps/BombMap");
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
		if (AShooterPlayerController* ShooterPlayerController = Cast<AShooterPlayerController>(Iterator->Get()))
		{
			ShooterPlayerController->ClientShowLoadingScreen(0.2f);
		}
	}

	bUseSeamlessTravel = true;
	World->ServerTravel(PendingGameMapPath + TEXT("?listen"));
}

void ALobbyGameMode::HandleStartRoomGameComplete(bool bWasSuccessful)
{
	UGameInstance* GameInstance = GetGameInstance();
	USessionSubSystem* SessionSubsystem = GameInstance ? GameInstance->GetSubsystem<USessionSubSystem>() : nullptr;
	if (SessionSubsystem)
	{
		SessionSubsystem->OnStartRoomGameComplete.RemoveDynamic(this, &ThisClass::HandleStartRoomGameComplete);
	}

	bStartRoomGamePending = false;

	if (bWasSuccessful)
	{
		TravelToPendingGameMap();
	}
}
