#pragma once

#include "CoreMinimal.h"
#include "GameFlow/Round/BaseRoundGameMode.h"
#include "BombGameMode.generated.h"

class ABaseTeamPlayerState;
class ADataJack;
class UBombObjectiveManager;

UCLASS()
class FPS_API ABombGameMode : public ABaseRoundGameMode
{
	GENERATED_BODY()

public:
	ABombGameMode();

	// 서버 GameplayAbility가 Objective 작업을 요청하는 진입점이다.
	UBombObjectiveManager* GetBombObjectiveManager() const;
	void NotifyDataJackPlanted(ADataJack* DataJack);
	void NotifyDataJackDetonated(ADataJack* DataJack);
	void NotifyDataJackDefused(ADataJack* DataJack);

protected:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

	virtual void ResetModeState() override;
	virtual EPlayerStartReservationResult ReserveModePlayerStarts(const FRoundContext& RoundContext) override;
	virtual bool PrepareModeRound(const FRoundContext& RoundContext) override;
	virtual bool ActivateModeRound() override;
	virtual void DeactivateModeRound(EGameTeam WinningTeam, ERoundEndReason EndReason) override;
	virtual void HandleModePlayerEliminated(ABaseTeamPlayerState* EliminatedPlayerState) override;
	virtual void ResolveModeRoundTimeExpired() override;
	virtual bool HasModeRequiredPlayers() const override;
	virtual bool IsModeConfigurationValid() const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack")
	TSubclassOf<ADataJack> DataJackClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Timing", meta = (ClampMin = "0.01"))
	float PlantDuration = 4.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Timing", meta = (ClampMin = "0.01"))
	float CountdownDuration = 45.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Timing", meta = (ClampMin = "0.01"))
	float DefuseDuration = 7.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Timing", meta = (ClampMin = "0.01"))
	float DropHoldDuration = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Timing", meta = (ClampMin = "0.02"))
	float InteractionValidationInterval = 0.1f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Elimination", meta = (ClampMin = "0.0", ClampMax = "0.25"))
	float MutualEliminationGracePeriod = 0.05f;

private:
	void ScheduleEliminationResolution();
	void HandleEliminationGraceExpired();
	bool TryResolveEliminationOutcome();

	UPROPERTY(Transient)
	TObjectPtr<UBombObjectiveManager> BombObjectiveManager;

	FTimerHandle EliminationResolutionTimerHandle;
};

