// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RogueTargetFilter.generated.h"

UENUM(BlueprintType)
enum class ERogueTargetRelation : uint8
{
	Invalid     UMETA(DisplayName = "대상 아님"),
	Self        UMETA(DisplayName = "자신"),
	Friendly    UMETA(DisplayName = "아군"),
	Hostile     UMETA(DisplayName = "적"),
	TeamUnknown UMETA(DisplayName = "팀 미확인"),
	MAX			UMETA(Hidden)
};

// 효과를 받을 대상 및 범위
UENUM(BlueprintType)
enum class ERogueTargetScope : uint8
{
	Single   UMETA(DisplayName = "단일 대상"),
	Radius   UMETA(DisplayName = "원점 중심 범위"),
	TeamWide UMETA(DisplayName = "팀 전체"),
};

// 범위 안에서 대상 수집 시 쿼리 조건
USTRUCT(BlueprintType)
struct FRogueTargetQuery
{
	GENERATED_BODY()

	// 시전자 포함 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rogue|Target")
	bool bIncludeSelf = true;

	// 아군 포함 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rogue|Target")
	bool bIncludeFriendly = false;

	// 적군 포함 여부
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rogue|Target")
	bool bIncludeHostile = false;

	// 시전자를 뺀 최대 대상 수. 0 이면 무제한
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Rogue|Target", meta = (ClampMin = "0"))
	int32 MaxTargets = 0;
};

UCLASS()
class FPS_API URogueTargetFilter : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Rogue|Target")
	static ERogueTargetRelation GetRelation(const AActor* Source, const AActor* Target);

	// 생존 여부. State.Combat.IsDead 를 갖고 있지 않으면 true
	UFUNCTION(BlueprintPure, Category = "Rogue|Target")
	static bool IsAlive(const AActor* Target);

	// 쿼리 조건에 맞는지 여부
	UFUNCTION(BlueprintPure, Category = "Rogue|Target")
	static bool PassesQuery(const AActor* Source, const AActor* Target, const FRogueTargetQuery& Query);

	// 시전자 주위 액터 수집
	UFUNCTION(BlueprintCallable, Category = "Rogue|Target")
	static void GatherInRadius(AActor* Source, FVector Origin, float Radius,
	                           const FRogueTargetQuery& Query, TArray<AActor*>& OutTargets);

	// 거리와 무관하게 접속한 플레이어 전원에서 수집
	UFUNCTION(BlueprintCallable, Category = "Rogue|Target")
	static void GatherTeamWide(AActor* Source, const FRogueTargetQuery& Query,
	                           TArray<AActor*>& OutTargets);
};
