#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RogueDamageTypes.generated.h"

USTRUCT(BlueprintType)
struct FRogueDamageRequest
{
    GENERATED_BODY()

    // 가해자
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    TObjectPtr<AActor> InstigatorActor = nullptr;

	// 피해자
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    TObjectPtr<AActor> TargetActor = nullptr;

    // 나중에 알람봇 같은 액터 추가 고려
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    TObjectPtr<AActor> CauserActor = nullptr;

    // 무기, 스킬 여부
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    FGameplayTag SourceTag;

    // 무기 종류
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage") 
    FGameplayTag WeaponTag;

    // 기본 데미지
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    float BaseDamage = 0.f;

	// 공격 위치
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    FGameplayTag HitZoneTag;

	// 공격 거리
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    float DistanceMeters = 0.f;

    // 시야 외부 공격 여부
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage") 
    bool bIsBackstab = false;

    // 샷건 팰릿 수
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    int32 PelletsHit = 1;
};

// 데미지 처리 결과
USTRUCT(BlueprintType)
struct FRogueDamageResult
{
    GENERATED_BODY()

	// 데미지 처리 성공 여부
    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage") 
    bool bApplied = false;

	// 회피 여부
    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage") 
    bool bDodged = false;

	// 방어막 데미지
    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage") 
    float ShieldDamage = 0.f;

	// 체력 데미지
    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage") 
    float HealthDamage = 0.f;

	// 방어막 파괴 여부
    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage") 
    bool bShieldBroken = false;

	// 체력 소진 여부
    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage") 
    bool bHealthDepleted = false;
};