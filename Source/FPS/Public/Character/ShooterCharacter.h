// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "Interfaces/PlayerInterface.h"
#include "Interfaces/RogueDamageableTarget.h"
#include "Weapon/WeaponTypes.h"
#include "ShooterCharacter.generated.h"

class UAbilitySystemComponent;
class UCameraComponent;
class UCombatComponent;
class UInputAction;
class UPlayerCustomizationComponent;
class USpringArmComponent;
class AWeaponBase;
struct FInputActionValue;

UCLASS()
class FPS_API AShooterCharacter : public ACharacter, public IPlayerInterface, public IAbilitySystemInterface, public IRogueDamageableTarget
{
	GENERATED_BODY()

public:
	AShooterCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// ASC 는 AShooterPlayerState 소유
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

protected:
	virtual void BeginPlay() override;

public:
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode = 0) override;

	virtual void CheckJumpInput(float DeltaTime) override;

	virtual USkeletalMeshComponent* GetMesh1P_Implementation() const override;
	virtual USkeletalMeshComponent* GetMesh3P_Implementation() const override;
	virtual UCameraComponent* GetViewCamera_Implementation() const override;
	virtual FRotator GetFixedAimRotation_Implementation() const override;
	virtual AWeaponBase* GetCurrentWeapon_Implementation() const override;
	virtual void HandleDeath_Implementation(const FRogueDeathContext& Context) override;
	virtual void PlayDeathReaction_Implementation(const FRogueDeathReaction& Reaction) override;

	UFUNCTION(BlueprintCallable, Category = "FPS|Character")
	FRotator CalculateFixedAimRotation() const;

	UFUNCTION(BlueprintCallable, Category = "FPS|Character")
	bool HasCurrentWeapon() const;

	UPROPERTY(BlueprintReadWrite, Category = "FPS|FABRIK")
	FTransform FabricSocketTransform;

	void HandleMoveInput(const FInputActionValue& Value);
	void HandleLookInput(const FInputActionValue& Value);
	void HandleJumpPressed();
	void HandleJumpReleased();
	void HandleSprintPressed();
	void HandleSprintReleased();
	void HandleWeaponSlotPressed(EWeaponSlot WeaponSlot);

protected:
	bool InitializeDefaultWeaponLoadout();
	bool TrySwitchWeaponSlot(EWeaponSlot WeaponSlot);

	UFUNCTION(Server, Reliable)
	void ServerSwitchWeaponSlot(EWeaponSlot WeaponSlot);

	//--------------------------------------------------
	// 사망 연출 관련 설정
	// 임펄스 세기
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Death", meta = (ClampMin = "0.0"))
	float DeathImpulseSpeed = 600.f;

	// 헤드샷 임펄스 배율
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Death", meta = (ClampMin = "0.0"))
	float HeadshotDeathImpulseMultiplier = 1.8f;

	// 임펄스를 위로 기우는 각도 비율 (0~1, 0이면 수평, 1이면 수직)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Death", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DeathImpulseUpwardBias = 0.25f;

	// 임펄스 적용 본 이름, 기본은 pelvis
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Death")
	FName DeathImpulseBoneName = TEXT("pelvis");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Death")
	FName HeadshotDeathImpulseBoneName = TEXT("head");
	//--------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Combat|Loadout")
	TSubclassOf<AWeaponBase> DefaultPrimaryWeaponClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Combat|Loadout")
	TSubclassOf<AWeaponBase> DefaultSecondaryWeaponClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Avatar")
	TObjectPtr<USkeletalMeshComponent> Mesh1P;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Combat")
	TObjectPtr<UCombatComponent> Combat;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Customization")
	TObjectPtr<UPlayerCustomizationComponent> Customization;

private:
	// 랙돌에 피격 임펄스 적용
	void ApplyDeathImpulse(USkeletalMeshComponent& Mesh3P, const FRogueDeathReaction& Reaction) const;

	// 사망 연출 중복 진입 방지
	bool bDeathStateApplied = false;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> CycleWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> FireWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> ReloadWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> AimWeaponAction;
};
