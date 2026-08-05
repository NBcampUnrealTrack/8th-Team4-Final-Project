// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Interfaces/PlayerInterface.h"
#include "ShooterCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class USpringArmComponent;
class UWeaponData;
struct FInputActionValue;

UCLASS()
class FPS_API AShooterCharacter : public ACharacter, public IPlayerInterface
{
	GENERATED_BODY()

public:
	AShooterCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;

public:	
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	virtual USkeletalMeshComponent* GetMesh1P_Implementation() const override;
	virtual USkeletalMeshComponent* GetMesh3P_Implementation() const override;
	virtual UCameraComponent* GetViewCamera_Implementation() const override;
	virtual FRotator GetFixedAimRotation_Implementation() const override;

	UFUNCTION(BlueprintCallable, Category = "FPS|Character")
	FRotator CalculateFixedAimRotation() const;

	UFUNCTION(BlueprintPure, Category = "FPS|Character")
	UWeaponData* GetWeaponData() const { return WeaponData; }

	void HandleMoveInput(const FInputActionValue& Value);
	void HandleLookInput(const FInputActionValue& Value);
	void HandleJumpPressed();
	void HandleJumpReleased();
	void HandleSprintPressed();
	void HandleSprintReleased();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Camera")
	TObjectPtr<USpringArmComponent> SpringArm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Camera")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Avatar")
	TObjectPtr<USkeletalMeshComponent> Mesh1P;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Animation")
	TObjectPtr<UWeaponData> WeaponData;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> CycleWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> FireWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> ReloadWeaponAction;

	UPROPERTY(EditAnywhere, Category = "FPS|Input")
	TObjectPtr<UInputAction> AimWeaponAction;

private:
};
