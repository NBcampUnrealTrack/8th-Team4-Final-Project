#include "Training/TrainingPlayerComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Input/ShooterInteractionComponent.h"
#include "Training/TrainingAreaTransition.h"
#include "Training/TrainingArena.h"
#include "Training/TrainingInteractionActor.h"
#include "Training/TrainingSessionSubsystem.h"
#include "Training/CQB/CQBTrainingEndActor.h"
#include "Training/CQB/CQBTrainingPlayerSpawnPoint.h"
#include "Training/CQB/CQBTrainingSpawner.h"

#define LOCTEXT_NAMESPACE "TrainingPlayer"

bool UTrainingPlayerComponent::IsAreaTransitionBlockingInput() const
{
    return AreaTransition && AreaTransition->IsBlockingInput();
}

bool UTrainingPlayerComponent::CanReachPendingTrainingActor(const AActor* Candidate) const
{
    const UShooterInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UShooterInteractionComponent>();
    if (!Interaction) return false;
    return AreaTransition && AreaTransition->IsCommittingMove()
        ? Interaction->CanReachAcceptedInteractable(Candidate)
        : Interaction->CanReachInteractable(Candidate);
}

void UTrainingPlayerComponent::CancelAreaTransition()
{
    if (AreaTransition) AreaTransition->Cancel();
}

void UTrainingPlayerComponent::BeginAreaTransition(const FName AreaId,
    FTrainingAreaMove Move, FTrainingAreaMoveResolved Resolved)
{
    // 진행 중인 요청은 교체하거나 중복 실행하지 않습니다.
    if (IsAreaTransitionBlockingInput())
    {
        Resolved.ExecuteIfBound(false, LOCTEXT("TransitionBusy", "Please wait for the current area transition to finish."));
        return;
    }
    APlayerController* Participant = GetParticipantController();
    if (Participant && !AreaTransition)
    {
        AreaTransition = NewObject<UTrainingAreaTransition>(this);
    }
    if (!Participant || !AreaTransition || !AreaTransition->Begin(*Participant, AreaId, Move, Resolved))
    {
        Resolved.ExecuteIfBound(false, LOCTEXT("TransitionUnavailable", "Could not start the training area transition."));
    }
}

bool UTrainingPlayerComponent::TeleportParticipant(
    const FTransform& Destination, const FRotator& TargetControlRotation)
{
    APlayerController* Participant = GetParticipantController();
    APawn* Pawn = Participant ? Participant->GetPawn() : nullptr;
    if (!Pawn || !Pawn->TeleportTo(Destination.GetLocation(), Destination.Rotator())) return false;

    if (UPawnMovementComponent* Movement = Pawn->GetMovementComponent())
    {
        Movement->StopMovementImmediately();
    }
    Participant->SetControlRotation(TargetControlRotation);
    if (Participant->PlayerCameraManager)
    {
        Participant->PlayerCameraManager->SetGameCameraCutThisFrame();
    }
    return true;
}

void UTrainingPlayerComponent::ClearSavedTrainingPosition()
{
    ReturnArena.Reset();
    ReturnCQBEntryPoint.Reset();
    PreTrainingTransform = FTransform::Identity;
    PreTrainingControlRotation = FRotator::ZeroRotator;
    bHasPreTrainingTransform = false;
    PreTrainingAreaId = NAME_None;
}

bool UTrainingPlayerComponent::TeleportToSavedPreTrainingPosition(FText& OutFailure)
{
    if (!bHasPreTrainingTransform)
    {
        OutFailure = LOCTEXT("CQBOriginMissing", "No saved pre-training position is available.");
        return false;
    }
    if (!TeleportParticipant(PreTrainingTransform, PreTrainingControlRotation))
    {
        OutFailure = LOCTEXT("CQBOriginBlocked", "The saved return position is blocked.");
        return false;
    }
    ClearSavedTrainingPosition();
    return true;
}

bool UTrainingPlayerComponent::ResolveTrainingDestination(const int32 RequestId,
    const ETrainingDestination Destination, FTransform& OutTransform, FName& OutAreaId, FText& OutFailure) const
{
    if (!ValidatePendingRequest(RequestId, OutFailure) || PendingKind != ETrainingDialogKind::EnterArena)
    {
        if (OutFailure.IsEmpty()) OutFailure = LOCTEXT("DestinationExpired", "Training destination request expired.");
        return false;
    }
    const APlayerController* Participant = GetParticipantController();
    const UTrainingSessionSubsystem* Sessions = GetWorld()->GetSubsystem<UTrainingSessionSubsystem>();
    if (!Participant || !Participant->GetPawn() || !Sessions
        || Sessions->GetState() != ETrainingSessionState::Idle || IsCQBTrainingActive())
    {
        OutFailure = LOCTEXT("DestinationBusy", "Training is currently unavailable.");
        return false;
    }
    if (Destination == ETrainingDestination::Arena)
    {
        const ATrainingArena* Arena = PendingInteractable->Arena;
        if (!IsValid(Arena))
        {
            OutFailure = LOCTEXT("ArenaDestinationMissing", "Training Arena is not assigned to the entrance actor.");
            return false;
        }
        OutTransform = Arena->GetEntryTransform();
        OutAreaId = Arena->AreaId;
    }
    else if (Destination == ETrainingDestination::CQB)
    {
        const ACQBTrainingPlayerSpawnPoint* Entry = PendingInteractable->CQBEntryPoint;
        if (!IsValid(Entry))
        {
            OutFailure = LOCTEXT("CQBDestinationMissing", "CQB Entry Point is not assigned to the entrance actor.");
            return false;
        }
        OutTransform = Entry->GetActorTransform();
        OutAreaId = Entry->AreaId;
    }
    else
    {
        OutFailure = LOCTEXT("UnknownDestination", "The selected training destination is not supported.");
        return false;
    }
    return true;
}

void UTrainingPlayerComponent::SubmitTrainingDestination(const int32 RequestId, const ETrainingDestination Destination)
{
    FText Failure;
    FTransform Target;
    FName AreaId;
    if (!ResolveTrainingDestination(RequestId, Destination, Target, AreaId, Failure))
    {
        ResolveDialogRequest(RequestId, false, Failure);
        return;
    }

    BeginAreaTransition(AreaId,
        FTrainingAreaMove::CreateWeakLambda(this, [this, RequestId, Destination](FText& MoveFailure)
        {
            // 페이드가 끝난 시점의 요청과 목적지를 다시 확인합니다.
            FTransform ResolvedTarget;
            FName ResolvedAreaId;
            if (!ResolveTrainingDestination(RequestId, Destination, ResolvedTarget, ResolvedAreaId, MoveFailure)) return false;
            APlayerController* Participant = GetParticipantController();
            const FTransform PreviousTransform = Participant->GetPawn()->GetActorTransform();
            const FRotator PreviousRotation = Participant->GetControlRotation();
            const FName PreviousArea = PendingInteractable->OriginAreaId;
            const TWeakObjectPtr<ATrainingArena> Arena = PendingInteractable->Arena.Get();
            const TWeakObjectPtr<ACQBTrainingPlayerSpawnPoint> Entry = PendingInteractable->CQBEntryPoint.Get();
            if (!TeleportParticipant(ResolvedTarget, ResolvedTarget.Rotator()))
            {
                MoveFailure = LOCTEXT("DestinationBlocked", "The selected training entry point is blocked.");
                return false;
            }
            PreTrainingTransform = PreviousTransform;
            PreTrainingControlRotation = PreviousRotation;
            PreTrainingAreaId = PreviousArea;
            bHasPreTrainingTransform = true;
            ReturnArena = Destination == ETrainingDestination::Arena ? Arena : TWeakObjectPtr<ATrainingArena>();
            ReturnCQBEntryPoint = Destination == ETrainingDestination::CQB ? Entry : TWeakObjectPtr<ACQBTrainingPlayerSpawnPoint>();
            ClearPendingRequest();
            return true;
        }),
        FTrainingAreaMoveResolved::CreateWeakLambda(this, [this, RequestId](const bool bSuccess, const FText& MoveFailure)
        {
            ResolveDialogRequest(RequestId, bSuccess, MoveFailure);
        }));
}

bool UTrainingPlayerComponent::ValidateArenaReturnRequest(const int32 RequestId, FText& OutFailure) const
{
    if (!ValidatePendingRequest(RequestId, OutFailure)) return false;
    const ATrainingArena* Arena = PendingInteractable->Arena;
    const UTrainingSessionSubsystem* Sessions = GetWorld()->GetSubsystem<UTrainingSessionSubsystem>();
    if (PendingKind != ETrainingDialogKind::SelectMode)
    {
        OutFailure = LOCTEXT("ReturnStartWrongDialog", "Return to start is only available from the training mode selection screen.");
    }
    else if (!GetParticipantController() || !IsValid(Arena) || !Sessions)
    {
        OutFailure = LOCTEXT("ReturnStartUnavailable", "The training return position is no longer available.");
    }
    else if (Sessions->GetState() != ETrainingSessionState::Idle)
    {
        OutFailure = LOCTEXT("ReturnStartSessionActive", "End the active training session before returning to the start position.");
    }
    else if (!bHasPreTrainingTransform || ReturnArena.Get() != Arena)
    {
        OutFailure = LOCTEXT("ReturnStartMissing", "No saved pre-training position is available. Leave and enter the Training Arena again.");
    }
    return OutFailure.IsEmpty();
}

void UTrainingPlayerComponent::ReturnToPreTrainingPosition(const int32 RequestId)
{
    FText Failure;
    if (!ValidateArenaReturnRequest(RequestId, Failure))
    {
        ResolveDialogRequest(RequestId, false, Failure);
        return;
    }
    BeginAreaTransition(PreTrainingAreaId,
        FTrainingAreaMove::CreateWeakLambda(this, [this, RequestId](FText& MoveFailure)
        {
            if (!ValidateArenaReturnRequest(RequestId, MoveFailure)
                || !TeleportToSavedPreTrainingPosition(MoveFailure)) return false;
            ClearPendingRequest();
            return true;
        }),
        FTrainingAreaMoveResolved::CreateWeakLambda(this, [this, RequestId](const bool bSuccess, const FText& MoveFailure)
        {
            ResolveDialogRequest(RequestId, bSuccess, MoveFailure);
        }));
}

bool UTrainingPlayerComponent::ValidateCQBMoveRequest(
    const int32 RequestId, const bool bRequireComplete, FText& OutFailure) const
{
    if (!ValidatePendingCQBRequest(RequestId, OutFailure)) return false;
    if (!PendingCQBSpawner.IsValid()
        || (bRequireComplete && (!PendingCQBEndActor.IsValid() || !PendingCQBSpawner->IsTrainingComplete()))
        || (!bRequireComplete && (PendingCQBEndActor.IsValid() || IsCQBTrainingActive())))
    {
        OutFailure = LOCTEXT("CQBMoveUnavailable", "Return is only available before CQB starts or after it is complete.");
        return false;
    }
    return true;
}

void UTrainingPlayerComponent::SubmitCQBReturnToOrigin(const int32 RequestId)
{
    BeginCQBReturnToSavedPosition(RequestId, false);
}

void UTrainingPlayerComponent::SubmitCQBEndToOrigin(const int32 RequestId)
{
    BeginCQBReturnToSavedPosition(RequestId, true);
}

void UTrainingPlayerComponent::BeginCQBReturnToSavedPosition(const int32 RequestId, const bool bRequireComplete)
{
    FText Failure;
    if (!ValidateCQBMoveRequest(RequestId, bRequireComplete, Failure) || !bHasPreTrainingTransform)
    {
        ResolveCQBRequest(RequestId, false, Failure.IsEmpty()
            ? LOCTEXT("CQBOriginMissing", "No saved pre-training position is available.") : Failure);
        return;
    }
    BeginAreaTransition(PreTrainingAreaId,
        FTrainingAreaMove::CreateWeakLambda(this, [this, RequestId, bRequireComplete](FText& MoveFailure)
        {
            if (!ValidateCQBMoveRequest(RequestId, bRequireComplete, MoveFailure)) return false;
            const TWeakObjectPtr<ACQBTrainingSpawner> Spawner = PendingCQBSpawner;
            if (!TeleportToSavedPreTrainingPosition(MoveFailure)) return false;
            if (bRequireComplete && Spawner.IsValid()) Spawner->FinishTrainingAndReset();
            ClearPendingCQBRequest();
            return true;
        }),
        FTrainingAreaMoveResolved::CreateWeakLambda(this, [this, RequestId](const bool bSuccess, const FText& MoveFailure)
        {
            ResolveCQBRequest(RequestId, bSuccess, MoveFailure);
        }));
}

ACQBTrainingPlayerSpawnPoint* UTrainingPlayerComponent::ResolveCQBReturnStart(const int32 RequestId, FText& OutFailure) const
{
    if (!ValidateCQBMoveRequest(RequestId, true, OutFailure)) return nullptr;
    ACQBTrainingPlayerSpawnPoint* Start = PendingCQBSpawner->GetPlayerStartPoint();
    if (!IsValid(Start)) Start = ReturnCQBEntryPoint.Get();
    if (!IsValid(Start))
    {
        OutFailure = LOCTEXT("CQBStartPointMissing", "Assign a CQB Training Player Spawn Point to the spawner.");
        return nullptr;
    }
    return Start;
}

void UTrainingPlayerComponent::SubmitCQBEndToStart(const int32 RequestId)
{
    FText Failure;
    const ACQBTrainingPlayerSpawnPoint* Start = ResolveCQBReturnStart(RequestId, Failure);
    if (!Start)
    {
        ResolveCQBRequest(RequestId, false, Failure);
        return;
    }
    BeginAreaTransition(Start->AreaId,
        FTrainingAreaMove::CreateWeakLambda(this, [this, RequestId](FText& MoveFailure)
        {
            const ACQBTrainingPlayerSpawnPoint* CurrentStart = ResolveCQBReturnStart(RequestId, MoveFailure);
            if (!CurrentStart) return false;
            const TWeakObjectPtr<ACQBTrainingSpawner> Spawner = PendingCQBSpawner;
            const FTransform Destination = CurrentStart->GetActorTransform();
            if (!TeleportParticipant(Destination, Destination.Rotator()))
            {
                MoveFailure = LOCTEXT("CQBStartBlocked", "The CQB start point is blocked.");
                return false;
            }
            if (Spawner.IsValid()) Spawner->FinishTrainingAndReset();
            ClearPendingCQBRequest();
            return true;
        }),
        FTrainingAreaMoveResolved::CreateWeakLambda(this, [this, RequestId](const bool bSuccess, const FText& MoveFailure)
        {
            ResolveCQBRequest(RequestId, bSuccess, MoveFailure);
        }));
}

#undef LOCTEXT_NAMESPACE
