// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/WeaponBase.h"

#include "Camera/CameraComponent.h"
#include "Combat/Hitscan/HitscanResolverSubsystem.h"
#include "Combat/Hitscan/HitscanWeaponDefinition.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Interfaces/PlayerInterface.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Weapon/WeaponAimComponent.h"
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
	Aim = CreateDefaultSubobject<UWeaponAimComponent>(TEXT("Aim"));
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

	EffectiveRPM = BaseRPM;
	EffectiveAltRPM = (AltBaseRPM > 0.f) ? AltBaseRPM : BaseRPM;
	EffectiveMagazineSize = MagazineSize;
	EffectiveReloadDuration = ReloadDuration;
	EffectivePelletCount = PelletCountPerShot;
	EffectiveTraceRange = TraceRange;
	CurrentAmmo = EffectiveMagazineSize;
}

void AWeaponBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (Aim)
	{
		Aim->TickAim(DeltaTime);
	}
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
	if (Aim)
	{
		Aim->SetMovementSpreadMultiplier(NewMultiplier);
	}
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
	if (Aim)
	{
		Aim->SetEffectiveSpreadDegrees(NewSpreadDegrees);
	}
}

void AWeaponBase::SetEffectiveRecoilMultiplier(const float NewMultiplier)
{
	if (Aim)
	{
		Aim->SetEffectiveRecoilMultiplier(NewMultiplier);
	}
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
	if (Aim)
	{
		Aim->SetADS(bNewIsADS);
	}
}

bool AWeaponBase::IsADS() const
{
	return Aim && Aim->IsADS();
}

bool AWeaponBase::CanADS() const
{
	return Aim && Aim->CanADS();
}

float AWeaponBase::GetADSFieldOfView() const
{
	return Aim ? Aim->GetADSFieldOfView() : 0.f;
}

float AWeaponBase::GetBaseSpreadDegrees() const
{
	return Aim ? Aim->GetBaseSpreadDegrees() : 0.f;
}

float AWeaponBase::GetCurrentSpreadDegrees() const
{
	return Aim ? Aim->GetCurrentSpreadDegrees() : 0.f;
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
	if (Aim)
	{
		Aim->NotifyShotFired();
	}
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
	if (Aim)
	{
		Aim->ResetRecoilState();
	}
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
	if (Aim && Aim->IsADS())
	{
		// ADS 중에는 탄퍼짐이 없으므로 조준선 정중앙으로 발사한다.
		return AimDirection;
	}

	// 펠릿마다 독립적으로 호출되므로, 샷건처럼 PelletCountPerShot > 1인 무기는
	// 매 펠릿이 콘 안에서 각각 다른 방향으로 무작위 샘플링된다.
	const float SpreadDegrees = Aim ? Aim->GetCurrentSpreadDegrees() : 0.f;
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

	if (!IsValid(HitscanDefinition.Get()))
	{
		UE_LOG(
			LogWeaponHitscanBridge,
			Error,
			TEXT("HitscanDefinition is required by the GAS damage pipeline. Weapon=%s"),
			*GetNameSafe(this));
		return;
	}

	// Fire()의 기존 호출 경계를 유지하기 위한 레거시 인자다.
	// 신규 대미지는 WeaponDamageData와 ExecCalc_Damage에서 결정한다.
	(void)ShotDamage;

	FHitscanShotRequest Request;
	HitscanDefinition->ConfigureShotRequest(
		Request,
		EffectiveTraceRange,
		DefaultSurfaceThickness,
		DefaultSurfaceHardness);

	Request.InstigatorActor = GetOwner();
	Request.CauserActor = this;
	Request.TraceStart = TraceStart;

	// 충돌 Trace는 카메라에서 시작하지만 거리 감쇠는 총구 원점을 사용한다.
	// 발사 상태는 변경하지 않고 기존 요청 조립 경계에서 위치만 채운다.
	static const FName MuzzleSocketName(TEXT("Muzzle"));
	if (IsValid(Mesh3P.Get()) && Mesh3P->DoesSocketExist(MuzzleSocketName))
	{
		Request.DamageDistanceOrigin = Mesh3P->GetSocketLocation(MuzzleSocketName);
	}
	else
	{
		// Muzzle 소켓이 없는 레거시 무기는 카메라 Trace 시작점을 대신 사용한다.
		Request.DamageDistanceOrigin = TraceStart;
	}

	Request.bHasDamageDistanceOrigin = true;
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
			TEXT("Hitscan shot request was rejected. Weapon=%s PelletCount=%d Range=%.2f Damage=%.1f"),
			*GetNameSafe(this),
			PelletDirections.Num(),
			EffectiveTraceRange,
			ShotDamage);
	}
}
