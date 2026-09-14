// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/Actor/RogueDeployableActor.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GAS/RogueAbilitySystemComponent.h"
#include "GAS/RogueHealthComponent.h"
#include "GAS/RogueVitalAttributeSet.h"
#include "Tags/GameplayTags.h"

ARogueDeployableActor::ARogueDeployableActor()
{
	AbilitySystemComponent = CreateDefaultSubobject<URogueAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	VitalSet               = CreateDefaultSubobject<URogueVitalAttributeSet>(TEXT("VitalSet"));
	HealthComponent        = CreateDefaultSubobject<URogueHealthComponent>(TEXT("HealthComponent"));

	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
	bAutoDetonate = false;
}

UAbilitySystemComponent* ARogueDeployableActor::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

float ARogueDeployableActor::GetHealth() const
{
	return VitalSet ? VitalSet->GetHealth() : 0.f;
}

float ARogueDeployableActor::GetMaxHealth() const
{
	return VitalSet ? VitalSet->GetMaxHealth() : 0.f;
}

void ARogueDeployableActor::BeginPlay()
{
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	HealthComponent->InitializeWithAbilitySystem(AbilitySystemComponent);

	if (HasAuthority())
	{
		// PS를 Owner로 설정
		if (const APawn* InstigatorPawn = GetInstigator(); IsValid(InstigatorPawn))
		{
			if (APlayerState* CasterState = InstigatorPawn->GetPlayerState())
			{
				SetOwner(CasterState);
			}
		}

		AbilitySystemComponent->SetLooseGameplayTagCount(RogueTags::State_Combat_Deployable, 1);

		const float Health = FMath::Max(1.f, DeployHealth);
		AbilitySystemComponent->SetNumericAttributeBase(URogueVitalAttributeSet::GetMaxHealthAttribute(), Health);
		AbilitySystemComponent->SetNumericAttributeBase(URogueVitalAttributeSet::GetHealthAttribute(), Health);
	}

	Super::BeginPlay();
}

void ARogueDeployableActor::HandleDeath_Implementation(const FRogueDeathContext& Context)
{
	if (!HasAuthority() || bDestroyHandled)
	{
		return;
	}
	bDestroyHandled = true;

	if (DestroyedCueTag.IsValid())
	{
		FGameplayCueParameters CueParams;
		CueParams.Location     = GetActorLocation();
		CueParams.Instigator   = Context.InstigatorActor;
		CueParams.EffectCauser = this;

		AbilitySystemComponent->ExecuteGameplayCue(DestroyedCueTag, CueParams);
	}

	SetActorEnableCollision(false);
	SetActorHiddenInGame(true);
	SetLifeSpan(FMath::Max(KINDA_SMALL_NUMBER, DestroyDelaySeconds));
}

UCombatHitZoneDefinition* ARogueDeployableActor::GetCombatHitZoneDefinition_Implementation() const
{
	return nullptr;
}
