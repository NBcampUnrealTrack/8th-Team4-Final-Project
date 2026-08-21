// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/WeaponBase.h"

#include "Camera/CameraComponent.h"
#include "Combat/Hitscan/HitscanResolverSubsystem.h"
#include "Combat/Hitscan/HitscanWeaponDefinition.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/PlayerInterface.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"

// ** 임시
#include "Kismet/GameplayStatics.h"

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

FGameplayTag AWeaponBase::GetWeaponTag() const
{
	return IsValid(HitscanDefinition.Get()) ? HitscanDefinition->GetWeaponTag() : FGameplayTag();
}

void AWeaponBase::SetADS(bool bNewIsADS)
{
	bIsADS = bCanADS && bNewIsADS;
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
	BeginBurstIfNeeded(*World);

	const int32 ShotIndexInMagazine = ConsumeAmmoForShot();
	const float ShotDamage = ResolveShotDamage(bHeavyAttack);
	const TArray<FVector> PelletDirections = GeneratePelletDirections();

	ResolveHitscanShot(TraceStart, PelletDirections, ShotDamage, ShotIndexInMagazine);
	PlayFireEffects(TraceStart);
	ApplyRecoilIfADS();
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

void AWeaponBase::BeginBurstIfNeeded(const UWorld& World)
{
	if (ConsecutiveShotCount == 0)
	{
		BurstStartTime = World.GetTimeSeconds();
	}
}

int32 AWeaponBase::ConsumeAmmoForShot()
{
	const int32 ShotIndexInMagazine = bUsesAmmo
		? FMath::Max(0, MagazineSize - CurrentAmmo)
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
	const int32 PelletCount = FMath::Max(PelletCountPerShot, 1);

	TArray<FVector> PelletDirections;
	PelletDirections.Reserve(PelletCount);
	for (int32 PelletIndex = 0; PelletIndex < PelletCount; ++PelletIndex)
	{
		PelletDirections.Add(CalculateTraceDirection(AimDirection));
	}

	return PelletDirections;
}

// ** 임시 - 담당자 코드로 수정해주세요
void AWeaponBase::PlayFireEffects(const FVector& TraceStart)
{
	if (HasAuthority() && FireSound)
	{
		Multicast_PlayFireSound(TraceStart);
	}
}

void AWeaponBase::ApplyRecoilIfADS()
{
	// 3.3.1 개정 원칙: 지향 사격은 탄퍼짐만, ADS는 반동만 발생시킨다.
	if (bIsADS)
	{
		ApplyProceduralRecoil();
	}
}

void AWeaponBase::AdvanceFireCounters()
{
	ConsecutiveShotCount++;
	TimeSinceLastShot = 0.f;
}

// ** 임시 코드 - 담당자 코드로 수정해주세요
void AWeaponBase::Multicast_PlayFireSound_Implementation(FVector Location)
{
	if (FireSound)
	{
		UGameplayStatics::SpawnSoundAttached(
			FireSound,
			GetRootComponent(),
			NAME_None,
			Location,
			FRotator::ZeroRotator,
			EAttachLocation::KeepWorldPosition);
	}
}

void AWeaponBase::StartReload()
{
	if (!bUsesAmmo || bIsReloading || CurrentAmmo >= MagazineSize)
	{
		return;
	}

	bIsReloading = true;
	if (HasAuthority())
	{
		ForceNetUpdate();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ReloadTimerHandle, this, &AWeaponBase::FinishReload, ReloadDuration, false);
	}
}

void AWeaponBase::FinishReload()
{
	CurrentAmmo = MagazineSize;
	bIsReloading = false;
	if (HasAuthority())
	{
		ForceNetUpdate();
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
			TraceRange,
			DefaultSurfaceThickness,
			DefaultSurfaceHardness);
	}
	else
	{
		// Definition이 없는 레거시 무기도 기본 사격은 동작하되, 정적 정책은 기본값을 사용한다.
		Request.DamageSettings.BaseDamagePerPellet = FMath::Max(0.0f, ShotDamage);
		Request.DamageSettings.DistanceFalloff.MaximumRangeMeters = FMath::Max(0.0f, TraceRange / 100.0f);
		Request.TraceSettings.MaximumRangeCentimeters = FMath::Max(0.0f, TraceRange);
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
			TraceRange,
			ShotDamage);
	}
}
