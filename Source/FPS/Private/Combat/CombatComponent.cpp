// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/CombatComponent.h"

#include "Camera/CameraComponent.h"
#include "Character/ShooterMovementComponent.h"
#include "Data/WeaponData.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/PlayerInterface.h"
#include "Net/UnrealNetwork.h"
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
	DOREPLIFETIME(UCombatComponent, bAiming);
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
		}
	}
}

void UCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(WeaponSwitchTimerHandle);
	}

	ClearWeaponLoadout();
	Super::EndPlay(EndPlayReason);
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                     FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateMovementSpreadMultiplier();
	UpdateCameraZoom(DeltaTime);
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

	float Multiplier;
	if (Movement->IsFalling() || Movement->IsSprinting())
	{
		Multiplier = SpreadMultiplierAirOrSprint;
	}
	else if (Movement->Velocity.Size2D() <= StationarySpeedThreshold)
	{
		// 완전 정지든 방금 브레이킹(감속)으로 멈춘 것이든 동일하게 취급한다.
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
	const float TargetFieldOfView = (bAiming && IsValid(Weapon) && Weapon->CanADS())
		? Weapon->GetADSFieldOfView()
		: DefaultFieldOfView;

	if (FMath::IsNearlyEqual(Camera->FieldOfView, TargetFieldOfView, 0.01f))
	{
		return;
	}

	Camera->SetFieldOfView(FMath::FInterpTo(Camera->FieldOfView, TargetFieldOfView, DeltaTime, ADSZoomInterpSpeed));
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
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority() || bIsCycling)
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

	if (IsValid(CurrentWeapon.Get()) && CurrentWeapon->IsReloading())
	{
		return false;
	}

	AWeaponBase* PreviousWeapon = CurrentWeapon.Get();
	if (IsValid(PreviousWeapon))
	{
		PreviousWeapon->SetADS(false);
	}
	// GA_ADS가 이 스왑을 못 보고(예: 다른 입력으로 트리거된 스왑) 여전히 조준 애니메이션을 유지한 채로
	// 남지 않도록, 스왑 시점에 조준 상태를 즉시 해제한다. GA_ADS가 나중에 입력 해제로 정상 종료돼도
	// 자신이 캐싱해둔 이전 무기에 SetADS(false)를 한 번 더 호출할 뿐이라 문제 없다.
	SetAiming(false);

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
			if (IsValid(CurrentWeapon.Get()))
			{
				CurrentWeapon->SetADS(false);
			}
			SetAiming(false);

			ApplyCurrentWeapon(PreviousPrimary);
			BeginEquipCycle(PreviousPrimary);
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

	if (IsValid(CurrentWeapon.Get()) && CurrentWeapon.Get() != NextCurrent)
	{
		CurrentWeapon->SetADS(false);
		SetAiming(false);
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

void UCombatComponent::SetAiming(const bool bNewAiming)
{
	bAiming = bNewAiming;
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
	CurrentWeapon = NewWeapon;

	if (IsValid(CurrentWeapon.Get()) && IsValid(OwnerActor))
	{
		CurrentWeapon->SetOwner(OwnerActor);
	}

	RefreshWeaponPresentation();
}

void UCombatComponent::ClearWeaponLoadout()
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(OwnerActor) || !OwnerActor->HasAuthority())
	{
		return;
	}

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
	if (IsValid(LastWeapon) && LastWeapon != CurrentWeapon.Get())
	{
		LastWeapon->SetPresentationActive(false);
	}

	RefreshWeaponPresentation();
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
	if (IsValid(PrimaryWeapon.Get()))
	{
		PrimaryWeapon->SetPresentationActive(PrimaryWeapon.Get() == CurrentWeapon.Get());
	}
	if (IsValid(SecondaryWeapon.Get()))
	{
		SecondaryWeapon->SetPresentationActive(SecondaryWeapon.Get() == CurrentWeapon.Get());
	}
	if (IsValid(MeleeWeapon.Get()))
	{
		MeleeWeapon->SetPresentationActive(MeleeWeapon.Get() == CurrentWeapon.Get());
	}

	if (IsValid(CurrentWeapon.Get())
		&& CurrentWeapon.Get() != PrimaryWeapon.Get()
		&& CurrentWeapon.Get() != SecondaryWeapon.Get()
		&& CurrentWeapon.Get() != MeleeWeapon.Get())
	{
		CurrentWeapon->SetPresentationActive(true);
	}
}
