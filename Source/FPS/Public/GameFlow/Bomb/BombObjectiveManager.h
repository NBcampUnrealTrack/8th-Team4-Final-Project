#pragma once

#include "CoreMinimal.h"
#include "GameFlow/Bomb/DataJackTypes.h"
#include "UObject/Object.h"
#include "BombObjectiveManager.generated.h"

class ABaseTeamPlayerState;
class ABombGameMode;
class ADataJack;
class ADataJackSiteVolume;
class ADataJackSpawnPoint;
class APawn;
class APlayerController;

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOnDataJackOperationEnded,
	EDataJackOperation,
	APlayerController*);

struct FBombObjectiveConfig
{
	TSubclassOf<ADataJack> DataJackClass;
	float PlantDuration = 4.0f;
	float CountdownDuration = 45.0f;
	float DefuseDuration = 7.0f;
	float DropHoldDuration = 1.0f;
	float InteractionValidationInterval = 0.1f;
};

/**
 * 서버 전용 Data Jack 관리자.
 * Objective 상태와 요청 검증은 담당하지만 입력 바인딩, Phase, 점수와 승패는 변경하지 않는다.
 */
UCLASS()
class FPS_API UBombObjectiveManager : public UObject
{
	GENERATED_BODY()

public:
	void Initialize(ABombGameMode* InOwnerGameMode);
	void Shutdown();

	bool PrepareForRound(const FBombObjectiveConfig& Config);
	bool ActivateForRound();
	void DeactivateForRound();
	void ResetObjective();

	bool BeginInteraction(APlayerController* PlayerController);
	void CancelInteraction(APlayerController* PlayerController);
	bool BeginDrop(APlayerController* PlayerController);
	void CancelDrop(APlayerController* PlayerController);
	bool TryPickup(APawn* PlayerPawn);

	// DataJack의 서버 작업이 끝난 시점과 GameplayAbility의 수명주기를 연결한다.
	void NotifyInteractionStopped(APlayerController* PlayerController);
	void NotifyDropStopped(APlayerController* PlayerController);
	FOnDataJackOperationEnded& GetOperationEndedDelegate();

	void HandlePlayerEliminated(ABaseTeamPlayerState* EliminatedPlayerState);

	bool IsActiveDataJack(const ADataJack* DataJack) const;
	bool CanCompletePlanting(const ADataJack* DataJack) const;
	void NotifyDataJackPlanted(ADataJack* DataJack);
	void NotifyDataJackDefused(ADataJack* DataJack);
	void NotifyDataJackDetonated(ADataJack* DataJack);
	bool IsPlantedOrDefusing() const;

private:
	EDataJackState GetObjectiveState() const;
	bool SpawnObjective(const FBombObjectiveConfig& Config);
	void DestroyObjective();
	bool EnsureMapConfiguration();
	bool RefreshMapConfiguration();
	bool IsCachedMapConfigurationValid() const;
	void ClearCachedMapConfiguration();
	ADataJackSiteVolume* FindPlantSiteForPawn(const APawn* PlayerPawn) const;
	bool IsRequestValid(const APlayerController* PlayerController, ABaseTeamPlayerState*& OutPlayerState) const;
	ABombGameMode* GetOwnerGameMode() const;

	TWeakObjectPtr<ABombGameMode> OwnerGameMode;
	TWeakObjectPtr<ADataJackSpawnPoint> CachedSpawnPoint;
	TArray<TWeakObjectPtr<ADataJackSiteVolume>> CachedPlantSites;

	UPROPERTY(Transient)
	TObjectPtr<ADataJack> ActiveDataJack;

	FOnDataJackOperationEnded OperationEndedDelegate;
};
