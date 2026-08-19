// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/RogueTargetFilter.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Player/BaseTeamPlayerState.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Tags/GameplayTags.h"

namespace
{
	const ABaseTeamPlayerState* ResolveTeamPlayerState(const AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return nullptr;
		}

		if (const ABaseTeamPlayerState* PlayerState = Cast<ABaseTeamPlayerState>(Actor))
		{
			return PlayerState;
		}

		if (const APawn* Pawn = Cast<APawn>(Actor))
		{
			return Pawn->GetPlayerState<ABaseTeamPlayerState>();
		}

		if (const AController* Controller = Cast<AController>(Actor))
		{
			return Controller->GetPlayerState<ABaseTeamPlayerState>();
		}

		return nullptr;
	}
}

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

	const ABaseTeamPlayerState* SourcePlayerState = ResolveTeamPlayerState(Source);
	const ABaseTeamPlayerState* TargetPlayerState = ResolveTeamPlayerState(Target);
	if (!IsValid(SourcePlayerState) || !IsValid(TargetPlayerState)
		|| SourcePlayerState->GetTeam() == EGameTeam::None
		|| TargetPlayerState->GetTeam() == EGameTeam::None)
	{
		return ERogueTargetRelation::TeamUnknown;
	}

	return SourcePlayerState->GetTeam() == TargetPlayerState->GetTeam()
		? ERogueTargetRelation::Friendly
		: ERogueTargetRelation::Hostile;
}

bool URogueTargetFilter::IsAlive(const AActor* Target)
{
	const UAbilitySystemComponent* TargetASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(const_cast<AActor*>(Target));

	return TargetASC && !TargetASC->HasMatchingGameplayTag(RogueTags::State_Combat_IsDead);
}
