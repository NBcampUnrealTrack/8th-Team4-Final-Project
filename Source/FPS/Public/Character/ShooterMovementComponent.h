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

	// Resolved non-sprint speed, including the same weapon and augment modifiers.
	float GetMaxWalkingSpeed() const { return CalculateMaxSpeed(false); }

	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;

	virtual bool DoJump(bool bReplayingMoves, float DeltaTime) override;
	virtual bool CanCrouchInCurrentState() const override;

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

	// 달리기 애니메이션이 재생 중이거나, 끝난 뒤 총구가 다시 정면으로 돌아오는 중(SprintFireRecoveryTime)이면 true.
	// 이 동안은 사격하지 않는다(GA_RangedAttack).
	bool IsRecoveringFromSprint() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void SetPostLandedPhysics(const FHitResult& Hit) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Movement")
	float WalkSpeed = 600.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Movement")
	float SprintSpeed = 850.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Movement",
		meta = (ClampMin = "0.0", Units = "cm/s^2", ToolTip = "Initial acceleration above walking speed, easing to zero at sprint speed. Higher is quicker; 0 uses normal Character Movement acceleration."))
	float SprintAcceleration = 800.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Movement")
	float CrouchSpeed = 300.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Movement")
	float CrouchHalfHeight = 44.f;

	UPROPERTY(BlueprintReadOnly, Category = "FPS|Movement")
	bool bWantsToSprint = false;

	bool bSprintBlocked = false;

	// 달리기 애니메이션이 끝난 뒤 사격을 다시 허용하기까지의 시간. 1인칭 달리기 자세가 정면 자세로
	// 블렌드되는 시간(BPC_TacticalPresentation의 Gait 보간)에 맞춰 조정한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FPS|Movement", meta = (ClampMin = "0.0", Units = "s"))
	float SprintFireRecoveryTime = 0.35f;

private:
	// 마지막으로 달리기 애니메이션 조건이 참이었던 월드 시간.
	double LastSprintAnimationTime = -1.0e9;

	float CalculateMaxSpeed(bool bSprinting) const;
};
