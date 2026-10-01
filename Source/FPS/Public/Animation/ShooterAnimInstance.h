#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Character/ShooterCharacterTypes.h"
#include "GameplayTagContainer.h"
#include "ShooterAnimInstance.generated.h"

class AShooterCharacter;
class AWeaponBase;
class UAnimMontage;
class UAnimSequence;
class UCharacterMovementComponent;
class UCombatComponent;
class USkeletalMeshComponent;
enum class EDominationCombatClass : uint8;

// Written on the game thread after montage advance; read-only during graph work.
struct FReloadContactPlayback
{
	const UAnimMontage* Montage = nullptr;
	float NormalizedTime = 0.f;
	float Weight = 0.f;
};

UCLASS()
class FPS_API UShooterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeThreadSafeUpdateAnimation(float DeltaSeconds) override;
	virtual void NativeUninitializeAnimation() override;
	const TArray<FReloadContactPlayback>& GetContactPlayback() const { return ContactPlayback; }

	// GA_RangedAttack이 발사 성공 시점에 직접 호출한다(Equip/Reload처럼 상태를 폴링해서 감지할 수 있는
	// 지속 상태가 아니라 한 프레임짜리 이벤트라 폴링 대신 콜 사이트에서 직접 트리거하는 방식을 쓴다).
	// 현재 장착 무기 태그로 WeaponData->WeaponMontages를 찾아 FireMontage가 있으면 재생한다 - 무기별로
	// 애셋이 없으면(예: 샷건/저격 - 추후 추가 예정) 조용히 스킵한다.
	void PlayFireMontageForCurrentWeapon(bool bPlayObserverWeaponAction = false);

	// Cache on the game thread: blend idle-only posture with the slot's base pose,
	// instead of enabling it abruptly after the final montage instance disappears.
	UFUNCTION(BlueprintPure, Category = "FPS|Animation|Posture")
	float GetIdlePostureSourceWeight() const;

	// Equip and reload keep the fitted idle torso/shoulder basis through blend-out.
	// Unrelated full-body actions retain their original posture suppression.
	UFUNCTION(BlueprintPure, Category = "FPS|Animation|Posture")
	float GetEquipAwarePostureSourceWeight() const;

protected:
	// Opt-in from the third-person graph only while a support-hand-held action plays.
	// Keeps the weapon with the supporting hand so the trigger hand can leave the grip.
	UPROPERTY(BlueprintReadWrite, Category = "FPS|Animation|Weapon")
	bool bThirdPersonSupportAnchor = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|References")
	TObjectPtr<AShooterCharacter> ShooterCharacter;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|References")
	TObjectPtr<UCharacterMovementComponent> CharacterMovementComponent;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|References")
	TObjectPtr<UCombatComponent> Combat;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Weapon")
	TObjectPtr<AWeaponBase> CurrentWeapon;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Weapon")
	bool bHasCurrentWeapon = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Weapon")
	FGameplayTag CurrentWeaponType;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Weapon")
	TObjectPtr<UAnimSequence> CurrentIdleAnim;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Weapon")
	TObjectPtr<UAnimSequence> CurrentAimIdleAnim;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Weapon")
	bool bIsCycling = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Weapon")
	bool bWeaponReady = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Weapon")
	bool bIsReloading = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Weapon")
	bool bFirstPersonAnimation = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Combat")
	bool bAiming = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Movement")
	bool bIsCrouching = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Movement")
	bool bIsFalling = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Movement")
	bool bIsMoving = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Movement")
	bool bIsAccelerating = false;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Movement")
	float GroundSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Aiming")
	FRotator AimRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Aiming")
	float AO_Yaw = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Aiming")
	float AO_Pitch = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|Movement")
	float MovementOffsetYaw = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Animation|TurnInPlace")
	ETurningInPlace TurningStatus = ETurningInPlace::NotTurning;

private:
	TArray<FReloadContactPlayback> ContactPlayback;
	void RestoreSupportAnchor();
	void UpdateSupportAnchor();
	void UnregisterSupportAnchor();
	TWeakObjectPtr<USkeletalMeshComponent> SupportAnchorBody;
	FDelegateHandle SupportAnchorFinalizedHandle;
	TWeakObjectPtr<USkeletalMeshComponent> SupportAnchorMesh;
	TWeakObjectPtr<USkeletalMeshComponent> PreviousWeaponMesh;
	FTransform SupportAnchorLocal = FTransform::Identity;
	FTransform OriginalWeaponRelative = FTransform::Identity;
	FName OriginalWeaponSocket;
	FTransform PreviousSupportRelative = FTransform::Identity;
	bool bSupportAnchorActive = false;

	void CacheReferences();
	void UpdateMovementValues();
	void UpdateAimValues(float DeltaSeconds);
	void UpdateWeaponValues();
	void UpdateWeaponAnimationData();
	void PlayEquipMontageForCurrentWeapon();
	void PlayReloadMontageForCurrentWeapon();
	void TurnInPlace(float DeltaSeconds);
	EDominationCombatClass GetCurrentCombatClass() const;

	FRotator StartingAimRotation = FRotator::ZeroRotator;
	float InterpAO_Yaw = 0.f;
	TWeakObjectPtr<AWeaponBase> LastAnimationWeapon;
	bool bWasReloading = false;
	bool bWasUtilityHeld = false;
	bool bCQBTrainingAI = false;
};
