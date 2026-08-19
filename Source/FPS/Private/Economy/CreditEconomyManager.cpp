#include "Economy/CreditEconomyManager.h"

#include "Economy/CreditWalletComponent.h"
#include "Economy/RogueEconomySettings.h"
#include "GameMode/TeamTypes.h"
#include "Player/ShooterPlayerState.h"
#include "Templates/UnrealTemplate.h"

DEFINE_LOG_CATEGORY_STATIC(LogCreditEconomy, Log, All);

namespace
{
	bool IsPlayableTeam(const EGameTeam Team)
	{
		return Team == EGameTeam::TeamA || Team == EGameTeam::TeamB;
	}

	// 검증 단계에서 확정한 지급 정보를 실제 지급 단계까지 유지한다.
	struct FPendingCreditGrant
	{
		TWeakObjectPtr<AShooterPlayerState> PlayerState;
		TWeakObjectPtr<UCreditWalletComponent> Wallet;
		int32 RoundReward = 0;
		int32 KillReward = 0;
		int32 ObjectiveReward = 0;
		int32 TotalReward = 0;
		ECreditChangeReason Reason = ECreditChangeReason::RoundLoss;
	};
}

bool UCreditEconomyManager::InitializePlayerForMatch(AShooterPlayerState* PlayerState)
{
	if (!IsValid(PlayerState) || !PlayerState->HasAuthority())
	{
		return false;
	}

	const TWeakObjectPtr<AShooterPlayerState> PlayerKey(PlayerState);
	if (InitializedPlayersThisMatch.Contains(PlayerKey))
	{
		return true;
	}

	if (bIsProcessingEconomyUpdate || !IsConfigurationValid())
	{
		return false;
	}

	TGuardValue<bool> ProcessingGuard(bIsProcessingEconomyUpdate, true);

	const int32 StartingCredits = GetDefault<URogueEconomySettings>()->StartingCredits;
	if (!ResetPlayerCredits(PlayerState, StartingCredits, ECreditChangeReason::InitialGrant))
	{
		return false;
	}

	InitializedPlayersThisMatch.Add(PlayerKey);
	return true;
}

bool UCreditEconomyManager::ResetForNewMatch(const TArray<AShooterPlayerState*>& PlayerStates)
{
	if (bIsProcessingEconomyUpdate || !IsConfigurationValid())
	{
		return false;
	}

	TSet<TWeakObjectPtr<AShooterPlayerState>> UniquePlayers;
	for (AShooterPlayerState* PlayerState : PlayerStates)
	{
		const TWeakObjectPtr<AShooterPlayerState> PlayerKey(PlayerState);

		if (!IsValid(PlayerState)
			|| !PlayerState->HasAuthority()
			|| !IsValid(PlayerState->GetCreditWallet())
			|| UniquePlayers.Contains(PlayerKey))
		{
			UE_LOG(LogCreditEconomy, Error,
				TEXT("Cannot reset credits for invalid or duplicate player %s."),
				*GetNameSafe(PlayerState));

			return false;
		}

		UniquePlayers.Add(PlayerKey);
	}

	TGuardValue<bool> ProcessingGuard(bIsProcessingEconomyUpdate, true);

	InitializedPlayersThisMatch.Reset();
	PendingKillRewards.Reset();
	ObjectiveRewardedTeamsThisRound.Reset();
	ActiveRoundNumber = 0;
	LastSettledRoundNumber = 0;

	const int32 StartingCredits = GetDefault<URogueEconomySettings>()->StartingCredits;

	for (AShooterPlayerState* PlayerState : PlayerStates)
	{
		if (!ResetPlayerCredits(PlayerState, StartingCredits, ECreditChangeReason::NewMatchReset))
		{
			UE_LOG(LogCreditEconomy, Error,
				TEXT("Failed to reset credits for %s."),
				*GetNameSafe(PlayerState));

			return false;
		}

		InitializedPlayersThisMatch.Add(TWeakObjectPtr<AShooterPlayerState>(PlayerState));
	}

	return true;
}

bool UCreditEconomyManager::BeginRound(const int32 RoundNumber)
{
	if (RoundNumber <= 0
		|| RoundNumber <= LastSettledRoundNumber
		|| ActiveRoundNumber != 0
		|| bIsProcessingEconomyUpdate
		|| !IsConfigurationValid())
	{
		return false;
	}

	if (!PendingKillRewards.IsEmpty() || !ObjectiveRewardedTeamsThisRound.IsEmpty())
	{
		UE_LOG(LogCreditEconomy, Warning,
			TEXT("Discarding stale rewards before starting round %d."),
			RoundNumber);
	}

	PendingKillRewards.Reset();
	ObjectiveRewardedTeamsThisRound.Reset();
	ActiveRoundNumber = RoundNumber;

	return true;
}

void UCreditEconomyManager::CancelActiveRound()
{
	if (bIsProcessingEconomyUpdate)
	{
		UE_LOG(LogCreditEconomy, Error,
			TEXT("Cannot cancel an economy round during settlement."));

		return;
	}

	PendingKillRewards.Reset();
	ObjectiveRewardedTeamsThisRound.Reset();
	ActiveRoundNumber = 0;
}

bool UCreditEconomyManager::RecordKillReward(AShooterPlayerState* PlayerState)
{
	if (!IsConfigurationValid())
	{
		return false;
	}

	return AddPendingKillReward(
		PlayerState,
		GetDefault<URogueEconomySettings>()->KillReward);
}

bool UCreditEconomyManager::RecordTeamObjectiveReward(const EGameTeam Team)
{
	if (ActiveRoundNumber <= 0
		|| !IsPlayableTeam(Team)
		|| bIsProcessingEconomyUpdate
		|| !IsConfigurationValid())
	{
		return false;
	}

	// 동일 라운드에서 같은 팀의 목표 보상이 중복 기록되는 것을 방지한다.
	if (ObjectiveRewardedTeamsThisRound.Contains(Team))
	{
		return true;
	}

	ObjectiveRewardedTeamsThisRound.Add(Team);

	UE_LOG(LogCreditEconomy, Verbose,
		TEXT("Round %d objective reward recorded for %s."),
		ActiveRoundNumber,
		*UEnum::GetValueAsString(Team));

	return true;
}

bool UCreditEconomyManager::SettleRoundCredits(
	const int32 RoundNumber,
	const EGameTeam WinningTeam,
	const TArray<AShooterPlayerState*>& RoundParticipants)
{
	if (RoundNumber <= 0
		|| RoundNumber != ActiveRoundNumber
		|| RoundNumber <= LastSettledRoundNumber
		|| bIsProcessingEconomyUpdate
		|| !IsPlayableTeam(WinningTeam)
		|| RoundParticipants.IsEmpty()
		|| !IsConfigurationValid())
	{
		return false;
	}

	TGuardValue<bool> ProcessingGuard(bIsProcessingEconomyUpdate, true);

	const URogueEconomySettings* Settings = GetDefault<URogueEconomySettings>();

	TSet<TWeakObjectPtr<AShooterPlayerState>> SeenParticipants;
	TArray<FPendingCreditGrant> PendingGrants;
	PendingGrants.Reserve(RoundParticipants.Num());

	// 모든 참가자를 먼저 검증하고 실제 지급할 금액을 확정한다.
	for (AShooterPlayerState* PlayerState : RoundParticipants)
	{
		if (!IsValid(PlayerState)
			|| !PlayerState->HasAuthority()
			|| !IsPlayableTeam(PlayerState->GetTeam()))
		{
			UE_LOG(LogCreditEconomy, Error,
				TEXT("Round %d contains an invalid credit participant: %s."),
				RoundNumber,
				*GetNameSafe(PlayerState));

			return false;
		}

		const TWeakObjectPtr<AShooterPlayerState> PlayerKey(PlayerState);

		if (!InitializedPlayersThisMatch.Contains(PlayerKey)
			|| SeenParticipants.Contains(PlayerKey))
		{
			UE_LOG(LogCreditEconomy, Error,
				TEXT("Round %d contains an uninitialized or duplicate credit participant: %s."),
				RoundNumber,
				*GetNameSafe(PlayerState));

			return false;
		}

		SeenParticipants.Add(PlayerKey);

		UCreditWalletComponent* Wallet = PlayerState->GetCreditWallet();

		const bool bWonRound = PlayerState->GetTeam() == WinningTeam;
		const int32 RoundReward =
			bWonRound ? Settings->RoundWinReward : Settings->RoundLossReward;

		const int32 KillReward = PendingKillRewards.FindRef(PlayerKey);

		const int32 ObjectiveReward =
			ObjectiveRewardedTeamsThisRound.Contains(PlayerState->GetTeam())
			? Settings->TeamObjectiveRewardPerPlayer
			: 0;

		// Wallet 변경 알림이 한 번만 발생하도록 보상을 합산해서 지급한다.
		const int64 TotalReward =
			static_cast<int64>(RoundReward)
			+ static_cast<int64>(KillReward)
			+ static_cast<int64>(ObjectiveReward);

		if (!IsValid(Wallet)
			|| TotalReward < 0
			|| TotalReward > MAX_int32
			|| !Wallet->CanAddCredits(static_cast<int32>(TotalReward)))
		{
			UE_LOG(LogCreditEconomy, Error,
				TEXT("Cannot grant %lld credits to %s for round %d."),
				static_cast<long long>(TotalReward),
				*GetNameSafe(PlayerState),
				RoundNumber);

			return false;
		}

		FPendingCreditGrant& Grant = PendingGrants.AddDefaulted_GetRef();
		Grant.PlayerState = PlayerKey;
		Grant.Wallet = Wallet;
		Grant.RoundReward = RoundReward;
		Grant.KillReward = KillReward;
		Grant.ObjectiveReward = ObjectiveReward;
		Grant.TotalReward = static_cast<int32>(TotalReward);
		Grant.Reason =
			bWonRound
			? ECreditChangeReason::RoundWin
			: ECreditChangeReason::RoundLoss;
	}

	// 지급 전에 라운드를 닫아 델리게이트 재진입과 동일 라운드 중복 정산을 방지한다.
	PendingKillRewards.Reset();
	ObjectiveRewardedTeamsThisRound.Reset();
	ActiveRoundNumber = 0;
	LastSettledRoundNumber = RoundNumber;

	bool bAllRewardsGranted = true;

	for (const FPendingCreditGrant& Grant : PendingGrants)
	{
		UCreditWalletComponent* Wallet = Grant.Wallet.Get();

		const bool bGranted =
			IsValid(Wallet)
			&& Wallet->AddCredits(Grant.TotalReward, Grant.Reason);

		// 사전 검증 이후의 실패는 정상적인 처리 경로가 아니다.
		if (!ensureMsgf(
			bGranted,
			TEXT("Round %d credit grant failed for %s after validation succeeded."),
			RoundNumber,
			*GetNameSafe(Grant.PlayerState.Get())))
		{
			bAllRewardsGranted = false;
			continue;
		}

		UE_LOG(LogCreditEconomy, Verbose,
			TEXT("Round %d settled for %s: round=%d, kills=%d, objective=%d, total=%d."),
			RoundNumber,
			*GetNameSafe(Grant.PlayerState.Get()),
			Grant.RoundReward,
			Grant.KillReward,
			Grant.ObjectiveReward,
			Grant.TotalReward);
	}

	return bAllRewardsGranted;
}

bool UCreditEconomyManager::AddPendingKillReward(
	AShooterPlayerState* PlayerState,
	const int32 Amount)
{
	if (ActiveRoundNumber <= 0
		|| !IsValid(PlayerState)
		|| !PlayerState->HasAuthority()
		|| Amount < 0
		|| bIsProcessingEconomyUpdate)
	{
		return false;
	}

	const TWeakObjectPtr<AShooterPlayerState> PlayerKey(PlayerState);

	if (!InitializedPlayersThisMatch.Contains(PlayerKey))
	{
		return false;
	}

	if (Amount == 0)
	{
		return true;
	}

	const int32 CurrentReward = PendingKillRewards.FindRef(PlayerKey);

	if (CurrentReward > MAX_int32 - Amount)
	{
		return false;
	}

	PendingKillRewards.Add(PlayerKey, CurrentReward + Amount);

	UE_LOG(LogCreditEconomy, Verbose,
		TEXT("Round %d pending reward for %s: %d -> %d."),
		ActiveRoundNumber,
		*GetNameSafe(PlayerState),
		CurrentReward,
		CurrentReward + Amount);

	return true;
}

bool UCreditEconomyManager::ResetPlayerCredits(
	AShooterPlayerState* PlayerState,
	const int32 StartingCredits,
	const ECreditChangeReason Reason)
{
	if (!IsValid(PlayerState) || !PlayerState->HasAuthority())
	{
		return false;
	}

	UCreditWalletComponent* Wallet = PlayerState->GetCreditWallet();

	return IsValid(Wallet)
		&& Wallet->ResetCredits(StartingCredits, Reason);
}

bool UCreditEconomyManager::IsConfigurationValid() const
{
	const URogueEconomySettings* Settings = GetDefault<URogueEconomySettings>();

	if (IsValid(Settings)
		&& Settings->StartingCredits >= 0
		&& Settings->RoundWinReward >= 0
		&& Settings->RoundLossReward >= 0
		&& Settings->KillReward >= 0
		&& Settings->TeamObjectiveRewardPerPlayer >= 0)
	{
		return true;
	}

	UE_LOG(LogCreditEconomy, Error,
		TEXT("Credit economy settings contain a negative value."));

	return false;
}