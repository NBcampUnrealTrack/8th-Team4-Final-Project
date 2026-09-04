#include "GameMode/BaseTeamGameMode.h"

#include "EngineUtils.h"
#include "Actor/BaseTeamPlayerStart.h"
#include "GameMode/State/BaseTeamGameState.h"
#include "GameMode/Manager/SurrenderVoteManager.h"
#include "GameMode/Manager/TeamPlayerManager.h"
#include "GameFramework/PlayerController.h"
#include "Player/BaseTeamPlayerState.h"
#include "Player/ShooterPlayerController.h"
#include "Player/ShooterPlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogBaseTeamGameMode, Log, All);

ABaseTeamGameMode::ABaseTeamGameMode()
{
	GameStateClass = ABaseTeamGameState::StaticClass();
	PlayerStateClass = AShooterPlayerState::StaticClass();
	DefaultPawnClass = nullptr;

	TeamPlayerManager = CreateDefaultSubobject<UTeamPlayerManager>(TEXT("TeamPlayerManager"));
	SurrenderVoteManager = CreateDefaultSubobject<USurrenderVoteManager>(TEXT("SurrenderVoteManager"));
}

void ABaseTeamGameMode::Logout(AController* Exiting)
{
	if (AShooterPlayerController* ExitingPlayerController = Cast<AShooterPlayerController>(Exiting))
	{
		SurrenderVoteManager->HandleVoterLeaving(ExitingPlayerController);
	}

	Super::Logout(Exiting);
}

int32 ABaseTeamGameMode::GetTeamPlayerCount(const EGameTeam Team) const
{
	const UTeamPlayerManager* PlayerManager = GetTeamPlayerManager();
	return IsValid(PlayerManager) ? PlayerManager->GetTeamPlayerCount(Team) : 0;
}

bool ABaseTeamGameMode::TrySelectAvailableSpawnTransform(
	const TArray<FTransform>& CandidateTransforms,
	const int32 PreferredStartIndex,
	FTransform& OutSpawnTransform,
	int32& OutSelectedIndex) const
{
	const UTeamPlayerManager* PlayerManager = GetTeamPlayerManager();
	return IsValid(PlayerManager)
		&& PlayerManager->SelectAvailableSpawnTransform(
			CandidateTransforms,
			PreferredStartIndex,
			OutSpawnTransform,
			OutSelectedIndex);
}

void ABaseTeamGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	if (IsValid(TeamPlayerManager))
	{
		TeamPlayerManager->Initialize(this, SpawnClearanceRadius);
	}

	if (IsValid(SurrenderVoteManager))
	{
		SurrenderVoteManager->Initialize(this, SurrenderVoteDuration, SurrenderVoteCooldown);
	}
}

void ABaseTeamGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(SurrenderVoteManager))
	{
		SurrenderVoteManager->Shutdown();
	}

	Super::EndPlay(EndPlayReason);
}

void ABaseTeamGameMode::HandleMatchHasStarted()
{
	Super::HandleMatchHasStarted();
	ResetSurrenderForNewMatch();
}

void ABaseTeamGameMode::HandleMatchHasEnded()
{
	if (IsValid(SurrenderVoteManager))
	{
		SurrenderVoteManager->ResetForNewMatch();
	}

	SurrenderUnlockServerTime = 0.0f;
	Super::HandleMatchHasEnded();
}

void ABaseTeamGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (!InitializeTeamPlayer(NewPlayer))
	{
		return;
	}

	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
}

AActor* ABaseTeamGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const ABaseTeamPlayerState* TeamPlayerState = IsValid(Player)
		? Player->GetPlayerState<ABaseTeamPlayerState>()
		: nullptr;
	if (!IsValid(TeamPlayerState) || TeamPlayerState->GetTeam() == EGameTeam::None)
	{
		UE_LOG(LogBaseTeamGameMode, Error, TEXT("Cannot choose a team spawn because %s has no assigned team."), *GetNameSafe(Player));
		return nullptr;
	}

	TArray<AActor*> MatchingStarts;
	for (TActorIterator<ABaseTeamPlayerStart> Iterator(GetWorld()); Iterator; ++Iterator)
	{
		ABaseTeamPlayerStart* PlayerStart = *Iterator;
		if (IsValid(PlayerStart) && PlayerStart->GetSpawnTeam() == TeamPlayerState->GetTeam())
		{
			MatchingStarts.Add(PlayerStart);
		}
	}

	UTeamPlayerManager* PlayerManager = GetTeamPlayerManager();
	return IsValid(PlayerManager)
		? PlayerManager->SelectAvailablePlayerStart(MatchingStarts, TEXT("BaseTeamPlayerStart"))
		: nullptr;
}

EGameTeam ABaseTeamGameMode::ChooseTeamForNewPlayer() const
{
	const int32 TeamACount = GetTeamPlayerCount(EGameTeam::TeamA);
	const int32 TeamBCount = GetTeamPlayerCount(EGameTeam::TeamB);
	return TeamACount <= TeamBCount ? EGameTeam::TeamA : EGameTeam::TeamB;
}

ESurrenderVoteStartResult ABaseTeamGameMode::RequestStartSurrenderVote(
	AShooterPlayerController* Requester)
{
	EGameTeam Team = EGameTeam::None;
	TArray<AShooterPlayerController*> Voters;
	if (!BuildSurrenderVoteRequest(Requester, Team, Voters))
	{
		return ESurrenderVoteStartResult::NotAllowed;
	}

	return SurrenderVoteManager->TryStartVote(Team, Voters);
}

void ABaseTeamGameMode::SubmitSurrenderVote(
	AShooterPlayerController* Voter,
	const int32 VoteId,
	const bool bApprove)
{
	if (!HasAuthority()
		|| !IsValid(Voter)
		|| Voter->GetWorld() != GetWorld()
		|| !IsSurrenderVoteAllowedNow())
	{
		return;
	}

	const ABaseTeamPlayerState* PlayerState =
		Voter->GetPlayerState<ABaseTeamPlayerState>();
	if (!IsValid(PlayerState) || PlayerState->GetTeam() == EGameTeam::None)
	{
		return;
	}

	const EGameTeam SurrenderingTeam = SurrenderVoteManager->SubmitVote(
		PlayerState->GetTeam(),
		Voter,
		VoteId,
		bApprove);
	if (SurrenderingTeam != EGameTeam::None)
	{
		HandleSurrenderApproved(SurrenderingTeam);
	}
}

bool ABaseTeamGameMode::IsSurrenderVoteAllowedNow() const
{
	return HasAuthority()
		&& HasMatchStarted()
		&& !HasMatchEnded()
		&& GetWorld()->GetTimeSeconds() >= SurrenderUnlockServerTime;
}

bool ABaseTeamGameMode::InitializeTeamPlayer(APlayerController* PlayerController)
{
	UTeamPlayerManager* PlayerManager = GetTeamPlayerManager();
	return IsValid(PlayerManager)
		&& PlayerManager->AssignTeamIfNeeded(PlayerController, ChooseTeamForNewPlayer());
}

bool ABaseTeamGameMode::IsSurrenderConfigurationValid() const
{
	return SurrenderUnlockDelay >= 0.0f
		&& SurrenderVoteDuration > 0.0f
		&& SurrenderVoteCooldown >= 0.0f
		&& IsValid(SurrenderVoteManager);
}

void ABaseTeamGameMode::ResetSurrenderForNewMatch()
{
	SurrenderUnlockServerTime = GetWorld()->GetTimeSeconds() + SurrenderUnlockDelay;
	SurrenderVoteManager->ResetForNewMatch();
}

ABaseTeamGameState* ABaseTeamGameMode::GetBaseTeamGameState() const
{
	return GetGameState<ABaseTeamGameState>();
}

int32 ABaseTeamGameMode::GetAlivePlayerCount(const EGameTeam Team) const
{
	const UTeamPlayerManager* PlayerManager = GetTeamPlayerManager();
	return IsValid(PlayerManager) ? PlayerManager->GetAlivePlayerCount(Team) : 0;
}

UTeamPlayerManager* ABaseTeamGameMode::GetTeamPlayerManager() const
{
	return TeamPlayerManager.Get();
}

bool ABaseTeamGameMode::BuildSurrenderVoteRequest(
	AShooterPlayerController* Requester,
	EGameTeam& OutTeam,
	TArray<AShooterPlayerController*>& OutVoters) const
{
	OutTeam = EGameTeam::None;
	OutVoters.Reset();

	if (!IsValid(Requester)
		|| Requester->GetWorld() != GetWorld()
		|| !IsSurrenderVoteAllowedNow()
		|| GetTeamPlayerCount(EGameTeam::TeamA) <= 0
		|| GetTeamPlayerCount(EGameTeam::TeamB) <= 0)
	{
		return false;
	}

	const ABaseTeamPlayerState* RequesterState =
		Requester->GetPlayerState<ABaseTeamPlayerState>();
	if (!IsValid(RequesterState) || RequesterState->GetTeam() == EGameTeam::None)
	{
		return false;
	}

	OutTeam = RequesterState->GetTeam();
	for (FConstPlayerControllerIterator Iterator = GetWorld()->GetPlayerControllerIterator();
		Iterator;
		++Iterator)
	{
		AShooterPlayerController* Candidate =
			Cast<AShooterPlayerController>(Iterator->Get());
		const ABaseTeamPlayerState* CandidateState = IsValid(Candidate)
			? Candidate->GetPlayerState<ABaseTeamPlayerState>()
			: nullptr;
		if (IsValid(CandidateState) && CandidateState->GetTeam() == OutTeam)
		{
			OutVoters.Add(Candidate);
		}
	}

	return OutVoters.Contains(Requester);
}
