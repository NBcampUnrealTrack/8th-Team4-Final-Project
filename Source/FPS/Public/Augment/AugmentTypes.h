#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "AugmentTypes.generated.h"

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
    Stat            UMETA(DisplayName = "신체강화"),
    Passive         UMETA(DisplayName = "지속효과"),
    Active          UMETA(DisplayName = "액티브스킬"),
    Util            UMETA(DisplayName = "전술유틸리티"),
    Weapon_General  UMETA(DisplayName = "총기공통"),
    Weapon_Rifle    UMETA(DisplayName = "돌격소총"),
    Weapon_Sniper   UMETA(DisplayName = "저격소총"),
    Weapon_Shotgun  UMETA(DisplayName = "산탄총"),
    Weapon_Pistol   UMETA(DisplayName = "권총"),
    Weapon_Knife    UMETA(DisplayName = "전술칼"),
    MAX             UMETA(Hidden)
};

// 조건부 대미지 규칙이 걸리는 방향
UENUM(BlueprintType)
enum class EAugmentDamageRuleSide : uint8
{
    Dealt UMETA(DisplayName = "주는 피해"),
    Taken UMETA(DisplayName = "받는 피해"),
    MAX   UMETA(Hidden)
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

    // 조건을 만족했을 때 곱하는 배율. 1.0 이면 변화 없음
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect", meta = (ClampMin = "0.0"))
    float DamageMultiplier = 1.f;

    // 배율 이후에 더하는 고정값. 경감 규칙은 음수
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect")
    float FlatDamageBonus = 0.f;
    // 헤드 보너스 여부
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect")
    bool bAppliesToHeadshotBonusOnly = false;
};

// 증강 하나가 부여하는 GE
USTRUCT(BlueprintType)
struct FAugmentGrantedEffect
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment")
    TSoftClassPtr<UGameplayEffect> EffectClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment",
        meta = (Categories = "Data.Magnitude", ForceInlineRow))
    TMap<FGameplayTag, float> SetByCallerMagnitudes;
};

// 증강 하나가 부여하는 어빌리티
USTRUCT(BlueprintType)
struct FAugmentGrantedAbility
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment")
    TSoftClassPtr<URogueGameplayAbility> AbilityClass;

    // 이 어빌리티를 발동시키는 입력 슬롯 (InputTag.Ability.*).
    // 이벤트로 발동하는 어빌리티는 입력 X
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment",
        meta = (Categories = "InputTag.Ability"))
    FGameplayTag InputTag;
};

// 보유 증강 한 칸
USTRUCT(BlueprintType)
struct FAugmentInventoryEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Augment")
    FPrimaryAssetId DefinitionId;

    bool operator==(const FAugmentInventoryEntry& Other) const
    {
        return DefinitionId == Other.DefinitionId;
    }
};
