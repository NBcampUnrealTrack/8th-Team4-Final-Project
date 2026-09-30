#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "GameMode/Types/DominationTypes.h"
#include "DominationClassSettings.generated.h"

class AShooterCharacter;
class USoundBase;
class UTexture2D;
class UGameplayAppearanceConfig;

/**
 * Domination 전투 클래스 하나의 구성 정보다.
 * 클래스 식별자와 Pawn, 주무기, UI 표시 정보를 한 곳에서 관리한다.
 */
USTRUCT(BlueprintType)
struct FPS_API FDominationCombatClassDefinition
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Domination|Class")
	EDominationCombatClass CombatClass = EDominationCombatClass::Unselected;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Domination|Class")
	TSoftClassPtr<AShooterCharacter> PawnClass;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Domination|Class", meta = (Categories = "Weapon.Type"))
	FGameplayTag PrimaryWeaponTag;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Domination|Class")
	FText DisplayName;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Domination|Class")
	FText Description;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Domination|Class")
	TSoftObjectPtr<UTexture2D> Icon;

	// 클래스 선택 화면의 팀원 슬롯용 소형 초상화
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Domination|Class")
	TSoftObjectPtr<UTexture2D> Portrait;

	// 특성 태그
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Domination|Class")
	TArray<FText> TraitTags;

	// 클래스 선택 카드의 큰 무기 렌더
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Domination|Class")
	TSoftObjectPtr<UTexture2D> WeaponArt;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Domination|Class")
	TSoftObjectPtr<USoundBase> SelectionVoice;
};

/**
 * Domination 전투 클래스의 단일 정의 지점이다.
 * 클래스 추가/변경 시 GameMode, Loadout, PlayerState, UI가 각각 매핑을 소유하지 않고
 * 이 설정을 조회한다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Domination Classes"))
class FPS_API UDominationClassSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UDominationClassSettings();

	const FDominationCombatClassDefinition* FindDefinition(
		EDominationCombatClass CombatClass) const;

	bool IsSelectableClass(EDominationCombatClass CombatClass) const;
	bool HasValidDefinitions() const;
	UClass* ResolvePawnClass(
		EDominationCombatClass CombatClass) const;

	UPROPERTY(Config, EditAnywhere, Category = "Class")
	EDominationCombatClass DefaultCombatClass = EDominationCombatClass::Trooper;

	UPROPERTY(Config, EditAnywhere, Category = "Class")
	TArray<FDominationCombatClassDefinition> CombatClasses;

	// 기존 클래스 행과 독립적으로 보관해 이전 설정 파일에서도 기본 카탈로그를 유지한다.
	UPROPERTY(Config, EditAnywhere, Category = "Class|Appearance")
	TMap<FName, TSoftObjectPtr<UGameplayAppearanceConfig>> GameplayAppearanceConfigs;
};
