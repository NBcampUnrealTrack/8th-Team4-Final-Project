#include "Actor/DominationSpawnBarrier.h"

#include "Actor/DominationCapturePoint.h"
#include "Actor/DominationSpawnVolume.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameMode/State/DominationGameState.h"
#include "Killcam/KillcamReplaySubsystem.h"
#include "Player/BaseTeamPlayerState.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSpawnBarrier, Log, All);

TArray<TWeakObjectPtr<ADominationSpawnBarrier>> ADominationSpawnBarrier::ActiveBarriers;

ADominationSpawnBarrier::ADominationSpawnBarrier()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	BarrierZone = CreateDefaultSubobject<UBoxComponent>(TEXT("BarrierZone"));
	SetRootComponent(BarrierZone);
	BarrierZone->SetBoxExtent(FVector(20.0f, 400.0f, 300.0f));
	BarrierZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BarrierZone->SetCollisionObjectType(ECC_WorldDynamic);
	BarrierZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	BarrierZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	// 히트스캔 트레이스 채널(ECC_Visibility)도 막아야 총알이 물리적으로 벽에서 멈춤
	// 같은 팀의 총알은 HitscanTrace.cpp의 ResolveCurrentPellet에서 이 액터를 무시 목록에 추가해 통과
	BarrierZone->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	BarrierZone->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	BarrierZone->SetGenerateOverlapEvents(false);
	BarrierZone->SetCanEverAffectNavigation(false);

	DetectionZone = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectionZone"));
	DetectionZone->SetupAttachment(BarrierZone);
	DetectionZone->SetBoxExtent(FVector(60.0f, 420.0f, 320.0f));
	DetectionZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DetectionZone->SetCollisionObjectType(ECC_WorldDynamic);
	DetectionZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	DetectionZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	DetectionZone->SetGenerateOverlapEvents(false);
	DetectionZone->SetCanEverAffectNavigation(false);
	DetectionZone->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleDetectionBeginOverlap);

	// 연출 전용
	BarrierMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BarrierMesh"));
	BarrierMesh->SetupAttachment(BarrierZone);
	BarrierMesh->SetMobility(EComponentMobility::Movable);
	BarrierMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BarrierMesh->SetGenerateOverlapEvents(false);
	BarrierMesh->SetCanEverAffectNavigation(false);
	BarrierMesh->SetHiddenInGame(true);
}

void ADominationSpawnBarrier::BeginPlay()
{
	Super::BeginPlay();

	// 레벨/BP에서 디자이너가 조정해둔 스케일을 활성 상태의 "정상 크기"로 기억
	// 생성자 시점에는 아직 그 커스터마이징이 적용되기 전이라 여기서 캡처
	MeshVisibleScale = BarrierMesh->GetRelativeScale3D();

	ActiveBarriers.AddUnique(this);

	// Modified: GameStateSetEvent 하나에만 의존하지 않는다 - 그 이벤트가 유독 늦게 발동하는 세션이
	// 있었고(최대 40초 이상), 그동안 아무 것도 재시도하지 않아서 장벽이 계속 비활성 상태로 멈춰있었다.
	// 이벤트는 빠른 경로로 계속 구독해두되, RefreshFromCurrentGameState 쪽에서도 GameState를 직접
	// 다시 찾아보게 해서 이벤트가 늦더라도 0.25초 폴링만으로 스스로 복구되게 한다.
	if (UWorld* World = GetWorld())
	{
		World->GameStateSetEvent.AddUObject(this, &ThisClass::HandleGameStateSet);
	}
	RefreshFromCurrentGameState();

	FTimerHandle VisibilityRefreshTimer;
	GetWorldTimerManager().SetTimer(VisibilityRefreshTimer, this, &ThisClass::ForceRefreshMeshVisibility, 0.1f, false);
}

void ADominationSpawnBarrier::ForceRefreshMeshVisibility()
{
	ApplyMeshVisibility(CurrentGuardedTeam == EGameTeam::None);
}

void ADominationSpawnBarrier::ApplyMeshVisibility(const bool bNewHidden)
{
	BarrierMesh->SetHiddenInGame(bNewHidden);
	BarrierMesh->SetRelativeScale3D(bNewHidden ? FVector::ZeroVector : MeshVisibleScale);
}

void ADominationSpawnBarrier::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ActiveBarriers.RemoveAll([this](const TWeakObjectPtr<ADominationSpawnBarrier>& Ptr)
	{
		return Ptr.Get() == this;
	});

	GetWorldTimerManager().ClearTimer(GameStateRetryTimer);

	// Ignoring Actor 초기화
	for (const TWeakObjectPtr<AActor>& IgnoringActor : IgnoringActors)
	{
		AActor* Actor = IgnoringActor.Get();
		UPrimitiveComponent* MovementComponent = IsValid(Actor) ? Cast<UPrimitiveComponent>(Actor->GetRootComponent()) : nullptr;
		if (MovementComponent)
		{
			MovementComponent->IgnoreActorWhenMoving(this, false);
		}
	}
	IgnoringActors.Reset();

	if (UWorld* World = GetWorld())
	{
		World->GameStateSetEvent.RemoveAll(this);
	}

	if (IsValid(BoundGameState))
	{
		BoundGameState->OnActivePointChanged.RemoveDynamic(this, &ThisClass::HandleActivePointChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void ADominationSpawnBarrier::HandleGameStateSet(AGameStateBase* NewGameState)
{
	if (IsValid(NewGameState) && !UKillcamReplaySubsystem::IsLiveWorldActor(NewGameState))
	{
		return;
	}

	BoundGameState = Cast<ADominationGameState>(NewGameState);
	if (!IsValid(BoundGameState))
	{
		return;
	}

	BoundGameState->OnActivePointChanged.AddUniqueDynamic(this, &ThisClass::HandleActivePointChanged);

	RefreshFromCurrentGameState();
}

void ADominationSpawnBarrier::RefreshFromCurrentGameState()
{
	UWorld* World = GetWorld();

	// Modified: GameStateSetEvent가 아직 안 왔어도(또는 유독 늦게 왔어도) 여기서 직접 다시 찾아본다.
	// 0.25초마다 이 함수가 재호출되니, 결국 여기서 자체적으로 찾아내서 스스로 복구된다.
	if (!IsValid(BoundGameState) && IsValid(World))
	{
		BoundGameState = World->GetGameState<ADominationGameState>();
		if (IsValid(BoundGameState))
		{
			BoundGameState->OnActivePointChanged.AddUniqueDynamic(this, &ThisClass::HandleActivePointChanged);
		}
	}

	const EDominationPointId ActivePointId = IsValid(BoundGameState)
		? BoundGameState->GetActivePointId()
		: EDominationPointId::None;

	// Modified: ActivePointId가 None이면 C라고 추측하지 않는다. DominationObjectiveManager::
	// ActivateInitialPoint()는 매치 시작 후 PointTransitionDuration(기본 30초) 동안 의도적으로
	// 아무 거점도 활성화하지 않는다 - 그동안 None은 리플리케이션 지연이 아니라 정상 상태다.
	// 이 구간에 C로 잘못 추측하면 아직 열리지도 않은 거점을 기준으로 장벽이 너무 일찍 켜진다.
	bool bBarrierStateResolved = false;
	if (ActivePointId != EDominationPointId::None)
	{
		bBarrierStateResolved = RefreshBarrierState(ActivePointId);
	}
	else
	{
		SetBarrierActive(EGameTeam::None);
	}

	int32 SpawnVolumeCount = 0;
	if (IsValid(World))
	{
		for (TActorIterator<ADominationSpawnVolume> It(World); It; ++It)
		{
			++SpawnVolumeCount;
		}
	}

	if (ActivePointId != EDominationPointId::None
		&& SpawnVolumeCount == DominationRules::SpawnBoundaryCount
		&& bBarrierStateResolved)
	{
		GetWorldTimerManager().ClearTimer(GameStateRetryTimer);
		return;
	}

	if (!GetWorldTimerManager().IsTimerActive(GameStateRetryTimer))
	{
		GetWorldTimerManager().SetTimer(GameStateRetryTimer, this, &ThisClass::RefreshFromCurrentGameState, 0.25f, true);
	}
}

void ADominationSpawnBarrier::HandleActivePointChanged(const EDominationPointId ActivePointId)
{
	RefreshFromCurrentGameState();
}

void ADominationSpawnBarrier::HandleDetectionBeginOverlap(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32,
	bool,
	const FHitResult&)
{
	if (!IsValid(OtherActor) || ResolveActorTeam(OtherActor) != CurrentGuardedTeam)
	{
		return;
	}

	// 아군만 통과, 상대는 막음
	if (UPrimitiveComponent* MovementComponent = Cast<UPrimitiveComponent>(OtherActor->GetRootComponent()))
	{
		MovementComponent->IgnoreActorWhenMoving(this, true);
		IgnoringActors.AddUnique(OtherActor);
	}
}

EGameTeam ADominationSpawnBarrier::ResolveActorTeam(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return EGameTeam::None;
	}

	const APawn* Pawn = Cast<APawn>(Actor);
	if (!IsValid(Pawn))
	{
		Pawn = Actor->GetInstigator();
	}

	const ABaseTeamPlayerState* PlayerState = IsValid(Pawn) ? Pawn->GetPlayerState<ABaseTeamPlayerState>() : nullptr;
	return IsValid(PlayerState) ? PlayerState->GetTeam() : EGameTeam::None;
}

bool ADominationSpawnBarrier::RefreshBarrierState(const EDominationPointId ActivePointId)
{
	ADominationSpawnVolume* TeamASpawnVolume = nullptr;
	ADominationSpawnVolume* TeamBSpawnVolume = nullptr;
	const bool bFoundTeamA = FindTeamSpawnVolume(ActivePointId, EGameTeam::TeamA, TeamASpawnVolume);
	const bool bFoundTeamB = FindTeamSpawnVolume(ActivePointId, EGameTeam::TeamB, TeamBSpawnVolume);
	if (!bFoundTeamA || !bFoundTeamB || !IsValid(BoundSpawnVolume))
	{
		return false;
	}

	EGameTeam NewGuardedTeam = EGameTeam::None;
	if (IsValid(BoundSpawnVolume) && BoundSpawnVolume == TeamASpawnVolume)
	{
		NewGuardedTeam = EGameTeam::TeamA;
	}
	else if (IsValid(BoundSpawnVolume) && BoundSpawnVolume == TeamBSpawnVolume)
	{
		NewGuardedTeam = EGameTeam::TeamB;
	}

	SetBarrierActive(NewGuardedTeam);
	return true;
}

void ADominationSpawnBarrier::SetBarrierActive(const EGameTeam NewGuardedTeam)
{
	if (CurrentGuardedTeam == NewGuardedTeam)
	{
		// 팀이 같아 조기 반환하더라도 메시 상태는 재적용
		ApplyMeshVisibility(CurrentGuardedTeam == EGameTeam::None);
		return;
	}

	// 스폰 변경에 따른 해제
	for (const TWeakObjectPtr<AActor>& IgnoringActor : IgnoringActors)
	{
		AActor* Actor = IgnoringActor.Get();
		UPrimitiveComponent* MovementComponent = IsValid(Actor) ? Cast<UPrimitiveComponent>(Actor->GetRootComponent()) : nullptr;
		if (MovementComponent)
		{
			MovementComponent->IgnoreActorWhenMoving(this, false);
		}
	}
	IgnoringActors.Reset();

	CurrentGuardedTeam = NewGuardedTeam;

	const bool bActive = CurrentGuardedTeam != EGameTeam::None;
	BarrierZone->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	DetectionZone->SetCollisionEnabled(bActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	DetectionZone->SetGenerateOverlapEvents(bActive);
	ApplyMeshVisibility(!bActive);
}

bool ADominationSpawnBarrier::FindTeamSpawnVolume(
	const EDominationPointId ActivePointId,
	const EGameTeam Team,
	ADominationSpawnVolume*& OutSpawnVolume) const
{
	OutSpawnVolume = nullptr;

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return false;
	}

	const int32 ActivePointIndex = DominationRules::GetPointIndex(ActivePointId);
	const int32 BoundaryIndex = DominationRules::GetSpawnBoundaryIndex(Team, ActivePointIndex);
	if (BoundaryIndex == INDEX_NONE)
	{
		return false;
	}

	const ADominationCapturePoint* PointA = FindCapturePoint(EDominationPointId::A);
	const ADominationCapturePoint* PointE = FindCapturePoint(EDominationPointId::E);
	if (!IsValid(PointA) || !IsValid(PointE))
	{
		return false;
	}

	const FVector PointALocation = PointA->GetActorLocation();
	const FVector SpawnAxis = (PointE->GetActorLocation() - PointALocation).GetSafeNormal();
	if (SpawnAxis.IsNearlyZero())
	{
		return false;
	}
	
	TArray<ADominationSpawnVolume*> SpawnVolumes;
	for (TActorIterator<ADominationSpawnVolume> It(World); It; ++It)
	{
		SpawnVolumes.Add(*It);
	}

	if (!SpawnVolumes.IsValidIndex(BoundaryIndex))
	{
		return false;
	}

	if (SpawnVolumes.Num() != DominationRules::SpawnBoundaryCount)
	{
		UE_LOG(LogSpawnBarrier, Warning,
			TEXT("%s: expected %d ADominationSpawnVolume actors but found %d - boundary index mapping may be wrong."),
			*GetName(), DominationRules::SpawnBoundaryCount, SpawnVolumes.Num());
	}
	
	SpawnVolumes.Sort(
		[PointALocation, SpawnAxis](const ADominationSpawnVolume& Left, const ADominationSpawnVolume& Right)
		{
			const double LeftDistance = FVector::DotProduct(Left.GetActorLocation() - PointALocation, SpawnAxis);
			const double RightDistance = FVector::DotProduct(Right.GetActorLocation() - PointALocation, SpawnAxis);
			if (!FMath::IsNearlyEqual(LeftDistance, RightDistance, 1.0))
			{
				return LeftDistance < RightDistance;
			}
			return Left.GetFName().LexicalLess(Right.GetFName());
		});

	OutSpawnVolume = SpawnVolumes[BoundaryIndex];
	return true;
}

const ADominationCapturePoint* ADominationSpawnBarrier::FindCapturePoint(const EDominationPointId PointId) const
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return nullptr;
	}

	for (TActorIterator<ADominationCapturePoint> It(World); It; ++It)
	{
		if (It->GetPointId() == PointId)
		{
			return *It;
		}
	}

	return nullptr;
}
