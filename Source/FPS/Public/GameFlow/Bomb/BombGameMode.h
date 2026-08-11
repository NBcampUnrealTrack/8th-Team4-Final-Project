#pragma once

#include "CoreMinimal.h"
#include "GameFlow/Round/BaseRoundGameMode.h"
#include "BombGameMode.generated.h"

class ABaseTeamPlayerState;
class ABombPlayerController;
class ADataJack;
class ADataJackSiteVolume;
class APawn;
class APlayerController;

UCLASS()
class FPS_API ABombGameMode : public ABaseRoundGameMode
{
	GENERATED_BODY()

public:
	ABombGameMode();

	bool StartDataJackInteraction(ABombPlayerController* PlayerController);
	void StopDataJackInteraction(ABombPlayerController* PlayerController);
	bool StartDataJackDrop(ABombPlayerController* PlayerController);
	void StopDataJackDrop(ABombPlayerController* PlayerController);
	bool TryPickupDataJack(APawn* PlayerPawn);
	void NotifyDataJackInteractionStopped(APlayerController* PlayerController);
	void NotifyDataJackPlanted(ADataJack* DataJack);
	void NotifyDataJackDetonated(ADataJack* DataJack);
	void NotifyDataJackDefused(ADataJack* DataJack);

protected:
	virtual void BeginPlay() override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual bool PrepareRound() override;
	virtual void HandleGameFlowReset() override;
	virtual void HandleRoundStarted() override;
	virtual void HandleRoundEnded(EGameTeam WinningTeam, ERoundEndReason EndReason) override;
	virtual void HandlePlayerEliminated(ABaseTeamPlayerState* EliminatedPlayerState) override;
	virtual void HandleRoundTimeExpired() override;
	virtual bool HasEnoughPlayersToStart() const override;
	virtual bool IsGameFlowConfigurationValid() const override;

private:
	void ScheduleEliminationResolution();
	void ResolveEliminationOutcome();
	bool TryResolveEliminationOutcome();
	bool SpawnDataJackForRound();
	void DestroyDataJackForRound();
	bool ValidateDataJackMapConfiguration() const;
	ADataJackSiteVolume* FindPlantSiteForPawn(const APawn* PlayerPawn) const;
	bool IsDataJackRequestValid(const ABombPlayerController* PlayerController, ABaseTeamPlayerState*& OutPlayerState) const;

protected:
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
	UPROPERTY(Transient)
	TObjectPtr<ADataJack> ActiveDataJack;

	FTimerHandle EliminationResolutionTimerHandle;
};
