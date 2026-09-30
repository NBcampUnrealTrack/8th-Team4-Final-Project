// Fill out your copyright notice in the Description page of Project Settings.

#include "Augment/Abilities/RogueAddAmmoAbility.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Augment/AugmentDefinition.h"
#include "Combat/CombatComponent.h"
#include "Engine/World.h"
#include "Interfaces/PlayerInterface.h"
#include "Tags/GameplayTags.h"
#include "TimerManager.h"
#include "Weapon/WeaponBase.h"

URogueAddAmmoAbility::URogueAddAmmoAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalPredicted;
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void URogueAddAmmoAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                           const FGameplayAbilityActorInfo* ActorInfo,
                                           const FGameplayAbilityActivationInfo ActivationInfo,
                                           const FGameplayEventData* TriggerEventData)
{
	AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const FAugmentGrantedAbility* Entry = FindGrantedEntry(Handle, ActorInfo);
	if (!Avatar || !Entry)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	if (!Entry->RequiredWeaponTags.IsEmpty()
		&& !ResolveEventWeaponTag(Avatar, TriggerEventData).MatchesAny(Entry->RequiredWeaponTags))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 한 번만
	if (RepeatDuration <= 0.f)
	{
		const int32 Added = AddAmmo();
		FPredictionKey PredictionKey = ActivationInfo.GetActivationPredictionKey();
		if (Added != 0 && !HasAuthority(&ActivationInfo) && PredictionKey.IsValidKey())
		{
			PredictionKey.NewRejectedDelegate().BindUObject(this, &ThisClass::HandleAddRejected, Added);
		}
		EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
		return;
	}

	// 반복. 서버와 소유 클라가 각자 타이머
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(AddAmmoTimer, FTimerDelegate::CreateWeakLambda(this, [this]() { AddAmmo(); }),
			RepeatInterval, true, CastDelaySeconds > 0.f ? CastDelaySeconds : RepeatInterval);
	}

	if (UAbilityTask_WaitDelay* Task = UAbilityTask_WaitDelay::WaitDelay(this, RepeatDuration))
	{
		Task->OnFinish.AddDynamic(this, &ThisClass::HandleRepeatFinished);
		Task->ReadyForActivation();
	}
}

void URogueAddAmmoAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	// 부모의 서버 발동은 건너뜀
	UGameplayAbility::OnAvatarSet(ActorInfo, Spec);

	if (!bActivateOnAvatarSet || !ActorInfo || !ActorInfo->IsLocallyControlled() || Spec.IsActive()
		|| !IsValid(ActorInfo->AvatarActor.Get()) || !ActorInfo->AvatarActor->Implements<UPlayerInterface>())
	{
		return;
	}

	if (UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get())
	{
		ASC->TryActivateAbility(Spec.Handle);
	}
}

void URogueAddAmmoAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
                                      const FGameplayAbilityActorInfo* ActorInfo,
                                      const FGameplayAbilityActivationInfo ActivationInfo,
                                      bool bReplicateEndAbility, bool bWasCancelled)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AddAmmoTimer);
	}

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

int32 URogueAddAmmoAbility::AddAmmo()
{
	const UAugmentDefinition* Definition = Cast<UAugmentDefinition>(GetCurrentSourceObject());
	const FAugmentLevelData* LevelData = Definition ? Definition->GetLevelData() : nullptr;
	const int32 Amount = LevelData
		? FMath::FloorToInt(LevelData->SetByCallerMagnitudes.FindRef(RogueTags::Data_Magnitude_Primary))
		: 0;

	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UCombatComponent* Combat = Avatar ? Avatar->FindComponentByClass<UCombatComponent>() : nullptr;
	AWeaponBase* Weapon = Combat ? Combat->GetEquippedWeapon() : nullptr;

	return Weapon ? Weapon->AdjustMagazineAmmoLocal(Amount) : 0;
}

void URogueAddAmmoAbility::HandleAddRejected(const int32 Added)
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	const UCombatComponent* Combat = Avatar ? Avatar->FindComponentByClass<UCombatComponent>() : nullptr;
	if (AWeaponBase* Weapon = Combat ? Combat->GetEquippedWeapon() : nullptr)
	{
		Weapon->AdjustMagazineAmmoLocal(-Added);
	}
}

void URogueAddAmmoAbility::HandleRepeatFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}
