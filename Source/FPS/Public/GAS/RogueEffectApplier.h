// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GAS/RogueTargetFilter.h"
#include "RogueEffectApplier.generated.h"

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

	UPROPERTY(BlueprintReadWrite, Category = "Rogue|Effect")
	TArray<TSubclassOf<UGameplayEffect>> EffectClasses;

	// GE 에 넣을 SetByCaller 수치
	UPROPERTY(BlueprintReadWrite, Category = "Rogue|Effect")
	TMap<FGameplayTag, float> Magnitudes;

	// 켜면 주기 GE 의 Primary 를 틱당 정수로 나눔
	UPROPERTY(BlueprintReadWrite, Category = "Rogue|Effect")
	bool bSplitIntoIntegerTicks = false;

	UPROPERTY(BlueprintReadWrite, Category = "Rogue|Effect")
	TWeakObjectPtr<AActor> Instigator;

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
	// 주기 GE 의 Primary 를 틱당 정수로, Duration 을 그 틱 수에 맞춰 수정
	static void SplitIntoIntegerTicks(const FGameplayEffectSpecHandle& SpecHandle,
	                                  UClass* EffectClass, int32 Level);
};
