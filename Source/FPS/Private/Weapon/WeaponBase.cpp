// Fill out your copyright notice in the Description page of Project Settings.

#include "Weapon/WeaponBase.h"
#include "Customization/GameplayAppearanceComponent.h"
#include "Customization/AppearanceGrip.h"
#include "Engine/SkeletalMesh.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Combat/CombatComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/ShooterAnimInstance.h"
#include "Character/ShooterCharacter.h"
#include "GameMode/Types/DominationTypes.h"
#include "Player/DominationPlayerState.h"

#include "Camera/CameraComponent.h"
#include "Combat/Hitscan/HitscanResolverSubsystem.h"
#include "Combat/Hitscan/Definitions/HitscanSurfaceDefinition.h"
#include "Combat/Hitscan/Definitions/HitscanWeaponDefinition.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/WeaponDamageData.h" 
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/PlayerInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Particles/ParticleSystem.h"
#include "Tags/GameplayTags.h"
#include "Weapon/WeaponAimComponent.h"
#include "Weapon/WeaponEffectsComponent.h"

AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = true;

	// 데디케이티드/리슨 서버에서 CombatComponent가 스폰한 이 액터가 클라이언트에도 존재해야
	// OnRep_EquippedWeapon에서 각자 로컬로 부착할 대상이 생긴다.
	bReplicates = true;
	bNetUseOwnerRelevancy = true;

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
	Mesh1P->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;

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
	DOREPLIFETIME(AWeaponBase, EffectiveReloadDuration);
	DOREPLIFETIME(AWeaponBase, CurrentReserveAmmo);
	DOREPLIFETIME(AWeaponBase, bInfiniteAmmo);
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
	// Initial replication can precede BeginPlay on observers. Do not replace the
	// server's duration with the weapon BP's fallback (notably 1.4s for pistols).
	if (HasAuthority()) EffectiveReloadDuration = ReloadDuration;
	EffectivePelletCount = PelletCountPerShot;
	EffectiveTraceRange = TraceRange;
	CurrentAmmo = EffectiveMagazineSize;
	// 초기 복제가 BeginPlay보다 먼저 도착한 클라이언트의 예비탄을 덮어쓰지 않는다.
	if (HasAuthority()) CurrentReserveAmmo = MaxReserveAmmo;
	EffectiveMaxReserveAmmo = MaxReserveAmmo;
	ShotIndexInMagazine = 0;
	bEmptyClipNotified = false;
	bEmptyReloadArmed = false;

	// 스나이퍼는 좌우(Yaw) 반동이 없어야 한다 - 무기 BP/DataTable에 설정된 값과 무관하게 코드에서
	// 강제한다(CanADS()/GetCurrentSpreadDegrees()의 스나이퍼 강제 규칙과 같은 패턴). ApplySpreadRecoilTuningTable이
	// 이미 실행된 뒤(Aim->BeginPlay는 위 Super::BeginPlay()에서 호출됨)라 여기서 덮어써야 최종값이 된다.
	if (Aim && GetWeaponTag() == RogueTags::Weapon_Type_Sniper)
	{
		Aim->SetRecoilYawMultiplier(0.f);
	}

	if (GetOwner() && GetOwner()->Implements<UPlayerInterface>())
	{
		IPlayerInterface::Execute_OnWeaponSpawned(GetOwner(), this);
	}
}

void AWeaponBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 재장전 중 무기가 파괴(스왑 정리/사망/레벨 이동)되면 ReloadTimerHandle 콜백이 파괴된 무기를
	// 가리키게 되므로, 이 객체가 건 타이머를 전부 한 번에 해제한다.
	GetWorldTimerManager().ClearAllTimersForObject(this);

	Super::EndPlay(EndPlayReason);
}

void AWeaponBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (Aim)
	{
		Aim->TickAim(DeltaTime);
	}
}

void AWeaponBase::OnRep_Owner()
{
	Super::OnRep_Owner();

	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor))
	{
		return;
	}

	// BeginPlay가 오너보다 먼저 실행됐다면 카메라 캐시가 비어 있으므로 여기서 다시 잡는다.
	if (!CachedOwnerCamera)
	{
		CachedOwnerCamera = OwnerActor->FindComponentByClass<UCameraComponent>();
	}

	if (OwnerActor->Implements<UPlayerInterface>())
	{
		IPlayerInterface::Execute_OnWeaponSpawned(OwnerActor, this);
	}

	// 현재 손에 든 무기(서버가 SetActorHiddenInGame(false)로 복제한 상태)만 다시 부착/표시한다.
	// 숨겨진 슬롯 무기는 부착만 확정하고 가시성은 건드리지 않는다.
	AttachToOwnerMeshes();

	const bool bPresented = !IsHidden();
	SetFirstPersonPresentationVisible(bPresented);
	if (IsValid(Mesh3P))
	{
		Mesh3P->SetVisibility(bPresented, true);
	}
}

bool AWeaponBase::CanFireNow(bool bHeavyAttack) const
{
	if (const AActor* OwnerActor = GetOwner())
	{
		if (const UCombatComponent* Combat = OwnerActor->FindComponentByClass<UCombatComponent>())
		{
			if (!Combat->IsWeaponReady() || Combat->GetCurrentWeapon() != this) return false;
		}
		if (const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner()))
		{
			if (ASC->HasMatchingGameplayTag(RogueTags::State_Combat_IsSwapping)) return false;
		}
	}
	const bool bUseAltRPM = bHeavyAttack && AltBaseRPM > 0.f;
	const float RPMForRequestedAttack = bUseAltRPM ? EffectiveAltRPM : EffectiveRPM;

	if (bIsReloading || (bUsesAmmo && !bInfiniteAmmo && CurrentAmmo <= 0) || RPMForRequestedAttack <= 0.f)
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

bool AWeaponBase::TryPlayEmptyClipSound()
{
	// 재장전 중에는 탄약이 채워지기 전까지 IsOutOfAmmo()가 계속 true라, 재장전 도중 사격
	// 입력을 눌러도 빈 탄창 클릭음이 또 재생되지 않도록 막는다.
	if (!IsOutOfAmmo() || bIsReloading)
	{
		return false;
	}

	// 탄창이 빈 뒤 첫 호출에서만 소리를 낸다 - 그 뒤 호출엔 false를 반환한다. 호출부(GA_RangedAttack)는
	// 이 false + IsEmptyReloadArmed()가 참일 때만(=발사 입력을 떼었다 다시 누름) 재장전을 시작한다.
	if (bEmptyClipNotified)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	bEmptyClipNotified = true;
	bEmptyReloadArmed = false;
	LastFireTime = World->GetTimeSeconds();

	if (Effects)
	{
		Effects->PlayEmptyClipSound();
	}

	return true;
}

void AWeaponBase::PlayEquipSound()
{
	if (Effects)
	{
		Effects->PlayEquipSound();
	}
}

void AWeaponBase::PlayMeleeAttackSound(const bool bHeavyAttack)
{
	if (Effects)
	{
		Effects->PlayMeleeAttackSound(bHeavyAttack);
	}
}

void AWeaponBase::NotifyPrimaryInputReleased()
{
	// 탄창이 빈 상태에서 클릭음을 이미 들었을 때만 의미가 있다 - 이 시점에 발사 입력을 떼었으니
	// 다음 발사 입력은 "떼었다 다시 누른 새 입력"으로 보고 재장전을 허용한다(GA_RangedAttack).
	if (IsOutOfAmmo() && bEmptyClipNotified && !bIsReloading)
	{
		bEmptyReloadArmed = true;
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

void AWeaponBase::SetIsAirborne(bool bNewIsAirborne)
{
	if (Aim)
	{
		Aim->SetIsAirborne(bNewIsAirborne);
	}
}

void AWeaponBase::SetIsStationary(bool bNewIsStationary)
{
	if (Aim)
	{
		Aim->SetIsStationary(bNewIsStationary);
	}
}

void AWeaponBase::SetIsCrouching(bool bNewIsCrouching)
{
	if (Aim)
	{
		Aim->SetIsCrouching(bNewIsCrouching);
	}
}

void AWeaponBase::SetEffectiveMagazineSize(const int32 NewMagazineSize)
{
	EffectiveMagazineSize = NewMagazineSize;
	OnAmmoChanged.Broadcast();
}

void AWeaponBase::ResizeMagazine(const int32 NewMagazineSize)
{
	const int32 Delta = NewMagazineSize - EffectiveMagazineSize;
	EffectiveMagazineSize = NewMagazineSize;
	CurrentAmmo = FMath::Clamp(CurrentAmmo + Delta, 0, EffectiveMagazineSize);
	OnAmmoChanged.Broadcast();
}

void AWeaponBase::ResizeReserve(const int32 NewMaxReserveAmmo)
{
	const int32 Delta = NewMaxReserveAmmo - EffectiveMaxReserveAmmo;
	EffectiveMaxReserveAmmo = NewMaxReserveAmmo;
	CurrentReserveAmmo = FMath::Max(CurrentReserveAmmo + Delta, 0);
	OnAmmoChanged.Broadcast();
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

bool AWeaponBase::GetAimTraceSettings(FHitscanTraceSettings& OutTraceSettings) const
{
	if (!IsValid(HitscanDefinition.Get()))
	{
		return false;
	}
	FHitscanShotRequest Request;
	HitscanDefinition->ConfigureShotRequest(Request, GetPhysicalTraceRange(false, false));
	OutTraceSettings = Request.TraceSettings;
	return FMath::IsFinite(OutTraceSettings.MaximumRangeCentimeters)
		&& OutTraceSettings.MaximumRangeCentimeters > 0.f;
}

float AWeaponBase::GetPhysicalTraceRange(const bool bHeavyAttack, const bool bAllowLoadDamageData) const
{
	// MaximumRangeMeters를 넘어가도 대미지는 0이 아니라 MinimumDamage로 수렴
	// EffectiveTraceRange 자체를 바꾸면 이펙트 종료 지점도 같이 늘어나 예광탄이
	// 1km 밖까지 날아가는 것처럼 보이므로 로컬 변수로만 분리
	// 근접무기(bUsesAmmo=false)는 사거리 감쇠 표현 대상이 아니므로 이 확장 로직을 타면 안 된다 -
	// 안 그러면 DamageEntry에 MaximumRangeMeters가 잘못 들어있을 때 칼의 물리 Trace가 1km까지
	// 늘어나서, 실제 근접 사거리 밖의 적에게도 히트(→히트마커)만 등록되고 대미지는 0(MinimumDamage로
	// 수렴)인 상황이 생긴다.
	float PhysicalTraceRangeCentimeters = EffectiveTraceRange;
	if (bUsesAmmo)
	{
		// 조준 표시만으로 동기 로드하지 않는다. 프리로드 전에는 무기 기본 사거리를 쓴다.
		const FWeaponDamageEntry* DamageEntry =
			UWeaponDamageData::FindEntry(GetWeaponTag(bHeavyAttack), bAllowLoadDamageData);
		if (DamageEntry)
		{
			if (DamageEntry->MaximumRangeMeters > 0.f)
			{
				constexpr float EffectivelyUnlimitedTraceRangeCentimeters = 100000.f; // 1km
				PhysicalTraceRangeCentimeters = FMath::Max(PhysicalTraceRangeCentimeters, EffectivelyUnlimitedTraceRangeCentimeters);
			}
		}
	}

	return PhysicalTraceRangeCentimeters;
}

FGameplayTag AWeaponBase::GetWeaponTag(const bool bHeavyAttack) const
{
	if (bHeavyAttack && AltWeaponTag.IsValid())
	{
		return AltWeaponTag;
	}

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
	// 라이플과 스나이퍼는 조준을 지원하고, 각 무기의 Aim 설정을 따른다.
	const FGameplayTag WeaponTag = GetWeaponTag();
	const bool bSupportedWeapon = WeaponTag == RogueTags::Weapon_Type_Rifle
		|| WeaponTag == RogueTags::Weapon_Type_Sniper;
	return bSupportedWeapon && Aim && Aim->CanADS();
}

float AWeaponBase::GetADSFieldOfView() const
{
	return Aim ? Aim->GetADSFieldOfView() : 0.f;
}

float AWeaponBase::GetADSCameraInterpSpeedOverride() const
{
	return Aim ? Aim->GetADSCameraInterpSpeedOverride() : 0.f;
}

float AWeaponBase::GetADSMoveSpeedMultiplier() const
{
	return Aim ? Aim->GetADSMoveSpeedMultiplier() : 1.f;
}

bool AWeaponBase::SupportsVariableADSZoom() const
{
	return Aim && Aim->SupportsVariableADSZoom();
}

bool AWeaponBase::AdjustADSZoom(const float InputDelta)
{
	return Aim && Aim->AdjustADSZoom(InputDelta);
}

int32 AWeaponBase::GetCurrentADSZoomStep() const
{
	return Aim ? Aim->GetCurrentADSZoomStep() : 0;
}

int32 AWeaponBase::GetADSZoomStepCount() const
{
	return Aim ? Aim->GetADSZoomStepCount() : 0;
}

bool AWeaponBase::ShouldHideFirstPersonPresentationWhileADS() const
{
	return Aim && Aim->ShouldHideFirstPersonPresentationWhileADS();
}

bool AWeaponBase::ShouldHideMainHUDWhileADS() const
{
	return Aim && Aim->ShouldHideMainHUDWhileADS();
}

float AWeaponBase::GetBaseSpreadDegrees() const
{
	return Aim ? Aim->GetBaseSpreadDegrees() : 0.f;
}

float AWeaponBase::GetCurrentSpreadDegrees() const
{
	if (!Aim)
	{
		return 0.f;
	}

	// 스나이퍼는 스코프 없이(비ADS) 쏘면 이동/블룸 계산과 무관하게 항상 크게 흔들려야 한다 -
	// 반드시 정조준해야만 쓸 수 있는 무기로 만들기 위한 강제 규칙(CanADS()와 같은 패턴).
	if (!Aim->IsADS() && GetWeaponTag() == RogueTags::Weapon_Type_Sniper)
	{
		return SniperUnscopedSpreadDegrees;
	}

	return Aim->GetCurrentSpreadDegrees();
}

void AWeaponBase::SetFirstPersonPresentationVisible(const bool bVisible)
{
	if (IsValid(Mesh1P))
	{
		// 스코프/도트사이트처럼 BP에서 Mesh1P 소켓 아래에 추가한 1P 부착물도
		// ADS 표현 정책을 그대로 따라가게 한다.
		Mesh1P->SetVisibility(bVisible, true);
	}
}

void AWeaponBase::SetPresentationActive(const bool bActive)
{
	SetActorHiddenInGame(!bActive);

	SetFirstPersonPresentationVisible(bActive);
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

	if (IsValid(CharacterMesh1P) && IsValid(Mesh1P))
	{
		Mesh1P->AttachToComponent(
			CharacterMesh1P, FAttachmentTransformRules::SnapToTargetNotIncludingScale, GripSocketName1P);

		// 무기별 보정값
		Mesh1P->SetRelativeLocation(GripOffsetLocation1P);
		Mesh1P->SetRelativeRotation(GripOffsetRotation1P);
	}

    RefreshThirdPersonAttachment();
}

void AWeaponBase::RefreshThirdPersonAttachment() const
{
    AActor* OwnerActor = GetOwner();
    if (!IsValid(OwnerActor) || !OwnerActor->GetClass()->ImplementsInterface(UPlayerInterface::StaticClass())) return;
    USkeletalMeshComponent* CharacterMesh3P = IPlayerInterface::Execute_GetMesh3P(OwnerActor);
	if (IsValid(CharacterMesh3P) && IsValid(Mesh3P))
	{
        // A missing socket otherwise resolves to the mesh origin (the feet).
        // Hide this presentation until the correct mesh/socket is available.
        if (!CharacterMesh3P->DoesSocketExist(GripSocketName3P))
        {
            Mesh3P->SetVisibility(false, true);
            return;
        }
        Mesh3P->SetVisibility(!IsHidden(), true);
        bool bAttachedToAppearance = false;
        if (const auto* Appearance = OwnerActor->FindComponentByClass<UGameplayAppearanceComponent>())
        {
            USkeletalMeshComponent* Body = Appearance->GetReadyBody();
            FTransform Basis;
            const FName GripBone = CharacterMesh3P->GetSocketBoneName(GripSocketName3P);
            if (Body && CharacterMesh3P->GetSkeletalMeshAsset() &&
                CharacterMesh3P->DoesSocketExist(GripSocketName3P) &&
                AppearanceGrip::BuildGripBoneBasis(CharacterMesh3P->GetSkeletalMeshAsset()->GetRefSkeleton(),
                    Body->GetSkeletalMeshAsset()->GetRefSkeleton(), GripBone, Basis))
            {
                const FTransform Relative = FTransform(GripOffsetRotation3P, GripOffsetLocation3P) *
                    CharacterMesh3P->GetSocketTransform(GripSocketName3P, RTS_ParentBoneSpace) * Basis;
                Mesh3P->AttachToComponent(Body, FAttachmentTransformRules::SnapToTargetNotIncludingScale, GripBone);
                Mesh3P->SetRelativeLocationAndRotation(Relative.GetLocation(), Relative.GetRotation());
                bAttachedToAppearance = true;
            }
        }
        if (!bAttachedToAppearance)
        {
			Mesh3P->AttachToComponent(
				CharacterMesh3P, FAttachmentTransformRules::SnapToTargetNotIncludingScale, GripSocketName3P);

			// 무기별 보정값
			Mesh3P->SetRelativeLocation(GripOffsetLocation3P);
			Mesh3P->SetRelativeRotation(GripOffsetRotation3P);
        }

		// 새 무기도 팀/발각/폭풍 거리 조건을 적용한다. 비표시 드라이버 메시 값은 복사하지 않는다.
		if (AShooterCharacter* Character = Cast<AShooterCharacter>(OwnerActor))
		{
			Character->RefreshTeamOutline();
		}
		else
		{
			Mesh3P->SetCustomDepthStencilValue(CharacterMesh3P->CustomDepthStencilValue);
			Mesh3P->SetRenderCustomDepth(CharacterMesh3P->bRenderCustomDepth);
		}
	}
}

bool AWeaponBase::Fire(bool bHeavyAttack)
{
	return FireMelee(bHeavyAttack);
}

bool AWeaponBase::FireMelee(bool bHeavyAttack, float AnimationRecoverySeconds)
{
	// ClientFireServerTime을 0으로 넘기면 FLagCompensationRewindScope가 아예 생성되지 않아
	// (LagCompensationRewindScope.cpp의 <= 0.0f 가드) 부위별 정밀 히트박스가 계속 NoCollision
	// 상태로 남는다. 그러면 트레이스가 본 정보 없는 캡슐에 맞아 항상 폴백(Torso) 판정으로
	// 떨어지고, 머리를 노려도 헤드샷이 잡히지 않는다. 근접무기는 보정할 클라이언트 지연이
	// 없으므로 현재 서버 시간을 넘겨 "지금 시점"으로 리와인드시켜 정밀 히트박스를 활성화한다.
	const UWorld* World = GetWorld();
	const float CurrentServerTime = World ? World->GetTimeSeconds() : 0.0f;
	if (!FireInternal(bHeavyAttack, CurrentServerTime)) return false;
	// Shared by light/heavy attacks, including authority without an animation tick.
	if (FMath::IsFinite(AnimationRecoverySeconds))
	{
		LastFireInterval = FMath::Max(LastFireInterval, AnimationRecoverySeconds);
	}
	return true;
}

bool AWeaponBase::FireRanged(float ClientFireServerTime)
{
	return FireInternal(false, ClientFireServerTime);
}

bool AWeaponBase::FireInternal(bool bHeavyAttack, float ClientFireServerTime)
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

	const int32 FiredShotIndex = ConsumeAmmoForShot();
	const float ShotDamage = ResolveShotDamage(bHeavyAttack);
	const TArray<FVector> PelletDirections = GeneratePelletDirections();

	ResolveHitscanShot(TraceStart, PelletDirections, ShotDamage, FiredShotIndex, ClientFireServerTime, bHeavyAttack);
	if (Effects)
	{
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
	if (!bUsesAmmo)
	{
		return 0;
	}

	const int32 SafeMagazineSize = FMath::Max(EffectiveMagazineSize, 1);
	const int32 CurrentShotIndex = FMath::Clamp(ShotIndexInMagazine, 0, SafeMagazineSize - 1);

	if (!bInfiniteAmmo)
	{
		CurrentAmmo = FMath::Max(CurrentAmmo - 1, 0);
		OnAmmoChanged.Broadcast();
	}

	ShotIndexInMagazine = (CurrentShotIndex + 1) % SafeMagazineSize;
	return CurrentShotIndex;
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
		Aim->NotifyShotFired(LastFireInterval);
	}
}

void AWeaponBase::PlayFireEffects(
	const FVector& TraceStart,
	const TArray<FVector>& PelletDirections,
	const TArray<FHitscanImpactEffectData>& ImpactEffects)
{
	if (!HasAuthority() || PelletDirections.IsEmpty())
	{
		return;
	}

	TArray<FHitscanImpactEffectData> FireEffectData;
	FireEffectData.Reserve(PelletDirections.Num());

	for (int32 PelletIndex = 0; PelletIndex < PelletDirections.Num(); ++PelletIndex)
	{
		const FVector Direction = PelletDirections[PelletIndex].GetSafeNormal();

		FHitscanImpactEffectData EffectData;
		EffectData.ImpactPoint = TraceStart + Direction * EffectiveTraceRange;
		EffectData.ImpactNormal = -Direction;
		EffectData.ImpactSurfaceType = SurfaceType_Default;
		EffectData.PelletIndex = PelletIndex;

		for (const FHitscanImpactEffectData& ImpactEffect : ImpactEffects)
		{
			if (ImpactEffect.PelletIndex == PelletIndex)
			{
				EffectData = ImpactEffect;
				break;
			}
		}

		FireEffectData.Add(EffectData);
	}

	Multicast_PlayFireEffects(FireEffectData);
}

void AWeaponBase::Multicast_PlayFireEffects_Implementation(const TArray<FHitscanImpactEffectData>& ImpactEffects)
{
	// One shot may carry many shotgun pellet impacts; audio is not a per-impact effect.
	if (!ImpactEffects.IsEmpty())
	{
		if (Effects) Effects->PlayFireSound();
	}
	const APawn* OwningPawn = Cast<APawn>(GetOwner());
	const bool bIsFirstPerson = OwningPawn && OwningPawn->IsLocallyControlled();

	// The predicting owner and authority play rifle/knife animation through the ability.
	// Simulated observers receive fire effects, but do not execute that ability.
	// Sniper's gameplay-event relay is owner-local; remote proxies also need the
	// character action, not just muzzle/impact effects, to operate the bolt.
	const bool bKnifeAttack = GetWeaponTag() == RogueTags::Weapon_Type_Knife;
	const bool bSniperAttack = GetWeaponTag() == RogueTags::Weapon_Type_Sniper;
	if (OwningPawn && OwningPawn->GetLocalRole() == ROLE_SimulatedProxy
		&& (bKnifeAttack || bSniperAttack || GetWeaponTag() == RogueTags::Weapon_Type_Rifle))
	{
		const AShooterCharacter* Character = Cast<AShooterCharacter>(OwningPawn);
		UShooterAnimInstance* Anim = Character && Character->GetMesh()
			? Cast<UShooterAnimInstance>(Character->GetMesh()->GetAnimInstance()) : nullptr;
		const ADominationPlayerState* State = Character ? Character->GetPlayerState<ADominationPlayerState>() : nullptr;
		if (Anim && (bKnifeAttack || bSniperAttack || (State && State->GetCombatClass() == EDominationCombatClass::Trooper))
			&& Character->GetCombatComponent()
			&& Character->GetCombatComponent()->GetCurrentWeapon() == this)
		{
			Anim->PlayFireMontageForCurrentWeapon(true);
		}
	}

	// 표면별 임팩트 파티클(혈흔 등)은 FireEffects BP 그래프와 무관하게 여기서 직접 스폰한다 -
	// FireEffects는 BlueprintImplementableEvent라 C++에서 로직을 얹을 수 없다.
	UHitscanSurfaceDefinition* SurfaceDefinition = HitscanDefinition ? HitscanDefinition->GetSurfaceDefinition() : nullptr;

	for (const FHitscanImpactEffectData& ImpactEffect : ImpactEffects)
	{
		const bool bHeadshot = RogueTags::IsHeadZone(ImpactEffect.HitZoneTag);

		// 명중 관련 캐시/표면 임팩트 파티클은 실제 Hit일 때만 갱신
		if (ImpactEffect.bHasHit)
		{
			LastImpactPoint = ImpactEffect.ImpactPoint;
			LastImpactNormal = ImpactEffect.ImpactNormal;
			LastImpactSurfaceType = ImpactEffect.ImpactSurfaceType;
			LastImpactPelletIndex = ImpactEffect.PelletIndex;

			if (SurfaceDefinition && ImpactEffect.bSpawnImpactParticle)
			{
				// 혈흔 등 임팩트 파티클은 헤드샷 고정 / 그 외 랜덤 규칙으로 직접 스폰한다.
				// 임팩트 사운드(바디/헤드샷/사망)는 PlayerController가 소유한 기존 히트 피드백 시스템에서 처리한다.
				UParticleSystem* ImpactParticle = SurfaceDefinition->ResolveImpactParticle(ImpactEffect.ImpactSurfaceType, bHeadshot);
				if (ImpactParticle)
				{
					// ImpactNormal.Rotation()은 X축(전방)이 표면 노멀을 향하게 정렬할 뿐 Roll은 항상 0이라,
					// 방향성 있는 파티클(예: P_Blood_Splat_Cone)이 매번 같은 각도로만 튄다. 노멀 축(Roll)을
					// 랜덤으로 돌려 맞을 때마다 다른 방향으로 튀는 것처럼 보이게 한다.
					FRotator ImpactRotation = ImpactEffect.ImpactNormal.Rotation();
					ImpactRotation.Roll = FMath::FRandRange(0.f, 360.f);

					UGameplayStatics::SpawnEmitterAtLocation(
						this, ImpactParticle, ImpactEffect.ImpactPoint, ImpactRotation);
				}
			}
		}

		// FireEffects와 명중 노드는 bHasHit으로 분기. 혈흔 파티클은 위에서 이미 C++이 직접 처리하므로,
		// bHeadshot 핀은 그 외 부가 연출(카메라 셰이크 등)을 BP에서 분기하고 싶을 때만 참고하면 된다.
		FireEffects(
			ImpactEffect.ImpactPoint,
			ImpactEffect.ImpactNormal,
			ImpactEffect.ImpactSurfaceType.GetValue(),
			ImpactEffect.bHasHit && bUsesAmmo,
			bIsFirstPerson,
			bHeadshot);
	}
}

void AWeaponBase::SetInfiniteAmmo(const bool bNewInfiniteAmmo)
{
	if (!HasAuthority() || bInfiniteAmmo == bNewInfiniteAmmo)
	{
		return;
	}

	bInfiniteAmmo = bNewInfiniteAmmo;
	OnAmmoChanged.Broadcast();
	ForceNetUpdate();
}

bool AWeaponBase::CanReceiveReserveAmmo(const int32 Amount) const
{
	return HasAuthority() && bUsesAmmo && !bInfiniteAmmo && Amount > 0
		&& CurrentReserveAmmo >= 0 && CurrentReserveAmmo < MAX_int32;
}

int32 AWeaponBase::AddReserveAmmo(const int32 Amount)
{
	if (!CanReceiveReserveAmmo(Amount))
	{
		return 0;
	}

	const int64 NewReserveAmmo = FMath::Min<int64>(
		static_cast<int64>(CurrentReserveAmmo) + Amount, static_cast<int64>(MAX_int32));
	const int32 AppliedAmount = static_cast<int32>(NewReserveAmmo - CurrentReserveAmmo);
	CurrentReserveAmmo = static_cast<int32>(NewReserveAmmo);
	OnAmmoChanged.Broadcast();
	ForceNetUpdate();
	return AppliedAmount;
}

bool AWeaponBase::CanReload() const
{
	return bUsesAmmo && !bInfiniteAmmo && !bIsReloading
		&& CurrentAmmo < EffectiveMagazineSize && CurrentReserveAmmo > 0;
}

void AWeaponBase::StartReload()
{
	if (!CanReload())
	{
		return;
	}

	bIsReloading = true;
	bEmptyReloadArmed = false;
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

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ReloadTimerHandle, this, &AWeaponBase::FinishReload, EffectiveReloadDuration, false);
	}
}

void AWeaponBase::FinishReload()
{
	const int32 MissingAmmo = FMath::Max(EffectiveMagazineSize - CurrentAmmo, 0);
	const int32 AmountToReload = FMath::Clamp(CurrentReserveAmmo, 0, MissingAmmo);

	CurrentAmmo += AmountToReload;
	CurrentReserveAmmo -= AmountToReload;
	ShotIndexInMagazine = 0;
	if (CurrentAmmo > 0)
	{
		bEmptyClipNotified = false;
	}
	bEmptyReloadArmed = false;

	bIsReloading = false;
	OnAmmoChanged.Broadcast();
	if (HasAuthority())
	{
		ForceNetUpdate();
	}
}

int32 AWeaponBase::AdjustMagazineAmmoLocal(const int32 Delta)
{
	if (!bUsesAmmo || bInfiniteAmmo || Delta == 0)
	{
		return 0;
	}

	const int32 NewAmmo = FMath::Clamp(CurrentAmmo + Delta, 0, EffectiveMagazineSize);
	const int32 Applied = NewAmmo - CurrentAmmo;
	if (Applied != 0)
	{
		CurrentAmmo = NewAmmo;
		bEmptyClipNotified = false;
		OnAmmoChanged.Broadcast();
	}
	return Applied;
}

void AWeaponBase::OnRep_CurrentReserveAmmo()
{
	OnAmmoChanged.Broadcast();
}

void AWeaponBase::OnRep_InfiniteAmmo()
{
	OnAmmoChanged.Broadcast();
}

void AWeaponBase::CancelReload()
{
	if (!bIsReloading)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReloadTimerHandle);
	}

	bIsReloading = false;
	Multicast_StopCancelledReload();
	if (HasAuthority())
	{
		ForceNetUpdate();
	}
}

void AWeaponBase::Multicast_StopCancelledReload_Implementation()
{
	TArray<USkeletalMeshComponent*> Meshes; GetComponents(Meshes);
	AActor* OwnerActor = GetOwner();
	// An old weapon's delayed RPC must not stop the new weapon's reload.
	if (IsValid(OwnerActor) && OwnerActor->Implements<UPlayerInterface>()
		&& IPlayerInterface::Execute_GetCurrentWeapon(OwnerActor) == this)
	{
		Meshes.AddUnique(IPlayerInterface::Execute_GetMesh1P(OwnerActor));
		Meshes.AddUnique(IPlayerInterface::Execute_GetMesh3P(OwnerActor));
	}
	for (USkeletalMeshComponent* Mesh : Meshes)
	{
		UAnimInstance* Anim = IsValid(Mesh) ? Mesh->GetAnimInstance() : nullptr;
		if (!Anim) continue;
		TArray<UAnimMontage*> Montages;
		for (FAnimMontageInstance* Instance : Anim->MontageInstances)
			if (Instance && Instance->Montage && Instance->Montage->GetName().Contains(TEXT("Reload"))) Montages.AddUnique(Instance->Montage);
		for (UAnimMontage* Montage : Montages) Anim->Montage_Stop(0.05f, Montage);
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
	// GetCurrentSpreadDegrees()가 ADS/지상 중엔 이미 0을 반환하므로(RandomUnitVectorInConeInDegrees에
	// 0도를 넘기면 AimDirection 그대로 나온다), 여기서 ADS를 따로 조기 반환하지 않는다 - 예전엔
	// "ADS면 무조건 정중앙"으로 지름길을 탔는데, 그러면 공중에서 ADS 중에도 붙어야 하는
	// InAirSpreadPenaltyDegrees(Aim->SetIsAirborne)가 무시돼 버그가 됐다.
	//
	// 펠릿마다 독립적으로 호출되므로, 샷건처럼 PelletCountPerShot > 1인 무기는
	// 매 펠릿이 콘 안에서 각각 다른 방향으로 무작위 샘플링된다.
	// GetCurrentSpreadDegrees()를 거쳐야 스나이퍼 비ADS 강제 탄퍼짐(SniperUnscopedSpreadDegrees)이
	// 실제 탄 방향에도 반영된다 - Aim->GetCurrentSpreadDegrees()를 직접 부르면 크로스헤어 UI만
	// 커지고 실제 탄은 그대로 나가는 불일치가 생긴다.
	const float SpreadDegrees = GetCurrentSpreadDegrees();
	return UKismetMathLibrary::RandomUnitVectorInConeInDegrees(AimDirection, SpreadDegrees);
}

void AWeaponBase::ResolveHitscanShot(
	const FVector& TraceStart,
	const TArray<FVector>& PelletDirections,
	const float ShotDamage,
	const int32 InShotIndexInMagazine,
	const float ClientFireServerTime,
	const bool bHeavyAttack)
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	UHitscanResolverSubsystem* Resolver = World->GetSubsystem<UHitscanResolverSubsystem>();
	if (!IsValid(Resolver))
	{
		return;
	}

	if (!IsValid(HitscanDefinition.Get()))
	{
		return;
	}

	// Fire()의 기존 호출 경계를 유지하기 위한 레거시 인자다.
	// 신규 대미지는 WeaponDamageData와 ExecCalc_Damage에서 결정한다.
	(void)ShotDamage;

	FHitscanShotRequest Request;
	HitscanDefinition->ConfigureShotRequest(Request, GetPhysicalTraceRange(bHeavyAttack));

	// 좌/우 공격이 서로 다른 Weapon.Type.* 태그를 쓰는 무기(칼 등)는 ConfigureShotRequest가 채운
	// 기본 태그를 강공격 태그로 덮어써야 한다. AltWeaponTag가 비어있으면 GetWeaponTag가 기본 태그를
	// 그대로 반환하므로 대부분의 총기는 이 줄이 사실상 no-op이다.
	Request.WeaponTag = GetWeaponTag(bHeavyAttack);

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

	Request.PelletDirections = MakeArrayView(PelletDirections);
	Request.ShotId = FGuid::NewGuid();
	Request.ShotIndexInMagazine = FMath::Max(0, InShotIndexInMagazine);
	Request.ClientFireServerTime = ClientFireServerTime;

	TArray<FHitscanImpactEffectData> ImpactEffects;
	Resolver->ResolveServerShot(Request, ImpactEffects);
	PlayFireEffects(TraceStart, PelletDirections, ImpactEffects);
}
