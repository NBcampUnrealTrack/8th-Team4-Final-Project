#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "AugmentTypes.generated.h"

class UAugmentDefinition;
class UGameplayEffect;
class URogueGameplayAbility;


// 증강 등급
UENUM(BlueprintType)
enum class EAugmentTier : uint8
{
    Common    UMETA(DisplayName = "일반"),
    Rare      UMETA(DisplayName = "희귀"),
    Epic      UMETA(DisplayName = "영웅"),
    Legendary UMETA(DisplayName = "전설"),
    MAX       UMETA(Hidden)
};

// 증강 카테고리
UENUM(BlueprintType)
enum class EAugmentCategory : uint8
{
    None            = 0 UMETA(Hidden),
    Passive         = 1 UMETA(DisplayName = "지속효과"),
    Active          = 2 UMETA(DisplayName = "액티브스킬"),
    Util            = 3 UMETA(DisplayName = "전술유틸리티"),
    Weapon_General  = 4 UMETA(DisplayName = "총기공통"),
    Weapon_Rifle    = 5 UMETA(DisplayName = "돌격소총"),
    Weapon_Sniper   = 6 UMETA(DisplayName = "저격소총"),
    Weapon_Shotgun  = 7 UMETA(DisplayName = "산탄총"),
    Weapon_Pistol   = 8 UMETA(DisplayName = "권총"),
    Weapon_Knife    = 9 UMETA(DisplayName = "전술칼")
};

// 조건부 대미지 규칙이 걸리는 방향
UENUM(BlueprintType)
enum class EAugmentDamageRuleSide : uint8
{
    Dealt UMETA(DisplayName = "주는 피해"),
    Taken UMETA(DisplayName = "받는 피해")
};

// 조건부 대미지 규칙
USTRUCT(BlueprintType)
struct FAugmentConditionalDamageRule
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment")
    EAugmentDamageRuleSide Side = EAugmentDamageRuleSide::Dealt;

    // 전부 만족해야 발동. 비어 있으면 태그 조건 없음
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment")
    FGameplayTagContainer RequiredContextTags;

	// 하나라도 있으면 발동 X, 비어 있으면 태그 조건 없음
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment")
    FGameplayTagContainer BlockedContextTags;

    // 무기 종류 조건 (Weapon.Type.*), 비어 있으면 무기 조건 없음
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment", meta = (Categories = "Weapon.Type"))
    FGameplayTagContainer RequiredWeaponTags;

    // 거리 조건.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition")
    bool bUseDistanceRange = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition",
        meta = (EditCondition = "bUseDistanceRange", ClampMin = "0.0"))
    float MinDistanceMeters = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition",
        meta = (EditCondition = "bUseDistanceRange", ClampMin = "0.0"))
    float MaxDistanceMeters = 0.f;

	// 증강이 거리 조건에 따라 스케일링되는지 여부
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition",
        meta = (EditCondition = "bUseDistanceRange"))
    bool bDistanceScalesWithRangeBonus = false;

    // 피해자 체력 조건 (%)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition")
    bool bUseTargetHealthRange = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition",
        meta = (EditCondition = "bUseTargetHealthRange", ClampMin = "0.0", ClampMax = "100.0"))
    float TargetHealthMinPct = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition",
        meta = (EditCondition = "bUseTargetHealthRange", ClampMin = "0.0", ClampMax = "100.0"))
    float TargetHealthMaxPct = 100.f;

    // 가해자 체력 조건 (%)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition")
    bool bUseSourceHealthRange = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition",
        meta = (EditCondition = "bUseSourceHealthRange", ClampMin = "0.0", ClampMax = "100.0"))
    float SourceHealthMinPct = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition",
        meta = (EditCondition = "bUseSourceHealthRange", ClampMin = "0.0", ClampMax = "100.0"))
    float SourceHealthMaxPct = 100.f;

    // 탄창 내 발사 인덱스 조건 ("탄창 첫 3발 대미지 증가")
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition")
    bool bUseShotIndexRange = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition",
        meta = (EditCondition = "bUseShotIndexRange", ClampMin = "0"))
    int32 MinShotIndexInMagazine = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition",
        meta = (EditCondition = "bUseShotIndexRange", ClampMin = "0"))
    int32 MaxShotIndexInMagazine = 0;

    // 조건을 만족했을 때 더하는 피해 증감률. 0.25 면 +25%
    // 상시 증강 GE 의 SetByCaller 와 같은 표기
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect")
    float DamageBonusRate = 0.f;

    // 배율 이후에 더하는 고정값. 경감 규칙은 음수
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect")
    float FlatDamageBonus = 0.f;

    // 전역 LifestealRate 어트리뷰트와 합산되는 조건부 회복
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LifestealRate = 0.f;

    // 헤드 보너스 여부
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect")
    bool bAppliesToHeadshotBonusOnly = false;
};

// 증강 하나가 부여하는 어빌리티
USTRUCT(BlueprintType)
struct FAugmentGrantedAbility
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment")
    TSoftClassPtr<URogueGameplayAbility> AbilityClass;

	// 액티브 증강이 쓸 슬롯. Slot1(Q) / Slot2(E) / Slot3(V)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment",
        meta = (Categories = "InputTag.Ability"))
    FGameplayTag InputTag;

    // 이벤트가 왔을 때 적용할 GE
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Trigger")
    TArray<TSoftClassPtr<UGameplayEffect>> EffectsToApply;

    // 켜면 이벤트가 실어 온 대상에게, 끄면 자신에게 적용
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Trigger")
    bool bApplyToEventTarget = false;

    // 대상에게 걸 때 적만 허용. 아군 오폭 방지
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Trigger",
        meta = (EditCondition = "bApplyToEventTarget"))
    bool bHostileTargetOnly = true;

    // 이 무기로 발생한 이벤트만 발동. 비어 있으면 무기 조건 없음.
    // 대미지 이벤트는 컨텍스트의 무기 태그 사용, 그 외에는 현재 장착 무기 태그 사용
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Trigger",
        meta = (Categories = "Weapon.Type"))
    FGameplayTagContainer RequiredWeaponTags;

    // 교전 거리 조건
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Trigger")
    bool bUseDistanceRange = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Trigger",
        meta = (EditCondition = "bUseDistanceRange", ClampMin = "0.0"))
    float MinDistanceMeters = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Trigger",
        meta = (EditCondition = "bUseDistanceRange", ClampMin = "0.0"))
    float MaxDistanceMeters = 0.f;

    // 증강이 거리 조건에 따라 스케일링되는지 여부
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Trigger",
        meta = (EditCondition = "bUseDistanceRange"))
    bool bDistanceScalesWithRangeBonus = false;
};

// 계열 하나의 수치. 구조체 이름은 애셋 호환을 위해 유지
USTRUCT(BlueprintType)
struct FAugmentLevelData
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Value", meta = (MultiLine = "true"))
    FText Description;

	// 계열이 부여하는 GE 전부에 적용되는 SetByCaller
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Value",
        meta = (Categories = "Data.Magnitude", ForceInlineRow))
    TMap<FGameplayTag, float> SetByCallerMagnitudes;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Value")
    TArray<FAugmentConditionalDamageRule> ConditionalDamageRules;
};

// 등급 추첨 가중치
USTRUCT()
struct FAugmentTierDraftWeight
{
    GENERATED_BODY()

    // 1라운드 가중치
    UPROPERTY(EditAnywhere, Category = "Rogue|Augment")
    float BaseWeight = 0.f;

    // 라운드당 증감
    UPROPERTY(EditAnywhere, Category = "Rogue|Augment")
    float PerRoundDelta = 0.f;
};

// 추첨 입력
USTRUCT(BlueprintType)
struct FAugmentDraftContext
{
    GENERATED_BODY()

    // 1부터. 등급 가중치 스케일
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rogue|Augment")
    int32 RoundNumber = 1;

    // 0 이면 서버가 만들어 로그
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rogue|Augment")
    int32 RoundSeed = 0;

	// 팀 점수, 패배팅 보정, 현재 적용 X
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rogue|Augment")
    int32 TeamScore = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rogue|Augment")
    int32 OpponentScore = 0;
};

// 보유 증강 한 칸
USTRUCT(BlueprintType)
struct FAugmentInventoryEntry
{
    GENERATED_BODY()

    // 정의 애셋 = 계열 하나
    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Augment")
    FPrimaryAssetId DefinitionId;

    // 증강 계열
    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Augment", meta = (Categories = "Augment.Line"))
    FGameplayTag LineTag;

    static FAugmentInventoryEntry Make(const UAugmentDefinition& Definition);
};
