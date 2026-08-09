

#include "Weapon/WeaponBase.h"

#include "Tags/GameplayTags.h"


AWeaponBase::AWeaponBase()
{
	PrimaryActorTick.bCanEverTick = true;
	WeaponType = ShooterTags::TAG_WeaponType_None;

}

void AWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	
}

void AWeaponBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

