// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "TimerManager.h"
#include "Weapon/WeaponTypes.h"
#include "WeaponBase.generated.h"

class UCameraComponent;
class UCurveVector;
class UDataTable;
class UHitscanWeaponDefinition;
class USkeletalMeshComponent;
class UWeaponEffectsComponent;
struct FHitResult;


// 무기 발사 상태와 최종 발사 방향 생성까지 담당한다.
// GAS 내부 구현에는 의존하지 않으며, 서버 Hit 판정은 B의 Hitscan Resolver 공개 계약으로 전달한다.
// EffectiveRPM처럼 GAS 쪽 영향이 필요한 값은 외부(GA_RangedAttack)가 계산해서 Setter로 꽂아준다.
UCLASS()
class FPS_API AWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AWeaponBase();

	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 이전 펠릿별 Hit 이벤트와의 블루프린트 호환을 위해 유지한다.
	// 현재 서버 판정 경로는 B의 UHitscanResolverSubsystem을 사용하며 이 이벤트는 더 이상 발신하지 않는다.
	UPROPERTY(BlueprintAssignable, Category = "FPS|Weapon")
	FOnWeaponHit OnWeaponHit;

	// RPM 쿨다운 확인 -> 재장전/탄약 확인 -> Spread가 반영된 펠릿별 최종 방향 생성
	// -> B Hitscan Resolver에 사격 1회 전달 -> 반동 적용 -> 연사 카운트 증가.
	// bHeavyAttack: 전술 칼처럼 좌/우클릭 대미지가 다른 무기용. AltBaseDamage가 0이면 무시되고 BaseDamage를 쓴다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	bool Fire(bool bHeavyAttack = false);

	// 재장전 상태 머신 진입 (Idle -> Reloading). 재장전 중에는 Fire()가 차단된다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void StartReload();

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

	// 아래 SetEffective* 계열은 전부 같은 규칙: 증강/스탯 보정을 반영해 "지금 이 무기에 실제로
	// 적용할 값"을 외부(GA_RangedAttack/GA_MeleeAttack/GA_Reload)가 URogueAugmentStatResolver로
	// 계산해서 꽂아주는 Setter다. AWeaponBase는 누가 어떻게 계산했는지 몰라도 된다.

	// 현재 적용 중인 실제 탄창 크기.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void SetEffectiveMagazineSize(int32 NewMagazineSize);

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
	bool IsOutOfAmmo() const { return bUsesAmmo && CurrentAmmo <= 0; }

	// 탄약이 없어 발사가 막혔을 때 호출한다. 실제 RPM 간격과 같은 쿨다운으로 스스로 스팸을 막으므로
	// (CanActivateAbility는 트리거를 누르고 있는 동안 매 프레임 재평가되지만) 호출자가 별도로 디바운스할
	// 필요가 없다.
	UFUNCTION(BlueprintCallable, Category = "FPS|Weapon")
	void TryPlayEmptyClipSound();

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

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	bool IsADS() const { return bIsADS; }

	// 이 무기가 애초에 ADS를 지원하는지(샷건/칼 등은 false). GA_ADS가 활성화 전에 확인한다.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|ADS")
	bool CanADS() const { return bCanADS; }

	// ADS 중 목표 카메라 FOV(도). CombatComponent의 카메라 줌 보간이 참고한다.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|ADS")
	float GetADSFieldOfView() const { return ADSFieldOfView; }

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	int32 GetCurrentAmmo() const { return CurrentAmmo; }

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	int32 GetMagazineSize() const { return MagazineSize; }

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	int32 GetEffectiveMagazineSize() const { return EffectiveMagazineSize; }

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	float GetReloadDuration() const { return ReloadDuration; }

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	float GetEffectiveReloadDuration() const { return EffectiveReloadDuration; }

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	int32 GetPelletCountPerShot() const { return PelletCountPerShot; }

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	float GetBaseSpreadDegrees() const { return BaseSpreadDegrees; }

	// 지금 이 순간 실제로 적용 중인 탄퍼짐 각도(도). ADS/연사 블룸/이동 배율/증강까지 전부 반영된 최종값.
	// 크로스헤어 UI(UShooterReticle) 등 외부에서 화면 스프레드를 그리는 용도로 공개한다.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	float GetCurrentSpreadDegrees() const { return CalculateCurrentSpreadDegrees(); }

	// B Hitscan Resolver에 넘어가는 실제 Trace 사거리(cm). GetFinalMeleeRange 배선 전 기본값.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	float GetTraceRange() const { return TraceRange; }

	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	bool UsesAmmo() const { return bUsesAmmo; }

	// Hitscan Definition의 Weapon.Type.* 태그를 그대로 노출한다.
	// 스왑/애니메이션/로그가 DamageRequest와 동일한 태그 원본을 사용하게 하기 위한 접근자다.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	FGameplayTag GetWeaponTag() const;

	// 이 무기를 장착했을 때 ASC에 붙는 State.Weapon.Equipped.* 태그(예: State.Weapon.Equipped.Firearm.Pistol).
	// 무기별 BP에서 값을 지정한다. GA_SwitchWeapon과 초기 로드아웃이 CombatComponent 경유로 이 값을 읽어
	// RogueAbilitySystemComponent::SetEquippedWeaponType에 전달한다 (WeaponBase는 ASC를 직접 모른다).
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon")
	FGameplayTag GetEquippedStateTag() const { return EquippedStateTag; }

	// 1인칭(캐릭터 팔)에 부착될 때 쓰는 메쉬. CombatComponent가 장착 시 오너 캐릭터의 Mesh1P에 붙인다.
	UFUNCTION(BlueprintPure, Category = "FPS|Weapon|Mesh")
	USkeletalMeshComponent* GetMesh1P() const { return Mesh1P; }

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

protected:
	virtual void BeginPlay() override;

	// SpreadRecoilTable + SpreadRecoilRowName이 지정된 경우, 해당 행의 값으로
	// BaseSpreadDegrees 등 아래 탄퍼짐/반동 튜닝 값들을 덮어쓴다. BeginPlay에서 호출된다.
	void ApplySpreadRecoilTuningTable();

	// --- 탄퍼짐 ---
	float CalculateCurrentSpreadDegrees() const;
	void TickSpreadDecay(float DeltaTime);

	// --- 반동(ADS 전용 - 전투 시스템 명세서 "ADS 반동" 참고) ---
	// ADS 중 사격 유지 구간(TimeSinceLastShot < RecoilRecoveryDelay)에 매 프레임 호출된다. RecoilCurve
	// (UCurveVector, X=Yaw/Y=Pitch)를 FiringElapsedTime으로 계속 evaluate해 이전 프레임과의 델타만
	// 컨트롤러에 적용한다 - 발당 1회가 아니라 시간 기반 연속 적용이라 프레임/RPM과 무관하게 항상 같은
	// 반동 패턴을 그린다.
	void TickProceduralRecoilGeneration(float DeltaTime);
	void TickRecoilRecovery(float DeltaTime);
	// GA_Reload 진입/ADS 해제/무기 교체 시 호출해 반동 상태를 즉시 0으로 리셋한다(서서히 복귀하지 않음).
	void ResetProceduralRecoilState();

	// --- 사격 방향 및 B Resolver 연결 ---
	bool GetTraceOrigin(FVector& OutStart) const;
	FVector CalculateTraceDirection(const FVector& AimDirection) const;
	void ResolveHitscanShot(
		const FVector& TraceStart,
		const TArray<FVector>& PelletDirections,
		float ShotDamage,
		int32 ShotIndexInMagazine);

	// --- Fire() 단계 분리 ---
	// bHeavyAttack에 따라 이번 발사에 적용할 RPM(Base/Alt)을 고른다.
	float ResolveRPMForShot(bool bHeavyAttack) const;
	// LastFireTime/LastFireInterval을 갱신해 다음 CanFireNow() 쿨다운 기준을 세운다.
	void RegisterFireCooldown(const UWorld& World, float RPMUsed);
	// 탄창에서 이번 발의 탄약을 소모하고, 관통/데미지 계산에 쓰일 탄창 내 인덱스를 반환한다.
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

	// ADS 가능한 무기인지 여부. 샷건/칼처럼 ADS가 없는 무기는 false로 설정한다.
	// false면 SetADS(true)를 호출해도 무시된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|ADS")
	bool bCanADS = true;

	// ADS 중 목표 카메라 FOV(도). 무기마다 줌 배율이 다르므로(예: 저격소총이 가장 좁게 줌) 여기서
	// 값을 갖는다. CombatComponent가 bAiming/CanADS()를 보고 이 값과 평상시 FOV 사이를 보간한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|ADS", meta = (ClampMin = "5.0", ClampMax = "170.0"))
	float ADSFieldOfView = 60.f;

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
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "FPS|Weapon|Ammo")
	float EffectiveReloadDuration = 2.1f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon|Ammo")
	int32 CurrentAmmo = 30;

	UPROPERTY(BlueprintReadOnly, Replicated, Category = "FPS|Weapon|Ammo")
	bool bIsReloading = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon|ADS")
	bool bIsADS = false;

	// 연속 사격 횟수. 탄퍼짐 블룸과 반동 커브 샘플링 기준 시간 계산에 사용.
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	int32 ConsecutiveShotCount = 0;

	// --- 탄퍼짐/반동 DT 튜닝 ---
	// 지정하면 BeginPlay 시 FWeaponSpreadRecoilRow의 값으로 아래 탄퍼짐/반동 튜닝 값들을 덮어쓴다.
	// 기획 담당자가 코드/블루프린트를 건드리지 않고 이 DataTable 행만 수정해서 에디터에서 바로
	// 테스트/조정할 수 있게 하기 위함. 비어있으면(=미지정) 아래 기본값/BP 오버라이드를 그대로 쓴다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Tuning")
	TObjectPtr<UDataTable> SpreadRecoilTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Tuning")
	FName SpreadRecoilRowName;

	// --- 탄퍼짐 튜닝 값 ---
	// 무기 기본 탄퍼짐 각도. SpreadRecoilTable 미지정 시 사용되는 기본값(=fallback).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float BaseSpreadDegrees = 1.f;

	// 현재 적용 중인 실제 기본 탄퍼짐 각도. SetEffectiveSpreadDegrees로만 갱신되며, 기본값은
	// (SpreadRecoilTable 적용 이후의) BaseSpreadDegrees와 동일하게 초기화된다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float EffectiveBaseSpreadDegrees = 1.f;

	// 연속 사격 시 사격당 누적되는 탄퍼짐 각도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float BloomPerShotDegrees = 0.2f;

	// 연사 블룸 누적 최대치
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float MaxBloomDegrees = 5.f;

	// 사격을 멈춘 뒤 이 시간(초)이 지나야 ConsecutiveShotCount 감소가 시작된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float SpreadDecayDelay = 0.3f;

	// 초당 감소하는 연속 사격 카운트 (예: 5 = 초당 5발씩 감소)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float ConsecutiveShotDecayPerSecond = 5.f;

	// 이동 상태 탄퍼짐 배율. SetMovementSpreadMultiplier로만 갱신된다.
	UPROPERTY(BlueprintReadOnly, Category = "FPS|Weapon|Spread")
	float MovementSpreadMultiplier = 1.f;

	// --- 반동 튜닝 값 (전투 시스템 명세서 2.1/2.2) ---
	// ADS 반동 커브. X채널=Yaw(좌우, +우측), Y채널=Pitch(상하, +위쪽). In 값은 "연속 사격 시작 후
	// 경과 시간(초)"이고, 값은 매 사격마다 더하는 게 아니라 그 시점까지 쌓여야 할 절대 누적
	// 오프셋이다 - TickProceduralRecoilGeneration이 매 프레임 다시 evaluate해서 이전 프레임과의
	// 차이만 적용한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Recoil")
	TObjectPtr<UCurveVector> RecoilCurve;

	// 매 프레임 Yaw 델타에 추가되는 무작위 편차 범위(도, ±). Pitch는 커브값을 그대로 따르는 결정론적
	// 패턴으로 남겨 학습 가능하게 하고, Yaw만 소폭 랜덤화해 완전한 예측 사격을 막는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Recoil")
	float RecoilRandomness = 0.f;

	// 누적 가능한 최대 상방 반동(도). 초과분은 Clamp - 연사 중 반동이 무한정 커지지 않고 이 이상은
	// 좌우 흔들림(RecoilRandomness)만 남는 상한선 역할을 한다. Yaw에는 상한이 없다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Recoil")
	float RecoilMaxPitch = 10.f;

	// 사격 중단 후 이 시간(초)이 지나야 반동 복귀가 시작된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Recoil")
	float RecoilRecoveryDelay = 0.15f;

	// 반동 복귀 FInterpTo 속도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Recoil")
	float RecoilRecoverySpeed = 10.f;

	// 현재 적용 중인 실제 반동 배율. SetEffectiveRecoilMultiplier로만 갱신되며, RecoilCurve 샘플링
	// 값에 곱해진다. 기본값 1이면 배율 없음(원본 커브 그대로).
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "FPS|Weapon|Recoil")
	float EffectiveRecoilMultiplier = 1.f;

	// --- B Hitscan Resolver 설정 ---
	// 부위, 거리 감쇄, 연속 관통, Trace 안전 설정을 담는 B 전용 DataAsset.
	// 미지정 시 직접 Hit는 기본 1배 대미지로 처리되며 표면 관통은 비활성화된다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Hitscan")
	TObjectPtr<UHitscanWeaponDefinition> HitscanDefinition;

	// --- 관통 튜닝 값 ---
	// B Resolver에 전달할 무기 기본 관통력.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Penetration")
	float PenetrationPower = 70.f;

	// B Resolver의 최대 Trace 사거리 (cm)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Penetration")
	float TraceRange = 4000.f;

	// 현재 적용 중인 실제 Trace 사거리(cm). SetEffectiveTraceRange로만 갱신되며, 기본값은 TraceRange와
	// 동일하게 초기화된다. 근접무기(GA_MeleeAttack)만 GetFinalMeleeRange로 이 값을 갱신하므로,
	// 원거리 무기는 값을 건드리는 호출자가 없어 항상 TraceRange와 같다.
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "FPS|Weapon|Penetration")
	float EffectiveTraceRange = 4000.f;

	// 실제 출구점을 찾지 못했고 UPenetrableSurfaceComponent도 없는 표면에 적용할 Fallback 두께
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Penetration")
	float DefaultSurfaceThickness = 10.f;

	// SurfaceDefinition, SurfaceHardnessMap, PenetrableSurfaceComponent에 별도 값이 없을 때 사용할 기본 경도.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Penetration")
	float DefaultSurfaceHardness = 0.f;

	// 기존 무기 BP의 EPhysicalSurface별 경도 오버라이드.
	// 최종 우선순위는 PenetrableSurfaceComponent > 이 맵 > HitscanDefinition/기본 규칙이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Penetration")
	TMap<TEnumAsByte<EPhysicalSurface>, float> SurfaceHardnessMap;

	// 발사/재장전/화면흔들림 사운드·VFX 슬롯과 재생 로직을 전담하는 서브 컴포넌트. 이 무기의
	// 발사/탄약/탄퍼짐/반동 상태와는 결합이 없어 별도 컴포넌트로 분리했다 - WeaponBase는 "언제"
	// 재생할지(Fire()/StartReload() 등 호출 시점)만 알려주고, "무엇을 어떻게"는 이쪽이 담당한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Weapon")
	TObjectPtr<UWeaponEffectsComponent> Effects;

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

private:
	// 사격 위치/방향 계산에 사용하는 카메라. 소유 액터에서 BeginPlay 시 캐싱한다.
	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> CachedOwnerCamera;

	FTimerHandle ReloadTimerHandle;

	float LastFireTime = -1000.f;

	// 직전 발사(좌/우 클릭 무관)에 적용된 쿨다운 간격(초). 마지막으로 발사된 쪽의 RPM 기준으로 갱신되며,
	// 반대쪽 클릭이라도 이 간격이 지나기 전에는 CanFireNow()가 false를 반환한다(좌우 쿨다운 상호 차단).
	float LastFireInterval = 0.f;

	float TimeSinceLastShot = 1000.f;
	float ConsecutiveShotDecayAccumulator = 0.f;

	float AccumulatedRecoilPitch = 0.f;
	float AccumulatedRecoilYaw = 0.f;

	// ADS 반동 전용 - 연속 사격 시작 후 경과 시간(초). RecoilCurve를 이 값으로 evaluate한다.
	float FiringElapsedTime = 0.f;
	// 직전 프레임에 evaluate한 RecoilCurve 값(X=Yaw, Y=Pitch, Pitch는 RecoilMaxPitch로 Clamp된 값).
	// 이번 프레임 값과의 차이만 컨트롤러에 적용하므로 매 프레임 갱신해둔다.
	FVector2D PrevRecoilOffset = FVector2D::ZeroVector;
	// 지금 ADS 반동을 생성 중인지(사격 유지 구간). 새 버스트 시작 감지에 쓰인다. 탄퍼짐 블룸
	// 카운터(ConsecutiveShotCount)와는 별개로 관리해서, ADS 중 사격 후 지향 사격으로 전환해도
	// 그동안 쌓인 블룸이 갑자기 튀어나오지 않는다.
	bool bRecoilBurstActive = false;
};