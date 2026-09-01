// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GAS/RogueTargetFilter.h"
#include "RogueEffectApplier.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;

// 페이로드를 통해 GE 를 적용할 대상과 범위, 효과 클래스, Magnitude 를 지정
USTRUCT(BlueprintType)
struct FRogueEffectPayload
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Rogue|Effect")
	ERogueTargetScope Scope = ERogueTargetScope::Single;

	UPROPERTY(BlueprintReadWrite, Category = "Rogue|Effect")
	FRogueTargetQuery Query;

	// Scope 가 Single 일 때 효과를 받을 액터
	UPROPERTY(BlueprintReadWrite, Category = "Rogue|Effect")
	TWeakObjectPtr<AActor> SingleTarget;

	// Scope 가 Radius 일 때 구의 중심
	UPROPERTY(BlueprintReadWrite, Category = "Rogue|Effect")
	FVector Origin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Rogue|Effect")
	float Radius = 0.f;

	// Scope 가 AimSweep 일 때 쓸어갈 방향과 사거리
	UPROPERTY(BlueprintReadWrite, Category = "Rogue|Effect")
	FVector Direction = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Rogue|Effect")
	float Length = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Rogue|Effect")
	TArray<TSubclassOf<UGameplayEffect>> EffectClasses;

	// GE 에 넣을 SetByCaller 수치
	UPROPERTY(BlueprintReadWrite, Category = "Rogue|Effect")
	TMap<FGameplayTag, float> Magnitudes;

	// 켜면 주기 GE 의 Primary 를 틱당 정수로 나눔
	UPROPERTY(BlueprintReadWrite, Category = "Rogue|Effect")
	bool bSplitIntoIntegerTicks = false;

	// 대상당 고정 대미지
	UPROPERTY(BlueprintReadWrite, Category = "Rogue|Effect")
	float FlatDamage = 0.f;

	// 범위 가장자리에서의 대미지 배율. 1 이면 감쇄 없음. Radius 일 때만 적용
	UPROPERTY(BlueprintReadWrite, Category = "Rogue|Effect")
	float FalloffAtEdge = 1.f;

	UPROPERTY(BlueprintReadWrite, Category = "Rogue|Effect")
	FGameplayTag DamageSourceTag;

	// 이벤트 수신자와 히트마커 대상. 폰
	UPROPERTY(BlueprintReadWrite, Category = "Rogue|Effect")
	TWeakObjectPtr<AActor> Instigator;

	// 효과를 거는 주체. PlayerState 에 있어 폰이 죽어도 존재
	TWeakObjectPtr<UAbilitySystemComponent> InstigatorASC;

	// 효과를 실제로 일으킨 액터. 투척물, 드론이면 그 액터, 아니면 시전자
	UPROPERTY(BlueprintReadWrite, Category = "Rogue|Effect")
	TWeakObjectPtr<AActor> Causer;

	UPROPERTY(BlueprintReadWrite, Category = "Rogue|Effect")
	int32 Level = 1;
};

UCLASS()
class FPS_API URogueEffectApplier : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 페이로드가 지정한 대상 전원에게 GE 적용. 서버 전용
	UFUNCTION(BlueprintCallable, Category = "Rogue|Effect")
	static void Apply(const FRogueEffectPayload& Payload);

	// 페이로드가 지정한 대상 수집
	UFUNCTION(BlueprintCallable, Category = "Rogue|Effect")
	static void ResolveTargets(const FRogueEffectPayload& Payload, TArray<AActor*>& OutTargets);

private:
	// 거리 감쇄를 적용해 대미지 서브시스템에 전달
	static void ApplyDamage(const FRogueEffectPayload& Payload, AActor* Instigator, AActor* TargetActor);

	// 주기 GE 의 Primary 를 틱당 정수로, Duration 을 그 틱 수에 맞춰 수정
	static void SplitIntoIntegerTicks(const FGameplayEffectSpecHandle& SpecHandle,
	                                  UClass* EffectClass, int32 Level);
};
