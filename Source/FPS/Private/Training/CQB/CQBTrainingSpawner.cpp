#include "Training/CQB/CQBTrainingSpawner.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "../TrainingInteractionRegistration.h"
#include "Training/CQB/CQBTrainingAIComponent.h"
#include "Training/CQB/CQBTrainingAIController.h"
#include "Training/CQB/CQBTrainingEndActor.h"
#include "Training/CQB/CQBTrainingPlayerSpawnPoint.h"
#include "Training/CQB/CQBTrainingPatrolPoint.h"
#include "Training/CQB/CQBTrainingSpawnPoint.h"
#include "Training/TrainingPlayerComponent.h"

#define LOCTEXT_NAMESPACE "CQBTrainingSpawner"

DEFINE_LOG_CATEGORY_STATIC(LogCQBTrainingSpawner, Log, All);

ACQBTrainingSpawner::ACQBTrainingSpawner()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;

    InteractionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionCollision"));
    SetRootComponent(InteractionCollision);
    TrainingInteractionRegistration::ConfigureCollision(*InteractionCollision, FVector(45.0, 45.0, 60.0));
    InteractionCollision->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleInteractionBeginOverlap);
    InteractionCollision->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleInteractionEndOverlap);

    DisplayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"));
    TrainingInteractionRegistration::ConfigureDisplayMesh(*DisplayMesh, *InteractionCollision);
}

void ACQBTrainingSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (PendingLoadHandle.IsValid())
    {
        PendingLoadHandle->CancelHandle();
        PendingLoadHandle.Reset();
    }

    TrainingInteractionRegistration::Cleanup(*this, LocalOverlappingController);
    Super::EndPlay(EndPlayReason);
}

void ACQBTrainingSpawner::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ThisClass, SessionState);
}

bool ACQBTrainingSpawner::CanInteract(const APlayerController& RequestingPlayer) const
{
    return SessionState == ECQBTrainingSessionState::Idle
        && !TrooperClass.IsNull()
        && RequestingPlayer.GetPawn() != nullptr
        && RequestingPlayer.FindComponentByClass<UTrainingPlayerComponent>() != nullptr;
}

void ACQBTrainingSpawner::Interact(APlayerController& RequestingPlayer)
{
    if (!CanInteract(RequestingPlayer))
    {
        return;
    }

    if (InteractionSound)
    {
        UGameplayStatics::PlaySoundAtLocation(
            this, InteractionSound, GetActorLocation(), InteractionSoundVolume);
    }

    if (UTrainingPlayerComponent* Training = RequestingPlayer.FindComponentByClass<UTrainingPlayerComponent>())
    {
        Training->OpenCQBStartDialog(*this);
    }
}

bool ACQBTrainingSpawner::StartTrainingFor(APlayerController& Participant, const int32 RequestId)
{
    if (!HasAuthority()
        || SessionState != ECQBTrainingSessionState::Idle
        || TrooperClass.IsNull()
        || RequestId <= 0)
    {
        return false;
    }

    ActiveParticipant = &Participant;
    PendingStartRequestId = RequestId;
    SessionState = ECQBTrainingSessionState::Loading;
    ForceNetUpdate();

    if (EndActor)
    {
        EndActor->Spawner = this;
        EndActor->SetEndEnabled(false);
    }

    BeginTraining();
    return true;
}

void ACQBTrainingSpawner::CancelPendingStart(APlayerController& Participant, const int32 RequestId)
{
    if (!HasAuthority()
        || SessionState != ECQBTrainingSessionState::Loading
        || ActiveParticipant.Get() != &Participant
        || PendingStartRequestId != RequestId)
    {
        return;
    }

    ResetRuntimeData();
    PendingStartRequestId = 0;
    ActiveParticipant.Reset();
    SessionState = ECQBTrainingSessionState::Idle;
    ForceNetUpdate();
}

void ACQBTrainingSpawner::BeginTraining()
{
    if (SessionState != ECQBTrainingSessionState::Loading || TrooperClass.IsNull())
    {
        return;
    }

    if (!IsValid(PlayerStartPoint))
    {
        AbortPendingStart(LOCTEXT("PlayerStartMissing",
            "Assign a CQB Training Player Spawn Point to the CQB spawner."));
        return;
    }
    if (!IsValid(EndActor) || !EndActor->EndDialogWidgetClass)
    {
        AbortPendingStart(LOCTEXT("EndFlowMissing",
            "Assign the CQB end actor and its End Dialog Widget Class before starting."));
        return;
    }
    if (!HUDWidgetClass)
    {
        AbortPendingStart(LOCTEXT("HUDMissing",
            "Assign the CQB HUD Widget Class before starting."));
        return;
    }

    if (!CollectSpawnPoints())
    {
        AbortPendingStart(LOCTEXT("InvalidSpawnLayout",
            "CQB spawn layout is invalid. Check SpawnOrder and Patrol Points."));
        return;
    }

    if (UClass* LoadedClass = TrooperClass.Get())
    {
        StartLoadedTraining(*LoadedClass);
        return;
    }

    PendingLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
        TrooperClass.ToSoftObjectPath(),
        FStreamableDelegate::CreateUObject(this, &ThisClass::HandleTrooperClassLoaded));

    if (!PendingLoadHandle.IsValid())
    {
        AbortPendingStart(LOCTEXT("TrooperLoadRequestFailed",
            "Could not request the CQB trooper class."));
    }
}

void ACQBTrainingSpawner::HandleTrooperClassLoaded()
{
    PendingLoadHandle.Reset();

    if (SessionState != ECQBTrainingSessionState::Loading)
    {
        return;
    }

    if (!ActiveParticipant.IsValid())
    {
        AbortPendingStart(LOCTEXT("ParticipantLost", "The CQB participant is no longer available."));
        return;
    }

    UClass* LoadedClass = TrooperClass.Get();
    if (!LoadedClass)
    {
        AbortPendingStart(LOCTEXT("TrooperLoadFailed", "The CQB trooper class failed to load."));
        return;
    }

    StartLoadedTraining(*LoadedClass);
}

void ACQBTrainingSpawner::ResetRuntimeData()
{
    if (PendingLoadHandle.IsValid())
    {
        PendingLoadHandle->CancelHandle();
        PendingLoadHandle.Reset();
    }

    DestroySpawnedTroopersOnly();
    OrderedSpawnPoints.Reset();
    CountedDeathIndices.Reset();
    NextSpawnIndex = 0;
    KilledTroopers = 0;

    if (EndActor)
    {
        EndActor->SetEndEnabled(false);
    }
}

void ACQBTrainingSpawner::ResetSessionState(const bool bNotifyParticipant)
{
    ResetRuntimeData();
    PendingStartRequestId = 0;
    SessionState = ECQBTrainingSessionState::Idle;

    if (bNotifyParticipant)
    {
        NotifyParticipantStopped();
    }
    ActiveParticipant.Reset();
    ForceNetUpdate();
}

void ACQBTrainingSpawner::DespawnSpawnedTroopers()
{
    if (!HasAuthority())
    {
        return;
    }

    if (SessionState == ECQBTrainingSessionState::Loading)
    {
        AbortPendingStart(LOCTEXT("StartCancelled", "CQB start was cancelled."));
        return;
    }

    ResetSessionState(SessionState == ECQBTrainingSessionState::Running
        || SessionState == ECQBTrainingSessionState::Completed);
}

void ACQBTrainingSpawner::FinishTrainingAndReset()
{
    DespawnSpawnedTroopers();
}

void ACQBTrainingSpawner::ResolvePendingStart(const bool bSuccess, const FText& Failure)
{
    const int32 RequestId = PendingStartRequestId;
    PendingStartRequestId = 0;

    if (RequestId <= 0)
    {
        return;
    }

    if (APlayerController* Participant = ActiveParticipant.Get())
    {
        if (UTrainingPlayerComponent* Training = Participant->FindComponentByClass<UTrainingPlayerComponent>())
        {
            Training->NotifyCQBStartRequestResolved(*this, RequestId, bSuccess, Failure);
        }
    }
}

void ACQBTrainingSpawner::AbortPendingStart(const FText& Failure)
{
    if (SessionState != ECQBTrainingSessionState::Loading)
    {
        return;
    }

    ResetRuntimeData();
    SessionState = ECQBTrainingSessionState::Idle;
    ResolvePendingStart(false, Failure);
    ActiveParticipant.Reset();
    ForceNetUpdate();
}

void ACQBTrainingSpawner::NotifyParticipantStarted()
{
    if (APlayerController* Participant = ActiveParticipant.Get())
    {
        if (UTrainingPlayerComponent* Training = Participant->FindComponentByClass<UTrainingPlayerComponent>())
        {
            Training->NotifyCQBStarted(*this, OrderedSpawnPoints.Num(), HUDWidgetClass);
        }
    }
}

void ACQBTrainingSpawner::NotifyParticipantProgress()
{
    if (APlayerController* Participant = ActiveParticipant.Get())
    {
        if (UTrainingPlayerComponent* Training = Participant->FindComponentByClass<UTrainingPlayerComponent>())
        {
            Training->NotifyCQBProgress(KilledTroopers, OrderedSpawnPoints.Num(), IsTrainingComplete());
        }
    }
}

void ACQBTrainingSpawner::NotifyParticipantStopped()
{
    if (APlayerController* Participant = ActiveParticipant.Get())
    {
        if (UTrainingPlayerComponent* Training = Participant->FindComponentByClass<UTrainingPlayerComponent>())
        {
            Training->NotifyCQBStopped(*this);
        }
    }
}

void ACQBTrainingSpawner::HandleInteractionBeginOverlap(
    UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    TrainingInteractionRegistration::Register(*this, OtherActor, LocalOverlappingController);
}

void ACQBTrainingSpawner::HandleInteractionEndOverlap(
    UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
    TrainingInteractionRegistration::Unregister(*this, OtherActor, LocalOverlappingController);
}

#undef LOCTEXT_NAMESPACE
