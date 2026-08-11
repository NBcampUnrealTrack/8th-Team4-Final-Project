// Fill out your copyright notice in the Description page of Project Settings.

#include "Abilities/GA_WeaponAbilityBase.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "Combat/CombatComponent.h"
#include "GameplayTagContainer.h"
#include "GameFlow/Round/BaseRoundPlayerController.h"
#include "GameFramework/Pawn.h"
#include "Weapon/WeaponBase.h"

UGA_WeaponAbilityBase::UGA_WeaponAbilityBase()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
}

bool UGA_WeaponAbilityBase::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	const APawn* AvatarPawn = ActorInfo ? Cast<APawn>(ActorInfo->AvatarActor.Get()) : nullptr;
	const ABaseRoundPlayerController* RoundPlayerController = IsValid(AvatarPawn)
		? Cast<ABaseRoundPlayerController>(AvatarPawn->GetController())
		: nullptr;

	if (IsValid(RoundPlayerController) && !RoundPlayerController->CanUseCombatInput())
	{
		return false;
	}

	const UCombatComponent* CombatComponent = IsValid(AvatarPawn)
		? AvatarPawn->FindComponentByClass<UCombatComponent>()
		: nullptr;

	return !IsValid(CombatComponent) || CombatComponent->IsWeaponReady();
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

void UGA_WeaponAbilityBase::SendWeaponGameplayEvent(FGameplayTag EventTag) const
{
	if (!EventTag.IsValid())
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = EventTag;
	Payload.Instigator = GetAvatarActorFromActorInfo();

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetAvatarActorFromActorInfo(), EventTag, Payload);
}