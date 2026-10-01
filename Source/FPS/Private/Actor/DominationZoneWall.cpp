#include "Actor/DominationZoneWall.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameMode/State/DominationGameState.h"
#include "TimerManager.h"

ADominationZoneWall::ADominationZoneWall()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	WallZone = CreateDefaultSubobject<UBoxComponent>(TEXT("WallZone"));
	SetRootComponent(WallZone);
	WallZone->SetBoxExtent(FVector(20.0f, 400.0f, 300.0f));
	WallZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WallZone->SetCollisionObjectType(ECC_WorldDynamic);
	WallZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	WallZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	WallZone->SetGenerateOverlapEvents(false);
	WallZone->SetCanEverAffectNavigation(false);

	WallMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WallMesh"));
	WallMesh->SetupAttachment(WallZone);
	WallMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WallMesh->SetGenerateOverlapEvents(false);
	WallMesh->SetCanEverAffectNavigation(false);
	WallMesh->SetHiddenInGame(true);
}

void ADominationZoneWall::BeginPlay()
{
	Super::BeginPlay();

	WallVisibleScale = WallMesh->GetRelativeScale3D();

	WallZone->SetCollisionResponseToChannel(ECC_Visibility, bBlockBullets ? ECR_Block : ECR_Ignore);

	// GameState가 잡히기 전에도 첫 거점 기준 상태로 시작
	HandleActivePointChanged(EDominationPointId::None);

	if (UWorld* World = GetWorld())
	{
		World->GameStateSetEvent.AddUObject(this, &ThisClass::HandleGameStateSet);
	}
	RefreshFromCurrentGameState();
}

void ADominationZoneWall::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(GameStateRetryTimer);

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

void ADominationZoneWall::HandleGameStateSet(AGameStateBase* NewGameState)
{
	BoundGameState = Cast<ADominationGameState>(NewGameState);
	RefreshFromCurrentGameState();
}

void ADominationZoneWall::RefreshFromCurrentGameState()
{
	UWorld* World = GetWorld();
	if (!IsValid(BoundGameState) && IsValid(World))
	{
		BoundGameState = World->GetGameState<ADominationGameState>();
	}

	if (IsValid(BoundGameState))
	{
		BoundGameState->OnActivePointChanged.AddUniqueDynamic(this, &ThisClass::HandleActivePointChanged);
		HandleActivePointChanged(BoundGameState->GetActivePointId());
		GetWorldTimerManager().ClearTimer(GameStateRetryTimer);
		return;
	}

	// 클라이언트는 GameState 리플리케이션이 늦을 수 있어 잡힐 때까지 재시도
	if (!GetWorldTimerManager().IsTimerActive(GameStateRetryTimer))
	{
		GetWorldTimerManager().SetTimer(GameStateRetryTimer, this, &ThisClass::RefreshFromCurrentGameState, 0.25f, true);
	}
}

void ADominationZoneWall::HandleActivePointChanged(const EDominationPointId ActivePointId)
{
	// 아직 활성 거점이 정해지지 않은 매치 시작 전(None)에는 첫 거점(C)으로 간주해 C용 벽이 처음부터 켜져 있게 한다.
	// 거점 전환 중에는 GameState가 이미 다음 거점 ID를 가지므로 None은 매치 시작 전/라운드 리셋에서만 나온다.
	const EDominationPointId EffectivePointId = ActivePointId == EDominationPointId::None
		? DominationRules::InitialPointId
		: ActivePointId;
	SetWallActive(ActiveDuringPoints.Contains(EffectivePointId));
}

void ADominationZoneWall::SetWallActive(const bool bNewActive)
{
	bWallActive = bNewActive;
	WallZone->SetCollisionEnabled(bNewActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	WallMesh->SetHiddenInGame(!bNewActive);
	WallMesh->SetRelativeScale3D(bNewActive ? WallVisibleScale : FVector::ZeroVector);
}
