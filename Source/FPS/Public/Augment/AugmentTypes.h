#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GAS/RogueTargetFilter.h"
#include "UObject/PrimaryAssetId.h"
#include "AugmentTypes.generated.h"

class ARogueAugmentActor;
class UAugmentDefinition;
class UGameplayEffect;
class URogueGameplayAbility;
class UStaticMesh;


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

// 후보를 어느 풀에서 뽑는지
UENUM(BlueprintType)
enum class EAugmentRewardPool : uint8
{
    // 액티브, 패시브 둘 다 추첨
    Any     UMETA(DisplayName = "전체"),

    // 패시브만, 등급을 기준으로 추첨
    Passive UMETA(DisplayName = "패시브"),

    // 액티브만, 입력 슬롯으로 추첨
    Active  UMETA(DisplayName = "액티브")
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
        meta = (EditCondition = "bUseDistanceRange", EditConditionHides, ClampMin = "0.0"))
    float MinDistanceMeters = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition",
        meta = (EditCondition = "bUseDistanceRange", EditConditionHides, ClampMin = "0.0"))
    float MaxDistanceMeters = 0.f;

	// 증강이 거리 조건에 따라 스케일링되는지 여부
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition",
        meta = (EditCondition = "bUseDistanceRange", EditConditionHides))
    bool bDistanceScalesWithRangeBonus = false;

    // 0 보다 크면 DamageBonusRate 를 교전 거리 N m 마다 한 번씩 더함 (거리 10m당 +5% 등)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition",
        meta = (EditCondition = "bUseDistanceRange", EditConditionHides, ClampMin = "0.0"))
    float DamageBonusDistanceStep = 0.f;

    // 피해자 체력 조건 (%)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition")
    bool bUseTargetHealthRange = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition",
        meta = (EditCondition = "bUseTargetHealthRange", EditConditionHides, ClampMin = "0.0", ClampMax = "100.0"))
    float TargetHealthMinPct = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition",
        meta = (EditCondition = "bUseTargetHealthRange", EditConditionHides, ClampMin = "0.0", ClampMax = "100.0"))
    float TargetHealthMaxPct = 100.f;

    // 가해자 체력 조건 (%)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition")
    bool bUseSourceHealthRange = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition",
        meta = (EditCondition = "bUseSourceHealthRange", EditConditionHides, ClampMin = "0.0", ClampMax = "100.0"))
    float SourceHealthMinPct = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition",
        meta = (EditCondition = "bUseSourceHealthRange", EditConditionHides, ClampMin = "0.0", ClampMax = "100.0"))
    float SourceHealthMaxPct = 100.f;

    // 탄창 내 발사 인덱스 조건 ("탄창 첫 3발 대미지 증가")
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition")
    bool bUseShotIndexRange = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition",
        meta = (EditCondition = "bUseShotIndexRange", EditConditionHides, ClampMin = "0"))
    int32 MinShotIndexInMagazine = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition",
        meta = (EditCondition = "bUseShotIndexRange", EditConditionHides, ClampMin = "0"))
    int32 MaxShotIndexInMagazine = 0;

    // 0 보다 크면 탄창의 N 번째 발마다 발동 (매 5발마다 등)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition",
        meta = (EditCondition = "bUseShotIndexRange", EditConditionHides, ClampMin = "0"))
    int32 ShotIndexInterval = 0;

    // 발동 확률 조건. 켜면 다른 조건을 통과해도 이 확률로만 발동
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition")
    bool bUseActivationChance = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Condition",
        meta = (EditCondition = "bUseActivationChance", EditConditionHides, ClampMin = "0.0", ClampMax = "1.0"))
    float ActivationChance = 1.f;

    // 조건을 만족했을 때 더하는 피해 증감률. 0.25 면 +25%
    // 상시 증강 GE 의 SetByCaller 와 같은 표기
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect")
    float DamageBonusRate = 0.f;

    // 배율 이후에 더하는 고정값. 경감 규칙은 음수
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect")
    float FlatDamageBonus = 0.f;

    // 무작위 배율. 켜면 발동 시 Min ~ Max 사이 배율을 피해에 곱함 (증감률 합산과 별도)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect")
    bool bUseRandomMultiplier = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect",
        meta = (EditCondition = "bUseRandomMultiplier", EditConditionHides, ClampMin = "0.0"))
    float RandomMultiplierMin = 1.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect",
        meta = (EditCondition = "bUseRandomMultiplier", EditConditionHides, ClampMin = "0.0"))
    float RandomMultiplierMax = 1.f;

    // 켜면 발동 시 이번 피해를 헤드샷으로 처리 (모든 부위를 머리 배율로, 헤드샷 이벤트 발신)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect")
    bool bForceHeadshot = false;

    // 전역 LifestealRate 어트리뷰트와 합산되는 조건부 회복
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect",
        meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float LifestealRate = 0.f;

    // 발동 시 준 피해 대비 자해 비율. 최소 체력 1 은 남김 (0.05 면 준 피해의 5%)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect", meta = (ClampMin = "0.0"))
    float SelfDamageRate = 0.f;

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
        meta = (EditCondition = "bApplyToEventTarget", EditConditionHides))
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
        meta = (EditCondition = "bUseDistanceRange", EditConditionHides, ClampMin = "0.0"))
    float MinDistanceMeters = 0.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Trigger",
        meta = (EditCondition = "bUseDistanceRange", EditConditionHides, ClampMin = "0.0"))
    float MaxDistanceMeters = 0.f;

    // 증강이 거리 조건에 따라 스케일링되는지 여부
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Trigger",
        meta = (EditCondition = "bUseDistanceRange", EditConditionHides))
    bool bDistanceScalesWithRangeBonus = false;

    // 효과를 받을 대상 범위. Single 이면 bApplyToEventTarget 이 결정
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Area")
    ERogueTargetScope TargetScope = ERogueTargetScope::Single;

    // 반경은 레벨 수치의 Data.Magnitude.Radius
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Area",
        meta = (EditCondition = "TargetScope != ERogueTargetScope::Single", EditConditionHides))
    FRogueTargetQuery TargetQuery;

    // 범위의 중심을 이벤트 대상에 둘지 여부, 끄면 시전자 위치
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Area",
        meta = (EditCondition = "TargetScope == ERogueTargetScope::Radius", EditConditionHides))
    bool bAreaCentersOnEventTarget = true;

    // 주기 GE 의 Primary 를 틱당 정수로 나눌지 여부
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect")
    bool bSplitIntoIntegerTicks = false;

    // 범위 가장자리에서의 대미지 배율. 1 이면 감쇄 없음. 대미지는 Data.Magnitude.Damage
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Effect",
        meta = (EditCondition = "TargetScope == ERogueTargetScope::Radius", EditConditionHides, ClampMin = "0.0", ClampMax = "1.0"))
    float DamageFalloffAtEdge = 1.f;

    // 페이로드를 들고 갈 액터. 투척물/설치물 증강만 사용
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Spawn")
    TSoftClassPtr<ARogueAugmentActor> SpawnActorClass;
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

// 손에 든 동안 보여줄 메시
USTRUCT(BlueprintType)
struct FAugmentHeldVisual
{
    GENERATED_BODY()

    // 메시가 없으면 몽타주만 재생
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Held")
    TSoftObjectPtr<UStaticMesh> Mesh;

    // 1인칭 부착 소켓 이름
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Held")
    FName GripSocketName1P;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Held")
    FVector GripOffsetLocation1P = FVector::ZeroVector;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Held")
    FRotator GripOffsetRotation1P = FRotator::ZeroRotator;

    // 1인칭에서 들었을 때 크기
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Held")
    FVector GripScale1P = FVector::OneVector;

    // 3인칭 부착 소켓 이름
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Held")
    FName GripSocketName3P = TEXT("Weapon_R_Socket");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Held")
    FVector GripOffsetLocation3P = FVector::ZeroVector;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Held")
    FRotator GripOffsetRotation3P = FRotator::ZeroRotator;

    // 3인칭에서 들었을 때 크기
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rogue|Augment|Held")
    FVector GripScale3P = FVector::OneVector;
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

    UPROPERTY(BlueprintReadOnly, Category = "Rogue|Augment", meta = (Categories = "InputTag.Ability"))
    FGameplayTag ActiveInputSlot;

    static FAugmentInventoryEntry Make(const UAugmentDefinition& Definition);
};

// 적립된 증강 보상 후보
USTRUCT(BlueprintType)
struct FAugmentPendingReward
{
    GENERATED_BODY()

    // 패시브, 액티브 여부
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Augment")
    EAugmentRewardPool Pool = EAugmentRewardPool::Passive;

	// 증강 등급, 액티브는 슬롯으로 추첨하므로 무시
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Augment")
    EAugmentTier Tier = EAugmentTier::Common;

    // 액티브 증강 슬롯
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Augment", meta = (Categories = "InputTag.Ability"))
    FGameplayTag ActiveSlot;

    // 시드와 로그용
    UPROPERTY(BlueprintReadWrite, Category = "Rogue|Augment")
    int32 RoundNumber = 0;
};
