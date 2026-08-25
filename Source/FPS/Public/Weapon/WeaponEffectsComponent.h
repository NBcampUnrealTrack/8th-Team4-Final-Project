// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "WeaponEffectsComponent.generated.h"

class AWeaponBase;
class UCameraShakeBase;
class UNiagaraSystem;
class USkeletalMeshComponent;
class USoundBase;

// AWeaponBase의 발사/재장전/탄약/탄퍼짐/반동 게임플레이 로직과는 결합이 없는 순수 프레젠테이션
// (사운드/VFX/카메라 흔들림)만 담당한다. AWeaponBase가 "언제" 재생할지(디바운스, 재장전 타이밍 등
// 게임플레이 판단)를 결정해 정해진 진입점을 호출하면, 이 컴포넌트는 "무엇을, 어떻게" 재생할지만
// 안다 - 그래서 함수들은 스스로 판단하지 않고 호출된 시점을 그대로 신뢰한다.
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPS_API UWeaponEffectsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponEffectsComponent();

	// AWeaponBase::Fire()에서 사격 1회당 호출. 발사 사운드 + 총구화염/탄궤적을 멀티캐스트로 재생한다.
	void PlayFireEffects(const FVector& TraceStart, const TArray<FVector>& PelletDirections);

	// AWeaponBase::Fire()에서 사격 1회당 호출. 화면 흔들림은 쏜 본인 화면에만 있어야 하므로
	// 멀티캐스트하지 않고 로컬 컨트롤러 여부로 직접 걸러 재생한다.
	void PlayFireCameraShake() const;

	// AWeaponBase::TryPlayEmptyClipSound()가 디바운스(RPM 간격) 판정을 마친 뒤 호출한다.
	void PlayEmptyClipSound();

	// AWeaponBase::ResolveHitscanShot()에서 서버 트레이스 결과(Hitscan Resolver가 돌려준 실제 피격
	// 지점들)를 받아 호출한다. 지점마다 HitImpactEffect를 재생한다 - 대상/총구에 부착할 필요가 없는
	// 월드 좌표 스폰이라 PlayFireEffects와 달리 소켓을 몰라도 된다.
	void PlayHitEffects(const TArray<FVector>& ImpactPoints);

	// AWeaponBase::StartReload() 시작 시점에 호출한다. ReloadStartSound를 즉시 재생하고,
	// ReloadChargeSoundDelay가 ReloadDuration보다 짧으면 스스로 타이머를 걸어 ReloadChargeSound도
	// 재생한다 - 재장전 중간 사운드 타이밍은 이 컴포넌트가 전담한다.
	void NotifyReloadStarted(float ReloadDuration);

	// AWeaponBase::FinishReload()에서 호출한다.
	void PlayReloadFinishSound();

protected:
	AWeaponBase* GetWeaponOwner() const;

	void PlayReloadChargeSound();

	bool IsLocalShooterListening() const;

	// MuzzleFlashEffect가 지정된 경우 Mesh1P/Mesh3P 각 Muzzle 소켓에 부착해 재생한다(둘 다 시도 -
	// bOnlyOwnerSee/bOwnerNoSee가 이미 보는 사람을 갈라주므로 중복 재생처럼 보이지 않는다).
	void SpawnMuzzleFlash() const;
	// ProjectileTrailEffect가 지정된 경우 펠릿별 방향으로 Mesh1P/Mesh3P 각 Muzzle 소켓에서 재생한다.
	// 실제 피격 위치는 아직 Hitscan Resolver 결과에 없어 방향만 맞추고 도착 지점은 못 맞춘다.
	void SpawnProjectileTrails(const TArray<FVector>& PelletDirections) const;
	void SpawnProjectileTrailsFromMesh(USkeletalMeshComponent* Mesh, FName SocketName, const TArray<FVector>& PelletDirections) const;

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayFireSound(FVector Location);

	// 재장전 사운드 등, 위치 부착이 필요 없는 무기 자체 사운드 재생용 공용 RPC.
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayWeaponSound(USoundBase* Sound);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayMuzzleEffects(const TArray<FVector>& PelletDirections);

	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayHitEffects(const TArray<FVector>& ImpactPoints);

	// --- 사운드 슬롯 ---
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Audio")
	TObjectPtr<USoundBase> FireSound;

	// 재장전 시작 시 재생할 사운드. 비어 있으면 재생하지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Audio")
	TObjectPtr<USoundBase> ReloadStartSound;

	// 재장전 완료 시 재생할 사운드. 비어 있으면 재생하지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Audio")
	TObjectPtr<USoundBase> ReloadFinishSound;

	// 재장전 도중(예: 노리쇠 장전음) 재생할 사운드. 비어 있으면 재생하지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Audio")
	TObjectPtr<USoundBase> ReloadChargeSound;

	// 재장전 시작부터 ReloadChargeSound를 재생하기까지의 지연(초). 재장전 애니메이션에서 노리쇠를
	// 당기는 타이밍에 맞춰 무기마다 조정한다. ReloadDuration보다 크거나 같으면 재생되지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Audio", meta = (ClampMin = "0.0"))
	float ReloadChargeSoundDelay = 1.f;

	// 탄약 없이 발사를 시도했을 때 재생할 사운드(딸깍 소리). 비어 있으면 재생하지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Audio")
	TObjectPtr<USoundBase> EmptyClipSound;

	// 본인에게 들리는 발사/재장전 사운드 볼륨 배율
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Audio", meta = (ClampMin = "0.0"))
	float LocalListenerVolumeMultiplier = 1.0f;

	// 다른 플레이어에게 들리는 발사/재장전 사운드 볼륨 배율.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Audio", meta = (ClampMin = "0.0"))
	float RemoteListenerVolumeMultiplier = 0.2f;

	// --- VFX 슬롯 ---
	// 총구화염. 비어 있으면 재생하지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Effects")
	TObjectPtr<UNiagaraSystem> MuzzleFlashEffect;

	// 발사 방향으로 재생되는 탄 궤적(트레이서). 비어 있으면 재생하지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Effects")
	TObjectPtr<UNiagaraSystem> ProjectileTrailEffect;

	// 피격 지점(Hitscan Resolver가 돌려준 실제 트레이스 충돌 지점)마다 재생할 임팩트 이펙트
	// (스파크/피격 파티클 등). 비어 있으면 재생하지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Effects")
	TObjectPtr<UNiagaraSystem> HitImpactEffect;

	// 1인칭(Mesh1P) 총구 소켓 이름. GripSocketName1P와 같은 이유로 3인칭과 분리한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Effects")
	FName MuzzleSocketName1P;

	// 3인칭(Mesh3P) 총구 소켓 이름.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Effects")
	FName MuzzleSocketName3P;

	// 발사할 때마다 재생할 화면 흔들림. 반동(컨트롤러 회전, 마우스로 상쇄 가능)과는 별개의 순수
	// 시각 효과다. 비어 있으면 재생하지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Weapon|Effects")
	TSubclassOf<UCameraShakeBase> FireCameraShake;

private:
	FTimerHandle ReloadChargeTimerHandle;
};
