// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Burst.h"
#include "RogueFxCueNotify.generated.h"

class UParticleSystem;
class UNiagaraSystem;

// 큐가 스폰할 이펙트. Particle 과 Niagara 중 채운 것만 재생
USTRUCT(BlueprintType)
struct FRogueFxCueEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rogue|Fx")
	TObjectPtr<UParticleSystem> Particle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rogue|Fx")
	TObjectPtr<UNiagaraSystem> Niagara;

	// 켜면 Niagara 의 User.Color 를 덮어씀
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rogue|Fx")
	bool bOverrideColor = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rogue|Fx",
		meta = (EditCondition = "bOverrideColor"))
	FLinearColor Color = FLinearColor::White;

	// 큐 위치 기준 오프셋
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rogue|Fx")
	FVector Offset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rogue|Fx", meta = (ClampMin = "0.01"))
	float Scale = 1.f;

	// 켜면 CueParameters 의 RawMagnitude(반경)에 비례해 크기 조정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rogue|Fx")
	bool bScaleByRadius = false;
};

// 순간 이펙트를 재생하는 Burst 큐
UCLASS()
class FPS_API URogueFxCueNotify : public UGameplayCueNotify_Burst
{
	GENERATED_BODY()

protected:
	virtual bool OnExecute_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rogue|Fx")
	TArray<FRogueFxCueEntry> Particles;

	// bScaleByRadius 의 기준 반경(cm). RawMagnitude 가 이 값일 때 배율 1
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rogue|Fx", meta = (ClampMin = "1.0"))
	float ReferenceRadius = 250.f;

	// 켜면 대상에 부착, 끄면 CueParameters 의 Location 에 월드 스폰
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rogue|Fx")
	bool bAttachToTarget = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rogue|Fx",
		meta = (EditCondition = "bAttachToTarget", EditConditionHides))
	FName AttachSocket = NAME_None;
};
