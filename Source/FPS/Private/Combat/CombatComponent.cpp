// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/CombatComponent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Interfaces/PlayerInterface.h"
#include "Net/UnrealNetwork.h"
#include "Weapon/WeaponBase.h"

// Sets default values for this component's properties
UCombatComponent::UCombatComponent()
{
	// 지금은 매 프레임 갱신할 로직이 없다 (이동 탄퍼짐 배율 소비 등이 추가되면 다시 켤 것).
	PrimaryComponentTick.bCanEverTick = false;

	// EquippedWeapon을 리플리케이트하려면 컴포넌트 자체가 리플리케이트 대상으로 등록되어 있어야 한다.
	SetIsReplicatedByDefault(true);
}


// Called when the game starts
void UCombatComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnequipWeapon();

	Super::EndPlay(EndPlayReason);
}

// Called every frame
void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                     FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCombatComponent, EquippedWeapon);
}

AWeaponBase* UCombatComponent::EquipWeapon(TSubclassOf<AWeaponBase> WeaponClass)
{
	if (!WeaponClass)
	{
		return nullptr;
	}

	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!OwnerActor || !World || !OwnerActor->HasAuthority())
	{
		return nullptr;
	}

	// 무기 교체 시작 - 애니메이션(템포)이 끝날 때까지 사격을 막고 싶은 GAS 쪽이 이 신호를 구독한다.
	bIsCycling = true;
	OnCyclingStateChanged.Broadcast(true);

	UnequipWeapon();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerActor;
	SpawnParams.Instigator = Cast<APawn>(OwnerActor);
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	EquippedWeapon = World->SpawnActor<AWeaponBase>(WeaponClass, OwnerActor->GetActorTransform(), SpawnParams);
	if (!EquippedWeapon)
	{
		return nullptr;
	}

	// 서버(리슨 서버면 호스트 자신 포함) 로컬 부착. 원격 클라이언트는 EquippedWeapon 리플리케이션이
	// 도착하면 OnRep_EquippedWeapon에서 동일하게 부착한다.
	AttachWeaponMeshesToOwner(EquippedWeapon);
	return EquippedWeapon;
}

void UCombatComponent::UnequipWeapon()
{
	AActor* OwnerActor = GetOwner();
	if (!EquippedWeapon || !OwnerActor || !OwnerActor->HasAuthority())
	{
		return;
	}

	EquippedWeapon->Destroy();
	EquippedWeapon = nullptr;
}

void UCombatComponent::FinishCycling()
{
	if (!bIsCycling)
	{
		return;
	}

	bIsCycling = false;
	OnCyclingStateChanged.Broadcast(false);
}

void UCombatComponent::OnRep_EquippedWeapon(AWeaponBase* PreviousWeapon)
{
	// 서버(권위) 자신은 EquipWeapon에서 이미 직접 부착했으므로 OnRep이 불리지 않는다 - 여기는
	// 원격 클라이언트 전용 경로다. EquippedWeapon이 null이면(무기 해제) 별도 처리가 필요 없다 -
	// 이전 무기 액터는 서버 Destroy()가 리플리케이트되어 알아서 사라진다.
	if (EquippedWeapon)
	{
		AttachWeaponMeshesToOwner(EquippedWeapon);
	}
}

void UCombatComponent::AttachWeaponMeshesToOwner(AWeaponBase* Weapon) const
{
	AActor* OwnerActor = GetOwner();
	if (!Weapon || !OwnerActor || !OwnerActor->GetClass()->ImplementsInterface(UPlayerInterface::StaticClass()))
	{
		return;
	}

	USkeletalMeshComponent* CharacterMesh1P = IPlayerInterface::Execute_GetMesh1P(OwnerActor);
	USkeletalMeshComponent* CharacterMesh3P = IPlayerInterface::Execute_GetMesh3P(OwnerActor);

	if (CharacterMesh1P && Weapon->GetMesh1P())
	{
		Weapon->GetMesh1P()->AttachToComponent(CharacterMesh1P,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale, Weapon->GetGripSocketName1P());
	}

	if (CharacterMesh3P && Weapon->GetMesh3P())
	{
		Weapon->GetMesh3P()->AttachToComponent(CharacterMesh3P,
			FAttachmentTransformRules::SnapToTargetNotIncludingScale, Weapon->GetGripSocketName3P());
	}
}

