// Fill out your copyright notice in the Description page of Project Settings.


#include "GAS/RogueTargetFilter.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
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

		if (const ABaseTeamPlayerState* OwnerState = Cast<ABaseTeamPlayerState>(Actor->GetOwner()))
		{
			return OwnerState;
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

bool URogueTargetFilter::PassesQuery(const AActor* Source, const AActor* Target, const FRogueTargetQuery& Query)
{
	if (!IsAlive(Target))
	{
		return false;
	}

	// 조건에 맞는 관계인지 확인
	switch (GetRelation(Source, Target))
	{
	case ERogueTargetRelation::Self:     return Query.bIncludeSelf;
	case ERogueTargetRelation::Friendly: return Query.bIncludeFriendly;
	case ERogueTargetRelation::Hostile:  return Query.bIncludeHostile;
	default:                             return false;
	}
}

void URogueTargetFilter::GatherInRadius(AActor* Source, const FVector Origin, const float Radius,
                                        const FRogueTargetQuery& Query, TArray<AActor*>& OutTargets)
{
	const UWorld* World = IsValid(Source) ? Source->GetWorld() : nullptr;
	if (!World || Radius <= 0.f)
	{
		return;
	}

	FCollisionQueryParams Params(FName(TEXT("RogueGatherInRadius")), false, Source);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		Origin,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(Radius),
		Params);

	int32 Added = 0;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (!PassesQuery(Source, Candidate, Query))
		{
			continue;
		}

		const int32 CountBefore = OutTargets.Num();
		OutTargets.AddUnique(Candidate);

		if (OutTargets.Num() == CountBefore)
		{
			continue;
		}

		if (Query.MaxTargets > 0 && ++Added >= Query.MaxTargets)
		{
			break;
		}
	}
}

void URogueTargetFilter::GatherInAimSweep(AActor* Source, const FVector Origin, const FVector Direction,
                                          const float Length, const float Radius,
                                          const FRogueTargetQuery& Query, TArray<AActor*>& OutTargets)
{
	const UWorld* World = IsValid(Source) ? Source->GetWorld() : nullptr;
	if (!World || Length <= 0.f || Radius <= 0.f || Direction.IsNearlyZero())
	{
		return;
	}

	FCollisionQueryParams Params(FName(TEXT("RogueGatherInAimSweep")), false, Source);

	const FVector Forward = Direction.GetSafeNormal();

	TArray<FHitResult> Hits;
	World->SweepMultiByObjectType(
		Hits,
		Origin,
		Origin + Forward * Length,
		FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeSphere(Radius),
		Params);

	int32 Added = 0;
	for (const FHitResult& Hit : Hits)
	{
		AActor* Candidate = Hit.GetActor();
		if (!PassesQuery(Source, Candidate, Query))
		{
			continue;
		}

		// 조준 방향과 반대 방향에 있는 대상은 제외
		if (FVector::DotProduct(Candidate->GetActorLocation() - Origin, Forward) < 0.f)
		{
			continue;
		}

		const int32 CountBefore = OutTargets.Num();
		OutTargets.AddUnique(Candidate);

		if (OutTargets.Num() == CountBefore)
		{
			continue;
		}

		if (Query.MaxTargets > 0 && ++Added >= Query.MaxTargets)
		{
			break;
		}
	}
}

void URogueTargetFilter::GatherTeamWide(AActor* Source, const FRogueTargetQuery& Query,
                                        TArray<AActor*>& OutTargets)
{
	const UWorld* World = IsValid(Source) ? Source->GetWorld() : nullptr;
	const AGameStateBase* GameState = World ? World->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	int32 Added = 0;
	for (const APlayerState* PlayerState : GameState->PlayerArray)
	{
		AActor* Candidate = PlayerState ? PlayerState->GetPawn() : nullptr;
		if (!PassesQuery(Source, Candidate, Query))
		{
			continue;
		}

		const int32 CountBefore = OutTargets.Num();
		OutTargets.AddUnique(Candidate);

		if (OutTargets.Num() == CountBefore)
		{
			continue;
		}

		if (Query.MaxTargets > 0 && ++Added >= Query.MaxTargets)
		{
			break;
		}
	}
}
