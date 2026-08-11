// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/CombatComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Data/WeaponData.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/PlayerInterface.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Weapon/WeaponBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogCombatComponent, Log, All);

UCombatComponent::UCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCombatComponent, CurrentWeapon);
	DOREPLIFETIME(UCombatComponent, PrimaryWeapon);
	DOREPLIFETIME(UCombatComponent, SecondaryWeapon);
	DOREPLIFETIME(UCombatComponent, bIsCycling);
	DOREPLIFETIME(UCombatComponent, bAiming);
}

void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();
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

	ApplyCurrentWeapon(nullptr);
	WeaponToDestroy->Destroy();
}

bool UCombatComponent::InitializeWeaponLoadout(
	TSubclassOf<AWeaponBase> PrimaryWeaponClass,
	TSubclassOf<AWeaponBase> SecondaryWeaponClass)
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

	ApplyCurrentWeapon(PrimaryWeapon.Get());
	BeginEquipCycle(CurrentWeapon.Get());

	UE_LOG(
		LogCombatComponent,
		Log,
		TEXT("[CombatComponent] %s loadout initialized. Primary=%s Tag=%s Secondary=%s Tag=%s"),
		*GetNameSafe(OwnerActor),
		*GetNameSafe(PrimaryWeapon.Get()),
		*PrimaryWeapon->GetWeaponTag().ToString(),
		*GetNameSafe(SecondaryWeapon.Get()),
		IsValid(SecondaryWeapon.Get()) ? *SecondaryWeapon->GetWeaponTag().ToString() : TEXT("None"));

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

	ApplyCurrentWeapon(TargetWeapon);
	BeginEquipCycle(CurrentWeapon.Get());

	UE_LOG(
		LogCombatComponent,
		Log,
		TEXT("[CombatComponent] %s weapon swap. Slot=%s Previous=%s Current=%s WeaponTag=%s PrimaryAmmo=%d SecondaryAmmo=%d"),
		*GetNameSafe(OwnerActor),
		WeaponSlot == EWeaponSlot::Primary ? TEXT("Primary") : TEXT("Secondary"),
		*GetNameSafe(PreviousWeapon),
		*GetNameSafe(CurrentWeapon.Get()),
		*GetCurrentWeaponTag().ToString(),
		IsValid(PrimaryWeapon.Get()) ? PrimaryWeapon->GetCurrentAmmo() : INDEX_NONE,
		IsValid(SecondaryWeapon.Get()) ? SecondaryWeapon->GetCurrentAmmo() : INDEX_NONE);

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

	CurrentWeapon = nullptr;
	PrimaryWeapon = nullptr;
	SecondaryWeapon = nullptr;

	if (IsValid(PrimaryToDestroy))
	{
		PrimaryToDestroy->Destroy();
	}
	if (IsValid(SecondaryToDestroy) && SecondaryToDestroy != PrimaryToDestroy)
	{
		SecondaryToDestroy->Destroy();
	}
	if (IsValid(CurrentToDestroy) && CurrentToDestroy != PrimaryToDestroy && CurrentToDestroy != SecondaryToDestroy)
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
		SetWeaponPresentationActive(LastWeapon, false);
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
	SetWeaponPresentationActive(PrimaryWeapon.Get(), PrimaryWeapon.Get() == CurrentWeapon.Get());
	SetWeaponPresentationActive(SecondaryWeapon.Get(), SecondaryWeapon.Get() == CurrentWeapon.Get());

	if (IsValid(CurrentWeapon.Get()) && CurrentWeapon.Get() != PrimaryWeapon.Get() && CurrentWeapon.Get() != SecondaryWeapon.Get())
	{
		SetWeaponPresentationActive(CurrentWeapon.Get(), true);
	}
}

void UCombatComponent::SetWeaponPresentationActive(AWeaponBase* Weapon, const bool bActive) const
{
	if (!IsValid(Weapon))
	{
		return;
	}

	Weapon->SetActorHiddenInGame(!bActive);

	if (USkeletalMeshComponent* WeaponMesh1P = Weapon->GetMesh1P())
	{
		WeaponMesh1P->SetVisibility(bActive, true);
	}
	if (USkeletalMeshComponent* WeaponMesh3P = Weapon->GetMesh3P())
	{
		WeaponMesh3P->SetVisibility(bActive, true);
	}

	if (bActive)
	{
		AttachWeaponMeshesToOwner(Weapon);
	}
}

void UCombatComponent::AttachWeaponMeshesToOwner(AWeaponBase* Weapon) const
{
	AActor* OwnerActor = GetOwner();
	if (!IsValid(Weapon) || !IsValid(OwnerActor) || !OwnerActor->GetClass()->ImplementsInterface(UPlayerInterface::StaticClass()))
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh1P = IPlayerInterface::Execute_GetMesh1P(OwnerActor);
	USkeletalMeshComponent* CharacterMesh3P = IPlayerInterface::Execute_GetMesh3P(OwnerActor);

	if (IsValid(CharacterMesh1P) && IsValid(Weapon->GetMesh1P()))
	{
		Weapon->GetMesh1P()->AttachToComponent(
			CharacterMesh1P, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Weapon->GetGripSocketName1P());
	}

	if (IsValid(CharacterMesh3P) && IsValid(Weapon->GetMesh3P()))
	{
		Weapon->GetMesh3P()->AttachToComponent(
			CharacterMesh3P, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Weapon->GetGripSocketName3P());
	}
}
