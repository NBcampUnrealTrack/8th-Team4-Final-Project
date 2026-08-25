#pragma once

#include "CoreMinimal.h"
#include "Abilities/GA_DataJackHoldAbilityBase.h"
#include "GA_DataJackInteract.generated.h"

UCLASS()
class FPS_API UGA_DataJackInteract : public UGA_DataJackHoldAbilityBase
{
	GENERATED_BODY()

public:
	UGA_DataJackInteract();

	virtual bool CanActivateAbility(
		FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

protected:
	virtual EDataJackOperation GetDataJackOperation() const override;
	virtual bool BeginDataJackOperation(
		UBombObjectiveManager& ObjectiveManager,
		APlayerController& PlayerController) override;
	virtual void CancelDataJackOperation(
		UBombObjectiveManager& ObjectiveManager,
		APlayerController& PlayerController) override;
	virtual void HandleDataJackOperationStarted(APlayerController& PlayerController) override;

private:
	// 복제된 Objective 상태만으로 설치/해체 가능 여부를 판정한다. 서버 검증을 대체하지 않는다.
	bool CanStartInteraction(const APawn* AvatarPawn) const;
};
