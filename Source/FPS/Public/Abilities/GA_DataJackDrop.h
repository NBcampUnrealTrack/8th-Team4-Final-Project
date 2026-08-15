#pragma once

#include "CoreMinimal.h"
#include "Abilities/GA_DataJackHoldAbilityBase.h"
#include "GA_DataJackDrop.generated.h"

UCLASS()
class FPS_API UGA_DataJackDrop : public UGA_DataJackHoldAbilityBase
{
	GENERATED_BODY()

public:
	UGA_DataJackDrop();

protected:
	virtual EDataJackOperation GetDataJackOperation() const override;
	virtual bool BeginDataJackOperation(
		UBombObjectiveManager& ObjectiveManager,
		APlayerController& PlayerController) override;
	virtual void CancelDataJackOperation(
		UBombObjectiveManager& ObjectiveManager,
		APlayerController& PlayerController) override;
};
