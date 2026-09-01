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
	DOREPLIFETIME_CONDITION(ARogueAugmentActor, UsedInputTag, COND_InitialOnly);
}

void ARogueAugmentActor::BeginPlay()
{
	Super::BeginPlay();

	ScatterAnchor = GetActorLocation();

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

	const FVector DetonateOrigin = ResolveDetonateOrigin();

	Payload.Origin = DetonateOrigin;
	Payload.Causer = this;

	URogueEffectApplier::Apply(Payload);

	Multicast_Detonated(DetonateOrigin);

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

FVector ARogueAugmentActor::ResolveDetonateOrigin() const
{
	if (ScatterRadius <= 0.f)
	{
		return GetActorLocation();
	}

	const FVector2D Offset = FMath::RandPointInCircle(ScatterRadius);
	const FVector Scattered = ScatterAnchor + FVector(Offset.X, Offset.Y, 0.f);

	const UWorld* World = GetWorld();
	if (!bScatterSnapToGround || !World)
	{
		return Scattered;
	}

	const FVector TraceStart = Scattered + FVector(0.f, 0.f, ScatterTraceDistance);
	const FVector TraceEnd   = Scattered - FVector(0.f, 0.f, ScatterTraceDistance);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(RogueScatterGround), false, this);

	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		return Hit.ImpactPoint;
	}

	return Scattered;
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

void ARogueAugmentActor::Multicast_Detonated_Implementation(const FVector_NetQuantize Location)
{
	LastDetonateLocation = Location;

	OnDetonated();
}
