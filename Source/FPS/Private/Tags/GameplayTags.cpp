// Fill out your copyright notice in the Description page of Project Settings.

#include "Tags/GameplayTags.h"


namespace RogueTags
{
	// 데미지 종류
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Source_Weapon,      "Damage.Source.Weapon",      "총기/근접 무기 피해");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Source_Skill,       "Damage.Source.Skill",       "증강 액티브 스킬 피해");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Source_DoT,         "Damage.Source.DoT",         "중독/화상 등 지속 피해");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Source_Environment, "Damage.Source.Environment", "낙하/장애물 등 환경 피해");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Source_SelfCost,    "Damage.Source.SelfCost",    "자기 체력을 대가로 지불");

	// 무기 종류
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Type_None,    "Weapon.Type.None",    "무기 없음"); 
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Type_Rifle,   "Weapon.Type.Rifle",   "돌격소총");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Type_Sniper,  "Weapon.Type.Sniper",  "저격소총");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Type_Shotgun, "Weapon.Type.Shotgun", "산탄총");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Type_Pistol,  "Weapon.Type.Pistol",  "기본 권총");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Type_Knife,       "Weapon.Type.Knife",       "전술 칼");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Type_Knife_Light, "Weapon.Type.Knife.Light", "전술 칼 약공격");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Weapon_Type_Knife_Heavy, "Weapon.Type.Knife.Heavy", "전술 칼 강공격");

	// 히트 문맥
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hit_Zone_Head,  "Hit.Zone.Head",  "머리 피격");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hit_Zone_Torso, "Hit.Zone.Torso", "상체 피격");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hit_Zone_Limb,  "Hit.Zone.Limb",  "사지 피격");

	// 히트 상태
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hit_Angle_Back,          "Hit.Angle.Back",          "배후 공격");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Hit_Instigator_Airborne, "Hit.Instigator.Airborne", "가해자가 공중에 있었음");

	// 가해자, 대상 상태
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Target_State_HasShield,  "Target.State.HasShield",  "대상 보호막 잔존");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Target_State_NoShield,   "Target.State.NoShield",   "대상 보호막 없음");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Source_State_HasShield,  "Source.State.HasShield",  "가해자 보호막 잔존");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Source_State_NoShield,   "Source.State.NoShield",   "가해자 보호막 없음");

	// SetByCaller 키
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Damage_Base, "Data.Damage.Base", "기본 대미지. 거리 감쇄/관통 감쇄/부위 배율 적용 완료, 증강 보정 이전");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Magnitude_Primary,   "Data.Magnitude.Primary",   "증강 주 수치");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Magnitude_Secondary, "Data.Magnitude.Secondary", "증강 부 수치");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Magnitude_Duration,  "Data.Magnitude.Duration",  "증강 지속 시간");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Magnitude_Radius,    "Data.Magnitude.Radius",    "증강 반경");

	// 게임플레이 입력 차단/행동 상태
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Gameplay_InputBlocked, "State.Gameplay.InputBlocked", "게임플레이 입력 차단 부모 태그");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Gameplay_InputBlocked_AugmentDraft, "State.Gameplay.InputBlocked.AugmentDraft", "증강 선택 중 게임플레이 입력 차단");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Gameplay_ActionInProgress, "State.Gameplay.ActionInProgress", "Data Jack 설치/해체와 동시에 진행할 수 없는 GameplayAbility 실행 중");

	// Data Jack 상태
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Combat_DataJack_Carrying, "State.Combat.DataJack.Carrying", "Data Jack 보유 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Combat_DataJack_Interacting, "State.Combat.DataJack.Interacting", "Data Jack 설치/해체 중. 부모 태그");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Combat_DataJack_Interacting_Planting, "State.Combat.DataJack.Interacting.Planting", "Data Jack 설치 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Combat_DataJack_Interacting_Defusing, "State.Combat.DataJack.Interacting.Defusing", "Data Jack 해체 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Combat_DataJack_Dropping, "State.Combat.DataJack.Dropping", "Data Jack 드롭 입력 유지 중");

	// 전투 상태
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Combat_IsAiming,    "State.Combat.IsAiming",    "조준 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Combat_IsReloading, "State.Combat.IsReloading", "재장전 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Combat_IsFiring,    "State.Combat.IsFiring",    "사격 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Combat_IsSwapping,  "State.Combat.IsSwapping",  "무기 교체 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Combat_IsDead,      "State.Combat.IsDead",      "사망 확정");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Combat_Revealed,    "State.Combat.Revealed",    "발각됨");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Combat_Invulnerable, "State.Combat.Invulnerable", "무적. 외부에서 오는 피해를 전부 무시");

	// 이동 상태
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Movement_Sprinting, "State.Movement.Sprinting", "전력 질주 중");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Movement_InAir,     "State.Movement.InAir",     "공중 (점프·낙하)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Movement_Crouching, "State.Movement.Crouching", "웅크림");

	// 현재 장착 무기 상태
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Weapon_Equipped_Firearm,         "State.Weapon.Equipped.Firearm",         "총기 장착(부모 태그)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Weapon_Equipped_Firearm_Rifle,   "State.Weapon.Equipped.Firearm.Rifle",   "돌격소총 장착");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Weapon_Equipped_Firearm_Sniper,  "State.Weapon.Equipped.Firearm.Sniper",  "저격소총 장착");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Weapon_Equipped_Firearm_Shotgun, "State.Weapon.Equipped.Firearm.Shotgun", "산탄총 장착");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Weapon_Equipped_Firearm_Pistol,  "State.Weapon.Equipped.Firearm.Pistol",  "권총 장착");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Weapon_Equipped_Melee,           "State.Weapon.Equipped.Melee",           "근접무기 장착(부모 태그)");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Weapon_Equipped_Melee_Knife,     "State.Weapon.Equipped.Melee.Knife",     "전술 칼 장착");

	// 전투 관련 이벤트
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Kill,              "Event.Combat.Kill",              "처치 확정 -> 처치자");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Death,             "Event.Combat.Death",             "사망 확정 -> 사망자");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_Headshot,          "Event.Combat.Headshot",          "헤드 히트 확정 -> 사격자");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_DamageDealt,       "Event.Combat.DamageDealt",       "대미지 적용 직후 -> 가해자. 샷건은 사격 1회당 대상별 1건만 발신");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_DamageTaken,       "Event.Combat.DamageTaken",       "대미지 수신 직후 -> 피격자");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_ShieldBroken,      "Event.Combat.ShieldBroken",      "보호막 파괴 -> 소유자");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_EnemyShieldBroken, "Event.Combat.EnemyShieldBroken", "적 보호막 파괴 -> 가해자");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Combat_LethalSurvived,    "Event.Combat.LethalSurvived",    "생존 개입 발동 -> 피격자");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Team_AllyDeath,           "Event.Team.AllyDeath",           "팀원 사망 -> 같은 팀 전원");

	// 스킬 관련 이벤트
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Skill_Used, "Event.Skill.Used", "액티브 스킬 시전 확정 -> 시전자");

	// 무기 관련 이벤트
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Weapon_Fired,          "Event.Weapon.Fired",          "매 발사");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Weapon_ReloadStart,    "Event.Weapon.ReloadStart",    "재장전 시작");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Weapon_ReloadComplete, "Event.Weapon.ReloadComplete", "재장전 완료");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Weapon_Swap,           "Event.Weapon.Swap",           "무기 교체");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Weapon_MagazineEmpty,  "Event.Weapon.MagazineEmpty",  "탄창 소진");

	// 이동 관련 이벤트
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Movement_Sprint, "Event.Movement.Sprint", "전력 질주");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Movement_Landed, "Event.Movement.Landed", "착지");

	// 라운드 관련 이벤트
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Round_Start, "Event.Round.Start", "라운드 시작");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Event_Round_End,   "Event.Round.End",   "라운드 종료");

	// 증강 해금 관련 이벤트
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Unlock_Weapon_Rifle_Owned,     "Unlock.Weapon.Rifle.Owned",     "돌격소총 보유");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Unlock_Weapon_Sniper_Owned,    "Unlock.Weapon.Sniper.Owned",    "저격소총 보유");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Unlock_Weapon_Shotgun_Owned,   "Unlock.Weapon.Shotgun.Owned",   "산탄총 보유");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Unlock_Weapon_Pistol_Upgraded, "Unlock.Weapon.Pistol.Upgraded", "권총 업그레이드");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Unlock_Weapon_Knife_Upgraded,  "Unlock.Weapon.Knife.Upgraded",  "칼 업그레이드");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Unlock_Weapon_Firearm_Any,       "Unlock.Weapon.Firearm.Any",       "총기 그룹 중 1개 이상 해금");

	// 증강 계열
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Augment_Line, "Augment.Line", "증강 계열 앵커. 하위 계열 태그는 에디터에서 추가");

	// 효과 수명
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_Lifetime_Round, "Effect.Lifetime.Round", "이번 라운드에만 유지되는 효과. 라운드 시작 시 제거");

	// 상호 배타 효과 그룹
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_Shop_Shield, "Effect.Shop.Shield", "상점 보호막 티어. 새 티어 적용 시 기존 티어를 밀어냄");

	// 어빌리티 입력 슬롯
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(InputTag_Ability, "InputTag.Ability", "어빌리티 입력 슬롯 앵커. 고정 입력 슬롯은 에디터에서 추가");

	// 액티브 증강 쿨타임
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Augment_Dash,       "Cooldown.Augment.Dash",       "대시 쿨타임");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Cooldown_Augment_ReconDrone, "Cooldown.Augment.ReconDrone", "정찰 드론 쿨타임");
}
