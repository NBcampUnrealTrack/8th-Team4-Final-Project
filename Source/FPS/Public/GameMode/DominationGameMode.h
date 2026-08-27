#pragma once

#include "CoreMinimal.h"
#include "GameMode/BaseTeamGameMode.h"
#include "GameMode/DominationTypes.h"
#include "DominationGameMode.generated.h"

class APlayerController;
class ADominationPlayerController;
class UDominationLoadoutManager;
class UDominationMatchStartManager;

// Domination의 엔진 진입점과 매치 시작 절차의 연결을 담당한다.
UCLASS()
class FPS_API ADominationGameMode : public ABaseTeamGameMode
{
	GENERATED_BODY()

public:
	ADominationGameMode();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void NotifyPlayerReadyForGameFlow(APlayerController* PlayerController) override;

	void RequestCombatClass(
		ADominationPlayerController* PlayerController,
		EDominationCombatClass RequestedClass);
	UDominationLoadoutManager* GetLoadoutManager() const;
	UDominationMatchStartManager* GetMatchStartManager() const;

protected:
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual bool ShouldSpawnAtStartSpot(AController* Player) override;

private:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Domination|Match Start", meta = (AllowPrivateAccess = "true", ClampMin = "1", ClampMax = "4"))
	int32 RequiredPlayersPerTeam = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Domination|Match Start", meta = (AllowPrivateAccess = "true", ClampMin = "1.0"))
	float ClassSelectionDuration = 15.0f;

	UPROPERTY(Transient)
	TObjectPtr<UDominationMatchStartManager> MatchStartManager;

	UPROPERTY(Transient)
	TObjectPtr<UDominationLoadoutManager> LoadoutManager;
};
