// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "AbilitySystemInterface.h"
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interfaces/PlayerInterface.h"
#include "Interfaces/RogueDamageableTarget.h"
#include "Weapon/WeaponTypes.h"
#include "ShooterCharacter.generated.h"

class UAbilitySystemComponent;
class UCameraComponent;
class UCombatHitZoneDefinition;
class UCombatComponent;
class UInputAction;
class ULagCompensationComponent;
class UPlayerCustomizationComponent;
class USpringArmComponent;
class AWeaponBase;
struct FInputActionValue;

UCLASS()
class FPS_API AShooterCharacter : public ACharacter,
								  public IPlayerInterface,
								  public IAbilitySystemInterface,
								  public IRogueDamageableTarget
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

	virtual USkeletalMeshComponent* GetMesh1P_Implementation() const override;
	virtual USkeletalMeshComponent* GetMesh3P_Implementation() const override;
	virtual UCameraComponent* GetViewCamera_Implementation() const override;
	virtual FRotator GetFixedAimRotation_Implementation() const override;
	virtual AWeaponBase* GetCurrentWeapon_Implementation() const override;
	virtual void HandleDeath_Implementation(const FRogueDeathContext& Context) override;
	virtual UCombatHitZoneDefinition* GetCombatHitZoneDefinition_Implementation() const override;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Combat")
	TObjectPtr<ULagCompensationComponent> LagCompensation;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Combat|Hit Zone")
	TObjectPtr<UCombatHitZoneDefinition> CombatHitZoneDefinition;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Customization")
	TObjectPtr<UPlayerCustomizationComponent> Customization;

private:
	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> CycleWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> FireWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> ReloadWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> AimWeaponAction;
};
