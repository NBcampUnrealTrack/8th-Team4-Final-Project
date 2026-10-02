#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponEffectsComponent.generated.h"

class AWeaponBase;
class UCameraShakeBase;
class USceneComponent;
class USoundAttenuation;
class USoundBase;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPS_API UWeaponEffectsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponEffectsComponent();

	void PlayFireCameraShake() const;
	void PlayFireSound() const;
	void PlayEmptyClipSound();

	// 무기를 꺼낼(장착 사이클 시작) 때 서버에서 호출한다. 무기를 든 본인에게만 Client RPC로 재생한다.
	void PlayEquipSound();

	// 근접 공격이 승인됐을 때 GA_MeleeAttack이 호출한다(예측 클라이언트 + 서버 양쪽).
	// 본인은 즉시 2D로 재생하고, 서버는 나머지 클라이언트에 멀티캐스트해 공격자 위치에서
	// 거리 감쇠(MeleeAttackAttenuation)를 적용해 재생한다 - 꾹 누르기 연속 공격에서도
	// 공격 몽타주와 같은 타이밍에 한 번씩 재생된다.
	void PlayMeleeAttackSound(bool bHeavyAttack);

protected:
	AWeaponBase* GetWeaponOwner() const;

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayEmptyClipSound();

	UFUNCTION(Client, Unreliable)
	void Client_PlayEquipSound();

	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayMeleeAttackSound(bool bHeavyAttack);

	USoundBase* ResolveMeleeAttackSound(bool bHeavyAttack) const;
	bool IsOwnerLocallyControlled() const;
	USceneComponent* GetSoundAttachComponent() const;

protected:
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon|Effects|Sound")
	TObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon|Effects|Sound")
	TObjectPtr<USoundBase> EmptyClipSound;

	// Separate from the shared sound asset so each weapon can have its own range.
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon|Effects|Sound")
	TObjectPtr<USoundAttenuation> FireAttenuation;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon|Effects|Sound")
	TObjectPtr<USoundAttenuation> ActionAttenuation;

	// 무기를 꺼낼 때(무기 교체/리스폰/유틸리티 해제 후 재장착) 본인에게만 재생할 사운드.
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon|Effects|Sound")
	TObjectPtr<USoundBase> EquipSound;

	// 다른 플레이어가 듣는 근접 공격음의 거리 감쇠. 비어 있으면 사운드 에셋 자체의 감쇠 설정을 쓴다
	// (그것도 없으면 거리와 무관하게 들린다).
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon|Effects|Sound")
	TObjectPtr<USoundAttenuation> MeleeAttackAttenuation;

	// 근접무기 좌클릭(약공격) 사운드.
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon|Effects|Sound")
	TObjectPtr<USoundBase> MeleeAttackSound;

	// 근접무기 우클릭(강공격) 사운드. 비어 있으면 MeleeAttackSound를 대신 재생한다.
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon|Effects|Sound")
	TObjectPtr<USoundBase> HeavyMeleeAttackSound;

	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon|Effects|Camera")
	TSubclassOf<UCameraShakeBase> FireCameraShake;
};
