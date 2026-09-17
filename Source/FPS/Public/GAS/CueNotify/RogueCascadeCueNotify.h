// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Burst.h"
#include "RogueCascadeCueNotify.generated.h"

class UParticleSystem;

// 큐가 스폰할 Cascade 이펙트
USTRUCT(BlueprintType)
struct FRogueCascadeCueEntry
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Cascade")
	TObjectPtr<UParticleSystem> Particle;

	// 큐 위치 기준 오프셋
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Cascade")
	FVector Offset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Cascade", meta = (ClampMin = "0.01"))
	float Scale = 1.f;

	// 켜면 CueParameters 의 RawMagnitude(반경)에 비례해 크기 조정
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Cascade")
	bool bScaleByRadius = false;
};

// Cascade 이펙트를 재생하는 Burst 큐
UCLASS()
class FPS_API URogueCascadeCueNotify : public UGameplayCueNotify_Burst
{
	GENERATED_BODY()

protected:
	virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Cascade")
	TArray<FRogueCascadeCueEntry> Particles;

	// bScaleByRadius 의 기준 반경(cm). RawMagnitude 가 이 값일 때 배율 1
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Cascade", meta = (ClampMin = "1.0"))
	float ReferenceRadius = 250.f;

	// 켜면 대상에 부착, 끄면 CueParameters 의 Location 에 월드 스폰
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Cascade")
	bool bAttachToTarget = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Cascade",
		meta = (EditCondition = "bAttachToTarget", EditConditionHides))
	FName AttachSocket = NAME_None;
};
