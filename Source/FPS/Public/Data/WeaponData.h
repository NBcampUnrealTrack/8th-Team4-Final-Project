#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameMode/Types/DominationTypes.h"
#include "GameplayTagContainer.h"
#include "WeaponData.generated.h"

class UAnimMontage;
class UAnimSequence;
class UBlendSpace;

USTRUCT(BlueprintType)
struct FPlayerAnims
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimSequence> IdleAnim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimSequence> AimIdleAnim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimSequence> CrouchIdleAnim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimSequence> SprintAnim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UBlendSpace> AimOffset_Hip = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UBlendSpace> AimOffset_Aim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UBlendSpace> Strafe_Standing = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UBlendSpace> Strafe_Crouching = nullptr;
};

USTRUCT(BlueprintType)
struct FWeaponMontageData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimMontage> EquipMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimMontage> ReloadMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimMontage> FireMontage = nullptr;
};

USTRUCT(BlueprintType)
struct FCombatClassWeaponAnims
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FirstPerson")
	TMap<FGameplayTag, FPlayerAnims> FirstPersonAnims;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThirdPerson")
	TMap<FGameplayTag, FPlayerAnims> ThirdPersonAnims;
};

UCLASS(BlueprintType)
class FPS_API UWeaponData : public UDataAsset
{
	GENERATED_BODY()

public:
	const FPlayerAnims* FindPlayerAnims(
		FGameplayTag WeaponTag,
		bool bFirstPerson,
		EDominationCombatClass CombatClass = EDominationCombatClass::Unselected) const;
	const FWeaponMontageData* FindWeaponMontages(FGameplayTag WeaponTag, bool bFirstPerson) const;
	float GetEquipDuration(FGameplayTag WeaponTag) const;
	float GetReloadDuration(FGameplayTag WeaponTag) const;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|WeaponData|Weapons")
	TMap<FGameplayTag, FName> GripPoints;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|WeaponData|Weapons")
	TMap<FGameplayTag, FWeaponMontageData> WeaponMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|WeaponData|FirstPerson")
	TMap<FGameplayTag, FPlayerAnims> FirstPersonAnims;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|WeaponData|ThirdPerson")
	TMap<FGameplayTag, FPlayerAnims> ThirdPersonAnims;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|WeaponData|FirstPerson")
	TMap<FGameplayTag, FWeaponMontageData> FirstPersonMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|WeaponData|ThirdPerson")
	TMap<FGameplayTag, FWeaponMontageData> ThirdPersonMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|WeaponData|CombatClasses")
	TMap<EDominationCombatClass, FCombatClassWeaponAnims> ClassAnimationOverrides;
};
