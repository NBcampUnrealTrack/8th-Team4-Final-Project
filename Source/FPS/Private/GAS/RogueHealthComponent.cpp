// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/RogueHealthComponent.h"

#include "AbilitySystemComponent.h"
#include "GAS/Damage/RogueGameplayEffectContext.h"
#include "GAS/RogueVitalAttributeSet.h"
#include "Interfaces/RogueDamageableTarget.h"
#include "Net/UnrealNetwork.h"
#include "Tags/GameplayTags.h"

URogueHealthComponent::URogueHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
}

void URogueHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(URogueHealthComponent, bIsDead);
}

void URogueHealthComponent::InitializeWithAbilitySystem(UAbilitySystemComponent* InASC)
{
	// 이미 초기화된 ASC는 다시 초기화 X
	if (AbilitySystemComponent == InASC)
	{
		return;
	}

	UninitializeFromAbilitySystem();

	if (!InASC)
	{
		return;
	}

	URogueVitalAttributeSet* Set =
		const_cast<URogueVitalAttributeSet*>(InASC->GetSet<URogueVitalAttributeSet>());

	if (!Set)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[RogueHealth] %s: ASC 에 URogueVitalAttributeSet 이 없습니다."),
			*GetNameSafe(GetOwner()));
		return;
	}

	AbilitySystemComponent = InASC;
	VitalSet               = Set;

	// 구독은 서버에서만
	// 클라이언트는 레플리케이션으로 태그를 추가
	if (InASC->IsOwnerActorAuthoritative())
	{
		OutOfHealthHandle = VitalSet->OnOutOfHealth.AddUObject(this, &URogueHealthComponent::HandleOutOfHealth);
	}

	// 이미 사망한 상태면 태그 추가
	if (bIsDead)
	{
		ApplyDeathTag(true);
	}
}

void URogueHealthComponent::UninitializeFromAbilitySystem()
{
	if (VitalSet && OutOfHealthHandle.IsValid())
	{
		VitalSet->OnOutOfHealth.Remove(OutOfHealthHandle);
	}

	OutOfHealthHandle.Reset();
	VitalSet               = nullptr;
	AbilitySystemComponent = nullptr;
}

void URogueHealthComponent::ServerResetDeathState()
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return;
	}

	// 데미지 처리를 위한 AttributeSet의 플래그 초기화
	if (VitalSet)
	{
		VitalSet->ClearDeathDeclared();
	}

	if (!bIsDead)
	{
		return;
	}

	bIsDead = false;

	// 서버에서는 직접 반영
	ApplyDeathTag(false);
	OnDeathStateChanged.Broadcast(false);
}

void URogueHealthComponent::OnUnregister()
{
	UninitializeFromAbilitySystem();

	Super::OnUnregister();
}

void URogueHealthComponent::OnRep_bIsDead()
{
	ApplyDeathTag(bIsDead);
	OnDeathStateChanged.Broadcast(bIsDead);
}

void URogueHealthComponent::HandleOutOfHealth(UAbilitySystemComponent* VictimASC,
                                              const FGameplayEffectContextHandle& ContextHandle)
{
	if (!VictimASC || VictimASC != AbilitySystemComponent)
	{
		return;
	}

	if (bIsDead)
	{
		return;
	}

	bIsDead = true;

	// 서버에서는 직접 반영
	ApplyDeathTag(true);
	OnDeathStateChanged.Broadcast(true);

	NotifyDeath(ContextHandle);
}

void URogueHealthComponent::ApplyDeathTag(bool bDead)
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// 사망 태그 추가
	AbilitySystemComponent->SetLooseGameplayTagCount(RogueTags::State_Combat_IsDead, bDead ? 1 : 0);
}

void URogueHealthComponent::NotifyDeath(const FGameplayEffectContextHandle& ContextHandle) const
{
	AActor* Victim = AbilitySystemComponent ? AbilitySystemComponent->GetAvatarActor() : nullptr;
	if (!Victim || !Victim->Implements<URogueDamageableTarget>())
	{
		return;
	}

	FRogueDeathContext DeathContext;
	DeathContext.VictimActor     = Victim;
	DeathContext.InstigatorActor = ContextHandle.GetOriginalInstigator();
	DeathContext.CauserActor     = ContextHandle.GetEffectCauser();

	if (const FRogueGameplayEffectContext* Ctx = FRogueGameplayEffectContext::ExtractFrom(ContextHandle))
	{
		DeathContext.WeaponTag  = Ctx->WeaponTag;
		DeathContext.HitZoneTag = Ctx->HitZoneTag;
		DeathContext.bHeadshot  = (Ctx->HeadshotDamage > 0.f);
	}

	IRogueDamageableTarget::Execute_HandleDeath(Victim, DeathContext);
}
