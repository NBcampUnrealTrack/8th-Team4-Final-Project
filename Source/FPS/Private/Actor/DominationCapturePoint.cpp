#include "Actor/DominationCapturePoint.h"

#include "Actor/DominationPointGimmick.h"
#include "Components/BoxComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/State/DominationGameState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Player/BaseTeamPlayerState.h"

ADominationCapturePoint::ADominationCapturePoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CaptureZone = CreateDefaultSubobject<UBoxComponent>(TEXT("CaptureZone"));
	CaptureZone->SetupAttachment(SceneRoot);
	CaptureZone->SetBoxExtent(CaptureBoxExtent);
	CaptureZone->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CaptureZone->SetCollisionObjectType(ECC_WorldDynamic);
	CaptureZone->SetCollisionResponseToAllChannels(ECR_Ignore);
	CaptureZone->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	CaptureZone->SetGenerateOverlapEvents(false);
	CaptureZone->SetCanEverAffectNavigation(false);
	CaptureZone->OnComponentBeginOverlap.AddDynamic(
		this,
		&ThisClass::HandleCaptureZoneBeginOverlap);
	CaptureZone->OnComponentEndOverlap.AddDynamic(
		this,
		&ThisClass::HandleCaptureZoneEndOverlap);
	
	BorderDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("BorderDecal"));
	BorderDecal->SetupAttachment(SceneRoot);
	BorderDecal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	BorderDecal->DecalSize = FVector(BorderDecalProjectionDepth, CaptureBoxExtent.Y, CaptureBoxExtent.X);
	BorderDecal->SetVisibility(false);
}

void ADominationCapturePoint::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	CaptureZone->SetBoxExtent(CaptureBoxExtent);
	PlaceBorderDecalOnGround();
	BorderDecal->SetVisibility(true);
	
	if (IsValid(BorderDecalMaterial))
	{
		BorderDecal->SetDecalMaterial(BorderDecalMaterial);
	}
}

void ADominationCapturePoint::BeginPlay()
{
	Super::BeginPlay();

	PlaceBorderDecalOnGround();

	if (IsValid(BorderDecalMaterial))
	{
		BorderDecalMID = UMaterialInstanceDynamic::Create(BorderDecalMaterial, this);
		BorderDecal->SetDecalMaterial(BorderDecalMID);
		BorderDecalMID->SetScalarParameterValue(TEXT("BorderThickness"), BorderThickness);
	}

	if (ADominationGameState* DominationGameState = GetWorld() ? GetWorld()->GetGameState<ADominationGameState>() : nullptr)
	{
		DominationGameState->OnActivePointChanged.AddDynamic(this, &ThisClass::HandleActivePointChanged);
		DominationGameState->OnCaptureStateChanged.AddDynamic(this, &ThisClass::HandleCaptureStateChanged);
	}

	UpdateBorderColor();
}

void ADominationCapturePoint::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ADominationGameState* DominationGameState = GetWorld() ? GetWorld()->GetGameState<ADominationGameState>() : nullptr)
	{
		DominationGameState->OnActivePointChanged.RemoveDynamic(this, &ThisClass::HandleActivePointChanged);
		DominationGameState->OnCaptureStateChanged.RemoveDynamic(this, &ThisClass::HandleCaptureStateChanged);
	}

	Super::EndPlay(EndPlayReason);
}

EDominationPointId ADominationCapturePoint::GetPointId() const
{
	return PointId;
}

FVector ADominationCapturePoint::GetCaptureBoxExtent() const
{
	return CaptureBoxExtent;
}

bool ADominationCapturePoint::ContainsWorldLocation(const FVector& WorldLocation) const
{
	const FVector LocalLocation = CaptureZone->GetComponentTransform().InverseTransformPosition(WorldLocation);
	return FMath::Abs(LocalLocation.X) <= CaptureBoxExtent.X
		&& FMath::Abs(LocalLocation.Y) <= CaptureBoxExtent.Y
		&& FMath::Abs(LocalLocation.Z) <= CaptureBoxExtent.Z;
}

void ADominationCapturePoint::GetOverlappingPawns(TArray<APawn*>& OutPawns) const
{
	OutPawns.Reset();

	TArray<AActor*> OverlappingActors;
	CaptureZone->GetOverlappingActors(OverlappingActors, APawn::StaticClass());
	for (AActor* OverlappingActor : OverlappingActors)
	{
		if (APawn* Pawn = Cast<APawn>(OverlappingActor))
		{
			OutPawns.Add(Pawn);
		}
	}
}

void ADominationCapturePoint::SetObjectiveActive(const bool bNewActive)
{
	bObjectiveActive = bNewActive;
	CaptureZone->SetGenerateOverlapEvents(bObjectiveActive);
	CaptureZone->SetCollisionEnabled(
		bObjectiveActive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);

	// 거점 활성 상태에 맞춰 기믹을 켜고 끈다(서버 전용 경로).
	if (IsValid(Gimmick))
	{
		Gimmick->SetGimmickActive(bObjectiveActive, this);
	}
}

void ADominationCapturePoint::HandleCaptureZoneBeginOverlap(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32,
	bool,
	const FHitResult&)
{
	if (APawn* Pawn = Cast<APawn>(OtherActor))
	{
		OnPawnEntered.Broadcast(Pawn);
	}
}

void ADominationCapturePoint::HandleCaptureZoneEndOverlap(
	UPrimitiveComponent*,
	AActor* OtherActor,
	UPrimitiveComponent*,
	int32)
{
	if (APawn* Pawn = Cast<APawn>(OtherActor))
	{
		OnPawnExited.Broadcast(Pawn);
	}
}

void ADominationCapturePoint::HandleActivePointChanged(EDominationPointId)
{
	UpdateBorderColor();
}

void ADominationCapturePoint::HandleCaptureStateChanged(FDominationCaptureState)
{
	UpdateBorderColor();
}

void ADominationCapturePoint::PlaceBorderDecalOnGround()
{
	if (!IsValid(BorderDecal))
	{
		return;
	}
	
	const FVector ScaledExtent = CaptureZone->GetScaledBoxExtent();
	BorderDecal->DecalSize = FVector(BorderDecalProjectionDepth, CaptureBoxExtent.Y, CaptureBoxExtent.X);

	const FVector Origin = GetActorLocation();
	const float TraceHalfRange = ScaledExtent.Z + 500.0f;
	const FVector TraceStart = Origin + FVector(0.0f, 0.0f, TraceHalfRange);
	const FVector TraceEnd = Origin - FVector(0.0f, 0.0f, TraceHalfRange);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DominationCapturePointGroundTrace), false, this);

	FHitResult HitResult;
	if (GetWorld() && GetWorld()->LineTraceSingleByChannel(HitResult, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
	{
		// 살짝 띄워서 바닥과의 Z-파이팅을 피한다.
		BorderDecal->SetWorldLocation(HitResult.Location + FVector(0.0f, 0.0f, 2.0f));
	}
	else
	{
		BorderDecal->SetRelativeLocation(FVector(0.0f, 0.0f, -CaptureBoxExtent.Z));
	}
}

void ADominationCapturePoint::UpdateBorderColor()
{
	if (!IsValid(BorderDecalMID))
	{
		return;
	}

	const ADominationGameState* DominationGameState = GetWorld() ? GetWorld()->GetGameState<ADominationGameState>() : nullptr;
	const bool bIsActivePoint = IsValid(DominationGameState) && DominationGameState->GetActivePointId() == PointId;

	// 활성 거점이 아니면 비활성
	BorderDecal->SetVisibility(bIsActivePoint);
	if (!bIsActivePoint)
	{
		return;
	}

	const EGameTeam CapturingTeam = DominationGameState->GetCapturingTeam();

	const APlayerController* LocalController = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	const ABaseTeamPlayerState* LocalPlayerState = IsValid(LocalController)
		? LocalController->GetPlayerState<ABaseTeamPlayerState>()
		: nullptr;
	const EGameTeam LocalTeam = IsValid(LocalPlayerState) ? LocalPlayerState->GetTeam() : EGameTeam::None;

	const EDominationCaptureRelation Relation = DominationRules::GetCaptureRelation(CapturingTeam, LocalTeam);

	FLinearColor BorderColor = FLinearColor::White;
	switch (Relation)
	{
	case EDominationCaptureRelation::Ally:
		BorderColor = FLinearColor::Green;
		break;
	case EDominationCaptureRelation::Enemy:
		BorderColor = FLinearColor::Red;
		break;
	case EDominationCaptureRelation::Neutral:
	default:
		BorderColor = FLinearColor::White;
		break;
	}

	BorderDecalMID->SetVectorParameterValue(TEXT("BorderColor"), BorderColor);
}
