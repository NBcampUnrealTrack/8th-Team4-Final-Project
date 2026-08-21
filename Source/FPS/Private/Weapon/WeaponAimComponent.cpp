// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/WeaponAimComponent.h"

#include "Curves/CurveVector.h"
#include "Engine/DataTable.h"
#include "GameFramework/Pawn.h"
#include "Weapon/WeaponTypes.h"

UWeaponAimComponent::UWeaponAimComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWeaponAimComponent::BeginPlay()
{
	Super::BeginPlay();

	ApplySpreadRecoilTuningTable();

	EffectiveBaseSpreadDegrees = BaseSpreadDegrees;
	EffectiveRecoilMultiplier = 1.f;
}

void UWeaponAimComponent::ApplySpreadRecoilTuningTable()
{
	if (!SpreadRecoilTable || SpreadRecoilRowName.IsNone())
	{
		return;
	}

	const FWeaponSpreadRecoilRow* Row = SpreadRecoilTable->FindRow<FWeaponSpreadRecoilRow>(
		SpreadRecoilRowName, TEXT("UWeaponAimComponent::ApplySpreadRecoilTuningTable"));
	if (!Row)
	{
		return;
	}

	BaseSpreadDegrees = Row->BaseSpreadDegrees;
	BloomPerShotDegrees = Row->BloomPerShotDegrees;
	MaxBloomDegrees = Row->MaxBloomDegrees;
	SpreadDecayDelay = Row->SpreadDecayDelay;
	ConsecutiveShotDecayPerSecond = Row->ConsecutiveShotDecayPerSecond;

	RecoilCurve = Row->RecoilCurve;
	RecoilRandomness = Row->RecoilRandomness;
	RecoilMaxPitch = Row->RecoilMaxPitch;
	RecoilRecoveryDelay = Row->RecoilRecoveryDelay;
	RecoilRecoverySpeed = Row->RecoilRecoverySpeed;
}

void UWeaponAimComponent::SetADS(const bool bNewIsADS)
{
	bIsADS = bCanADS && bNewIsADS;

	// ADS 해제 시 반동 값을 즉시 0으로 리셋한다(서서히 복귀하지 않음) - 지향 사격으로 전환되면
	// 탄퍼짐 계산으로 넘어가므로 잔여 반동이 이어지면 안 된다.
	if (!bIsADS)
	{
		ResetRecoilState();
	}
}

void UWeaponAimComponent::SetMovementSpreadMultiplier(const float NewMultiplier)
{
	MovementSpreadMultiplier = NewMultiplier;
}

void UWeaponAimComponent::SetEffectiveSpreadDegrees(const float NewSpreadDegrees)
{
	EffectiveBaseSpreadDegrees = NewSpreadDegrees;
}

void UWeaponAimComponent::SetEffectiveRecoilMultiplier(const float NewMultiplier)
{
	EffectiveRecoilMultiplier = NewMultiplier;
}

float UWeaponAimComponent::GetCurrentSpreadDegrees() const
{
	return CalculateCurrentSpreadDegrees();
}

void UWeaponAimComponent::TickAim(const float DeltaTime)
{
	TimeSinceLastShot += DeltaTime;

	TickSpreadDecay(DeltaTime);
	TickProceduralRecoilGeneration(DeltaTime);
	TickRecoilRecovery(DeltaTime);
}

void UWeaponAimComponent::NotifyShotFired()
{
	// 3.3.1 개정 원칙: 탄퍼짐 블룸은 지향 사격 전용이다. ADS 중에는 늘리지 않는다 - 안 그러면
	// ADS 중 사격이 화면엔 안 보이게(스프레드 0 고정) 블룸을 몰래 쌓다가, ADS를 풀자마자
	// 그 값이 한꺼번에 튀어나와 크로스헤어가 갑자기 벌어져 보인다.
	if (!bIsADS)
	{
		ConsecutiveShotCount++;
	}
	TimeSinceLastShot = 0.f;
}

void UWeaponAimComponent::ResetRecoilState()
{
	FiringElapsedTime = 0.f;
	PrevRecoilOffset = FVector2D::ZeroVector;
	AccumulatedRecoilPitch = 0.f;
	AccumulatedRecoilYaw = 0.f;
	bRecoilBurstActive = false;
}

float UWeaponAimComponent::CalculateCurrentSpreadDegrees() const
{
	// 3.3.1 개정 원칙: ADS 중에는 탄퍼짐이 0으로 고정된다.
	if (bIsADS)
	{
		return 0.f;
	}

	const float BloomDegrees = FMath::Min(ConsecutiveShotCount * BloomPerShotDegrees, MaxBloomDegrees);
	return (EffectiveBaseSpreadDegrees + BloomDegrees) * MovementSpreadMultiplier;
}

void UWeaponAimComponent::TickSpreadDecay(float DeltaTime)
{
	if (ConsecutiveShotCount <= 0 || TimeSinceLastShot < SpreadDecayDelay)
	{
		return;
	}

	ConsecutiveShotDecayAccumulator += ConsecutiveShotDecayPerSecond * DeltaTime;

	while (ConsecutiveShotDecayAccumulator >= 1.f && ConsecutiveShotCount > 0)
	{
		ConsecutiveShotCount--;
		ConsecutiveShotDecayAccumulator -= 1.f;
	}
}

void UWeaponAimComponent::TickProceduralRecoilGeneration(const float DeltaTime)
{
	// ADS 중 사격 유지 구간(마지막 발사 이후 RecoilRecoveryDelay가 지나지 않은 동안)에만 생성한다.
	// 이 구간을 벗어나면(사격 중단/ADS 해제) 다음 진입 시 새 버스트로 취급하도록 플래그를 내린다.
	if (!bIsADS || !RecoilCurve || TimeSinceLastShot >= RecoilRecoveryDelay)
	{
		bRecoilBurstActive = false;
		return;
	}

	if (!bRecoilBurstActive)
	{
		FiringElapsedTime = 0.f;
		PrevRecoilOffset = FVector2D::ZeroVector;
		bRecoilBurstActive = true;
	}

	FiringElapsedTime += DeltaTime;

	const FVector RawCurveValue = RecoilCurve->GetVectorValue(FiringElapsedTime);
	// Pitch만 RecoilMaxPitch로 상한을 둔다(초과분 Clamp) - Yaw는 상한 없이 커브+랜덤을 그대로 따른다.
	const FVector2D TargetOffset(RawCurveValue.X, FMath::Min(RawCurveValue.Y, RecoilMaxPitch));

	FVector2D Delta = TargetOffset - PrevRecoilOffset;
	Delta.X += FMath::FRandRange(-RecoilRandomness, RecoilRandomness);
	Delta *= EffectiveRecoilMultiplier;

	if (APawn* OwningPawn = Cast<APawn>(GetOwner() ? GetOwner()->GetOwner() : nullptr))
	{
		OwningPawn->AddControllerYawInput(Delta.X);
		// 위로 튀어오르는 반동이므로 Pitch는 음수 방향으로 적용한다(엔진 Pitch 부호 규약 반전).
		OwningPawn->AddControllerPitchInput(-Delta.Y);
	}

	AccumulatedRecoilPitch = TargetOffset.Y;
	AccumulatedRecoilYaw += Delta.X;
	PrevRecoilOffset = TargetOffset;
}

void UWeaponAimComponent::TickRecoilRecovery(float DeltaTime)
{
	if (TimeSinceLastShot < RecoilRecoveryDelay)
	{
		return;
	}

	if (FMath::IsNearlyZero(AccumulatedRecoilPitch) && FMath::IsNearlyZero(AccumulatedRecoilYaw))
	{
		return;
	}

	APawn* OwningPawn = Cast<APawn>(GetOwner() ? GetOwner()->GetOwner() : nullptr);
	if (!OwningPawn)
	{
		return;
	}

	const float NewPitch = FMath::FInterpTo(AccumulatedRecoilPitch, 0.f, DeltaTime, RecoilRecoverySpeed);
	const float NewYaw = FMath::FInterpTo(AccumulatedRecoilYaw, 0.f, DeltaTime, RecoilRecoverySpeed);

	// 누적된 반동을 되돌리는 방향(반대 부호)으로 입력을 넣어 원위치로 보간 복귀시킨다.
	OwningPawn->AddControllerPitchInput(-(NewPitch - AccumulatedRecoilPitch));
	OwningPawn->AddControllerYawInput(-(NewYaw - AccumulatedRecoilYaw));

	AccumulatedRecoilPitch = NewPitch;
	AccumulatedRecoilYaw = NewYaw;
}