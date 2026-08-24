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
	DOREPLIFETIME(URogueHealthComponent, DeathReaction);
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

	// 이미 사망 상태면 연출 재생
	if (bIsDead)
	{
		ApplyDeathStateLocally(true);
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
	ApplyDeathStateLocally(false);
	OnDeathStateChanged.Broadcast(false);
}

void URogueHealthComponent::OnUnregister()
{
	UninitializeFromAbilitySystem();

	Super::OnUnregister();
}

void URogueHealthComponent::OnRep_bIsDead()
{
	ApplyDeathStateLocally(bIsDead);
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

	// 사망 연출용 정보 구성
	ServerBuildDeathReaction(ContextHandle);

	bIsDead = true;

	// 서버에서는 직접 반영
	ApplyDeathStateLocally(true);
	OnDeathStateChanged.Broadcast(true);

	NotifyDeath(ContextHandle);
}

void URogueHealthComponent::ApplyDeathStateLocally(bool bDead)
{
	if (!AbilitySystemComponent)
	{
		return;
	}

	// 사망 태그 추가
	AbilitySystemComponent->SetLooseGameplayTagCount(RogueTags::State_Combat_IsDead, bDead ? 1 : 0);

	// 연출은 단방향. 원복은 폰 재스폰이 담당
	if (!bDead)
	{
		return;
	}

	AActor* Avatar = AbilitySystemComponent->GetAvatarActor();
	if (Avatar && Avatar->Implements<URogueDamageableTarget>())
	{
		IRogueDamageableTarget::Execute_PlayDeathReaction(Avatar, DeathReaction);
	}
}

void URogueHealthComponent::ServerBuildDeathReaction(const FGameplayEffectContextHandle& ContextHandle)
{
	DeathReaction = FRogueDeathReaction();

	const AActor* Victim = AbilitySystemComponent ? AbilitySystemComponent->GetAvatarActor() : nullptr;
	if (!Victim)
	{
		return;
	}

	// 히트스캔 사망 연출 정보 구성
	if (const FRogueGameplayEffectContext* Ctx = FRogueGameplayEffectContext::ExtractFrom(ContextHandle))
	{
		DeathReaction.bHeadshot   = Ctx->bHeadshot;
		DeathReaction.HitBoneName = Ctx->HitBoneName;

		// 수평 방향만 사용
		DeathReaction.Direction   = Ctx->HitShotDirection.GetSafeNormal2D();
	}

	// 히트스캔 종료
	if (!DeathReaction.Direction.IsZero())
	{
		return;
	}

	// 히트스캔이 아닌경우 방향만 계산
	const AActor* Instigator = ContextHandle.GetOriginalInstigator();
	if (IsValid(Instigator) && Instigator != Victim)
	{
		DeathReaction.Direction =
			(Victim->GetActorLocation() - Instigator->GetActorLocation()).GetSafeNormal2D();
	}
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
		DeathContext.WeaponTag = Ctx->WeaponTag;
		DeathContext.bHeadshot = Ctx->bHeadshot;
	}

	IRogueDamageableTarget::Execute_HandleDeath(Victim, DeathContext);
}
