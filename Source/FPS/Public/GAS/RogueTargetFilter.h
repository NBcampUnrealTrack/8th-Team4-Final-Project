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

UCLASS()
class FPS_API URogueTargetFilter : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Rogue|Target")
	static ERogueTargetRelation GetRelation(const AActor* Source, const AActor* Target);

	// 사망 태그 보유 여부
	UFUNCTION(BlueprintPure, Category = "Rogue|Target")
	static bool IsAlive(const AActor* Target);
};
