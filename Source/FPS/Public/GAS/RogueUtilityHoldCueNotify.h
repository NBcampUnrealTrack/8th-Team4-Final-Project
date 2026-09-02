// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Looping.h"
#include "RogueUtilityHoldCueNotify.generated.h"

class UAnimMontage;
class UStaticMeshComponent;

// 유틸리티를 손에 든 동안 1인칭/3인칭 몽타주와 계열별 메시를 유지하는 큐
UCLASS()
class FPS_API ARogueUtilityHoldCueNotify : public AGameplayCueNotify_Looping
{
	GENERATED_BODY()

public:
	ARogueUtilityHoldCueNotify();

protected:
	virtual bool OnActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;
	virtual bool WhileActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;
	virtual bool Recycle() override;

	// 시전자 본인만 보는 팔 메시용. 드는 인트로 + 홀드 루프 섹션으로 구성
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Montage")
	TObjectPtr<UAnimMontage> Montage1P;

	// 다른 플레이어가 보는 몸통 메시용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Montage")
	TObjectPtr<UAnimMontage> Montage3P;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Montage")
	FName LoopSectionName = TEXT("Loop");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Montage", meta = (ClampMin = "0.01"))
	float PlayRate = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Montage", meta = (ClampMin = "0.0"))
	float StopBlendOutTime = 0.25f;

	// 계열별 메시를 담는 그릇. 실제 메시는 UAugmentDefinition::HeldVisual
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rogue|Held")
	TObjectPtr<UStaticMeshComponent> HeldMesh;

private:
	void AttachHeldMesh(AActor* Target, const FGameplayCueParameters& Parameters);

	void ResetHeldMesh();
};
