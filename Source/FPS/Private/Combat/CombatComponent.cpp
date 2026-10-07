// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/CombatComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GAS/RogueAbilitySystemComponent.h"
#include "Customization/FirstPersonAppearanceComponent.h"

#include "Camera/CameraComponent.h"
#include "Character/ShooterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Data/WeaponData.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/PlayerInterface.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GameplayTags.h"
#include "TimerManager.h"
#include "Weapon/WeaponBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogCombatComponent, Log, All);

UCombatComponent::UCombatComponent()
{
	// UpdateMovementSpreadMultiplier/UpdateCameraZoom이 매 프레임 갱신되어야 해서 켜둔다.
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicatedByDefault(true);
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCombatComponent, CurrentWeapon);
	DOREPLIFETIME(UCombatComponent, PrimaryWeapon);
	DOREPLIFETIME(UCombatComponent, SecondaryWeapon);
	DOREPLIFETIME(UCombatComponent, MeleeWeapon);
	DOREPLIFETIME(UCombatComponent, bIsCycling);
	DOREPLIFETIME(UCombatComponent, bReplicatedAiming);
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* OwnerActor = GetOwner();
	if (IsValid(OwnerActor) && OwnerActor->GetClass()->ImplementsInterface(UPlayerInterface::StaticClass()))
	{
		if (const UCameraComponent* Camera = IPlayerInterface::Execute_GetViewCamera(OwnerActor))
		{
			DefaultFieldOfView = Camera->FieldOfView;
			bDefaultFieldOfViewInitialized = true;
		}
	}
}

void UCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 스왑 타이머 등 이 컴포넌트가 건 타이머를 한 번에 정리한다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}

	if (const AActor* OwnerActor = GetOwner(); IsValid(OwnerActor) && OwnerActor->HasAuthority())
	{
		ClearWeaponLoadout();
	}
	else
	{
		CancelAimingForWeapon(CurrentWeapon.Get());
		ResetAiming();
	}
	Super::EndPlay(EndPlayReason);
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                     FActorComponentTickFunction* ThisTickFunction)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UCombatComponent::TickComponent);

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateMovementSpreadMultiplier();
	UpdateCameraZoom(DeltaTime);
	BroadcastCrosshairSpreadInputsIfChanged();

	// 스왑 타이머가 완료 콜백(FinishCycling) 없이 사라지면 bIsCycling이 true로 굳어
	// IsWeaponReady()가 계속 false가 되고 사격/재장전이 영구히 막힌다 - 부활 직후
	// 기본 로드아웃 초기화와 클래스 로드아웃 적용이 겹칠 때 관측된다. 서버에서 방어적으로 해제한다.
	const AActor* OwnerActor = GetOwner();
	if (bIsCycling && IsValid(OwnerActor) && OwnerActor->HasAuthority())
	{
		const UWorld* World = GetWorld();
		if (IsValid(World) && !World->GetTimerManager().IsTimerActive(WeaponSwitchTimerHandle))
		{
			UE_LOG(LogCombatComponent, Warning,
				TEXT("[CombatComponent] %s: 스왑 타이머 없이 bIsCycling이 남아 있어 강제로 해제한다."),
				*GetNameSafe(OwnerActor));
			FinishCycling();
		}
	}
}

void UCombatComponent::UpdateMovementSpreadMultiplier() const
{
	AWeaponBase* Weapon = CurrentWeapon.Get();
	if (!IsValid(Weapon))
	{
		return;
	}

	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	const UShooterMovementComponent* Movement = OwnerCharacter
		? Cast<UShooterMovementComponent>(OwnerCharacter->GetCharacterMovement())
		: nullptr;
	if (!Movement)
	{
		return;
	}

	// ADS 중에도 공중에서는 탄퍼짐이 강제로 붙어야 해서(WeaponAimComponent::InAirSpreadPenaltyDegrees)
	// IsFalling() 결과를 별도로 무기에 전달한다 - Sprinting은 배율에는 같이 묶이지만 "공중 페널티"
	// 대상은 아니다(스프린트는 지상이라 ADS 자체가 대부분 불가능/취소되므로 별개로 취급).
	const bool bIsFalling = Movement->IsFalling();
	Weapon->SetIsAirborne(bIsFalling);

	// 이동 탄퍼짐을 무시하는 무기(예: 산탄총)는 항상 정지(브레이킹 성공) + 배율 1로 취급한다 -
	// MovingSpreadPenaltyDegrees/MovingADSSpreadDegrees/걷기·뛰기 배율이 전부 빠지고 공중 페널티만 남는다.
	if (Weapon->IgnoresMovementSpread())
	{
		Weapon->SetIsStationary(true);
		Weapon->SetMovementSpreadMultiplier(1.f);
		return;
	}

	// 이동 입력을 누르고 있는지와 무관하게, 실제 속도가 임계값 이하로 떨어지면 그 즉시 "정지
	// (브레이킹 성공)"로 취급한다 - 반대 방향 키로 급정지(카운터 스트레이프)해서 속도를 0으로
	// 만든 경우도 정지로 인정된다. 완전 정지든 감속으로 멈춘 것이든 동일하게 취급한다.
	const bool bIsStationary = !bIsFalling && !Movement->IsSprinting()
		&& Movement->Velocity.Size2D() <= StationarySpeedThreshold;
	// ADS 탄퍼짐이 0으로 고정되는 조건(브레이킹 성공)에 쓰인다 - WeaponAimComponent::
	// CalculateCurrentSpreadDegrees 참고.
	Weapon->SetIsStationary(bIsStationary);

	float Multiplier;
	if (bIsFalling || Movement->IsSprinting())
	{
		Multiplier = SpreadMultiplierAirOrSprint;
	}
	else if (bIsStationary)
	{
		Multiplier = SpreadMultiplierStationary;
	}
	else
	{
		Multiplier = SpreadMultiplierWalking;
	}

	Weapon->SetMovementSpreadMultiplier(Multiplier);
}

void UCombatComponent::UpdateCameraZoom(const float DeltaTime) const
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->GetClass()->ImplementsInterface(UPlayerInterface::StaticClass()))
	{
		return;
	}

	UCameraComponent* Camera = IPlayerInterface::Execute_GetViewCamera(OwnerActor);
	if (!IsValid(Camera))
	{
		return;
	}

	const AWeaponBase* Weapon = CurrentWeapon.Get();
	// 조준 중이고, 장착 무기가 ADS를 지원할 때만 줌 목표 FOV를 쓴다 - 그 외(무기 없음/ADS 불가/
	// 조준 해제)에는 평상시 FOV로 되돌아간다.
	const float TargetFieldOfView = IsAiming()
		? Weapon->GetADSFieldOfView()
		: DefaultFieldOfView;

	if (FMath::IsNearlyEqual(Camera->FieldOfView, TargetFieldOfView, 0.01f))
	{
		return;
	}

	float InterpSpeed = ADSZoomInterpSpeed;
	if (IsValid(Weapon))
	{
		const float WeaponInterpSpeed = Weapon->GetADSCameraInterpSpeedOverride();
		if (WeaponInterpSpeed > 0.f)
		{
			InterpSpeed = WeaponInterpSpeed;
		}
	}

	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, TargetFieldOfView, DeltaTime, InterpSpeed));
}

void UCombatComponent::BroadcastCrosshairSpreadInputsIfChanged()
{
	// 크로스헤어 위젯은 로컬 플레이어의 Pawn에만 붙으므로, 구독자가 없는 대다수(서버/시뮬레이티드 프록시)는
	// 비교 비용조차 쓰지 않는다.
	if (!OnCrosshairSpreadInputsChanged.IsBound())
	{
		return;
	}

	AWeaponBase* Weapon = CurrentWeapon.Get();
	const float SpreadDegrees = IsValid(Weapon) ? Weapon->GetCurrentSpreadDegrees() : 0.f;

	float FieldOfView = LastCrosshairFieldOfView;
	AActor* OwnerActor = GetOwner();
	if (IsValid(OwnerActor) && OwnerActor->GetClass()->ImplementsInterface(UPlayerInterface::StaticClass()))
	{
		if (const UCameraComponent* Camera = IPlayerInterface::Execute_GetViewCamera(OwnerActor))
		{
			FieldOfView = Camera->FieldOfView;
		}
	}

	if (LastCrosshairWeapon.Get() == Weapon
		&& FMath::IsNearlyEqual(LastCrosshairSpreadDegrees, SpreadDegrees, 0.001f)
		&& FMath::IsNearlyEqual(LastCrosshairFieldOfView, FieldOfView, 0.01f))
	{
		return;
	}

	LastCrosshairWeapon = Weapon;
	LastCrosshairSpreadDegrees = SpreadDegrees;
	LastCrosshairFieldOfView = FieldOfView;
	OnCrosshairSpreadInputsChanged.Broadcast();
}

void UCombatComponent::SetInfiniteAmmo(const bool bNewInfiniteAmmo)
{
	const AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	bInfiniteAmmo = bNewInfiniteAmmo;
	AWeaponBase* const Weapons[] =
	{
		PrimaryWeapon.Get(), SecondaryWeapon.Get(), MeleeWeapon.Get(), CurrentWeapon.Get()
	};
	for (AWeaponBase* Weapon : Weapons)
	{
		if (IsValid(Weapon))
		{
			Weapon->SetInfiniteAmmo(bInfiniteAmmo);
		}
	}
}

AWeaponBase* UCombatComponent::SpawnWeaponInstance(TSubclassOf<AWeaponBase> WeaponClass)
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!WeaponClass || !IsValid(OwnerActor) || !IsValid(World) || !OwnerActor->HasAuthority())
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerActor;
	SpawnParams.Instigator = Cast<APawn>(OwnerActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AWeaponBase* SpawnedWeapon = World->SpawnActor<AWeaponBase>(
		WeaponClass,
		OwnerActor->GetActorTransform(),
		SpawnParams);

	if (IsValid(SpawnedWeapon))
	{
		SpawnedWeapon->SetInfiniteAmmo(bInfiniteAmmo);
		SpawnedWeapon->SetActorHiddenInGame(true);
	}

	return SpawnedWeapon;
}

AWeaponBase* UCombatComponent::EquipWeapon(TSubclassOf<AWeaponBase> WeaponClass)
{
	AActor* OwnerActor = GetOwner();
	if (!WeaponClass || !IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return nullptr;
	}

	ClearWeaponLoadout();

	AWeaponBase* SpawnedWeapon = SpawnWeaponInstance(WeaponClass);
	if (!IsValid(SpawnedWeapon))
	{
		FinishCycling();
		return nullptr;
	}

	ApplyCurrentWeapon(SpawnedWeapon);
	BeginEquipCycle(CurrentWeapon.Get());
	return CurrentWeapon;
}

void UCombatComponent::UnequipWeapon()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !IsValid(CurrentWeapon.Get()))
	{
		return;
	}

	AWeaponBase* WeaponToDestroy = CurrentWeapon;
	if (PrimaryWeapon.Get() == WeaponToDestroy)
	{
		PrimaryWeapon = nullptr;
	}
	if (SecondaryWeapon.Get() == WeaponToDestroy)
	{
		SecondaryWeapon = nullptr;
	}
	if (MeleeWeapon.Get() == WeaponToDestroy)
	{
		MeleeWeapon = nullptr;
	}

	ApplyCurrentWeapon(nullptr);
	WeaponToDestroy->Destroy();
}

bool UCombatComponent::InitializeWeaponLoadout(
	TSubclassOf<AWeaponBase> PrimaryWeaponClass,
	TSubclassOf<AWeaponBase> SecondaryWeaponClass,
	TSubclassOf<AWeaponBase> MeleeWeaponClass)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !PrimaryWeaponClass)
	{
		return false;
	}

	ClearWeaponLoadout();

	PrimaryWeapon = SpawnWeaponInstance(PrimaryWeaponClass);
	if (!IsValid(PrimaryWeapon.Get()))
	{
		ClearWeaponLoadout();
		return false;
	}

	if (SecondaryWeaponClass)
	{
		SecondaryWeapon = SpawnWeaponInstance(SecondaryWeaponClass);
		if (!IsValid(SecondaryWeapon.Get()))
		{
			ClearWeaponLoadout();
			return false;
		}
	}

	if (MeleeWeaponClass)
	{
		MeleeWeapon = SpawnWeaponInstance(MeleeWeaponClass);
		if (!IsValid(MeleeWeapon.Get()))
		{
			ClearWeaponLoadout();
			return false;
		}
	}

	ApplyCurrentWeapon(PrimaryWeapon.Get());
	BeginEquipCycle(CurrentWeapon.Get());

	UE_LOG(
		LogCombatComponent,
		Log,
		TEXT("[CombatComponent] %s loadout initialized. Primary=%s Tag=%s Secondary=%s Tag=%s Melee=%s Tag=%s"),
		*GetNameSafe(OwnerActor),
		*GetNameSafe(PrimaryWeapon.Get()),
		*PrimaryWeapon->GetWeaponTag().ToString(),
		*GetNameSafe(SecondaryWeapon.Get()),
		IsValid(SecondaryWeapon.Get()) ? *SecondaryWeapon->GetWeaponTag().ToString() : TEXT("None"),
		*GetNameSafe(MeleeWeapon.Get()),
		IsValid(MeleeWeapon.Get()) ? *MeleeWeapon->GetWeaponTag().ToString() : TEXT("None"));

	return true;
}

bool UCombatComponent::SwitchWeaponSlot(const EWeaponSlot WeaponSlot)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return false;
	}

	AWeaponBase* TargetWeapon = GetWeaponInSlot(WeaponSlot);
	if (!IsValid(TargetWeapon))
	{
		return false;
	}

	if (TargetWeapon == CurrentWeapon.Get())
	{
		return true;
	}

	// 스왑 애니메이션 도중(bIsCycling)에도 다시 스왑할 수 있다 - BeginEquipCycle이 아래에서 기존
	// WeaponSwitchTimerHandle을 새로 세팅하기 전에 알아서 지우므로 별도 취소가 필요 없다.
	AWeaponBase* PreviousWeapon = CurrentWeapon.Get();
	if (IsValid(PreviousWeapon))
	{
		// 재장전 중이어도 스왑을 막지 않는다 - 대신 재장전은 즉시 취소한다(탄약 미충전). 재장전은
		// 모션이 끝까지 재생돼야만 완료되므로, 스왑으로 중단됐다면 무기로 돌아왔을 때 다시 걸어야 한다.
		if (PreviousWeapon->IsReloading())
		{
			PreviousWeapon->CancelReload();
		}
	}
	ApplyCurrentWeapon(TargetWeapon);
	BeginEquipCycle(CurrentWeapon.Get());

	UE_LOG(
		LogCombatComponent,
		Log,
		TEXT("[CombatComponent] %s weapon swap. Slot=%s Previous=%s Current=%s WeaponTag=%s PrimaryAmmo=%d SecondaryAmmo=%d"),
		*GetNameSafe(OwnerActor),
		*UEnum::GetDisplayValueAsText(WeaponSlot).ToString(),
		*GetNameSafe(PreviousWeapon),
		*GetNameSafe(CurrentWeapon.Get()),
		*GetCurrentWeaponTag().ToString(),
		IsValid(PrimaryWeapon.Get()) ? PrimaryWeapon->GetCurrentAmmo() : INDEX_NONE,
		IsValid(SecondaryWeapon.Get()) ? SecondaryWeapon->GetCurrentAmmo() : INDEX_NONE);

	return true;
}

bool UCombatComponent::ReplacePrimaryWeapon(const TSubclassOf<AWeaponBase> PrimaryWeaponClass)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return false;
	}

	AWeaponBase* PreviousPrimary = PrimaryWeapon.Get();

	// 제거할 주무기가 없는 정상 복구 경로다.
	if (!PrimaryWeaponClass && !IsValid(PreviousPrimary))
	{
		return true;
	}

	// 같은 클래스 재선택은 재생성하지 않고 탄약과 런타임 상태를 유지한다.
	if (IsValid(PreviousPrimary) && PreviousPrimary->GetClass() == PrimaryWeaponClass)
	{
		if (CurrentWeapon.Get() != PreviousPrimary)
		{
			ApplyCurrentWeapon(PreviousPrimary);
			BeginEquipCycle(PreviousPrimary);
		}
		else
		{
			// 부활 시 BeginPlay의 기본 로드아웃이 이미 같은 클래스를 장착해 둔 경로 -
			// 아무것도 바뀌지 않지만, 빙의/복제 타이밍상 부착·표시가 누락됐을 수 있어 한 번 더 확정한다.
			RefreshWeaponPresentation();
		}

		return true;
	}

	// 새 무기를 먼저 생성해 실패 시 기존 주무기와 현재 장착 상태를 유지한다.
	AWeaponBase* NewPrimary = nullptr;
	if (PrimaryWeaponClass)
	{
		NewPrimary = SpawnWeaponInstance(PrimaryWeaponClass);
		if (!IsValid(NewPrimary))
		{
			return false;
		}
	}

	PrimaryWeapon = NewPrimary;

	// 구매한 주무기는 즉시 장착하고, 주무기를 비우면 권총과 근접무기 순서로 전환한다.
	AWeaponBase* NextCurrent = NewPrimary;
	if (!IsValid(NextCurrent))
	{
		NextCurrent = SecondaryWeapon.Get();
	}
	if (!IsValid(NextCurrent))
	{
		NextCurrent = MeleeWeapon.Get();
	}

	ApplyCurrentWeapon(NextCurrent);

	if (IsValid(NextCurrent))
	{
		BeginEquipCycle(NextCurrent);
	}

	if (IsValid(PreviousPrimary))
	{
		PreviousPrimary->Destroy();
	}

	return true;
}

AWeaponBase* UCombatComponent::GetWeaponInSlot(const EWeaponSlot WeaponSlot) const
{
	switch (WeaponSlot)
	{
	case EWeaponSlot::Primary:
		return PrimaryWeapon.Get();
	case EWeaponSlot::Secondary:
		return SecondaryWeapon.Get();
	case EWeaponSlot::Melee:
		return MeleeWeapon.Get();
	default:
		return nullptr;
	}
}

EWeaponSlot UCombatComponent::GetCurrentWeaponSlot() const
{
	if (CurrentWeapon.Get() == PrimaryWeapon.Get())
	{
		return EWeaponSlot::Primary;
	}
	if (CurrentWeapon.Get() == SecondaryWeapon.Get())
	{
		return EWeaponSlot::Secondary;
	}
	if (CurrentWeapon.Get() == MeleeWeapon.Get())
	{
		return EWeaponSlot::Melee;
	}
	return EWeaponSlot::None;
}

FGameplayTag UCombatComponent::GetCurrentWeaponTag() const
{
	return IsValid(CurrentWeapon.Get()) ? CurrentWeapon->GetWeaponTag() : FGameplayTag();
}

bool UCombatComponent::IsWeaponReady() const
{
	return IsValid(CurrentWeapon.Get()) && !bIsCycling;
}

bool UCombatComponent::IsAiming() const
{
	return bAiming && IsValid(CurrentWeapon.Get()) && CurrentWeapon->CanADS();
}

void UCombatComponent::SetAiming(const bool bNewAiming, AWeaponBase* ExpectedWeapon)
{
	if (ExpectedWeapon && ExpectedWeapon != CurrentWeapon.Get())
	{
		if (!bNewAiming && IsValid(ExpectedWeapon))
		{
			ExpectedWeapon->SetADS(false);
		}
		return;
	}

	bAiming = bNewAiming && IsValid(CurrentWeapon.Get()) && CurrentWeapon->CanADS();
	if (const AActor* OwnerActor = GetOwner(); IsValid(OwnerActor) && OwnerActor->HasAuthority())
	{
		bReplicatedAiming = bAiming;
	}
	ApplyAimingPresentation();
}

bool UCombatComponent::UsesLocalAimingPrediction() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	return IsValid(OwnerPawn) && !OwnerPawn->HasAuthority()
		&& (OwnerPawn->GetLocalRole() == ROLE_AutonomousProxy || OwnerPawn->IsLocallyControlled());
}

void UCombatComponent::ApplyAimingPresentation()
{
	const bool bAppliedAiming = IsAiming();
	if (AWeaponBase* Previous = AppliedAimingWeapon.Get(); IsValid(Previous) && Previous != CurrentWeapon.Get())
	{
		Previous->SetADS(false);
	}
	if (IsValid(CurrentWeapon.Get()) && CurrentWeapon->IsADS() != bAppliedAiming)
	{
		CurrentWeapon->SetADS(bAppliedAiming);
	}
	AppliedAimingWeapon = bAppliedAiming ? CurrentWeapon.Get() : nullptr;
	RefreshFirstPersonADSPresentation();
	RefreshMainHUDSuppressionState();
	if (bLastBroadcastAiming != bAppliedAiming)
	{
		bLastBroadcastAiming = bAppliedAiming;
		OnAimingStateChanged.Broadcast(bAppliedAiming);
	}
}

void UCombatComponent::CancelAimingForWeapon(AWeaponBase* Weapon)
{
	if (URogueAbilitySystemComponent* ASC = Cast<URogueAbilitySystemComponent>(
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner())))
	{
		ASC->CancelADSAbilityForWeapon(Weapon);
	}
}

void UCombatComponent::ResetAiming()
{
	SetAiming(false);
	AActor* OwnerActor = GetOwner();
	if (bDefaultFieldOfViewInitialized && IsValid(OwnerActor) && OwnerActor->Implements<UPlayerInterface>())
	{
		if (UCameraComponent* Camera = IPlayerInterface::Execute_GetViewCamera(OwnerActor); IsValid(Camera))
		{
			Camera->SetFieldOfView(DefaultFieldOfView);
		}
	}
}

bool UCombatComponent::ShouldSuppressMainHUDForADS() const
{
	const AWeaponBase* Weapon = CurrentWeapon.Get();
	if (!IsAiming() || !IsValid(Weapon))
	{
		return false;
	}

	// 스나이퍼 ADS는 스코프/기본 크로스헤어/1인칭 표현만 별도로 제어하고,
	// 체력/탄약/게임 진행 HUD 같은 메인 HUD는 계속 보여준다. 무기 에셋에
	// bHideMainHUDWhileADS가 켜져 있더라도 스나이퍼만큼은 여기서 최종적으로
	// 제외해 두면, 기존 무기별 표현 정책 구조를 유지하면서도 스코프 중 HUD
	// 전체가 사라지는 문제를 한 곳에서 막을 수 있다.
	if (Weapon->GetWeaponTag() == RogueTags::Weapon_Type_Sniper)
	{
		return false;
	}

	return Weapon->ShouldHideMainHUDWhileADS();
}

void UCombatComponent::SetPresentationSuppressed(const bool bSuppressed)
{
	if (bPresentationSuppressed == bSuppressed)
	{
		return;
	}

	bPresentationSuppressed = bSuppressed;
	RefreshWeaponPresentation();
}

void UCombatComponent::ReEquipCurrentWeapon()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !IsValid(CurrentWeapon.Get()))
	{
		return;
	}

	BeginEquipCycle(CurrentWeapon.Get());
}

void UCombatComponent::RefreshFirstPersonADSPresentation() const
{
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn) || !OwnerPawn->IsLocallyControlled()
		|| !OwnerPawn->GetClass()->ImplementsInterface(UPlayerInterface::StaticClass()))
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh1P = IPlayerInterface::Execute_GetMesh1P(OwnerPawn);
	if (!IsValid(CharacterMesh1P))
	{
		return;
	}

	AWeaponBase* Weapon = CurrentWeapon.Get();
	const bool bHideFirstPerson = IsAiming()
		&& IsValid(Weapon)
		&& Weapon->ShouldHideFirstPersonPresentationWhileADS();

	// 캐릭터 팔은 자기 자신만 토글한다. 자식까지 전파하면 과거에 Mesh1P에 부착된 채
	// 비활성화된 다른 무기까지 ADS 해제 시 다시 보이게 만들 수 있다.
	// 사망 처리의 HiddenInGame 상태도 건드리지 않는다.
	CharacterMesh1P->SetVisibility(!bHideFirstPerson, false);
    if (auto* Appearance = OwnerPawn->FindComponentByClass<UFirstPersonAppearanceComponent>())
        Appearance->SetPresentationVisible(!bHideFirstPerson);

	if (IsValid(Weapon))
	{
		// 현재 무기 안에서는 Mesh1P가 1P 표현 루트다. BP에서 Mesh1P 소켓 아래에
		// 추가한 스코프/부착물까지 현재 무기와 함께 숨기도록 WeaponBase에 위임한다.
		Weapon->SetFirstPersonPresentationVisible(!bHideFirstPerson);
	}
}

void UCombatComponent::RefreshMainHUDSuppressionState()
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!IsValid(OwnerPawn) || !OwnerPawn->IsLocallyControlled())
	{
		return;
	}

	const bool bShouldSuppress = ShouldSuppressMainHUDForADS();
	if (bMainHUDSuppressedForADS == bShouldSuppress)
	{
		return;
	}

	bMainHUDSuppressedForADS = bShouldSuppress;
	OnMainHUDSuppressionChanged.Broadcast(bMainHUDSuppressedForADS);
}

void UCombatComponent::SetCurrentWeapon(AWeaponBase* NewWeapon)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	ApplyCurrentWeapon(NewWeapon);
}

void UCombatComponent::ApplyCurrentWeapon(AWeaponBase* NewWeapon)
{
	AActor* OwnerActor = GetOwner();
	if (CurrentWeapon.Get() != NewWeapon)
	{
		CancelAimingForWeapon(CurrentWeapon.Get());
		SetAiming(false);
	}
	CurrentWeapon = NewWeapon;

	if (IsValid(CurrentWeapon.Get()) && IsValid(OwnerActor))
	{
		CurrentWeapon->SetOwner(OwnerActor);
		CurrentWeapon->SetInfiniteAmmo(bInfiniteAmmo);
	}

	RefreshWeaponPresentation();
	ApplyAimingPresentation();
	OnCurrentWeaponChanged.Broadcast(CurrentWeapon.Get());
}

void UCombatComponent::ClearWeaponLoadout()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	CancelAimingForWeapon(CurrentWeapon.Get());
	ResetAiming();

	AWeaponBase* CurrentToDestroy = CurrentWeapon.Get();
	AWeaponBase* PrimaryToDestroy = PrimaryWeapon.Get();
	AWeaponBase* SecondaryToDestroy = SecondaryWeapon.Get();
	AWeaponBase* MeleeToDestroy = MeleeWeapon.Get();

	CurrentWeapon = nullptr;
	PrimaryWeapon = nullptr;
	SecondaryWeapon = nullptr;
	MeleeWeapon = nullptr;

	if (IsValid(PrimaryToDestroy))
	{
		PrimaryToDestroy->Destroy();
	}
	if (IsValid(SecondaryToDestroy) && SecondaryToDestroy != PrimaryToDestroy)
	{
		SecondaryToDestroy->Destroy();
	}
	if (IsValid(MeleeToDestroy) && MeleeToDestroy != PrimaryToDestroy && MeleeToDestroy != SecondaryToDestroy)
	{
		MeleeToDestroy->Destroy();
	}
	if (IsValid(CurrentToDestroy)
		&& CurrentToDestroy != PrimaryToDestroy
		&& CurrentToDestroy != SecondaryToDestroy
		&& CurrentToDestroy != MeleeToDestroy)
	{
		CurrentToDestroy->Destroy();
	}
}

void UCombatComponent::BeginEquipCycle(AWeaponBase* Weapon)
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || !IsValid(Weapon))
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WeaponSwitchTimerHandle);
	}

	SetCyclingState(true);
	Weapon->PlayEquipSound();

	const float EquipDuration = ResolveEquipDuration(Weapon);
	UE_LOG(
		LogCombatComponent,
		Log,
		TEXT("[CombatComponent] %s equip started. Weapon=%s WeaponTag=%s Duration=%.3fs"),
		*GetNameSafe(OwnerActor),
		*GetNameSafe(Weapon),
		*Weapon->GetWeaponTag().ToString(),
		EquipDuration);
	if (EquipDuration <= 0.f)
	{
		FinishCycling();
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			WeaponSwitchTimerHandle,
			this,
			&ThisClass::FinishCycling,
			EquipDuration,
			false);
	}
}

float UCombatComponent::ResolveEquipDuration(AWeaponBase* Weapon) const
{
	if (IsValid(WeaponData) && IsValid(Weapon))
	{
		const float MontageDuration = WeaponData->GetEquipDuration(Weapon->GetWeaponTag());
		if (MontageDuration > 0.f)
		{
			return MontageDuration;
		}
	}

	return FMath::Max(0.f, WeaponSwitchDuration);
}

void UCombatComponent::FinishCycling()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WeaponSwitchTimerHandle);
	}

	if (!bIsCycling)
	{
		return;
	}

	SetCyclingState(false);
	UE_LOG(
		LogCombatComponent,
		Log,
		TEXT("[CombatComponent] %s weapon ready. Weapon=%s WeaponTag=%s"),
		*GetNameSafe(OwnerActor),
		*GetNameSafe(CurrentWeapon.Get()),
		*GetCurrentWeaponTag().ToString());
}

void UCombatComponent::SetCyclingState(const bool bNewIsCycling)
{
	if (bIsCycling == bNewIsCycling)
	{
		return;
	}

	bIsCycling = bNewIsCycling;
	OnCyclingStateChanged.Broadcast(bIsCycling);
}

void UCombatComponent::OnRep_CurrentWeapon(AWeaponBase* LastWeapon)
{
	if (LastWeapon && LastWeapon != CurrentWeapon.Get())
	{
		// 이전 무기의 알림은 새 무기에서 시작한 ADS를 취소하지 않는다.
		CancelAimingForWeapon(LastWeapon);
		if (IsValid(LastWeapon))
		{
			LastWeapon->SetADS(false);
			LastWeapon->SetPresentationActive(false);
		}
	}

	if (UsesLocalAimingPrediction())
	{
		if (AppliedAimingWeapon.Get() != CurrentWeapon.Get())
		{
			SetAiming(false);
		}
	}
	else
	{
		bAiming = bReplicatedAiming;
	}
	RefreshWeaponPresentation();
	ApplyAimingPresentation();
	OnCurrentWeaponChanged.Broadcast(CurrentWeapon.Get());
}

void UCombatComponent::OnRep_Aiming()
{
	// 소유 클라이언트의 해제 예측은 GAS 종료/취소로 확정한다. 늦은 true는 적용하지 않는다.
	if (!UsesLocalAimingPrediction())
	{
		bAiming = bReplicatedAiming;
	}
	ApplyAimingPresentation();
}

void UCombatComponent::OnRep_WeaponSlots()
{
	RefreshWeaponPresentation();
}

void UCombatComponent::OnRep_IsCycling()
{
	OnCyclingStateChanged.Broadcast(bIsCycling);
}

void UCombatComponent::RefreshWeaponPresentation()
{
	const bool bAllowPresentation = !bPresentationSuppressed;

	if (IsValid(PrimaryWeapon.Get()))
	{
		PrimaryWeapon->SetPresentationActive(bAllowPresentation && PrimaryWeapon.Get() == CurrentWeapon.Get());
	}
	if (IsValid(SecondaryWeapon.Get()))
	{
		SecondaryWeapon->SetPresentationActive(bAllowPresentation && SecondaryWeapon.Get() == CurrentWeapon.Get());
	}
	if (IsValid(MeleeWeapon.Get()))
	{
		MeleeWeapon->SetPresentationActive(bAllowPresentation && MeleeWeapon.Get() == CurrentWeapon.Get());
	}

	if (IsValid(CurrentWeapon.Get())
		&& CurrentWeapon.Get() != PrimaryWeapon.Get()
		&& CurrentWeapon.Get() != SecondaryWeapon.Get()
		&& CurrentWeapon.Get() != MeleeWeapon.Get())
	{
		CurrentWeapon->SetPresentationActive(bAllowPresentation);
	}

	// SetPresentationActive가 현재 무기의 Mesh1P Visibility를 다시 켤 수 있으므로,
	// 장착/복제 갱신이 끝난 뒤 ADS 전용 1인칭 표시 규칙을 마지막에 다시 적용한다.
	RefreshFirstPersonADSPresentation();
}
