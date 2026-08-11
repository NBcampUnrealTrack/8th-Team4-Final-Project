// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/RogueTargetFilter.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Tags/GameplayTags.h"

ERogueTargetRelation URogueTargetFilter::GetRelation(const AActor* Source, const AActor* Target)
{
	if (!Source || !Target)
	{
		return ERogueTargetRelation::Invalid;
	}

	UAbilitySystemComponent* SourceASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Source));
	UAbilitySystemComponent* TargetASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Target));

	// ASC가 없는 경우
	if (!TargetASC)
	{
		return ERogueTargetRelation::Invalid;
	}

	// 자기 자신인 경우
	if (Source == Target || SourceASC == TargetASC)
	{
		return ERogueTargetRelation::Self;
	}

	// TODO: 팀 분간은 게임 플로우에서
	return ERogueTargetRelation::TeamUnknown;
}

bool URogueTargetFilter::IsAlive(const AActor* Target)
{
	const UAbilitySystemComponent* TargetASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Target));

	return TargetASC && !TargetASC->HasMatchingGameplayTag(RogueTags::State_Combat_IsDead);
}
