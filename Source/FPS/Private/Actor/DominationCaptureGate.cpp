#include "Actor/DominationCaptureGate.h"

#include "Components/StaticMeshComponent.h"
#include "GameMode/State/DominationGameState.h"

ADominationCaptureGate::ADominationCaptureGate()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	SetRootComponent(DoorMesh);
}

void ADominationCaptureGate::BeginPlay()
{
	Super::BeginPlay();

	BoundGameState = GetWorld() ? GetWorld()->GetGameState<ADominationGameState>() : nullptr;
	if (IsValid(BoundGameState))
	{
		BoundGameState->OnActivePointChanged.AddDynamic(this, &ThisClass::HandleActivePointChanged);
		BoundGameState->OnCaptureStateChanged.AddDynamic(this, &ThisClass::HandleCaptureStateChanged);
	}

	RefreshGateState();
}

void ADominationCaptureGate::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (IsValid(BoundGameState))
	{
		BoundGameState->OnActivePointChanged.RemoveDynamic(this, &ThisClass::HandleActivePointChanged);
		BoundGameState->OnCaptureStateChanged.RemoveDynamic(this, &ThisClass::HandleCaptureStateChanged);
	}

	Super::EndPlay(EndPlayReason);
}

void ADominationCaptureGate::HandleActivePointChanged(EDominationPointId)
{
	RefreshGateState();
}

void ADominationCaptureGate::HandleCaptureStateChanged(FDominationCaptureState)
{
	RefreshGateState();
}

void ADominationCaptureGate::RefreshGateState()
{
	if (bIsOpen)
	{
		return;
	}

	const bool bIsActivePoint = IsValid(BoundGameState) && BoundGameState->GetActivePointId() == PointId;
	const bool bShouldBeOpen = bIsActivePoint && BoundGameState->GetCaptureProgress() >= OpenThreshold;
	if (!bShouldBeOpen)
	{
		return;
	}

	bIsOpen = true;
	OnGateOpenStateChanged(true);
}
