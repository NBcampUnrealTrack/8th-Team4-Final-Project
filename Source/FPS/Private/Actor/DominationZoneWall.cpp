#include "Actor/DominationZoneWall.h"

#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
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

	// 안쪽(Forward) 방향 표시용 화살표. 에디터 전용 표시
	InsideArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("InsideArrow"));
	InsideArrow->SetupAttachment(WallZone);
	InsideArrow->SetArrowColor(FColor::Green);
	InsideArrow->ArrowSize = 3.0f;
	InsideArrow->bIsScreenSizeScaled = false;
	InsideArrow->SetHiddenInGame(true);
	InsideArrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InsideArrow->SetGenerateOverlapEvents(false);
	InsideArrow->SetCanEverAffectNavigation(false);

	// 벽 바깥에 있던 플레이어의 한 방향 통과 허용을 위한 감지 영역
	DetectionZone = CreateDefaultSubobject<UBoxComponent>(TEXT("DetectionZone"));
	DetectionZone->SetupAttachment(WallZone);
	DetectionZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DetectionZone->SetCollisionObjectType(ECC_WorldDynamic);
	DetectionZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	DetectionZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	DetectionZone->SetGenerateOverlapEvents(false);
	DetectionZone->SetCanEverAffectNavigation(false);
	DetectionZone->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleDetectionBeginOverlap);
	DetectionZone->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleDetectionEndOverlap);

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

	// WallZone 크기에 맞춰 감지 영역을 두께 방향으로 넓힌다.
	DetectionZone->SetBoxExtent(WallZone->GetUnscaledBoxExtent() + FVector(DetectionMargin, 0.0f, 0.0f));

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
	ClearIgnoringActors();

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

	// 감지 영역도 함께 켜고 끈다. 켜지는 순간 이미 영역 안에 있던 폰은 BeginOverlap으로 들어온다.
	// 꺼질 때는 통과 허용 등록을 모두 해제한다.
	if (!bNewActive)
	{
		ClearIgnoringActors();
	}
	DetectionZone->SetCollisionEnabled(bNewActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	DetectionZone->SetGenerateOverlapEvents(bNewActive);
}

bool ADominationZoneWall::IsOutsideOfWall(const AActor* Actor) const
{
	// 벽의 Forward 방향이 맵 안쪽. 반대편(내적이 음수)에 있으면 바깥으로 본다.
	return IsValid(Actor)
		&& FVector::DotProduct(Actor->GetActorLocation() - GetActorLocation(), GetActorForwardVector()) < 0.0f;
}

void ADominationZoneWall::SetIgnoredByPawn(APawn* Pawn, const bool bIgnore)
{
	UPrimitiveComponent* MovementComponent = IsValid(Pawn) ? Cast<UPrimitiveComponent>(Pawn->GetRootComponent()) : nullptr;
	if (!MovementComponent)
	{
		return;
	}

	MovementComponent->IgnoreActorWhenMoving(this, bIgnore);
	if (bIgnore)
	{
		IgnoringPawns.AddUnique(Pawn);
	}
	else
	{
		IgnoringPawns.Remove(Pawn);
	}
}

void ADominationZoneWall::ClearIgnoringActors()
{
	for (const TWeakObjectPtr<APawn>& IgnoringPawn : IgnoringPawns)
	{
		if (APawn* Pawn = IgnoringPawn.Get())
		{
			if (UPrimitiveComponent* MovementComponent = Cast<UPrimitiveComponent>(Pawn->GetRootComponent()))
			{
				MovementComponent->IgnoreActorWhenMoving(this, false);
			}
		}
	}
	IgnoringPawns.Reset();
}

void ADominationZoneWall::HandleDetectionBeginOverlap(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32,
	bool,
	const FHitResult&)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	// 폰의 루트(캡슐) 오버랩만 본다 - 메시 등 다른 컴포넌트의 중복 이벤트는 무시
	if (!bWallActive || !IsValid(Pawn) || OtherComp != Pawn->GetRootComponent() || !IsOutsideOfWall(Pawn))
	{
		return;
	}

	// 바깥에 있는 폰은 이 벽을 통과해 안으로 돌아올 수 있게 한다.
	SetIgnoredByPawn(Pawn, true);
}

void ADominationZoneWall::HandleDetectionEndOverlap(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32)
{
	APawn* Pawn = Cast<APawn>(OtherActor);
	if (!IsValid(Pawn) || OtherComp != Pawn->GetRootComponent())
	{
		return;
	}

	// 감지 영역을 벗어나면 안쪽으로 들어왔든 다시 바깥으로 나갔든 통과 허용을 해제해 다시 막는다.
	SetIgnoredByPawn(Pawn, false);
}
