#include "Training/CQB/CQBTrainingSpawner.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameMode/TrainingGameMode.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "../TrainingInteractionRegistration.h"
#include "Training/CQB/CQBTrainingEndActor.h"
#include "Training/CQB/CQBTrainingPlayerSpawnPoint.h"
#include "Training/TrainingPlayerComponent.h"
#include "Training/TrainingSessionSubsystem.h"
#include "Training/UI/TrainingSessionHUDWidget.h"

#define LOCTEXT_NAMESPACE "CQBTrainingSpawner"

ACQBTrainingSpawner::ACQBTrainingSpawner()
{
    PrimaryActorTick.bCanEverTick = false;

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
    ResetSessionState();
    TrainingInteractionRegistration::Cleanup(*this, LocalOverlappingController);
    Super::EndPlay(EndPlayReason);
}

bool ACQBTrainingSpawner::CanInteract(const APlayerController& RequestingPlayer) const
{
    const UWorld* World = GetWorld();
    if (bStopping || !World || !World->GetAuthGameMode<ATrainingGameMode>()
        || !RequestingPlayer.IsLocalPlayerController() || RequestingPlayer.GetWorld() != World
        || !RequestingPlayer.GetPawn())
    {
        return false;
    }

    const UTrainingPlayerComponent* Training =
        RequestingPlayer.FindComponentByClass<UTrainingPlayerComponent>();
    if (!Training)
    {
        return false;
    }

    if (SessionState == ECQBTrainingSessionState::Idle)
    {
        const UTrainingSessionSubsystem* Sessions = World->GetSubsystem<UTrainingSessionSubsystem>();
        return Sessions && Sessions->GetState() == ETrainingSessionState::Idle
            && !Training->IsCQBTrainingActive()
            && !TrooperClass.IsNull() && StartDialogWidgetClass != nullptr;
    }

    return SessionState == ECQBTrainingSessionState::Running
        && GetCancelDialogWidgetClass() != nullptr
        && Training->IsCQBTrainingActiveWith(*this);
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
        if (SessionState == ECQBTrainingSessionState::Running)
        {
            Training->OpenCQBCancelDialog(*this);
            return;
        }

        Training->OpenCQBStartDialog(*this);
    }
}

bool ACQBTrainingSpawner::StartTrainingFor(APlayerController& InParticipant, const int32 RequestId)
{
    UWorld* World = GetWorld();
    const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    UTrainingPlayerComponent* Training = InParticipant.FindComponentByClass<UTrainingPlayerComponent>();
    const UTrainingSessionSubsystem* Sessions = World ? World->GetSubsystem<UTrainingSessionSubsystem>() : nullptr;
    if (bStopping || IsActorBeingDestroyed()
        || !World || World->GetNetMode() != NM_Standalone || !World->GetAuthGameMode<ATrainingGameMode>()
        || !GameInstance || GameInstance->GetLocalPlayers().Num() != 1
        || !InParticipant.IsLocalPlayerController() || InParticipant.GetWorld() != World
        || !Training || Training->IsCQBTrainingActive()
        || !Sessions || Sessions->GetState() != ETrainingSessionState::Idle
        || !InParticipant.GetPawn()
        || SessionState != ECQBTrainingSessionState::Idle
        || TrooperClass.IsNull() || RequestId <= 0)
    {
        return false;
    }

    ++SessionGeneration;
    Participant = &InParticipant;
    PendingStartRequestId = RequestId;
    SessionState = ECQBTrainingSessionState::Loading;
    Training->NotifyCQBLoading(*this);

    if (IsValid(EndActor))
    {
        EndActor->Spawner = this;
        EndActor->SetEndEnabled(false);
    }

    BeginTraining();
    return true;
}

bool ACQBTrainingSpawner::CancelTraining()
{
    if (bStopping || SessionState == ECQBTrainingSessionState::Idle)
    {
        return false;
    }

    ResetSessionState();
    return true;
}

void ACQBTrainingSpawner::CancelPendingStart(const int32 RequestId)
{
    if (SessionState == ECQBTrainingSessionState::Loading && PendingStartRequestId == RequestId)
    {
        ResetSessionState();
    }
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
        FStreamableDelegate::CreateUObject(
            this, &ThisClass::HandleTrooperClassLoaded, PendingStartRequestId, SessionGeneration));

    if (!PendingLoadHandle.IsValid())
    {
        AbortPendingStart(LOCTEXT("TrooperLoadRequestFailed",
            "Could not request the CQB trooper class."));
    }
}

void ACQBTrainingSpawner::HandleTrooperClassLoaded(const int32 RequestId, const uint32 ExpectedGeneration)
{
    if (bStopping || SessionState != ECQBTrainingSessionState::Loading
        || PendingStartRequestId != RequestId || SessionGeneration != ExpectedGeneration)
    {
        return;
    }

    PendingLoadHandle.Reset();

    if (!Participant.IsValid() || !Participant->GetPawn())
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
    SessionTrooperClass = nullptr;
    OrderedSpawnPoints.Reset();
    CountedDeadTroopers.Reset();
    NextSpawnIndex = 0;
    KilledTroopers = 0;

    if (IsValid(EndActor))
    {
        EndActor->SetEndEnabled(false);
    }
}

void ACQBTrainingSpawner::ResetSessionState(const FText* StartFailure)
{
    if (bStopping)
    {
        return;
    }

    TGuardValue<bool> StopGuard(bStopping, true);
    ++SessionGeneration;
    SessionState = ECQBTrainingSessionState::Idle;
    ResetRuntimeData();
    if (!StartFailure)
    {
        PendingStartRequestId = 0;
    }

    NotifyParticipantStopped();
    if (StartFailure)
    {
        ResolvePendingStart(false, *StartFailure);
    }
    Participant.Reset();
}

void ACQBTrainingSpawner::DespawnSpawnedTroopers()
{
    if (SessionState == ECQBTrainingSessionState::Loading)
    {
        AbortPendingStart(LOCTEXT("StartCancelled", "CQB start was cancelled."));
        return;
    }

    ResetSessionState();
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

    if (APlayerController* Player = Participant.Get())
    {
        if (UTrainingPlayerComponent* Training = Player->FindComponentByClass<UTrainingPlayerComponent>())
        {
            Training->NotifyCQBStartRequestResolved(*this, RequestId, bSuccess, Failure);
        }
    }
}

void ACQBTrainingSpawner::AbortPendingStart(const FText& Failure)
{
    if (bStopping || SessionState != ECQBTrainingSessionState::Loading)
    {
        return;
    }

    ResetSessionState(&Failure);
}

void ACQBTrainingSpawner::NotifyParticipantStarted()
{
    if (APlayerController* Player = Participant.Get())
    {
        if (UTrainingPlayerComponent* Training = Player->FindComponentByClass<UTrainingPlayerComponent>())
        {
            Training->NotifyCQBStarted(*this, OrderedSpawnPoints.Num(), HUDWidgetClass);
        }
    }
}

void ACQBTrainingSpawner::NotifyParticipantProgress()
{
    if (APlayerController* Player = Participant.Get())
    {
        if (UTrainingPlayerComponent* Training = Player->FindComponentByClass<UTrainingPlayerComponent>())
        {
            Training->NotifyCQBProgress(KilledTroopers, OrderedSpawnPoints.Num(), IsTrainingComplete());
        }
    }
}

void ACQBTrainingSpawner::NotifyParticipantStopped()
{
    if (APlayerController* Player = Participant.Get())
    {
        if (UTrainingPlayerComponent* Training = Player->FindComponentByClass<UTrainingPlayerComponent>())
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
