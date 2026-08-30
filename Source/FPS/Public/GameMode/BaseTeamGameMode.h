#pragma once

#include "CoreMinimal.h"
#include "GameMode/SurrenderVoteTypes.h"
#include "GameMode/TeamTypes.h"
#include "GameMode/ShooterGameMode.h"
#include "BaseTeamGameMode.generated.h"

class ABaseTeamGameState;
class AShooterPlayerController;
class USurrenderVoteManager;
class UTeamPlayerManager;

UCLASS(Abstract)
class FPS_API ABaseTeamGameMode : public AShooterGameMode
{
	GENERATED_BODY()

public:
	ABaseTeamGameMode();
	virtual void Logout(AController* Exiting) override;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Team")
	int32 GetTeamPlayerCount(EGameTeam Team) const;
	bool TrySelectAvailableSpawnTransform(
		const TArray<FTransform>& CandidateTransforms,
		int32 PreferredStartIndex,
		FTransform& OutSpawnTransform,
		int32& OutSelectedIndex) const;

	ESurrenderVoteStartResult RequestStartSurrenderVote(AShooterPlayerController* Requester);
	void SubmitSurrenderVote(
		AShooterPlayerController* Voter,
		int32 VoteId,
		bool bApprove);

protected:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void HandleMatchHasStarted() override;
	virtual void HandleMatchHasEnded() override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual EGameTeam ChooseTeamForNewPlayer() const;
	virtual bool IsSurrenderVoteAllowedNow() const;
	virtual void HandleSurrenderApproved(EGameTeam SurrenderingTeam)
		PURE_VIRTUAL(ABaseTeamGameMode::HandleSurrenderApproved, );

	bool InitializeTeamPlayer(APlayerController* PlayerController);
	bool IsSurrenderConfigurationValid() const;
	void ResetSurrenderForNewMatch();

	ABaseTeamGameState* GetBaseTeamGameState() const;
	int32 GetAlivePlayerCount(EGameTeam Team) const;
	UTeamPlayerManager* GetTeamPlayerManager() const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Spawn", meta = (ClampMin = "0.0"))
	float SpawnClearanceRadius = 150.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Surrender", meta = (ClampMin = "0.0", Units = "s"))
	float SurrenderUnlockDelay = 180.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Surrender", meta = (ClampMin = "1.0", Units = "s"))
	float SurrenderVoteDuration = 20.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Surrender", meta = (ClampMin = "0.0", Units = "s"))
	float SurrenderVoteCooldown = 60.0f;

private:
	bool BuildSurrenderVoteRequest(
		AShooterPlayerController* Requester,
		EGameTeam& OutTeam,
		TArray<AShooterPlayerController*>& OutVoters) const;

	UPROPERTY(Transient)
	TObjectPtr<UTeamPlayerManager> TeamPlayerManager;

	UPROPERTY(Transient)
	TObjectPtr<USurrenderVoteManager> SurrenderVoteManager;

	float SurrenderUnlockServerTime = 0.0f;
};
