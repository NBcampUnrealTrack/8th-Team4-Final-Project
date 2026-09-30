// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayCueNotify_Looping.h"
#include "GAS/CueNotify/RogueFxCueNotify.h"
#include "RogueFxLoopingCueNotify.generated.h"

class UParticleSystemComponent;
class UNiagaraComponent;
class UMaterialInterface;
class USkeletalMeshComponent;

// 지속되는 연출
UCLASS()
class FPS_API ARogueFxLoopingCueNotify : public AGameplayCueNotify_Looping
{
	GENERATED_BODY()

protected:
	virtual bool WhileActive_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;
	virtual bool OnRemove_Implementation(AActor* Target, const FGameplayCueParameters& Parameters) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rogue|Fx")
	TArray<FRogueFxCueEntry> Particles;

	// 지속 동안 3인칭 메시에 입힐 오버레이. 비우면 미적용
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rogue|Fx")
	TObjectPtr<UMaterialInterface> OverlayMaterial;

	// bScaleByRadius 기준 반경
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rogue|Fx", meta = (ClampMin = "1.0"))
	float ReferenceRadius = 250.f;

	// 켜면 Target 에 부착
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rogue|Fx")
	bool bAttachToTarget = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rogue|Fx",
		meta = (EditCondition = "bAttachToTarget", EditConditionHides))
	FName AttachSocket = NAME_None;

	// 켜면 현재 무기의 WeaponSocket 에 부착
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rogue|Fx")
	bool bAttachToWeapon = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rogue|Fx",
		meta = (EditCondition = "bAttachToWeapon", EditConditionHides))
	FName WeaponSocket = TEXT("Muzzle");

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UParticleSystemComponent>> SpawnedParticles;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UNiagaraComponent>> SpawnedNiagara;

	// 오버레이를 걸어둔 메시. 해제 때 원복용
	UPROPERTY(Transient)
	TWeakObjectPtr<USkeletalMeshComponent> OverlayMesh;

	void StopSpawnedParticles();
	void SpawnOnWeapon(AActor* Target);
	void ApplyOverlay(AActor* Target);
	void ClearOverlay();
};
