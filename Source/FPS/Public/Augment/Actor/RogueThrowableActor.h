// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Augment/Actor/RogueAugmentActor.h"
#include "RogueThrowableActor.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class URotatingMovementComponent;
class USoundBase;
class USoundAttenuation;
class UNiagaraSystem;
class UNiagaraComponent;

// 던져지는 증강 액터
UCLASS(Abstract)
class FPS_API ARogueThrowableActor : public ARogueAugmentActor
{
	GENERATED_BODY()

public:
	ARogueThrowableActor();

protected:
	virtual void BeginPlay() override;

	// 회전 속도를 현재 속도에 비례
	virtual void Tick(float DeltaSeconds) override;

	virtual bool ShouldStartFuseOnBeginPlay() const override;

	// 튕길 때 재생할 소리. 볼륨은 충돌 속도에 비례
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Throwable")
	TObjectPtr<USoundBase> BounceSound;

	// 소리 재생 최소 속도. 이보다 느리면 소리 안 남
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Throwable", meta = (ClampMin = "0.0"))
	float MinBounceSoundSpeed = 250.f;

	// 비우면 사운드 에셋 설정을 따름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Throwable")
	TObjectPtr<USoundAttenuation> BounceAttenuation;

	// 비행 중 따라다닐 궤적. 비우면 미사용
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Throwable")
	TObjectPtr<UNiagaraSystem> TrailEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Throwable")
	FVector TrailOffset = FVector::ZeroVector;

	// 켜면 궤적의 User.Team_Color 덮어씀
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Throwable")
	bool bOverrideTrailColor = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Throwable",
		meta = (EditCondition = "bOverrideTrailColor"))
	FLinearColor TrailColor = FLinearColor::White;

	UFUNCTION(BlueprintImplementableEvent, Category = "Rogue|Throwable")
	void OnBounced(const FHitResult& Hit, float ImpactSpeed);

	// 켜면 착탄 이후 신관 시작
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Throwable")
	bool bStartFuseOnImpact = false;

	// 던진 사람의 이동 속도를 얼마나 물려받을지. 0 이면 상속 없음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Throwable", meta = (ClampMin = "0.0"))
	float InheritVelocityScale = 1.f;

	// 보조 입력(우클릭)으로 던졌을 때. 1.0 / 0 이면 주 입력과 같아진다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Throwable", meta = (ClampMin = "0.0"))
	float UnderhandSpeedScale = 0.45f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Throwable")
	float UnderhandPitchOffset = 25.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rogue|Throwable")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rogue|Throwable")
	TObjectPtr<UProjectileMovementComponent> Movement;

	// 비행 중 회전, 부딪히면 멈춤
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rogue|Throwable")
	TObjectPtr<URotatingMovementComponent> Spin;

private:
	UFUNCTION()
	void HandleBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

	UFUNCTION()
	void HandleStop(const FHitResult& ImpactResult);

	void HandleImpact();

	void StartTrail();
	void StopTrail();

	// BP에서 정한 회전 속도
	FRotator BaseSpinRate = FRotator::ZeroRotator;

	bool bImpacted = false;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> TrailComponent;
};
