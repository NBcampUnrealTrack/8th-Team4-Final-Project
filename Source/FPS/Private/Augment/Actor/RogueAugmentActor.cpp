// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/Actor/RogueAugmentActor.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogAugmentActor, Log, All);

ARogueAugmentActor::ARogueAugmentActor()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicateMovement(true);
}

void ARogueAugmentActor::InitializePayload(const FRogueEffectPayload& InPayload)
{
	Payload = InPayload;

	EffectRadius = InPayload.Radius;
}

void ARogueAugmentActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 스폰 시점에 정해지고 그 뒤로 안 바뀐다
	DOREPLIFETIME_CONDITION(ARogueAugmentActor, EffectRadius, COND_InitialOnly);
}

void ARogueAugmentActor::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}

	if (LifeSeconds > 0.f)
	{
		GetWorldTimerManager().SetTimer(LifeTimer, this, &ThisClass::K2_DestroyActor, LifeSeconds, false);
	}

	if (ShouldStartFuseOnBeginPlay())
	{
		StartFuse();
	}
}

void ARogueAugmentActor::StartFuse()
{
	if (!HasAuthority())
	{
		return;
	}

	if (DetonateInterval > 0.f)
	{
		GetWorldTimerManager().SetTimer(
			FuseTimer, this, &ThisClass::Detonate, DetonateInterval, true, FuseSeconds);
		return;
	}

	if (FuseSeconds > 0.f)
	{
		GetWorldTimerManager().SetTimer(FuseTimer, this, &ThisClass::Detonate, FuseSeconds, false);
		return;
	}

	Detonate();
}

void ARogueAugmentActor::Detonate()
{
	if (!HasAuthority())
	{
		return;
	}

	const bool bRepeating = DetonateInterval > 0.f;

	if (!bRepeating && bDetonated)
	{
		return;
	}

	bDetonated = true;

	Payload.Origin = GetActorLocation();
	Payload.Causer = this;

	URogueEffectApplier::Apply(Payload);

	Multicast_Detonated();

	if (bRepeating)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(FuseTimer);

	SpawnFollowUpActor();

	if (bDestroyOnDetonate)
	{
		Destroy();
	}
}

void ARogueAugmentActor::SpawnFollowUpActor()
{
	if (SpawnOnDetonateClass.IsNull())
	{
		return;
	}

	UClass* FollowUpClass = SpawnOnDetonateClass.LoadSynchronous();
	UWorld* World = GetWorld();
	if (!FollowUpClass || !World)
	{
		UE_LOG(LogAugmentActor, Error,
			TEXT("[AugmentActor] %s 의 SpawnOnDetonateClass 로드에 실패했습니다"), *GetName());
		return;
	}

	// 굴러간 자리가 아니라 기폭한 이 자리에 남긴다
	const FTransform SpawnTransform(FRotator::ZeroRotator, GetActorLocation());

	ARogueAugmentActor* FollowUp = World->SpawnActorDeferred<ARogueAugmentActor>(
		FollowUpClass, SpawnTransform, GetOwner(), GetInstigator(),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!FollowUp)
	{
		return;
	}

	// Origin 과 Causer 는 후속 액터가 자기 Detonate 에서 다시 채운다
	FollowUp->InitializePayload(Payload);
	FollowUp->FinishSpawning(SpawnTransform);
}

void ARogueAugmentActor::Multicast_Detonated_Implementation()
{
	OnDetonated();
}
