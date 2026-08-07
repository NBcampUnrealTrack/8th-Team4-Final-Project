// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/RogueAbilitySystemGlobals.h"

#include "GAS/Damage/RogueGameplayEffectContext.h"

FGameplayEffectContext* URogueAbilitySystemGlobals::AllocGameplayEffectContext() const
{
	return new FRogueGameplayEffectContext();
}
