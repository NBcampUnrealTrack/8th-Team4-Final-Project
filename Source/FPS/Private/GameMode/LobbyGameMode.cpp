#include "GameMode/LobbyGameMode.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameMode/LobbyGameState.h"
#include "GameFramework/GameStateBase.h"
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
