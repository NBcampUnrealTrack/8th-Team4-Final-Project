// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Combat/Hitscan/Types/HitscanShotTypes.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "TimerManager.h"
#include "Weapon/WeaponTypes.h"
#include "WeaponBase.generated.h"

class UCameraComponent;
class UHitscanWeaponDefinition;
class USkeletalMeshComponent;
class UWeaponAimComponent;
class UWeaponEffectsComponent;
struct FHitResult;

// 탄창/예비탄/무한탄약 중 하나라도 바뀔 때마다 브로드캐스트된다. UI(ShooterWeaponStatus)가 매 프레임
// 폴링하는 대신 이 델리게이트를 구독해서 실제로 값이 바뀐 시점에만 갱신하도록 하기 위함이다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWeaponAmmoChanged);


// 무기 발사 상태와 최종 발사 방향 생성까지 담당한다.
// GAS 내부 구현에는 의존하지 않으며, 서버 Hit 판정은 B의 Hitscan Resolver 공개 계약으로 전달한다.
// EffectiveRPM처럼 GAS 쪽 영향이 필요한 값은 외부(GA_RangedAttack)가 계산해서 Setter로 꽂아준다.
UCLASS()
class FPS_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
    // Rebind only the third-person presentation after an asynchronous appearance update.
    void RefreshThirdPersonAttachment() const;
	AWeaponBase();

	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 오너 참조가 늦게 복제되는 경우(부활 직후 클라이언트: 무기 액터/CurrentWeapon 복제가
	// 빙의보다 먼저 도착) AttachToOwnerMeshes가 no-op으로 끝나 무기가 허공에 남는다.
	// 오너가 채워지는 이 시점에 부착/표시를 다시 확정한다.
	virtual void OnRep_Owner() override;

	// RPM 쿨다운 확인 -> 재장전/탄약 확인 -> Spread가 반영된 펠릿별 최종 방향 생성
	// -> B Hitscan Resolver에 사격 1회 전달 -> 반동 적용 -> 연사 카운트 증가.
	// bHeavyAttack: 전술 칼처럼 좌/우클릭 대미지가 다른 무기용. AltBaseDamage가 0이면 무시되고 BaseDamage를 쓴다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	bool Fire(bool bHeavyAttack = false);

	bool FireMelee(bool bHeavyAttack = false, float AnimationRecoverySeconds = 0.f);
	bool FireRanged(float ClientFireServerTime);

	// 훈련장처럼 탄약 소모가 필요 없는 런타임에서만 켠다.
	// true면 발사 자체는 정상 처리하되 CurrentAmmo를 줄이지 않고 재장전도 필요하지 않다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "FPS|Weapon|Ammo")
	void SetInfiniteAmmo(bool bNewInfiniteAmmo);

	// 예비탄 지급은 서버에서만 처리하며 탄창/증강 최대치는 바꾸지 않는다.
	UFUNCTION(BlueprintPure, BlueprintAuthorityOnly, Category = "FPS|Weapon|Ammo")
	bool CanReceiveReserveAmmo(int32 Amount) const;

	// 기본 예비탄 수량을 넘어 누적할 수 있다. 실제 지급량을 반환한다.
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "FPS|Weapon|Ammo")
	int32 AddReserveAmmo(int32 Amount);

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|Ammo")
	bool HasInfiniteAmmo() const { return bUsesAmmo && bInfiniteAmmo; }

	// 무한 탄약에서는 탄창이 비지 않으므로 재장전하지 않는다.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|Ammo")
	bool CanReload() const;

	// 재장전 상태 머신 진입 (Idle -> Reloading). 재장전 중에는 Fire()가 차단된다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void StartReload();

	// 재장전 도중 무기 스왑 등으로 강제 중단할 때 호출한다. 타이머만 취소하고 탄약은 채우지 않는다 -
	// 재장전은 모션이 끝까지 재생돼야만 완료되므로, 중단되면 다시 처음부터 걸어야 한다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void CancelReload();

	// ADS 진입/해제. 탄퍼짐(0으로 고정)과 반동 적용 여부에 영향을 준다.
	// bCanADS가 false인 무기(샷건/칼 등)는 true를 넘겨도 무시되고 계속 지향 사격 상태로 남는다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void SetADS(bool bNewIsADS);

	// 현재 적용 중인 실제 발사속도(RPM)를 갱신한다.
	// 이 값을 누가 계산해서 넣어주는지(GAS 스탯/증강 등)는 AWeaponBase가 알 필요가 없다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void SetEffectiveRPM(float NewRPM);

	// 강공격(bHeavyAttack) 전용 실제 발사속도(RPM)를 갱신한다. AltBaseRPM이 0인 무기는 사용되지 않는다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void SetEffectiveAltRPM(float NewRPM);

	// 이동 상태에 따른 탄퍼짐 배율(정지 1.0 / 걷기 1.5 / 공중,뛰기 2.5 / 브레이킹 성공 1.0)을 외부에서 갱신한다.
	// AWeaponBase는 이동 컴포넌트를 직접 참조하지 않으므로, 호출자(Character/CombatComponent)가 계산해서 넣어준다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void SetMovementSpreadMultiplier(float NewMultiplier);

	// 공중(점프/낙하) 여부. ADS 중이어도 공중이면 탄퍼짐이 강제로 붙도록 Aim 컴포넌트에 위임한다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void SetIsAirborne(bool bNewIsAirborne);

	// 정지(또는 브레이킹 성공)로 취급되는 이동 상태인지. ADS 탄퍼짐이 0으로 고정되려면 이게 true여야
	// 한다 - CombatComponent가 매 틱 이동 속도를 판정해서 갱신한다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void SetIsStationary(bool bNewIsStationary);

	// 앉아 있는지. 앉으면 탄퍼짐에 CrouchSpreadMultiplier가 곱해지도록 Aim 컴포넌트에 위임한다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void SetIsCrouching(bool bNewIsCrouching);

	// 아래 SetEffective* 계열은 전부 같은 규칙: 증강/스탯 보정을 반영해 "지금 이 무기에 실제로
	// 적용할 값"을 외부(GA_RangedAttack/GA_MeleeAttack/GA_Reload)가 URogueAugmentStatResolver로
	// 계산해서 꽂아주는 Setter다. AWeaponBase는 누가 어떻게 계산했는지 몰라도 된다.

	// 현재 적용 중인 실제 탄창 크기.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void SetEffectiveMagazineSize(int32 NewMagazineSize);

	// 탄창 크기를 바꾸고 늘어난(줄어든) 만큼 현재 탄도 조정
	void ResizeMagazine(int32 NewMagazineSize);

	// 예비 탄약 최대치를 바꾸고 차이만큼 예비 탄약 조정. 서버 전용
	void ResizeReserve(int32 NewMaxReserveAmmo);

	// 이 머신의 탄창만 조정 (RPC 없음). 탄창 0 ~ 크기로 제한, 실제 바뀐 양 반환. 증강 예측 GA 용
	int32 AdjustMagazineAmmoLocal(int32 Delta);

	// 현재 적용 중인 실제 재장전 시간(초).
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void SetEffectiveReloadDuration(float NewReloadDuration);

	// 현재 적용 중인 실제 사격 1회당 펠릿 수.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void SetEffectivePelletCount(int32 NewPelletCount);

	// 현재 적용 중인 실제 기본 탄퍼짐 각도(도).
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void SetEffectiveSpreadDegrees(float NewSpreadDegrees);

	// 현재 적용 중인 실제 반동 배율. RecoilCurve 샘플링 값에 곱해진다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void SetEffectiveRecoilMultiplier(float NewMultiplier);

	// 현재 적용 중인 실제 Trace 사거리(cm). 근접무기의 GetFinalMeleeRange 배선용 - 원거리 무기의
	// 사거리/관통(Range/Penetration)은 데미지 파이프라인 쪽에서 별도로 처리하므로 여기서 건드리지 않는다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void SetEffectiveTraceRange(float NewTraceRangeCentimeters);

	// bHeavyAttack: 확인하려는 공격이 강공격인지. 실제 쿨다운 차단 여부는 좌/우 중 마지막으로 발사된
	// 쪽의 간격(LastFireInterval) 기준이므로, 반대쪽 클릭의 쿨다운 중에는 bHeavyAttack 값과 무관하게 막힌다.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	bool CanFireNow(bool bHeavyAttack = false) const;

	// CanFireNow()가 false인 여러 사유(재장전/쿨다운/탄약없음) 중 "탄약이 없어서"만 콕 집어 확인한다.
	// GA_RangedAttack이 빈 탄창 사운드를 재생할지 판단할 때 쓴다.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	bool IsOutOfAmmo() const { return bUsesAmmo && !bInfiniteAmmo && CurrentAmmo <= 0; }

	// 탄창이 빈 상태에서 사격 입력이 들어왔을 때 호출한다. 탄창이 빈 뒤 첫 호출에서만 클릭음을
	// 재생하고 true를 반환한다 - 이후 같은 빈 탄창 상태에서 또 호출되면(이미 한 번 클릭음을 들은
	// 뒤 재입력) 아무 것도 하지 않고 false를 반환한다. 자동 재장전은 하지 않으며, false 반환 뒤
	// 재장전 여부는 호출부(GA_RangedAttack)가 IsEmptyReloadArmed()로 판단한다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	bool TryPlayEmptyClipSound();

	// 기존 WeaponBase.cpp 구현에 대응하는 선언. 재생 시점은 호출자가 결정한다.
	void PlayEquipSound();
	void PlayMeleeAttackSound(bool bHeavyAttack);

	// 발사 입력(InputTag.Ability.Primary)이 떼어졌을 때 컨트롤러가 호출한다. 탄창이 빈 상태에서
	// 클릭음을 이미 들었다면(bEmptyClipNotified), 다음 발사 입력을 "떼었다 다시 누른 새 입력"으로
	// 인정해 재장전을 무장(arm)한다 - 누르고 있는 동안 자동으로 재장전되지 않게 하기 위함.
	void NotifyPrimaryInputReleased();

	// 빈 탄창에서 클릭음을 들은 뒤 발사 입력을 한 번 떼었는지. GA_RangedAttack이 이 값이 true일
	// 때만 재장전을 시작한다. 재장전 시작/완료 시 리셋된다.
	bool IsEmptyReloadArmed() const { return bEmptyReloadArmed; }
	void ConsumeEmptyReloadArm() { bEmptyReloadArmed = false; }

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	float GetBaseRPM() const { return BaseRPM; }

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	float GetEffectiveRPM() const { return EffectiveRPM; }

	// 0이면 이 무기에 강공격 전용 RPM이 없다는 뜻이며, Fire(true)를 호출해도 BaseRPM/EffectiveRPM을 그대로 쓴다.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	float GetAltBaseRPM() const { return AltBaseRPM; }

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	float GetEffectiveAltRPM() const { return EffectiveAltRPM; }

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	bool IsReloading() const { return bIsReloading; }

	// 아래 ADS/탄퍼짐 관련 함수들은 전부 Aim(UWeaponAimComponent)으로 위임하는 얇은 껍데기다.
	// 실제 상태/로직은 Aim이 갖고 있다 - AWeaponBase는 "무기별로 격리된 인스턴스가 필요하다"는
	// 이유만으로 이 컴포넌트를 소유할 뿐, 외부 호출부(GA_ADS/CombatComponent 등)는 이 함수들의
	// 시그니처가 그대로 유지되므로 변경이 필요 없다.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	bool IsADS() const;

	// 반동이 현재 카메라 Pitch에 얹어 둔 양. 3인칭 조준 각도에서 반동을 빼는 데 쓴다.
	float GetAccumulatedRecoilPitch() const;

	// 이 무기가 애초에 ADS를 지원하는지(샷건/칼 등은 false). GA_ADS가 활성화 전에 확인한다.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|ADS")
	bool CanADS() const;

	// ADS 중 목표 카메라 FOV(도). 가변 줌 무기라면 현재 저장된 줌 단계의 FOV를 반환한다.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|ADS")
	float GetADSFieldOfView() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|ADS")
	float GetADSCameraInterpSpeedOverride() const;

	// ADS 중 이동속도에 곱해지는 배율(1=영향 없음). ShooterMovementComponent::GetMaxSpeed가
	// IsADS()일 때만 GetMoveSpeedMultiplier()와 별개로 추가로 곱한다.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|ADS")
	float GetADSMoveSpeedMultiplier() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|ADS")
	bool SupportsVariableADSZoom() const;

	// 양수 입력은 줌 인, 음수 입력은 줌 아웃. 실제 단계 변경은 ADS 중일 때만 허용한다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon|ADS")
	bool AdjustADSZoom(float InputDelta);

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|ADS")
	int32 GetCurrentADSZoomStep() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|ADS")
	int32 GetADSZoomStepCount() const;

	// 스코프 ADS처럼 로컬 1인칭 팔/무기 표현을 숨겨야 하는지 Aim 설정을 그대로 노출한다.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|ADS")
	bool ShouldHideFirstPersonPresentationWhileADS() const;

	// ADS 중 일반 인게임 HUD를 숨겨야 하는지 Aim 설정을 그대로 노출한다.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|ADS")
	bool ShouldHideMainHUDWhileADS() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	int32 GetCurrentAmmo() const { return CurrentAmmo; }

	// 실제 예비탄 수량이다. 무한 탄약에서는 이 값이 발사/재장전에 사용되지 않는다.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	int32 GetCurrentReserveAmmo() const { return CurrentReserveAmmo; }

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	int32 GetMaxReserveAmmo() const { return MaxReserveAmmo; }

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	int32 GetMagazineSize() const { return MagazineSize; }

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	int32 GetEffectiveMagazineSize() const { return EffectiveMagazineSize; }

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	float GetReloadDuration() const { return ReloadDuration; }

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	float GetEffectiveReloadDuration() const { return EffectiveReloadDuration; }

	// 재장전 시작부터 실제 완료(FinishReload)까지의 시간. 몽타주 길이보다 ReloadCompleteLeadTime만큼 짧고,
	// 리드가 재장전 시간의 절반을 넘지 않도록 막는다.
	float GetReloadCompleteDelay() const
	{
		return FMath::Max(EffectiveReloadDuration - ReloadCompleteLeadTime, EffectiveReloadDuration * 0.5f);
	}

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	int32 GetPelletCountPerShot() const { return PelletCountPerShot; }

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	float GetBaseSpreadDegrees() const;

	// 지금 이 순간 실제로 적용 중인 탄퍼짐 각도(도). ADS/연사 블룸/이동 배율/증강까지 전부 반영된 최종값.
	// 크로스헤어 UI(UShooterReticle) 등 외부에서 화면 스프레드를 그리는 용도로 공개한다.
	// 스나이퍼가 비ADS(노줌) 상태면 이동/블룸 계산과 무관하게 항상 SniperUnscopedSpreadDegrees로
	// 고정된다 - 스코프를 껴야만 쓸 수 있는 무기로 만들기 위한 강제 규칙(CanADS()와 같은 패턴).
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	float GetCurrentSpreadDegrees() const;

	// B Hitscan Resolver에 넘어가는 실제 Trace 사거리(cm). GetFinalMeleeRange 배선 전 기본값.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	float GetTraceRange() const { return TraceRange; }

	// 일반 사격과 같은 채널/정밀도/사거리의 읽기 전용 조준 피드백 설정. 발사 상태는 바꾸지 않는다.
	bool GetAimTraceSettings(FHitscanTraceSettings& OutTraceSettings) const;

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	bool UsesAmmo() const { return bUsesAmmo; }

	// 이 무기를 장착 중일 때 캐릭터 이동속도(걷기/스프린트 공통)에 곱해지는 배율. 1이면 영향 없음.
	// ShooterMovementComponent::GetMaxSpeed()가 참고한다(예: 저격소총은 느리게, 칼/권총은 빠르게).
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	float GetMoveSpeedMultiplier() const { return MoveSpeedMultiplier; }

	// true면 이동 상태(걷기/뛰기/브레이킹 실패)에 따른 탄퍼짐 변화를 받지 않는다(예: 산탄총).
	// CombatComponent::UpdateMovementSpreadMultiplier가 참고한다. 공중 페널티는 별개로 유지된다.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|Spread")
	bool IgnoresMovementSpread() const { return bIgnoreMovementSpread; }

	// Hitscan Definition의 Weapon.Type.* 태그를 그대로 노출한다.
	// 스왑/애니메이션/로그가 DamageRequest와 동일한 태그 원본을 사용하게 하기 위한 접근자다.
	// bHeavyAttack이 true고 AltWeaponTag가 채워져 있으면(예: 전술 칼 우클릭) AltWeaponTag를 대신 반환한다 -
	// 좌/우 공격이 서로 다른 Weapon.Type.* 태그로 취급돼야 하는 무기용. 대부분의 총기는 AltWeaponTag가
	// 비어있어 bHeavyAttack 값과 무관하게 항상 기본 태그를 반환한다.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	FGameplayTag GetWeaponTag(bool bHeavyAttack = false) const;

	// 이 무기를 장착했을 때 ASC에 붙는 State.Weapon.Equipped.* 태그(예: State.Weapon.Equipped.Firearm.Pistol).
	// 무기별 BP에서 값을 지정한다. GA_SwitchWeapon과 초기 로드아웃이 CombatComponent 경유로 이 값을 읽어
	// RogueAbilitySystemComponent::SetEquippedWeaponType에 전달한다 (WeaponBase는 ASC를 직접 모른다).
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	FGameplayTag GetEquippedStateTag() const { return EquippedStateTag; }

	// 1인칭(캐릭터 팔)에 부착될 때 쓰는 메쉬. CombatComponent가 장착 시 오너 캐릭터의 Mesh1P에 붙인다.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|Mesh")
	USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }

	// 현재 무기의 1인칭 표현 트리만 켜고 끈다. BP에서 스코프/부착물을 Mesh1P 아래에
	// 자식으로 구성하면 함께 토글된다. Mesh3P에는 영향이 없어 다른 플레이어의 무기 표시는 유지된다.
	void SetFirstPersonPresentationVisible(bool bVisible);

	// 3인칭(다른 플레이어에게 보이는 몸통)에 부착될 때 쓰는 메쉬. CombatComponent가 장착 시 오너 캐릭터의 Mesh3P에 붙인다.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|Mesh")
	USkeletalMeshComponent* GetMesh3P() const { return Mesh3P; }

	// CombatComponent가 Mesh1P를 부착할 때 사용할, 캐릭터 팔 스켈레톤 쪽 소켓 이름
	// (예: 권총 PistolGripPoint, 소총 RifleGripPoint). 무기마다 그립 위치가 다르므로 무기 쪽에서 값을 갖는다.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|Mesh")
	FName GetGripSocketName1P() const { return GripSocketName1P; }

	// CombatComponent가 Mesh3P를 부착할 때 사용할, 캐릭터 바디 스켈레톤 쪽 소켓 이름.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|Mesh")
	FName GetGripSocketName3P() const { return GripSocketName3P; }

	// CombatComponent가 장착 상태를 바꿀 때 호출한다. 액터/Mesh1P/Mesh3P의 Visibility를 토글하고,
	// bActive가 true면 오너(IPlayerInterface)의 Mesh1P/Mesh3P에 GripSocketName1P/3P로 재부착한다.
	// 부착 메쉬/소켓을 자기 자신이 갖고 있으므로 CombatComponent는 활성화 여부만 알려주면 된다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon|Mesh")
	void SetPresentationActive(bool bActive);

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon|Effects")
	FVector LastImpactPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon|Effects")
	FVector LastImpactNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon|Effects")
	TEnumAsByte<EPhysicalSurface> LastImpactSurfaceType = SurfaceType_Default;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon|Effects")
	int32 LastImpactPelletIndex = 0;

	UFUNCTION(BlueprintImplementableEvent, BlueprintCosmetic, Category = "FPS|Weapon|Effects")
	void FireEffects(
		FVector ImpactPoint,
		FVector ImpactNormal,
		EPhysicalSurface ImpactSurfaceType,
		bool bHasHit,
		bool bIsFirstPerson,
		bool bHeadshot);

	// 탄창/예비탄/무한탄약 변경 시점에 브로드캐스트된다. ShooterWeaponStatus 등 탄약 UI가 구독한다.
	UPROPERTY(BlueprintAssignable, Category = "FPS|Weapon|Ammo")
	FOnWeaponAmmoChanged OnAmmoChanged;

protected:
	virtual void BeginPlay() override;
	// 재장전 등 이 무기가 건 타이머를 파괴 시점에 일괄 정리한다.
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// --- 사격 방향 및 B Resolver 연결 ---
	bool GetTraceOrigin(FVector& OutStart) const;
	float GetPhysicalTraceRange(bool bHeavyAttack, bool bAllowLoadDamageData = true) const;
	FVector CalculateTraceDirection(const FVector& AimDirection) const;
	void ResolveHitscanShot(
		const FVector& TraceStart,
		const TArray<FVector>& PelletDirections,
		float ShotDamage,
		int32 InShotIndexInMagazine,
		float ClientFireServerTime,
		bool bHeavyAttack);
	void NotifyBulletNearMisses(const FHitscanShotRequest& Request, const TArray<FHitscanImpactEffectData>& ImpactEffects);

	bool FireInternal(bool bHeavyAttack, float ClientFireServerTime);

	void PlayFireEffects(
		const FVector& TraceStart,
		const TArray<FVector>& PelletDirections,
		const TArray<FHitscanImpactEffectData>& ImpactEffects);

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayFireEffects(const TArray<FHitscanImpactEffectData>& ImpactEffects);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_StopCancelledReload();

	// --- Fire() 단계 분리 ---
	// bHeavyAttack에 따라 이번 발사에 적용할 RPM(Base/Alt)을 고른다.
	float ResolveRPMForShot(bool bHeavyAttack) const;
	// LastFireTime/LastFireInterval을 갱신해 다음 CanFireNow() 쿨다운 기준을 세운다.
	void RegisterFireCooldown(const UWorld& World, float RPMUsed);
	// 이번 발의 탄창 내 인덱스를 반환하고, 일반 탄약 모드에서만 CurrentAmmo를 1 소모한다.
	int32 ConsumeAmmoForShot();
	// bHeavyAttack에 따라 이번 발사에 적용할 기본 대미지(Base/Alt)를 고른다.
	float ResolveShotDamage(bool bHeavyAttack) const;
	// 탄퍼짐이 반영된 펠릿별 최종 발사 방향을 생성한다.
	TArray<FVector> GeneratePelletDirections() const;
	// 연사 카운트 증가 및 탄퍼짐 블룸 감쇠 타이머 리셋. 반동은 여기서 발생시키지 않는다 - ADS 반동은
	// 발당 1회가 아니라 Tick(TickProceduralRecoilGeneration)에서 시간 기반으로 연속 생성된다.
	void AdvanceFireCounters();

	// --- 재장전 ---
	void FinishReload();

	// SetPresentationActive(true)에서 호출. 오너의 Mesh1P/Mesh3P(IPlayerInterface)에
	// GripSocketName1P/3P로 Mesh1P/Mesh3P를 재부착한다.
	void AttachToOwnerMeshes() const;

	TMap<TWeakObjectPtr<class AShooterPlayerController>, double> LastBulletNearMissTimes;

protected:
	// GetEquippedStateTag() 참고. 무기별 BP 기본값에서 지정.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon")
	FGameplayTag EquippedStateTag;

	// 발당 기본 대미지 (관통 감쇠 이전 값)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Fire")
	float BaseDamage = 26.f;

	// 강공격(예: 전술 칼 우클릭) 대미지. 0이면 강공격이 없는 무기로 취급해 BaseDamage를 그대로 쓴다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Fire")
	float AltBaseDamage = 0.f;

	// 강공격 전용 Weapon.Type.* 태그(예: 칼 우클릭의 Weapon.Type.Knife.Heavy). 비어 있으면(대부분의
	// 총기) GetWeaponTag(true)도 기본 태그를 그대로 반환한다 - AltBaseDamage와 동일한 폴백 규칙.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Damage")
	FGameplayTag AltWeaponTag;

	// 사격 1회당 발사되는 펠릿 수. 샷건은 10, 그 외 단발 무기는 1.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Fire")
	int32 PelletCountPerShot = 1;

	// 현재 적용 중인 실제 펠릿 수. SetEffectivePelletCount로만 갱신되며, 기본값은 PelletCountPerShot과 동일하게 초기화된다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "FPS|Weapon|Fire")
	int32 EffectivePelletCount = 1;

	// 탄약/재장전을 사용하는 무기인지 여부. 전술 칼처럼 탄약이 없는 무기는 false로 설정한다.
	// false면 CanFireNow()가 탄약을 확인하지 않고, StartReload()도 아무 동작을 하지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Ammo")
	bool bUsesAmmo = true;

	// GetMoveSpeedMultiplier() 참고. 무기별 실제 값은 기획이 각 무기 BP에서 조정한다(기본값 1 = 무영향).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon", meta = (ClampMin = "0.0"))
	float MoveSpeedMultiplier = 1.f;

	// IgnoresMovementSpread() 참고. 기본값 false = 기존과 동일하게 이동 상태 탄퍼짐을 받는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	bool bIgnoreMovementSpread = false;

	// 스나이퍼가 비ADS(노줌)일 때 강제로 적용되는 탄퍼짐(도). 스나이퍼가 아닌 무기에는 영향 없다.
	// GetCurrentSpreadDegrees() 참고. 실제 값은 기획이 스나이퍼 BP에서 조정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread", meta = (ClampMin = "0.0"))
	float SniperUnscopedSpreadDegrees = 15.f;

	// 무기 자체의 고정 발사속도. 증강/스탯 보정 이전 값.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Fire")
	float BaseRPM = 600.f;

	// 현재 적용 중인 실제 발사속도. SetEffectiveRPM으로만 갱신되며, 기본값은 BaseRPM과 동일하게 초기화된다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "FPS|Weapon|Fire")
	float EffectiveRPM = 600.f;

	// 강공격(예: 전술 칼 우클릭) 전용 발사속도. 0이면 강공격이 없는 무기로 취급해 BaseRPM을 그대로 쓴다
	// (AltBaseDamage와 동일한 규칙). 좌/우 공격속도가 다른 무기만 값을 채운다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Fire")
	float AltBaseRPM = 0.f;

	// 현재 적용 중인 강공격 실제 발사속도. SetEffectiveAltRPM으로만 갱신되며, 기본값은 AltBaseRPM(또는
	// AltBaseRPM이 0이면 BaseRPM)과 동일하게 초기화된다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "FPS|Weapon|Fire")
	float EffectiveAltRPM = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Ammo")
	int32 MagazineSize = 30;

	// 현재 적용 중인 실제 탄창 크기. SetEffectiveMagazineSize로만 갱신되며, 기본값은 MagazineSize와 동일하게 초기화된다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "FPS|Weapon|Ammo")
	int32 EffectiveMagazineSize = 30;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Ammo")
	float ReloadDuration = 2.1f;

	// 현재 적용 중인 실제 재장전 시간(초). SetEffectiveReloadDuration으로만 갱신되며, 기본값은 ReloadDuration과 동일하게 초기화된다.
	UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly, Category = "FPS|Weapon|Ammo")
	float EffectiveReloadDuration = 2.1f;

	// 재장전 몽타주가 끝나기 이만큼(초) 전에 탄약을 채우고 사격 차단을 푼다. 몽타주 길이는
	// EffectiveReloadDuration 그대로라 애니메이션 꼬리 부분은 다음 사격 몽타주가 덮어쓴다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Ammo", meta = (ClampMin = "0.0", Units = "s"))
	float ReloadCompleteLeadTime = 0.2f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon|Ammo")
	int32 CurrentAmmo = 30;

	// 탄창을 채울 때 소모되는 예비(보유) 탄약의 최대치. bUsesAmmo가 false인 무기는 사용하지 않는다.
	// 무기별 실제 값은 기획이 각 무기 BP의 Details 패널에서 조정한다(기본값은 임시 플레이스홀더).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Ammo")
	int32 MaxReserveAmmo = 90;

	// 현재 적용 중인 예비 탄약 최대치. ResizeReserve 로만 갱신
	int32 EffectiveMaxReserveAmmo = 90;

	// 런타임 인스턴스 전용. 기본 게임에서는 false이고 Training에서만 서버가 true로 설정한다.
	// 서버 권한으로만 바뀌므로(SetInfiniteAmmo) OnRep에서 OnAmmoChanged를 브로드캐스트해 클라 UI를 갱신한다.
	UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, ReplicatedUsing = OnRep_InfiniteAmmo, Category = "FPS|Weapon|Ammo")
	bool bInfiniteAmmo = false;

	// 일반 게임에서 재장전 부족분을 채울 때 사용하는 예비탄 수량이다.
	// ResizeReserve처럼 서버 전용 경로로만 바뀌는 경우를 대비해 OnRep에서도 OnAmmoChanged를 브로드캐스트한다.
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CurrentReserveAmmo, Category = "FPS|Weapon|Ammo")
	int32 CurrentReserveAmmo = 90;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "FPS|Weapon|Ammo")
	bool bIsReloading = false;

	// --- B Hitscan Resolver 설정 ---
	// 부위, 거리 감쇄, 연속 관통, Trace 안전 설정을 담는 B 전용 DataAsset.
	// 미지정 시 직접 Hit는 기본 1배 대미지로 처리되며 표면 관통은 비활성화된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Hitscan")
	TObjectPtr<UHitscanWeaponDefinition> HitscanDefinition;

	// B Resolver의 최대 Trace 사거리 (cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Penetration")
	float TraceRange = 4000.f;

	// 현재 적용 중인 실제 Trace 사거리(cm). SetEffectiveTraceRange로만 갱신되며, 기본값은 TraceRange와
	// 동일하게 초기화된다. 근접무기(GA_MeleeAttack)만 GetFinalMeleeRange로 이 값을 갱신하므로,
	// 원거리 무기는 값을 건드리는 호출자가 없어 항상 TraceRange와 같다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "FPS|Weapon|Penetration")
	float EffectiveTraceRange = 4000.f;

	// 발사 카메라 쉐이크를 전담하는 서브 컴포넌트.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Weapon")
	TObjectPtr<UWeaponEffectsComponent> Effects;

	// ADS 상태 + 탄퍼짐(블룸) + ADS 반동을 전담하는 서브 컴포넌트. 셋 다 bIsADS 하나로 서로
	// 배타적으로 갈리는 한 묶음이라 하나로 묶었다. 무기별로 격리돼야 하는 런타임 상태(블룸 카운트,
	// 반동 누적치 등)라 CombatComponent가 아니라 무기 인스턴스가 소유한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Weapon")
	TObjectPtr<UWeaponAimComponent> Aim;

	// --- 메쉬 ---
	// 1인칭(로컬 플레이어 본인 시점) 전용 메쉬. bOnlyOwnerSee로 오너만 본다.
	// CombatComponent가 장착 시 오너 캐릭터의 Mesh1P 산하 GripSocketName1P에 붙인다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Weapon|Mesh")
	TObjectPtr<USkeletalMeshComponent> Mesh1P;

	// 3인칭(다른 플레이어 시점) 전용 메쉬. bOwnerNoSee로 오너 본인에게는 숨긴다.
	// CombatComponent가 장착 시 오너 캐릭터의 Mesh3P 산하 GripSocketName3P에 붙인다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Weapon|Mesh")
	TObjectPtr<USkeletalMeshComponent> Mesh3P;

	// 무기별로 다른 1인칭 그립 소켓 이름 (예: 권총 PistolGripPoint, 소총 RifleGripPoint).
	// 캐릭터 팔 스켈레톤(SK_Mannequin_Arms_Skeleton)에 미리 정의되어 있어야 한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Mesh")
	FName GripSocketName1P;

	// 3인칭 그립 소켓 이름. 대부분의 무기가 공용 소켓을 쓰므로 기본값을 채워둔다.
	// weapon_r "본" 자체가 아니라, 그 본 밑에 자식으로 추가한 "소켓"을 가리켜야 한다 - 본을 직접
	// 가리키면 캐릭터 애니메이션이 매 프레임 그 본의 회전을 덮어써서 소켓 보정 회전이 무시된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Mesh")
	FName GripSocketName3P = TEXT("Weapon_R_Socket");
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Mesh")
	FVector GripOffsetLocation1P = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Mesh")
	FRotator GripOffsetRotation1P = FRotator::ZeroRotator;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Mesh")
	FVector GripOffsetLocation3P = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Mesh")
	FRotator GripOffsetRotation3P = FRotator::ZeroRotator;

private:
	// 사격 위치/방향 계산에 사용하는 카메라. 소유 액터에서 BeginPlay 시 캐싱한다.
	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> CachedOwnerCamera;

	FTimerHandle ReloadTimerHandle;

	// 탄약 소모 여부와 분리된 논리적 탄창 인덱스. 무한 탄약에서도 "탄창 첫 N발" 증강 조건이
	// 매 발 첫 탄으로 오판되지 않도록 0..MagazineSize-1 범위를 순환한다. 재장전 시 0으로 리셋한다.
	int32 ShotIndexInMagazine = 0;

	float LastFireTime = -1000.f;

	// 탄창이 빈 뒤 TryPlayEmptyClipSound()가 클릭음을 이미 재생했는지. 탄약이 다시 채워지면
	// (FinishReload/BeginPlay) 리셋된다.
	bool bEmptyClipNotified = false;

	// 빈 탄창 클릭음을 들은 뒤 발사 입력을 떼었다가 다시 눌러 재장전을 요청할 수 있는 상태인지.
	// NotifyPrimaryInputReleased()에서 true, 재장전 시작(GA_RangedAttack) 및 FinishReload/BeginPlay에서
	// false. 비복제 - bEmptyClipNotified와 동일하게 로컬 입력 판정용이다.
	bool bEmptyReloadArmed = false;

	// 직전 발사(좌/우 클릭 무관)에 적용된 쿨다운 간격(초). 마지막으로 발사된 쪽의 RPM 기준으로 갱신되며,
	// 반대쪽 클릭이라도 이 간격이 지나기 전에는 CanFireNow()가 false를 반환한다(좌우 쿨다운 상호 차단).
	float LastFireInterval = 0.f;

	// 발사 간격 대비 허용 오차 비율. 연사 판정(RegisterFireCooldown)과 서버가 원격 클라이언트의 예측 발사를
	// 받아들이는 조기 도착 여유(CanFireNow)에 함께 쓰인다.
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon|Fire", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float FireIntervalTolerance = 0.3f;

	UFUNCTION()
	void OnRep_CurrentReserveAmmo();

	UFUNCTION()
	void OnRep_InfiniteAmmo();
};
