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

    // 기본 데미지. 거리/관통 감쇄와 부위 배율까지 적용 완료, 증강 보정 이전
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    float BaseDamage = 0.f;

    // BaseDamage 중 머리에 맞은 몫
    // 샷건은 펠릿이 부위별로 갈리므로 합산하면 헤드 정보가 사라짐
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    float HeadshotDamage = 0.f;

    // 전투팀이 HeadshotDamage 에 이미 곱한 무기 헤드 배율
    // 헤드샷 데미지 복원을 위해 필요
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    float AppliedHeadshotMultiplier = 1.f;

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

	// 발사된 총알 인덱스 (첫 발, 마지막 발 등을 구분)
	UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
	int32 ShotIndexInMagazine = 0;
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

// 사망 결과
USTRUCT(BlueprintType)
struct FRogueDeathContext
{
    GENERATED_BODY()

    // 사망자
    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage")
    TObjectPtr<AActor> VictimActor = nullptr;

    // 처치자. 킬 카운트 / 킬로그
    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage")
    TObjectPtr<AActor> InstigatorActor = nullptr;

    // 투사체 등 물리적 원인
    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage")
    TObjectPtr<AActor> CauserActor = nullptr;

    // 무기 종류
    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage")
    FGameplayTag WeaponTag;

    // 피격 부위
    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage")
    FGameplayTag HitZoneTag;

    // 헤드샷 여부
    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage")
    bool bHeadshot = false;
};