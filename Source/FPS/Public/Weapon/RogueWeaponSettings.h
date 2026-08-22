#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Weapon/WeaponTypes.h"

#include "RogueWeaponSettings.generated.h"

class AWeaponBase;

USTRUCT(BlueprintType)
struct FRoguePrimaryWeaponEntry
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Weapon")
	EWeaponType WeaponType = EWeaponType::None;

	UPROPERTY(Config, EditAnywhere, Category = "Weapon")
	TSoftClassPtr<AWeaponBase> WeaponClass;
};

/**
 * 주무기(AR/SR/SG) ID를 스폰 클래스로 해석한다.
 *
 * WeaponInventory는 폰 수명과 무관해야 하므로 EWeaponType만 저장하고, 클래스 해석은 여기를 거친다.
 * 권총과 칼은 상점이 지급하는 대상이 아니므로 여기 없다.
 * 그 둘의 클래스는 ShooterCharacter의 DefaultSecondaryWeaponClass / DefaultMeleeWeaponClass가
 * 단독으로 소유한다.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Weapon Registry"))
class FPS_API URogueWeaponSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	URogueWeaponSettings();

	TSubclassOf<AWeaponBase> ResolveWeaponClass(EWeaponType WeaponType) const;

	UPROPERTY(Config, EditAnywhere, Category = "Catalog")
	TArray<FRoguePrimaryWeaponEntry> PrimaryWeapons;
};
