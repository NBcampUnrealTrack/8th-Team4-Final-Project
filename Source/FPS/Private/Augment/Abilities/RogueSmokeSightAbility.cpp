#include "Augment/Abilities/RogueSmokeSightAbility.h"

#include "AbilitySystemComponent.h"
#include "Augment/AugmentDefinition.h"
#include "Engine/World.h"
#include "Interfaces/PlayerInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Tags/GameplayTags.h"

namespace
{
	const FName IgnoreSmokeParam(TEXT("IgnoreSmoke"));
	const FName IgnoreSmokeRangeParam(TEXT("IgnoreSmokeRange"));
}

URogueSmokeSightAbility::URogueSmokeSightAbility()
{
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::LocalOnly;
	InstancingPolicy   = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void URogueSmokeSightAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
                                              const FGameplayAbilityActorInfo* ActorInfo,
                                              const FGameplayAbilityActivationInfo ActivationInfo,
                                              const FGameplayEventData* TriggerEventData)
{
	if (!FindGrantedEntry(Handle, ActorInfo) || !CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	ApplyOverride(true);
}

void URogueSmokeSightAbility::OnAvatarSet(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	UGameplayAbility::OnAvatarSet(ActorInfo, Spec);

	if (!ActorInfo || !ActorInfo->IsLocallyControlled())
	{
		return;
	}

	UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
	const AActor* Avatar = ActorInfo->AvatarActor.Get();
	const bool bCombatAvatar = IsValid(Avatar) && Avatar->Implements<UPlayerInterface>();

	if (Spec.IsActive() && !bCombatAvatar && ASC)
	{
		ASC->CancelAbilityHandle(Spec.Handle);
	}
	else if (bActivateOnAvatarSet && bCombatAvatar && !Spec.IsActive() && ASC)
	{
		ASC->TryActivateAbility(Spec.Handle);
	}
}

void URogueSmokeSightAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
                                         const FGameplayAbilityActorInfo* ActorInfo,
                                         const FGameplayAbilityActivationInfo ActivationInfo,
                                         bool bReplicateEndAbility, bool bWasCancelled)
{
	ApplyOverride(false);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void URogueSmokeSightAbility::ApplyOverride(const bool bEnable) const
{
	UWorld* World = GetWorld();
	UMaterialParameterCollectionInstance* Instance = World && SmokeOcclusionCollection
		? World->GetParameterCollectionInstance(SmokeOcclusionCollection)
		: nullptr;
	if (!Instance)
	{
		return;
	}

	const UAugmentDefinition* Definition = Cast<UAugmentDefinition>(GetCurrentSourceObject());
	const FAugmentLevelData* LevelData = Definition ? Definition->GetLevelData() : nullptr;
	const float RangeMeters = LevelData ? LevelData->SetByCallerMagnitudes.FindRef(RogueTags::Data_Magnitude_Range) : 0.f;

	if (RangeMeters > 0.f)
	{
		Instance->SetScalarParameterValue(IgnoreSmokeRangeParam, bEnable ? RangeMeters * 100.f : 0.f);
	}
	else
	{
		Instance->SetScalarParameterValue(IgnoreSmokeParam, bEnable ? 1.f : 0.f);
	}
}
