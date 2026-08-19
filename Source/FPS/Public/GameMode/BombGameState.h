#pragma once

#include "CoreMinimal.h"
#include "GameMode/BaseRoundGameState.h"
#include "BombGameState.generated.h"

class ADataJack;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActiveDataJackChangedSignature, ADataJack*, ActiveDataJack);

UCLASS()
class FPS_API ABombGameState : public ABaseRoundGameState
{
	GENERATED_BODY()

public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack")
	ADataJack* GetActiveDataJack() const;

	void SetActiveDataJack(ADataJack* NewActiveDataJack);

protected:
	UFUNCTION()
	void OnRep_ActiveDataJack();

private:
	UPROPERTY(ReplicatedUsing = OnRep_ActiveDataJack, VisibleInstanceOnly, Category = "Game Flow|Data Jack")
	TObjectPtr<ADataJack> ActiveDataJack;

public:
	UPROPERTY(BlueprintAssignable, Category = "Game Flow|Data Jack")
	FOnActiveDataJackChangedSignature OnActiveDataJackChanged;
};
