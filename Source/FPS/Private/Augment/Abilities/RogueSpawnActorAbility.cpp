// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/Abilities/RogueSpawnActorAbility.h"

#include "Augment/Actor/RogueAugmentActor.h"
#include "Augment/AugmentTypes.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

DEFINE_LOG_CATEGORY_STATIC(LogSpawnActor, Log, All);

URogueSpawnActorAbility::URogueSpawnActorAbility()
{
	// 스폰은 서버에서만. 입력 발동 증강은 BP 에서 LocalPredicted로 변경
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void URogueSpawnActorAbility::Deliver(const FAugmentGrantedAbility& Entry,
                                      const FRogueEffectPayload& Payload)
{
	AActor* Avatar = CurrentActorInfo ? CurrentActorInfo->AvatarActor.Get() : nullptr;
	UWorld* World = IsValid(Avatar) ? Avatar->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	UClass* ActorClass = Entry.SpawnActorClass.LoadSynchronous();
	if (!ensureMsgf(ActorClass,
		TEXT("[SpawnActor] %s 를 부여한 계열의 SpawnActorClass 가 비어 있거나 로드에 실패했습니다"), *GetName()))
	{
		return;
	}

	FVector EyeLocation;
	FRotator EyeRotation;
	Avatar->GetActorEyesViewPoint(EyeLocation, EyeRotation);

	const FTransform SpawnTransform(
		EyeRotation, EyeLocation + EyeRotation.Vector() * SpawnForwardOffset);

	ARogueAugmentActor* Spawned = World->SpawnActorDeferred<ARogueAugmentActor>(
		ActorClass, SpawnTransform, Avatar, Cast<APawn>(Avatar),
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);

	if (!ensureMsgf(Spawned, TEXT("[SpawnActor] %s 스폰에 실패했습니다"), *GetNameSafe(ActorClass)))
	{
		return;
	}

	Spawned->InitializePayload(Payload);
	Spawned->SetUsedInputTag(UsedInputTag);
	Spawned->FinishSpawning(SpawnTransform);
}
