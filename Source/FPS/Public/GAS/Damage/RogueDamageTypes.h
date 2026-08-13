#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "RogueDamageTypes.generated.h"



USTRUCT(BlueprintType)
struct FRogueHitGroup
{
    GENERATED_BODY()

    // 피격 부위, Hit_Zone_*
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    FGameplayTag HitZoneTag;

    // 이 부위에 맞은 팰릿들의 남은 관통력 배율의 합
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage", meta = (ClampMin = "0.0"))
    float WallRetentionSum = 1.f;

    // 해당 부위에 맞은 팰릿 수
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage", meta = (ClampMin = "1"))
    int32 PelletCount = 1;
};

// 발사 시점에 결정되는 공격자 정보
USTRUCT(BlueprintType)
struct FRogueDamageParams
{
    GENERATED_BODY()

    // 공격자
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    TObjectPtr<AActor> Instigator = nullptr;

    // 공격 액터. 수류탄, 드론 등을 구분하기 위한 데이터
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    TObjectPtr<AActor> Causer = nullptr;

    // 공격 무기 태그, Weapon_Type_*
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    FGameplayTag WeaponTag;

    // 무기, 스킬 등 여부, Damage_Source_*
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    FGameplayTag SourceTag;

    // 현재 공격이 탄창의 몇 번째 탄인지
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage", meta = (ClampMin = "0"))
    int32 ShotIndexInMagazine = 0;
};

// 피격자 정보
USTRUCT(BlueprintType)
struct FRogueHitContext
{
    GENERATED_BODY()

    // 피격자
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    TObjectPtr<AActor> Target = nullptr;

    // 히트 스캔 거리
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage", meta = (ClampMin = "0.0"))
    float DistanceMeters = 0.f;

    // 후방 공격 여부
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    bool bBackstab = false;

    // 부위별 트레이스 모음
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    TArray<FRogueHitGroup> HitGroups;
};

USTRUCT(BlueprintType)
struct FRogueDamageResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage")
    bool bApplied = false;

    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage")
    float ShieldDamage = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage")
    float HealthDamage = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage")
    bool bShieldBroken = false;

    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage")
    bool bHealthDepleted = false;
};

USTRUCT(BlueprintType)
struct FRogueDeathContext
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage")
    TObjectPtr<AActor> VictimActor = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage")
    TObjectPtr<AActor> InstigatorActor = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage")
    TObjectPtr<AActor> CauserActor = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage")
    FGameplayTag WeaponTag;

    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage")
    bool bHeadshot = false;
};



// ----------------------------------------------------------------
// 레거시 코드, FRogueDamageParams + FRogueHitContext 로 대체 후 삭제 예정

USTRUCT(BlueprintType)
struct FRogueHitZoneDamagePart
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    FGameplayTag HitZoneTag;

    // Hitscan Resolver가 거리/관통까지만 반영한 부위별 원본 피해.
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage", meta = (ClampMin = "0.0"))
    float BaseDamage = 0.f;

    // 무기 부위 계수 메타데이터. 실제 곱셈은 ExecCalc_Damage가 수행한다.
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage", meta = (ClampMin = "0.0"))
    float HitZoneMultiplier = 1.f;

    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage", meta = (ClampMin = "1"))
    int32 PelletsHit = 1;

    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    bool bHeadshot = false;
};

// TODO(마이그레이션): FRogueDamageParams + FRogueHitContext 로 대체 후 삭제
USTRUCT(BlueprintType)
struct FRogueDamageRequest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    TObjectPtr<AActor> InstigatorActor = nullptr;

    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    TObjectPtr<AActor> TargetActor = nullptr;

    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    TObjectPtr<AActor> CauserActor = nullptr;

    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    FGameplayTag SourceTag;

    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    FGameplayTag WeaponTag;

    // Hitscan은 거리/관통 감쇄까지만 적용한 값을 넣는다.
    // HitZoneDamageParts가 있으면 ExecCalc_Damage가 부위 배율을 추가 적용한다.
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    float BaseDamage = 0.f;

    // 한 Shot에서 같은 Target에 맞은 부위별 원본 피해. 샷건 혼합 부위를 한 번의 GE로 처리한다.
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    TArray<FRogueHitZoneDamagePart> HitZoneDamageParts;

    // 대표 피격 부위. 혼합 산탄에서는 가장 많은 펠릿이 맞은 부위를 사용한다.
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    FGameplayTag HitZoneTag;

    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    float DistanceMeters = 0.f;

    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    bool bIsBackstab = false;

    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    int32 PelletsHit = 1;

    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    int32 ShotIndexInMagazine = 0;

    // 기존 호출자 호환용. 신규 Hitscan 경로는 HitZoneDamageParts를 사용한다.
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    float HeadshotDamage = 0.f;

    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    float AppliedHeadshotMultiplier = 1.f;
};

