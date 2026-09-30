// Fill out your copyright notice in the Description page of Project Settings.


#include "Augment/Actor/RogueAugmentActor.h"

#include "AbilitySystemGlobals.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameplayCueManager.h"
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

	PlaySpawnCue();

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

void ARogueAugmentActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearAllTimersForObject(this);

	Super::EndPlay(EndPlayReason);
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

	UE_CLOG(SpawnOnDetonateClass.IsPending(), LogAugmentActor, Warning,
		TEXT("[AugmentActor] %s 가 프리로드되지 않아 동기 로드합니다"), *SpawnOnDetonateClass.ToString());
	UClass* FollowUpClass = SpawnOnDetonateClass.LoadSynchronous();
	UWorld* World = GetWorld();
	if (!ensureMsgf(FollowUpClass && World,
		TEXT("[AugmentActor] %s 의 SpawnOnDetonateClass 로드에 실패했습니다"), *GetName()))
	{
		return;
	}

	// 굴러간 자리가 아니라 기폭한 이 자리에 남긴다
	const FTransform SpawnTransform(FRotator::ZeroRotator, GetActorLocation());

	ARogueAugmentActor* FollowUp = World->SpawnActorDeferred<ARogueAugmentActor>(
		FollowUpClass, SpawnTransform, GetOwner(), GetInstigator(),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (!ensureMsgf(FollowUp, TEXT("[AugmentActor] %s 의 후속 액터 %s 스폰에 실패했습니다"),
		*GetName(), *GetNameSafe(FollowUpClass)))
	{
		return;
	}

	// Origin 과 Causer 는 후속 액터가 자기 Detonate 에서 다시 채운다
	FollowUp->InitializePayload(Payload);
	FollowUp->FinishSpawning(SpawnTransform);
}

void ARogueAugmentActor::PlayDetonateCue(const FVector& Location) const
{
	UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (!DetonateCueTag.IsValid() || !CueManager)
	{
		return;
	}

	AActor* Self = const_cast<ARogueAugmentActor*>(this);

	FGameplayCueParameters CueParams;
	CueParams.Location     = Location;
	CueParams.Normal       = FVector::UpVector;
	CueParams.Instigator   = GetInstigator();
	CueParams.EffectCauser = Self;
	CueParams.RawMagnitude = EffectRadius;

	CueManager->HandleGameplayCue(Self, DetonateCueTag, EGameplayCueEvent::Executed, CueParams);
}

void ARogueAugmentActor::PlaySpawnCue() const
{
	UGameplayCueManager* CueManager = UAbilitySystemGlobals::Get().GetGameplayCueManager();
	if (!SpawnCueTag.IsValid() || !CueManager)
	{
		return;
	}

	AActor* Self = const_cast<ARogueAugmentActor*>(this);

	FGameplayCueParameters CueParams;
	CueParams.Location     = GetActorLocation();
	CueParams.Normal       = FVector::UpVector;
	CueParams.Instigator   = GetInstigator();
	CueParams.EffectCauser = Self;
	CueParams.RawMagnitude = EffectRadius;

	CueManager->HandleGameplayCue(Self, SpawnCueTag, EGameplayCueEvent::Executed, CueParams);
}

void ARogueAugmentActor::Multicast_Detonated_Implementation(const FVector_NetQuantize Location)
{
	LastDetonateLocation = Location;

	PlayDetonateCue(Location);

	OnDetonated();
}
