// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/WeaponAimComponent.h"

#include "Curves/CurveVector.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Weapon/WeaponTypes.h"

UWeaponAimComponent::UWeaponAimComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	ApplySpreadRecoilTuning(FWeaponSpreadRecoilRow{});
	EffectiveBaseSpreadDegrees = BaseSpreadDegrees;
}

void UWeaponAimComponent::BeginPlay()
{
	Super::BeginPlay();

	ApplySpreadRecoilTuningTable();

	EffectiveBaseSpreadDegrees = BaseSpreadDegrees;
	EffectiveRecoilMultiplier = 1.f;
	CurrentADSZoomStep = FMath::Clamp(DefaultADSZoomStep, 0, GetADSZoomStepCount() - 1);
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

	ApplySpreadRecoilTuning(*Row);
}

void UWeaponAimComponent::ApplySpreadRecoilTuning(const FWeaponSpreadRecoilRow& Tuning)
{
	BaseSpreadDegrees = Tuning.BaseSpreadDegrees;
	BloomPerShotDegrees = Tuning.BloomPerShotDegrees;
	MaxBloomDegrees = Tuning.MaxBloomDegrees;
	BloomRecoveryDelay = Tuning.BloomRecoveryDelay;
	BloomRecoveryTime = Tuning.BloomRecoveryTime;
	MovingADSSpreadDegrees = Tuning.MovingADSSpreadDegrees;
	CrouchSpreadMultiplier = Tuning.CrouchSpreadMultiplier;

	RecoilCurve = Tuning.RecoilCurve;
	RecoilRandomness = Tuning.RecoilRandomness;
	RecoilMaxPitch = Tuning.RecoilMaxPitch;
	RecoilRecoveryDelay = Tuning.RecoilRecoveryDelay;
	RecoilRecoverySpeed = Tuning.RecoilRecoverySpeed;
	RecoilYawMultiplier = Tuning.RecoilYawMultiplier;
}

void UWeaponAimComponent::SetADS(const bool bNewIsADS)
{
	bIsADS = bCanADS && bNewIsADS;

	// 줌 단계는 여기서 리셋하지 않는다. 마지막으로 선택한 배율을 무기 인스턴스가 기억해서
	// ADS를 다시 시작해도 같은 배율로 복귀한다.
	if (!bIsADS)
	{
		ResetRecoilState();
	}
}

float UWeaponAimComponent::GetADSFieldOfView() const
{
	if (!bEnableVariableADSZoom)
	{
		return ADSFieldOfView;
	}

	const float WideFieldOfView = FMath::Max(MinimumMagnificationFieldOfView, MaximumMagnificationFieldOfView);
	const float NarrowFieldOfView = FMath::Min(MinimumMagnificationFieldOfView, MaximumMagnificationFieldOfView);
	const int32 StepCount = GetADSZoomStepCount();
	const int32 ClampedStep = FMath::Clamp(CurrentADSZoomStep, 0, StepCount - 1);
	const float ZoomAlpha = static_cast<float>(ClampedStep) / static_cast<float>(StepCount - 1);

	return FMath::Lerp(WideFieldOfView, NarrowFieldOfView, ZoomAlpha);
}

bool UWeaponAimComponent::AdjustADSZoom(const float InputDelta)
{
	if (!bIsADS || !bEnableVariableADSZoom || FMath::IsNearlyZero(InputDelta))
	{
		return false;
	}

	const int32 Direction = InputDelta > 0.f ? 1 : -1;
	const int32 NewStep = FMath::Clamp(CurrentADSZoomStep + Direction, 0, GetADSZoomStepCount() - 1);
	if (NewStep == CurrentADSZoomStep)
	{
		return false;
	}

	CurrentADSZoomStep = NewStep;
	return true;
}

void UWeaponAimComponent::SetMovementSpreadMultiplier(const float NewMultiplier)
{
	MovementSpreadMultiplier = NewMultiplier;
}

void UWeaponAimComponent::SetIsAirborne(const bool bNewIsAirborne)
{
	bIsAirborne = bNewIsAirborne;
}

void UWeaponAimComponent::SetIsStationary(const bool bNewIsStationary)
{
	bIsStationary = bNewIsStationary;
}

void UWeaponAimComponent::SetIsCrouching(const bool bNewIsCrouching)
{
	bIsCrouching = bNewIsCrouching;
}

void UWeaponAimComponent::SetEffectiveSpreadDegrees(const float NewSpreadDegrees)
{
	EffectiveBaseSpreadDegrees = NewSpreadDegrees;
}

void UWeaponAimComponent::SetEffectiveRecoilMultiplier(const float NewMultiplier)
{
	EffectiveRecoilMultiplier = NewMultiplier;
}

void UWeaponAimComponent::SetRecoilYawMultiplier(const float NewMultiplier)
{
	RecoilYawMultiplier = NewMultiplier;
}

float UWeaponAimComponent::GetCurrentSpreadDegrees() const
{
	return CalculateCurrentSpreadDegrees();
}

float UWeaponAimComponent::GetCurrentBloomDegrees() const
{
	const float MaxBloom = FMath::Max(0.f, MaxBloomDegrees);
	const float StoredBloom = FMath::Clamp(BloomAtLastShot, 0.f, MaxBloom);
	const UWorld* World = GetWorld();
	if (StoredBloom <= 0.f || !World)
	{
		return StoredBloom;
	}

	const double RecoveryElapsed = World->GetTimeSeconds() - BloomRecoveryStartTime;
	if (RecoveryElapsed < 0.0)
	{
		return StoredBloom;
	}
	if (BloomRecoveryTime <= 0.f)
	{
		return 0.f;
	}

	// 최대 블룸 기준의 일정한 회복 속도. 작은 점사는 더 빨리, 최대 연사도 정해진 시간에 끝난다.
	const float RecoveryFraction = static_cast<float>(FMath::Clamp(
		RecoveryElapsed / static_cast<double>(BloomRecoveryTime), 0.0, 1.0));
	return FMath::Max(0.f, StoredBloom - MaxBloom * RecoveryFraction);
}

void UWeaponAimComponent::TickAim(const float DeltaTime)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UWeaponAimComponent::TickAim);

	TimeSinceLastShot += DeltaTime;

	UpdateLegacyShotCount();
	TickProceduralRecoilGeneration(DeltaTime);
	TickRecoilRecovery(DeltaTime);
}

void UWeaponAimComponent::NotifyShotFired(const float ShotInterval)
{
	if (!bIsADS)
	{
		// 이미 회복된 현재 값에 한 발분만 더한다. 최대치 이후 발수는 회복 부채로 남기지 않는다.
		BloomAtLastShot = FMath::Clamp(GetCurrentBloomDegrees() + FMath::Max(0.f, BloomPerShotDegrees),
			0.f, FMath::Max(0.f, MaxBloomDegrees));
		const UWorld* World = GetWorld();
		BloomRecoveryStartTime = (World ? World->GetTimeSeconds() : 0.0)
			+ FMath::Max(0.f, ShotInterval) + FMath::Max(0.f, BloomRecoveryDelay);
	}
	// ADS 발사는 지향 블룸을 누적하거나 회복을 붙잡지 않는다. 반동 시계는 모든 발사에 반응한다.
	UpdateLegacyShotCount();
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
	float SpreadDegrees = 0.f;

	// ADS 중에는 정지(브레이킹 성공) 상태일 때만 탄퍼짐이 0으로 고정된다. 이동 중(브레이킹 실패)에는
	// 조준해도 MovingADSSpreadDegrees만큼 탄퍼짐이 붙는다 - 스코프를 껴도 움직이면서 쏘면 빗나가야
	// 한다는 요구사항 대응. bIsADS가 아닐 때(지향 사격)는 기존 블룸/이동 배율 계산을 그대로 쓴다.
	if (bIsADS)
	{
		if (!bIsStationary)
		{
			SpreadDegrees = MovingADSSpreadDegrees;
		}
	}
	else
	{
		const float BloomDegrees = GetCurrentBloomDegrees();
		SpreadDegrees = (EffectiveBaseSpreadDegrees + BloomDegrees) * MovementSpreadMultiplier;
	}

	// 정지 상태가 아니면(걷기/스프린트, 공중 포함) ADS로 고정한 0도, 이동 배율을 곱해도 0인
	// 지향사격(기본 탄퍼짐 0인 무기)도 전부 뚫고 강제로 탄퍼짐이 붙는다 - InAirSpreadPenaltyDegrees와
	// 같은 이유로 배율과 분리된 덧셈 페널티다.
	if (!bIsStationary)
	{
		SpreadDegrees += MovingSpreadPenaltyDegrees;
	}

	// 앉아서 쏘면 지상 탄퍼짐(지향 블룸/이동 배율/ADS 이동 탄퍼짐/이동 페널티)이 줄어든다. 공중 페널티는
	// 앉기와 무관하게 그대로 붙어야 해서 그 앞에서 곱한다.
	if (bIsCrouching)
	{
		SpreadDegrees *= CrouchSpreadMultiplier;
	}

	// 공중에서는 ADS로 고정한 0도, 이동 배율을 곱해도 0인 지향사격(기본 탄퍼짐 0인 무기)도 전부
	// 뚫고 강제로 탄퍼짐이 붙는다 - 곱셈(배율)만으로는 0에 무엇을 곱해도 0이라 표현이 안 돼서
	// 덧셈 페널티로 분리했다. 점프하면 조준 중이어도 정확도가 떨어져야 한다는 요구사항 대응.
	if (bIsAirborne)
	{
		SpreadDegrees += InAirSpreadPenaltyDegrees;
	}

	return SpreadDegrees;
}

void UWeaponAimComponent::UpdateLegacyShotCount()
{
	ConsecutiveShotCount = BloomPerShotDegrees > KINDA_SMALL_NUMBER
		? FMath::CeilToInt(GetCurrentBloomDegrees() / BloomPerShotDegrees) : 0;
}

void UWeaponAimComponent::TickProceduralRecoilGeneration(const float DeltaTime)
{
	// 사격 유지 구간(마지막 발사 이후 RecoilRecoveryDelay가 지나지 않은 동안)에 생성한다. ADS 여부와
	// 무관하게 지향 사격/ADS 공용으로 동작한다(3.3.1 개정 - 일반 사격도 반동 적용).
	// 이 구간을 벗어나면(사격 중단) 다음 진입 시 새 버스트로 취급하도록 플래그를 내린다.
	if (!RecoilCurve || TimeSinceLastShot >= RecoilRecoveryDelay)
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
	// 이동 중에는 탄퍼짐과 마찬가지로 반동도 커져야 한다(정지 1.0 / 걷기 1.5 / 공중,뛰기 2.5) -
	// CombatComponent::UpdateMovementSpreadMultiplier가 매 틱 갱신해서 넘겨주는 값을 그대로 재사용한다.
	Delta *= MovementSpreadMultiplier;
	// Yaw(좌우) 반동만 별도로 축소/제거할 수 있게 한다 - 예: 스나이퍼는 X축 반동을 0으로 설정.
	Delta.X *= RecoilYawMultiplier;

	if (APawn* OwningPawn = Cast<APawn>(GetOwner() ? GetOwner()->GetOwner() : nullptr))
	{
		OwningPawn->AddControllerYawInput(Delta.X);
		// 위로 튀어오르는 반동이므로 Pitch는 음수 방향으로 적용한다(엔진 Pitch 부호 규약 반전).
		OwningPawn->AddControllerPitchInput(-Delta.Y);
	}

	// Yaw와 마찬가지로 "실제로 카메라에 적용한 델타의 누적합"이어야 한다(= TargetOffset.Y로 덮어쓰면
	// 안 됨) - 그래야 (1) EffectiveRecoilMultiplier가 적용된 실제 적용량과 일치하고, (2) 복귀가 다
	// 끝나기 전에 새 버스트가 시작돼도 아직 못 돌아온 잔여값이 유실되지 않고 계속 추적된다.
	AccumulatedRecoilPitch += Delta.Y;
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

	// 누적된 반동을 되돌리는 방향으로 입력을 넣어 원위치로 보간 복귀시킨다. Pitch는 발사 시 부호를
	// 반전해서 적용했으므로(-Delta.Y) 복귀도 반전(-)이 필요하지만, Yaw는 발사 시 반전 없이 그대로
	// 적용했으므로(Delta.X) 복귀도 반전 없이 그대로 적용해야 한다 - 부호를 통일하면 Yaw가 원위치로
	// 돌아오지 않고 반동이 밀린 방향으로 계속 쌓이는 버그가 된다.
	OwningPawn->AddControllerPitchInput(-(NewPitch - AccumulatedRecoilPitch));
	OwningPawn->AddControllerYawInput(NewYaw - AccumulatedRecoilYaw);

	AccumulatedRecoilPitch = NewPitch;
	AccumulatedRecoilYaw = NewYaw;
}
