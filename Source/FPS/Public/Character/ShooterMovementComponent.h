#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ShooterMovementComponent.generated.h"

class FSavedMove_Shooter : public FSavedMove_Character
{
public:
	typedef FSavedMove_Character Super;

	virtual void Clear() override;
	virtual uint8 GetCompressedFlags() const override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
		class FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;

	uint8 bSavedWantsToSprint : 1;
};

class FNetworkPredictionData_Client_Shooter : public FNetworkPredictionData_Client_Character
{
public:
	typedef FNetworkPredictionData_Client_Character Super;

	FNetworkPredictionData_Client_Shooter(const UCharacterMovementComponent& ClientMovement);

	virtual FSavedMovePtr AllocateNewMove() override;
};

UCLASS(ClassGroup=(Movement), meta=(BlueprintSpawnableComponent))
class FPS_API UShooterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

	friend class FSavedMove_Shooter;

public:
	UShooterMovementComponent();

	virtual void BeginPlay() override;

	virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

	UFUNCTION(BlueprintCallable, Category = "FPS|Movement")
	void StartSprint();

	UFUNCTION(BlueprintCallable, Category = "FPS|Movement")
	void StopSprint();

	// 차단 중에는 입력과 이동 패킷 모두 달리기를 켜지 않는다
	void SetSprintBlocked(bool bBlocked);

	virtual float GetMaxSpeed() const override;

	virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;

	// CombatComponent가 이동 상태 탄퍼짐 배율(WeaponBase::SetMovementSpreadMultiplier)을 계산할 때 참고한다.
	UFUNCTION(BlueprintPure, Category = "FPS|Movement")
	bool IsSprinting() const { return bWantsToSprint; }

	// Presentation only: preserve sprint input intent for movement/combat while idle.
	UFUNCTION(BlueprintPure, Category = "FPS|Movement|Animation")
	bool ShouldPlaySprintAnimation() const
	{
		return bWantsToSprint && !IsCrouching() && IsMovingOnGround()
			&& Velocity.SizeSquared2D() > FMath::Square(3.f);
	}

protected:
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Movement")
	float WalkSpeed = 600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Movement")
	float SprintSpeed = 850.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Movement")
	float CrouchSpeed = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Movement")
	float CrouchHalfHeight = 44.f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Movement")
	bool bWantsToSprint = false;

	bool bSprintBlocked = false;

};
