// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/WeaponBase.h"

#include "Camera/CameraComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/DataTable.h"
#include "Weapon/PenetrableSurfaceComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/KismetMathLibrary.h"

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* OwnerActor = GetOwner())
	{
		CachedOwnerCamera = OwnerActor->FindComponentByClass<UCameraComponent>();
	}

	ApplySpreadRecoilTuningTable();

	EffectiveRPM = BaseRPM;
	EffectiveAltRPM = (AltBaseRPM > 0.f) ? AltBaseRPM : BaseRPM;
	CurrentAmmo = MagazineSize;
}

void AWeaponBase::ApplySpreadRecoilTuningTable()
{
	if (!SpreadRecoilTable || SpreadRecoilRowName.IsNone())
	{
		return;
	}

	const FWeaponSpreadRecoilRow* Row = SpreadRecoilTable->FindRow<FWeaponSpreadRecoilRow>(
		SpreadRecoilRowName, TEXT("AWeaponBase::ApplySpreadRecoilTuningTable"));
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
	RecoilYawFactor = Row->RecoilYawFactor;
	RecoilRecoveryDelay = Row->RecoilRecoveryDelay;
	RecoilRecoverySpeed = Row->RecoilRecoverySpeed;
}

void AWeaponBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TimeSinceLastShot += DeltaTime;

	TickSpreadDecay(DeltaTime);
	TickRecoilRecovery(DeltaTime);
}

bool AWeaponBase::CanFireNow(bool bHeavyAttack) const
{
	const bool bUseAltRPM = bHeavyAttack && AltBaseRPM > 0.f;
	const float RPMForRequestedAttack = bUseAltRPM ? EffectiveAltRPM : EffectiveRPM;

	if (bIsReloading || (bUsesAmmo && CurrentAmmo <= 0) || RPMForRequestedAttack <= 0.f)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// 쿨다운은 직전에 실제로 발사된 공격(좌/우 무관)의 간격을 기준으로 하므로, 지금 요청한
	// 공격이 좌/우 어느 쪽이든 그 간격이 지나기 전에는 발사할 수 없다.
	return (World->GetTimeSeconds() - LastFireTime) >= LastFireInterval;
}

void AWeaponBase::SetEffectiveRPM(float NewRPM)
{
	EffectiveRPM = NewRPM;
}

void AWeaponBase::SetEffectiveAltRPM(float NewRPM)
{
	EffectiveAltRPM = NewRPM;
}

void AWeaponBase::SetMovementSpreadMultiplier(float NewMultiplier)
{
	MovementSpreadMultiplier = NewMultiplier;
}

void AWeaponBase::SetADS(bool bNewIsADS)
{
	bIsADS = bCanADS && bNewIsADS;
}

bool AWeaponBase::Fire(bool bHeavyAttack)
{
	if (!CanFireNow(bHeavyAttack))
	{
		// TODO(임시 검증용 로그): 좌/우클릭 쿨다운 상호 차단 확인 끝나면 제거할 것.
		if (const UWorld* BlockedWorld = GetWorld())
		{
			const float RemainingCooldown = LastFireInterval - (BlockedWorld->GetTimeSeconds() - LastFireTime);
			UE_LOG(LogTemp, Log, TEXT("[%s] Fire(bHeavyAttack=%s) 차단됨 - 남은 쿨다운 %.2fs"),
				*GetName(), bHeavyAttack ? TEXT("true") : TEXT("false"), FMath::Max(RemainingCooldown, 0.f));
		}
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FVector TraceStart;
	if (!GetTraceOrigin(TraceStart))
	{
		return false;
	}

	const bool bUseAltRPM = bHeavyAttack && AltBaseRPM > 0.f;
	const float RPMUsed = bUseAltRPM ? EffectiveAltRPM : EffectiveRPM;

	LastFireTime = World->GetTimeSeconds();
	LastFireInterval = 60.f / RPMUsed;

	// TODO(임시 검증용 로그): 좌/우클릭 쿨다운 상호 차단 확인 끝나면 제거할 것.
	UE_LOG(LogTemp, Log, TEXT("[%s] Fire(bHeavyAttack=%s) 성공 - RPM=%.1f, 다음 쿨다운=%.2fs"),
		*GetName(), bHeavyAttack ? TEXT("true") : TEXT("false"), RPMUsed, LastFireInterval);

	if (bUsesAmmo)
	{
		CurrentAmmo--;
	}

	if (ConsecutiveShotCount == 0)
	{
		BurstStartTime = World->GetTimeSeconds();
	}

	const float ShotDamage = (bHeavyAttack && AltBaseDamage > 0.f) ? AltBaseDamage : BaseDamage;
	const FVector AimDirection = CachedOwnerCamera->GetForwardVector();
	const int32 PelletCount = FMath::Max(PelletCountPerShot, 1);

	for (int32 PelletIndex = 0; PelletIndex < PelletCount; ++PelletIndex)
	{
		const FVector TraceDirection = CalculateTraceDirection(AimDirection);
		PerformPenetratingLineTrace(TraceStart, TraceDirection, PelletIndex, ShotDamage);
	}

	// 3.3.1 개정 원칙: 지향 사격은 탄퍼짐만, ADS는 반동만 발생시킨다.
	if (bIsADS)
	{
		ApplyProceduralRecoil();
	}

	ConsecutiveShotCount++;
	TimeSinceLastShot = 0.f;

	return true;
}

void AWeaponBase::StartReload()
{
	if (!bUsesAmmo || bIsReloading || CurrentAmmo >= MagazineSize)
	{
		return;
	}

	bIsReloading = true;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ReloadTimerHandle, this, &AWeaponBase::FinishReload, ReloadDuration, false);
	}
}

void AWeaponBase::FinishReload()
{
	CurrentAmmo = MagazineSize;
	bIsReloading = false;
}

float AWeaponBase::CalculateCurrentSpreadDegrees() const
{
	// 3.3.1 개정 원칙: ADS 중에는 탄퍼짐이 0으로 고정된다.
	if (bIsADS)
	{
		return 0.f;
	}

	const float BloomDegrees = FMath::Min(ConsecutiveShotCount * BloomPerShotDegrees, MaxBloomDegrees);
	return (BaseSpreadDegrees + BloomDegrees) * MovementSpreadMultiplier;
}

void AWeaponBase::TickSpreadDecay(float DeltaTime)
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

void AWeaponBase::ApplyProceduralRecoil()
{
	if (!RecoilCurve)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const float ElapsedTime = World ? (World->GetTimeSeconds() - BurstStartTime) : 0.f;
	const float RecoilValue = RecoilCurve->GetFloatValue(ElapsedTime);

	// 위로 튀어오르는 반동이므로 Pitch는 음수 방향으로 적용한다.
	const float PitchDelta = -RecoilValue;
	const float YawDelta = RecoilValue * RecoilYawFactor;

	if (APawn* OwningPawn = Cast<APawn>(GetOwner()))
	{
		OwningPawn->AddControllerPitchInput(PitchDelta);
		OwningPawn->AddControllerYawInput(YawDelta);
	}

	AccumulatedRecoilPitch += PitchDelta;
	AccumulatedRecoilYaw += YawDelta;
}

void AWeaponBase::TickRecoilRecovery(float DeltaTime)
{
	if (TimeSinceLastShot < RecoilRecoveryDelay)
	{
		return;
	}

	if (FMath::IsNearlyZero(AccumulatedRecoilPitch) && FMath::IsNearlyZero(AccumulatedRecoilYaw))
	{
		return;
	}

	APawn* OwningPawn = Cast<APawn>(GetOwner());
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

bool AWeaponBase::GetTraceOrigin(FVector& OutStart) const
{
	if (!CachedOwnerCamera)
	{
		return false;
	}

	OutStart = CachedOwnerCamera->GetComponentLocation();
	return true;
}

FVector AWeaponBase::CalculateTraceDirection(const FVector& AimDirection) const
{
	if (bIsADS)
	{
		// ADS 중에는 탄퍼짐이 없으므로 조준선 정중앙으로 발사한다.
		return AimDirection;
	}

	// 펠릿마다 독립적으로 호출되므로, 샷건처럼 PelletCountPerShot > 1인 무기는
	// 매 펠릿이 콘 안에서 각각 다른 방향으로 무작위 샘플링된다.
	const float SpreadDegrees = CalculateCurrentSpreadDegrees();
	return UKismetMathLibrary::RandomUnitVectorInConeInDegrees(AimDirection, SpreadDegrees);
}

float AWeaponBase::GetSurfaceThickness(const FHitResult& Hit) const
{
	if (const AActor* HitActor = Hit.GetActor())
	{
		if (const UPenetrableSurfaceComponent* SurfaceComp = HitActor->FindComponentByClass<UPenetrableSurfaceComponent>())
		{
			return SurfaceComp->PenetrationThickness;
		}
	}

	return DefaultSurfaceThickness;
}

float AWeaponBase::GetSurfaceHardness(const FHitResult& Hit) const
{
	if (const AActor* HitActor = Hit.GetActor())
	{
		if (const UPenetrableSurfaceComponent* SurfaceComp = HitActor->FindComponentByClass<UPenetrableSurfaceComponent>())
		{
			return SurfaceComp->SurfaceHardness;
		}
	}

	if (Hit.PhysMaterial.IsValid())
	{
		if (const float* FoundHardness = SurfaceHardnessMap.Find(Hit.PhysMaterial->SurfaceType))
		{
			return *FoundHardness;
		}
	}

	return DefaultSurfaceHardness;
}

float AWeaponBase::CalculateHitAngleDegrees(const FHitResult& Hit, const FVector& TraceDirection)
{
	const AActor* HitActor = Hit.GetActor();
	if (!HitActor)
	{
		return 0.f;
	}

	const FVector TargetForward = HitActor->GetActorForwardVector();
	const float DotProduct = FVector::DotProduct(TargetForward, TraceDirection.GetSafeNormal());
	return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotProduct, -1.f, 1.f)));
}

void AWeaponBase::PerformPenetratingLineTrace(const FVector& TraceStart, const FVector& TraceDirection, int32 PelletIndex, float ShotDamage)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector TraceEnd = TraceStart + TraceDirection * TraceRange;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WeaponBase_Fire), /*bTraceComplex=*/true, this);
	QueryParams.AddIgnoredActor(GetOwner());

	TArray<FHitResult> HitResults;
	World->LineTraceMultiByChannel(HitResults, TraceStart, TraceEnd, ECC_Visibility, QueryParams);

	float CurrentDamage = ShotDamage;
	float RemainingPower = PenetrationPower;

	for (const FHitResult& Hit : HitResults)
	{
		if (RemainingPower <= 0.f)
		{
			break;
		}

		const float DistanceMeters = FVector::Dist(TraceStart, Hit.Location) / 100.f;
		const float Thickness = GetSurfaceThickness(Hit);
		const float Hardness = GetSurfaceHardness(Hit);

		// 1. 관통 가능 여부 판정 (Penetration Power > Surface Hardness)
		const bool bCanPenetrate = RemainingPower > Hardness;

		RemainingPower = FMath::Max(RemainingPower - Thickness, 0.f);

		FWeaponHitResult WeaponHit;
		WeaponHit.SourceActor = GetOwner();
		WeaponHit.TargetActor = Hit.GetActor();
		WeaponHit.BaseDamage = CurrentDamage;
		WeaponHit.DistanceMeters = DistanceMeters;
		WeaponHit.HitLocation = Hit.Location;
		WeaponHit.HitAngleDegrees = CalculateHitAngleDegrees(Hit, TraceDirection);
		WeaponHit.HitBoneName = Hit.BoneName;
		WeaponHit.bPenetratedWall = bCanPenetrate;
		WeaponHit.RemainingPenetrationPower = RemainingPower;
		WeaponHit.PelletIndex = PelletIndex;

		OnWeaponHit.Broadcast(WeaponHit);

		if (!bCanPenetrate)
		{
			break;
		}

		// 2. 관통 두께 연산에 따른 대미지 감쇠: Rremain = 1 - (두께 / 무기 관통력)
		const float PenetrationRatio = FMath::Clamp(1.f - (Thickness / FMath::Max(PenetrationPower, KINDA_SMALL_NUMBER)), 0.f, 1.f);
		CurrentDamage *= PenetrationRatio;
	}
}