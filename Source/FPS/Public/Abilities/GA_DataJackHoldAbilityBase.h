#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameFlow/Bomb/DataJackTypes.h"
#include "GA_DataJackHoldAbilityBase.generated.h"

class ABaseRoundPlayerController;
class APlayerController;
class UBombObjectiveManager;

/**
 * Data Jack Hold 입력의 공통 수명주기.
 * 입력 태그는 모드 공통 Objective 의도를 표현하고, 실제 Bomb/DataJack 실행은 이 계층에서 제한한다.
 * Pressed에서 서버 작업을 시작하고 Released 또는 서버 완료에서 함께 종료한다.
 */
UCLASS(Abstract)
class FPS_API UGA_DataJackHoldAbilityBase : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UGA_DataJackHoldAbilityBase();

	virtual bool CanActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual void ActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	virtual EDataJackOperation GetDataJackOperation() const;
	virtual bool BeginDataJackOperation(
		UBombObjectiveManager& ObjectiveManager,
		APlayerController& PlayerController);
	virtual void CancelDataJackOperation(
		UBombObjectiveManager& ObjectiveManager,
		APlayerController& PlayerController);
	virtual void HandleDataJackOperationStarted(APlayerController& PlayerController);

private:
	ABaseRoundPlayerController* ResolveRoundPlayerController(
		const FGameplayAbilityActorInfo* ActorInfo) const;
	UBombObjectiveManager* ResolveBombObjectiveManager(APlayerController* PlayerController) const;
	void BindOperationEnded(UBombObjectiveManager& ObjectiveManager);
	void UnbindOperationEnded();

	UFUNCTION()
	void HandleInputReleased(float TimeHeld);

	void HandleDataJackOperationEnded(
		EDataJackOperation Operation,
		APlayerController* PlayerController);

	TWeakObjectPtr<APlayerController> OperationController;
	TWeakObjectPtr<UBombObjectiveManager> BoundObjectiveManager;
	FDelegateHandle OperationEndedDelegateHandle;
	bool bServerOperationStarted = false;
};
