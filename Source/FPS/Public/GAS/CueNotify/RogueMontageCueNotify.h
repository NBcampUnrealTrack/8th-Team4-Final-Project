// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Burst.h"
#include "RogueMontageCueNotify.generated.h"

class UAnimMontage;

// 1인칭/3인칭 몬타주를 각 메시에 재생하는 큐
UCLASS()
class FPS_API URogueMontageCueNotify : public UGameplayCueNotify_Burst
{
	GENERATED_BODY()

protected:
	virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;

	// 시전자 본인만 보는 팔 메시용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Montage")
	TObjectPtr<UAnimMontage> Montage1P;

	// 다른 플레이어가 보는 몸통 메시용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Montage")
	TObjectPtr<UAnimMontage> Montage3P;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Montage", meta = (ClampMin = "0.01"))
	float PlayRate = 1.f;
};
