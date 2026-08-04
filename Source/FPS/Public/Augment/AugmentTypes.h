#pragma once

#include "CoreMinimal.h"
#include "AugmentTypes.generated.h"


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