// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Looping.h"
#include "GAS/CueNotify/RogueCascadeCueNotify.h"
#include "RogueCascadeLoopingCueNotify.generated.h"

class UParticleSystemComponent;

// 지속되는 Cascade 연출
UCLASS()
class FPS_API ARogueCascadeLoopingCueNotify : public AGameplayCueNotify_Looping
{
	GENERATED_BODY()

protected:
	virtual bool WhileActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Cascade")
	TArray<FRogueCascadeCueEntry> Particles;

	// bScaleByRadius 기준 반경
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Cascade", meta = (ClampMin = "1.0"))
	float ReferenceRadius = 250.f;

	// 켜면 Target 에 부착
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Cascade")
	bool bAttachToTarget = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Cascade",
		meta = (EditCondition = "bAttachToTarget", EditConditionHides))
	FName AttachSocket = NAME_None;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UParticleSystemComponent>> SpawnedParticles;

	void StopSpawnedParticles();
};
