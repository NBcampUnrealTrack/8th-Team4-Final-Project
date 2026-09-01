// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Augment/Actor/RogueAugmentActor.h"
#include "RogueThrowableActor.generated.h"

class USphereComponent;
class UProjectileMovementComponent;

// 던져지는 증강 액터
UCLASS(Abstract)
class FPS_API ARogueThrowableActor : public ARogueAugmentActor
{
	GENERATED_BODY()

public:
	ARogueThrowableActor();

protected:
	virtual void BeginPlay() override;

	virtual bool ShouldStartFuseOnBeginPlay() const override;

	// 켜면 착탄 이후 신관 시작
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Throwable")
	bool bStartFuseOnImpact = false;

	// 던진 사람의 이동 속도를 얼마나 물려받을지. 0 이면 상속 없음
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Throwable", meta = (ClampMin = "0.0"))
	float InheritVelocityScale = 1.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rogue|Throwable")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rogue|Throwable")
	TObjectPtr<UProjectileMovementComponent> Movement;

private:
	UFUNCTION()
	void HandleBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

	UFUNCTION()
	void HandleStop(const FHitResult& ImpactResult);

	void HandleImpact();

	bool bImpacted = false;
};
