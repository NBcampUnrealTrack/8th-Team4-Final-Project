#include "Actor/AugmentPhaseBarrier.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameMode/State/BaseRoundGameState.h"
#include "Net/UnrealNetwork.h"

AAugmentPhaseBarrier::AAugmentPhaseBarrier()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	bAlwaysRelevant = true;
	SetReplicateMovement(false);
	SetCanBeDamaged(false);

	BarrierMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrierMesh"));
	SetRootComponent(BarrierMesh);
}

void AAugmentPhaseBarrier::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAugmentPhaseBarrier, bIsRuntimeBarrier);
}

void AAugmentPhaseBarrier::BeginPlay()
{
	Super::BeginPlay();

	ApplyInstanceRole();

	if (!HasAuthority() || bIsRuntimeBarrier)
	{
		return;
	}

	BoundRoundGameState = GetWorld()->GetGameState<ABaseRoundGameState>();
	if (!IsValid(BoundRoundGameState))
	{
		return;
	}

	BoundRoundGameState->OnRoundStateChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleRoundStateChanged);

	// 늦게 로드된 배치 인스턴스도 현재 페이즈를 즉시 반영한다.
	UpdateForCurrentPhase();
}

void AAugmentPhaseBarrier::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority() && !bIsRuntimeBarrier)
	{
		DestroyRuntimeBarrier();

		if (IsValid(BoundRoundGameState))
		{
			BoundRoundGameState->OnRoundStateChanged.RemoveDynamic(
				this,
				&ThisClass::HandleRoundStateChanged);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AAugmentPhaseBarrier::HandleRoundStateChanged()
{
	UpdateForCurrentPhase();
}

void AAugmentPhaseBarrier::OnRep_IsRuntimeBarrier()
{
	ApplyInstanceRole();
}

void AAugmentPhaseBarrier::UpdateForCurrentPhase()
{
	if (!HasAuthority() || bIsRuntimeBarrier || !IsValid(BoundRoundGameState))
	{
		return;
	}

	if (BoundRoundGameState->GetGameFlowPhase() == EGameFlowPhase::AugmentSelecting)
	{
		SpawnRuntimeBarrier();
	}
	else
	{
		DestroyRuntimeBarrier();
	}
}

void AAugmentPhaseBarrier::SpawnRuntimeBarrier()
{
	if (IsValid(RuntimeBarrier))
	{
		return;
	}

	RuntimeBarrier = nullptr;

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const FTransform SpawnTransform = GetActorTransform();
	AAugmentPhaseBarrier* NewBarrier = World->SpawnActorDeferred<AAugmentPhaseBarrier>(
		GetClass(),
		SpawnTransform,
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn,
		ESpawnActorScaleMethod::OverrideRootScale);
	if (!IsValid(NewBarrier))
	{
		return;
	}

	// BeginPlay 전에 런타임 장벽으로 표시해 자기 자신을 다시 생성하지 않게 한다.
	NewBarrier->bIsRuntimeBarrier = true;
	NewBarrier->FinishSpawning(
		SpawnTransform,
		false,
		nullptr,
		ESpawnActorScaleMethod::OverrideRootScale);

	RuntimeBarrier = NewBarrier;
}

void AAugmentPhaseBarrier::DestroyRuntimeBarrier()
{
	AAugmentPhaseBarrier* BarrierToDestroy = RuntimeBarrier.Get();
	RuntimeBarrier = nullptr;

	if (IsValid(BarrierToDestroy))
	{
		BarrierToDestroy->Destroy();
	}
}

void AAugmentPhaseBarrier::ApplyInstanceRole()
{
	// 맵 배치 인스턴스는 기준점이고, 생성된 복제본만 보이고 충돌한다.
	SetActorHiddenInGame(!bIsRuntimeBarrier);
	SetActorEnableCollision(bIsRuntimeBarrier);
}
