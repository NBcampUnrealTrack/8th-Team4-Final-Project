#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ShooterMovementComponent.generated.h"

UCLASS(ClassGroup=(Movement), meta=(BlueprintSpawnableComponent))
class FPS_API UShooterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UShooterMovementComponent();

	UFUNCTION(BlueprintCallable, Category = "FPS|Movement")
	void StartSprint();

	UFUNCTION(BlueprintCallable, Category = "FPS|Movement")
	void StopSprint();

	virtual float GetMaxSpeed() const override;

	// CombatComponent가 이동 상태 탄퍼짐 배율(WeaponBase::SetMovementSpreadMultiplier)을 계산할 때 참고한다.
	UFUNCTION(BlueprintPure, Category = "FPS|Movement")
	bool IsSprinting() const { return bWantsToSprint; }

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Movement")
	float WalkSpeed = 600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Movement")
	float SprintSpeed = 850.f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Movement")
	bool bWantsToSprint = false;

};
