#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WeaponEffectsComponent.generated.h"

class AWeaponBase;
class UAnimMontage;
class UCameraShakeBase;
class USceneComponent;
class USoundAttenuation;
class USoundBase;
class UAnimInstance;
struct FAnimNotifyEvent;
struct FAnimNotifyEventReference;

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class FPS_API UWeaponEffectsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponEffectsComponent();

	void PlayFireCameraShake() const;
	void PlayFireSound() const;
	void PlayEmptyClipSound();
	// Common 3P animation state calls this once per equip transition on each peer.
	// No extra RPC/notify: the owner hears 2D; observers use the world-space emitter.
	void PlayDrawSound() const;
	// Called by the common character anim instance when a registered Holster starts.
	// Returns false for unrelated montages; uses the same owner/observer audio routing.
	bool TryPlayHolsterSound(const UAnimMontage* Montage) const;
	// Synchronization may rewind a weapon montage across the same sound notify.
	// Consume that event once per playback, not once per frame or time window.
	bool ConsumeMontageSoundNotify(UAnimInstance* Source, const FAnimNotifyEventReference& EventReference);

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

	// Observer attenuation; the owning player bypasses it without changing the asset.
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon|Effects|Sound")
	TObjectPtr<USoundAttenuation> FireAttenuation;

	// Observer attenuation for weapon actions. The owning player hears 2D.
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon|Effects|Sound")
	TObjectPtr<USoundAttenuation> ActionAttenuation;

	// Gun Draw audio lives here, not on the Draw animation's notify tracks.
	// Uses ActionAttenuation for observers and 2D playback for the owning player.
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon|Effects|Sound")
	TObjectPtr<USoundBase> DrawSound;

	// 무기를 집어넣을 때 한 번 재생. 본인은 2D, 상대는 ActionAttenuation을 사용한다.
	UPROPERTY(EditDefaultsOnly, Category = "FPS|Weapon|Effects|Sound")
	TObjectPtr<USoundBase> HolsterSound;

	// Sound-free character montages that trigger HolsterSound (1P + each class 3P).
	// Exact assets, not name matching; weapon-mesh montages must not be added here.
	UPROPERTY(EditDefaultsOnly, AdvancedDisplay, Category = "FPS|Weapon|Effects|Sound")
	TArray<TObjectPtr<UAnimMontage>> HolsterMontages;

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

private:
	struct FPlayedMontageSound
	{
		TWeakObjectPtr<UAnimInstance> Source;
		int32 MontageInstanceID = INDEX_NONE;
		const FAnimNotifyEvent* Event = nullptr;
	};
	TArray<FPlayedMontageSound> PlayedMontageSounds;
};
