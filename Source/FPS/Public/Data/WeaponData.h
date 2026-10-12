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

	// Optional local-space additive walk cycle, layered over the idle while walking (first person).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimSequence> WalkAdditiveAnim = nullptr;

	// Ground speed (cm/s) at which WalkAdditiveAnim plays at rate 1 with full weight.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "1.0"))
	float WalkAdditiveReferenceSpeed = 600.f;

	// Optional local-space additive jump/land layer (first person), played by explicit time:
	// 0 -> JumpLandHoldTime = takeoff (held while airborne), JumpLandLandStart -> end = landing.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimSequence> JumpLandAdditiveAnim = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float JumpLandHoldTime = 0.25f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (ClampMin = "0.0"))
	float JumpLandLandStart = 1.f / 3.f;

	// Optional local-space additive fire recoil (third person), played from 0 on every shot seen by
	// other players: shoulders and gun kick back/up while the head stays level.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimSequence> FireRecoilAdditiveAnim = nullptr;

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

	// The matching magazine/bolt action for this view's weapon mesh.
	// Played on the same duration as ReloadMontage, not at an independent rate.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimMontage> WeaponReloadMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimMontage> FireMontage = nullptr;

	// Optional weapon action for observers that do not run the owner gameplay relay.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UAnimMontage> WeaponFireMontage = nullptr;
};

USTRUCT(BlueprintType)
struct FCombatClassWeaponAnims
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FirstPerson")
	TMap<FGameplayTag, FPlayerAnims> FirstPersonAnims;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThirdPerson")
	TMap<FGameplayTag, FPlayerAnims> ThirdPersonAnims;

	// Third-person actions fitted to each class body; first-person and fire routing stay shared.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThirdPerson")
	TMap<FGameplayTag, TObjectPtr<UAnimMontage>> ThirdPersonEquipMontages;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "ThirdPerson")
	TMap<FGameplayTag, TObjectPtr<UAnimMontage>> ThirdPersonReloadMontages;
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
	UAnimMontage* FindEquipMontage(FGameplayTag WeaponTag, bool bFirstPerson,
		EDominationCombatClass CombatClass = EDominationCombatClass::Unselected) const;
	float GetEquipDuration(FGameplayTag WeaponTag) const;
	UAnimMontage* FindReloadMontage(FGameplayTag WeaponTag, bool bFirstPerson,
		EDominationCombatClass CombatClass = EDominationCombatClass::Unselected) const;

	// Draw presentation must use the same rate as the equip readiness timer.
	UFUNCTION(BlueprintPure, Category = "FPS|WeaponData")
	static float GetSwitchAnimationRate(FGameplayTag WeaponTag);
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
