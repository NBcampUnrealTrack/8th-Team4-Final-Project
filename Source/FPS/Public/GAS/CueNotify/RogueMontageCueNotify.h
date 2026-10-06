// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Burst.h"
#include "RogueMontageCueNotify.generated.h"

class UAnimMontage;
class UAnimSequenceBase;
class USkeletalMeshComponent;

// 1인칭/3인칭 몬타주를 각 메시에 재생하는 큐
UCLASS()
class FPS_API URogueMontageCueNotify : public UGameplayCueNotify_Burst
{
	GENERATED_BODY()

protected:
	virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;
	// Specialized cues can select a presentation without changing cue effects or 1P playback.
	virtual UAnimMontage* ResolveMontage3P(AActor* Target) const;

	// 시전자 본인만 보는 팔 메시용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Montage")
	TObjectPtr<UAnimMontage> Montage1P;

	// 다른 플레이어가 보는 몸통 메시용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Montage")
	TObjectPtr<UAnimMontage> Montage3P;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Montage", meta = (ClampMin = "0.01"))
	float PlayRate = 1.f;

	// 손에 들리는 프롭, 비워두면 스폰하지 않음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Prop")
	TSubclassOf<AActor> PropClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Prop")
	TObjectPtr<UAnimSequenceBase> PropAnim1P;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Prop")
	TObjectPtr<UAnimSequenceBase> PropAnim3P;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Prop")
	FName PropSocket = TEXT("hand_l");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Prop")
	FTransform PropOffset1P;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Prop")
	FTransform PropOffset3P;

	// 손이 프롭을 꺼내기 전까지 숨기는 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Prop", meta = (ClampMin = "0.0"))
	float PropVisibilityDelay = 0.f;

	// 0 이면 프롭 애니메이션 길이만큼 유지
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Prop", meta = (ClampMin = "0.0"))
	float PropLifetime = 0.f;

private:
	void SpawnProp(USkeletalMeshComponent* Mesh, UAnimSequenceBase* Anim, const FTransform& Offset, bool bFirstPerson) const;
};
