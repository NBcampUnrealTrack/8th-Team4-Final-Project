#include "Actor/AugmentPhaseBarrier.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameMode/State/BaseRoundGameState.h"
#include "GameMode/State/DominationGameState.h"
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
	if (IsValid(BoundRoundGameState))
	{
		BoundRoundGameState->OnRoundStateChanged.AddUniqueDynamic(
			this,
			&ThisClass::HandleRoundStateChanged);

		// 늦게 로드된 배치 인스턴스도 현재 페이즈를 즉시 반영한다.
		UpdateForCurrentPhase();
		return;
	}

	// DominationMode에서 재사용할 수 있도록 수정
	BoundDominationGameState = GetWorld()->GetGameState<ADominationGameState>();
	if (!IsValid(BoundDominationGameState))
	{
		return;
	}

	BoundDominationGameState->OnDominationFlowStateChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleRoundStateChanged);
	BoundDominationGameState->OnObjectiveFlowStateChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleRoundStateChanged);
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

		// Domination 바인딩 해제
		if (IsValid(BoundDominationGameState))
		{
			BoundDominationGameState->OnDominationFlowStateChanged.RemoveDynamic(
				this,
				&ThisClass::HandleRoundStateChanged);
			BoundDominationGameState->OnObjectiveFlowStateChanged.RemoveDynamic(
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
	// 라운드 모드/Domination 모드 중 바인딩된 쪽의 상태로 장벽 필요 여부를 판단
	if (!HasAuthority() || bIsRuntimeBarrier
		|| (!IsValid(BoundRoundGameState) && !IsValid(BoundDominationGameState)))
	{
		return;
	}

	// Domination은 모든 플레이어의 초기 증강 선택이 끝나는 순간 첫 거점(C)이 활성화되며 ActivePointId가 None에서 바뀐다.
	// 따라서 매치가 진행 중이면서 활성 거점이 없는 동안이 초기 증강 선택 구간이고, 이후에는 스폰 보호 장벽이 대신한다.
	const bool bShouldBlock = IsValid(BoundRoundGameState)
		? BoundRoundGameState->GetGameFlowPhase() == EGameFlowPhase::AugmentSelecting
		: BoundDominationGameState->GetDominationPhase() == EDominationMatchPhase::InProgress
			&& BoundDominationGameState->GetActivePointId() == EDominationPointId::None;

	if (bShouldBlock)
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
