// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/WeaponBase.h"

#include "Camera/CameraComponent.h"
#include "Combat/Hitscan/HitscanResolverSubsystem.h"
#include "Combat/Hitscan/HitscanWeaponDefinition.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveVector.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/PlayerInterface.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Weapon/WeaponEffectsComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogWeaponHitscanBridge, Log, All);

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = true;

	// 데디케이티드/리슨 서버에서 CombatComponent가 스폰한 이 액터가 클라이언트에도 존재해야
	// OnRep_EquippedWeapon에서 각자 로컬로 부착할 대상이 생긴다.
	bReplicates = true;

	// 부착 지점 역할만 하는 빈 루트. Mesh1P/Mesh3P는 CombatComponent가 장착 시 각자 다른
	// 캐릭터 컴포넌트(Mesh1P/Mesh3P)로 재부착하므로 서로의 부모-자식이 되면 안 된다.
	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// 1인칭 전용: 오너 본인만 본다.
	Mesh1P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh1P"));
	Mesh1P->SetupAttachment(SceneRoot);
	Mesh1P->SetOnlyOwnerSee(true);
	Mesh1P->SetOwnerNoSee(false);
	Mesh1P->SetCastShadow(false);
	Mesh1P->bReceivesDecals = false;
	Mesh1P->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 3인칭 전용: 오너 본인에게는 숨기고 다른 플레이어에게만 보인다.
	Mesh3P = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh3P"));
	Mesh3P->SetupAttachment(SceneRoot);
	Mesh3P->SetOnlyOwnerSee(false);
	Mesh3P->SetOwnerNoSee(true);
	Mesh3P->bReceivesDecals = false;
	Mesh3P->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Effects = CreateDefaultSubobject<UWeaponEffectsComponent>(TEXT("Effects"));
}


void AWeaponBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AWeaponBase, bIsReloading);
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
	EffectiveMagazineSize = MagazineSize;
	EffectiveReloadDuration = ReloadDuration;
	EffectivePelletCount = PelletCountPerShot;
	EffectiveBaseSpreadDegrees = BaseSpreadDegrees;
	EffectiveRecoilMultiplier = 1.f;
	EffectiveTraceRange = TraceRange;
	CurrentAmmo = EffectiveMagazineSize;
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
	RecoilRandomness = Row->RecoilRandomness;
	RecoilMaxPitch = Row->RecoilMaxPitch;
	RecoilRecoveryDelay = Row->RecoilRecoveryDelay;
	RecoilRecoverySpeed = Row->RecoilRecoverySpeed;
}

void AWeaponBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TimeSinceLastShot += DeltaTime;

	TickSpreadDecay(DeltaTime);
	TickProceduralRecoilGeneration(DeltaTime);
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

void AWeaponBase::TryPlayEmptyClipSound()
{
	if (!IsOutOfAmmo())
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 연사 입력을 누르고 있으면 CanActivateAbility가 매 프레임 재평가되므로, 실제 총이 쐈을 법한
	// RPM 간격으로만 딸깍 소리가 나도록 LastFireTime을 그대로 재사용해 스스로 스팸을 막는다.
	const float ClickInterval = EffectiveRPM > 0.f ? (60.f / EffectiveRPM) : 0.5f;
	if ((World->GetTimeSeconds() - LastFireTime) < ClickInterval)
	{
		return;
	}

	LastFireTime = World->GetTimeSeconds();

	if (Effects)
	{
		Effects->PlayEmptyClipSound();
	}
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

void AWeaponBase::SetEffectiveMagazineSize(const int32 NewMagazineSize)
{
	EffectiveMagazineSize = NewMagazineSize;
}

void AWeaponBase::SetEffectiveReloadDuration(const float NewReloadDuration)
{
	EffectiveReloadDuration = NewReloadDuration;
}

void AWeaponBase::SetEffectivePelletCount(const int32 NewPelletCount)
{
	EffectivePelletCount = NewPelletCount;
}

void AWeaponBase::SetEffectiveSpreadDegrees(const float NewSpreadDegrees)
{
	EffectiveBaseSpreadDegrees = NewSpreadDegrees;
}

void AWeaponBase::SetEffectiveRecoilMultiplier(const float NewMultiplier)
{
	EffectiveRecoilMultiplier = NewMultiplier;
}

void AWeaponBase::SetEffectiveTraceRange(const float NewTraceRangeCentimeters)
{
	EffectiveTraceRange = NewTraceRangeCentimeters;
}

FGameplayTag AWeaponBase::GetWeaponTag() const
{
	return IsValid(HitscanDefinition.Get()) ? HitscanDefinition->GetWeaponTag() : FGameplayTag();
}

void AWeaponBase::SetADS(bool bNewIsADS)
{
	bIsADS = bCanADS && bNewIsADS;

	// ADS 해제 시 반동 값을 즉시 0으로 리셋한다(서서히 복귀하지 않음) - 지향 사격으로 전환되면
	// 탄퍼짐 계산으로 넘어가므로 잔여 반동이 이어지면 안 된다.
	if (!bIsADS)
	{
		ResetProceduralRecoilState();
	}
}

void AWeaponBase::SetPresentationActive(const bool bActive)
{
	SetActorHiddenInGame(!bActive);

	if (IsValid(Mesh1P))
	{
		Mesh1P->SetVisibility(bActive, true);
	}
	if (IsValid(Mesh3P))
	{
		Mesh3P->SetVisibility(bActive, true);
	}

	if (bActive)
	{
		AttachToOwnerMeshes();
	}
}

void AWeaponBase::AttachToOwnerMeshes() const
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->GetClass()->ImplementsInterface(UPlayerInterface::StaticClass()))
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh1P = IPlayerInterface::Execute_GetMesh1P(OwnerActor);
	USkeletalMeshComponent* CharacterMesh3P = IPlayerInterface::Execute_GetMesh3P(OwnerActor);

	if (IsValid(CharacterMesh1P) && IsValid(Mesh1P))
	{
		Mesh1P->AttachToComponent(
			CharacterMesh1P, FAttachmentTransformRules::SnapToTargetNotIncludingScale, GripSocketName1P);
	}

	if (IsValid(CharacterMesh3P) && IsValid(Mesh3P))
	{
		Mesh3P->AttachToComponent(
			CharacterMesh3P, FAttachmentTransformRules::SnapToTargetNotIncludingScale, GripSocketName3P);
	}
}

bool AWeaponBase::Fire(bool bHeavyAttack)
{
	if (!CanFireNow(bHeavyAttack))
	{
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

	const float RPMUsed = ResolveRPMForShot(bHeavyAttack);
	RegisterFireCooldown(*World, RPMUsed);

	const int32 ShotIndexInMagazine = ConsumeAmmoForShot();
	const float ShotDamage = ResolveShotDamage(bHeavyAttack);
	const TArray<FVector> PelletDirections = GeneratePelletDirections();

	ResolveHitscanShot(TraceStart, PelletDirections, ShotDamage, ShotIndexInMagazine);
	if (Effects)
	{
		Effects->PlayFireEffects(TraceStart, PelletDirections);
		Effects->PlayFireCameraShake();
	}
	AdvanceFireCounters();

	return true;
}

float AWeaponBase::ResolveRPMForShot(const bool bHeavyAttack) const
{
	const bool bUseAltRPM = bHeavyAttack && AltBaseRPM > 0.f;
	return bUseAltRPM ? EffectiveAltRPM : EffectiveRPM;
}

void AWeaponBase::RegisterFireCooldown(const UWorld& World, const float RPMUsed)
{
	LastFireTime = World.GetTimeSeconds();
	LastFireInterval = 60.f / RPMUsed;
}

int32 AWeaponBase::ConsumeAmmoForShot()
{
	const int32 ShotIndexInMagazine = bUsesAmmo
		? FMath::Max(0, EffectiveMagazineSize - CurrentAmmo)
		: 0;

	if (bUsesAmmo)
	{
		CurrentAmmo--;
	}

	return ShotIndexInMagazine;
}

float AWeaponBase::ResolveShotDamage(const bool bHeavyAttack) const
{
	return (bHeavyAttack && AltBaseDamage > 0.f) ? AltBaseDamage : BaseDamage;
}

TArray<FVector> AWeaponBase::GeneratePelletDirections() const
{
	const FVector AimDirection = CachedOwnerCamera->GetForwardVector();
	const int32 PelletCount = FMath::Max(EffectivePelletCount, 1);

	TArray<FVector> PelletDirections;
	PelletDirections.Reserve(PelletCount);
	for (int32 PelletIndex = 0; PelletIndex < PelletCount; ++PelletIndex)
	{
		PelletDirections.Add(CalculateTraceDirection(AimDirection));
	}

	return PelletDirections;
}

void AWeaponBase::AdvanceFireCounters()
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

void AWeaponBase::StartReload()
{
	if (!bUsesAmmo || bIsReloading || CurrentAmmo >= EffectiveMagazineSize)
	{
		return;
	}

	bIsReloading = true;
	// 재장전 진입 즉시 반동을 리셋한다(전투 시스템 명세서 7장 엣지 케이스) - 재장전 후 첫 발은
	// 항상 무반동 상태로 시작한다.
	ResetProceduralRecoilState();
	if (HasAuthority())
	{
		ForceNetUpdate();
	}

	if (Effects)
	{
		Effects->NotifyReloadStarted(EffectiveReloadDuration);
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ReloadTimerHandle, this, &AWeaponBase::FinishReload, EffectiveReloadDuration, false);
	}
}

void AWeaponBase::FinishReload()
{
	CurrentAmmo = EffectiveMagazineSize;
	bIsReloading = false;
	if (HasAuthority())
	{
		ForceNetUpdate();
	}

	if (Effects)
	{
		Effects->PlayReloadFinishSound();
	}
}

float AWeaponBase::CalculateCurrentSpreadDegrees() const
{
	// 3.3.1 개정 원칙: ADS 중에는 탄퍼짐이 0으로 고정된다.
	if (bIsADS)
	{
		return 0.f;
	}

	const float BloomDegrees = FMath::Min(ConsecutiveShotCount * BloomPerShotDegrees, MaxBloomDegrees);
	return (EffectiveBaseSpreadDegrees + BloomDegrees) * MovementSpreadMultiplier;
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

void AWeaponBase::TickProceduralRecoilGeneration(const float DeltaTime)
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

	if (APawn* OwningPawn = Cast<APawn>(GetOwner()))
	{
		OwningPawn->AddControllerYawInput(Delta.X);
		// 위로 튀어오르는 반동이므로 Pitch는 음수 방향으로 적용한다(엔진 Pitch 부호 규약 반전).
		OwningPawn->AddControllerPitchInput(-Delta.Y);
	}

	AccumulatedRecoilPitch = TargetOffset.Y;
	AccumulatedRecoilYaw += Delta.X;
	PrevRecoilOffset = TargetOffset;
}

void AWeaponBase::ResetProceduralRecoilState()
{
	FiringElapsedTime = 0.f;
	PrevRecoilOffset = FVector2D::ZeroVector;
	AccumulatedRecoilPitch = 0.f;
	AccumulatedRecoilYaw = 0.f;
	bRecoilBurstActive = false;
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

void AWeaponBase::ResolveHitscanShot(
	const FVector& TraceStart,
	const TArray<FVector>& PelletDirections,
	const float ShotDamage,
	const int32 ShotIndexInMagazine)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	UHitscanResolverSubsystem* Resolver = World->GetSubsystem<UHitscanResolverSubsystem>();
	if (!IsValid(Resolver))
	{
		if (HasAuthority())
		{
			UE_LOG(LogWeaponHitscanBridge, Warning, TEXT("Hitscan Resolver subsystem is unavailable. Weapon=%s"), *GetNameSafe(this));
		}
		return;
	}

	FHitscanShotRequest Request;
	if (IsValid(HitscanDefinition.Get()))
	{
		HitscanDefinition->ConfigureShotRequest(
			Request,
			ShotDamage,
			EffectiveTraceRange,
			DefaultSurfaceThickness,
			DefaultSurfaceHardness);
	}
	else
	{
		// Definition이 없는 레거시 무기도 기본 사격은 동작하되, 정적 정책은 기본값을 사용한다.
		Request.DamageSettings.BaseDamagePerPellet = FMath::Max(0.0f, ShotDamage);
		Request.DamageSettings.DistanceFalloff.MaximumRangeMeters = FMath::Max(0.0f, EffectiveTraceRange / 100.0f);
		Request.TraceSettings.MaximumRangeCentimeters = FMath::Max(0.0f, EffectiveTraceRange);
		Request.TraceSettings.FallbackSurfaceThicknessCentimeters = FMath::Max(0.1f, DefaultSurfaceThickness);
		Request.TraceSettings.DefaultSurfaceRule.Hardness = FMath::Max(0.0f, DefaultSurfaceHardness);
	}

	Request.InstigatorActor = GetOwner();
	Request.CauserActor = this;
	Request.TraceStart = TraceStart;
	Request.PelletDirections = MakeArrayView(PelletDirections);
	Request.ShotId = FGuid::NewGuid();
	Request.ShotIndexInMagazine = FMath::Max(0, ShotIndexInMagazine);
	Request.InitialPenetrationPower = FMath::Max(0.0f, PenetrationPower);
	Request.SurfaceHardnessOverrides = &SurfaceHardnessMap;

	const FHitscanShotResult ShotResult = Resolver->ResolveServerShot(Request);
	if (HasAuthority() && !ShotResult.bProcessed)
	{
		UE_LOG(
			LogWeaponHitscanBridge,
			Warning,
			TEXT("Hitscan shot request was rejected. Weapon=%s PelletCount=%d Range=%.2f Damage=%.2f"),
			*GetNameSafe(this),
			PelletDirections.Num(),
			EffectiveTraceRange,
			ShotDamage);
	}
}
