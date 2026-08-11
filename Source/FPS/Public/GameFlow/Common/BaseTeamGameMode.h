#pragma once

#include "CoreMinimal.h"
#include "GameFlow/Common/TeamTypes.h"
#include "GameMode/ShooterGameMode.h"
#include "BaseTeamGameMode.generated.h"

class ABaseTeamGameState;
class ABaseTeamPlayerState;

UCLASS(Abstract)
class FPS_API ABaseTeamGameMode : public AShooterGameMode
{
	GENERATED_BODY()

public:
	ABaseTeamGameMode();

	UFUNCTION(BlueprintPure, Category = "Game Flow|Team")
	int32 GetTeamPlayerCount(EGameTeam Team) const;

protected:
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;
	virtual EGameTeam ChooseTeamForNewPlayer() const;

	void EnsurePlayerTeamAssigned(APlayerController* PlayerController);
	ABaseTeamGameState* GetBaseTeamGameState() const;
	int32 GetAlivePlayerCount(EGameTeam Team) const;
	bool IsPlayerStartClear(const AActor* PlayerStart) const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Spawn", meta = (ClampMin = "0.0"))
	float SpawnClearanceRadius = 150.0f;
};
