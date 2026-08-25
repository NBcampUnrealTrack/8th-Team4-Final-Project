// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"


// TAG_WeaponType_Rifle / Pistol 과 Weapon_Type_Rifle / Pistol 은 같은 태그 문자열을 가리키고 있습니다.
// 하나로 통합하는 것이 좋아보이지만, 해당 머지에서는 두 태그 다 남기는 것으로 유지하겠습니다.
namespace RogueTags
{
	// 데미지 종류
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Source_Weapon);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Source_Skill);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Source_DoT);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Source_Environment);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage_Source_SelfCost);

	// 무기 종류
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Type_None);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Type_Rifle);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Type_Sniper);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Type_Shotgun);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Type_Pistol);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Type_Knife);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Type_Knife_Light);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Weapon_Type_Knife_Heavy);

	// 히트 부위
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Hit_Zone_Head);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Hit_Zone_Torso);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Hit_Zone_Limb);

	// 히트 상태
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Hit_Angle_Back);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Hit_Instigator_Airborne);

	// 가해자, 대상 상태
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Target_State_HasShield);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Target_State_NoShield);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Source_State_HasShield);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Source_State_NoShield);

	// SetByCaller 키
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Damage_Base);

	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Magnitude_Primary);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Magnitude_Secondary);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Magnitude_Duration);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Data_Magnitude_Radius);

	// 게임플레이 입력 차단/행동 상태
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Gameplay_InputBlocked);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Gameplay_InputBlocked_AugmentDraft);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Gameplay_ActionInProgress);

	// Data Jack 상태
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat_DataJack_Carrying);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat_DataJack_Interacting);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat_DataJack_Interacting_Planting);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat_DataJack_Interacting_Defusing);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat_DataJack_Dropping);

	// 전투 상태
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat_IsAiming);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat_IsReloading);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat_IsFiring);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat_IsSwapping);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat_IsDead);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat_Revealed);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Combat_Invulnerable);

	// 이동 상태
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Movement_Sprinting);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Movement_InAir);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Movement_Crouching);

	// 현재 장착 무기 상태
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Weapon_Equipped_Firearm);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Weapon_Equipped_Firearm_Rifle);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Weapon_Equipped_Firearm_Sniper);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Weapon_Equipped_Firearm_Shotgun);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Weapon_Equipped_Firearm_Pistol);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Weapon_Equipped_Melee);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(State_Weapon_Equipped_Melee_Knife);

	// 전투 관련 이벤트
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Kill);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Death);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_Headshot);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_DamageDealt);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_DamageTaken);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_ShieldBroken);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_EnemyShieldBroken);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Combat_LethalSurvived);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Team_AllyDeath);

	// 스킬 관련 이벤트
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Skill_Used);

	// 무기 관련 이벤트
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Weapon_Fired);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Weapon_ReloadStart);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Weapon_ReloadComplete);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Weapon_Swap);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Weapon_MagazineEmpty);

	// 이동 관련 이벤트
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Movement_Sprint);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Movement_Landed);

	// 라운드 관련 이벤트
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Round_Start);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Event_Round_End);

	// 무기 잠금 해제 상태
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Unlock_Weapon_Rifle_Owned);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Unlock_Weapon_Sniper_Owned);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Unlock_Weapon_Shotgun_Owned);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Unlock_Weapon_Pistol_Upgraded);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Unlock_Weapon_Knife_Upgraded);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Unlock_Weapon_Firearm_Any);

	// 증강 계열 앵커
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Augment_Line);

	// 효과 수명
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Lifetime_Round);

	// 상호 배타 효과 그룹 
	// 같은 태그를 가진 GE 는 Remove Other Effects 동시에 적용 시 기존의 것을 삭제
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Effect_Shop_Shield);

	// 어빌리티 입력 슬롯 앵커
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Ability);

	// 액티브 증강 슬롯. 컨트롤러 바인딩 전용
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Ability_Slot1);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Ability_Slot2);

	// 증강 애셋이 다는 표식
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(InputTag_Ability_AugmentSlot);

	// 액티브 증강 쿨타임
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Augment_Dash);
	FPS_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Cooldown_Augment_ReconDrone);

	// 부위 태그가 머리인지 판정, 동일한 판정 기준을 유지하기 위해 정의
	FORCEINLINE bool IsHeadZone(const FGameplayTag& HitZoneTag)
	{
		return HitZoneTag.MatchesTag(Hit_Zone_Head);
	}

	// 액티브 증강 입력 슬롯 수
	inline constexpr int32 ActiveInputSlotCount = 2;

	// 슬롯 인덱스에 해당하는 액티브 증강 입력 슬롯 태그 반환
	FPS_API FGameplayTag GetActiveInputSlot(int32 SlotIndex);
}
