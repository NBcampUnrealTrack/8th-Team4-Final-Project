#pragma once

#include "CoreMinimal.h"
#include "Engine/NetSerialization.h"
#include "GameplayTagContainer.h"
#include "RogueDamageTypes.generated.h"

// 한 히트에서 컨텍스트에 실을 수 있는 부위 그룹 수의 상한
inline constexpr int32 RogueMaxHitGroups = 16;


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

    // 탄 방향
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    FVector ShotDirection = FVector::ZeroVector;

    // 대표 펠릿이 맞은 본
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Damage")
    FName HitBoneName = NAME_None;

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

    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage")
    float LifestealRate = 0.f;
};

// 사망 연출 최소 정보
USTRUCT(BlueprintType)
struct FRogueDeathReaction
{
    GENERATED_BODY()

    // 가해자-피격자 수평 벡터
    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage")
    FVector_NetQuantizeNormal Direction = FVector::ZeroVector;

    // 실제 피격 본
    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage")
    FName HitBoneName = NAME_None;

    // 헤드샷일 경우 배율 임펄스 배율 증가
    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage")
    bool bHeadshot = false;
};

// 생사와 무관하게 매 피격마다 갱신되는 최소 정보. 피격 방향 인디케이터 UI 전용.
USTRUCT(BlueprintType)
struct FRogueHitReactionInfo
{
    GENERATED_BODY()

    // 피격자 -> 가해자 수평 벡터(정규화). "어느 쪽에서 맞았는지" UI가 그대로 쓸 수 있는 방향.
    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage")
    FVector_NetQuantizeNormal Direction = FVector::ZeroVector;

    // 같은 방향에서 연속으로 맞아도 OnRep가 매번 발화하도록 매 피격마다 증가시키는 카운터.
    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Damage")
    uint8 HitSequence = 0;
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

