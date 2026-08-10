// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/GA_WeaponAbilityBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Combat/CombatComponent.h"
#include "GameplayTagContainer.h"
#include "Weapon/WeaponBase.h"

UGA_WeaponAbilityBase::UGA_WeaponAbilityBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

AWeaponBase* UGA_WeaponAbilityBase::GetEquippedWeapon() const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		return nullptr;
	}

	const UCombatComponent* CombatComponent = AvatarActor->FindComponentByClass<UCombatComponent>();
	return CombatComponent ? CombatComponent->GetEquippedWeapon() : nullptr;
}

void UGA_WeaponAbilityBase::SendWeaponGameplayEvent(const TCHAR* EventTagString) const
{
	const FGameplayTag EventTag = FGameplayTag::RequestGameplayTag(FName(EventTagString));

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = GetAvatarActorFromActorInfo();

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetAvatarActorFromActorInfo(), EventTag, Payload);
}