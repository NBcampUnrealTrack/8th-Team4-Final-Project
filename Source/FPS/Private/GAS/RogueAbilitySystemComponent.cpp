// Fill out your copyright notice in the Description page of Project Settings.

#include "GAS/RogueAbilitySystemComponent.h"

URogueAbilitySystemComponent::URogueAbilitySystemComponent()
{
	ReplicationMode = EGameplayEffectReplicationMode::Mixed;

	SetIsReplicatedByDefault(true);
}
