#include "Training/TrainingPlayerComponent.h"

#include "GameFramework/PlayerController.h"
#include "Input/ShooterInteractionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Player/TrainingPlayerController.h"
#include "Tags/UITags.h"
#include "Training/CQB/CQBTrainingEndActor.h"
#include "Training/CQB/CQBTrainingPlayerSpawnPoint.h"
#include "Training/CQB/CQBTrainingSpawner.h"
#include "Training/TrainingSettings.h"
#include "Training/UI/CQBTrainingDialogWidget.h"
#include "Training/UI/TrainingPlayerPresentation.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"

#define LOCTEXT_NAMESPACE "TrainingPlayer"

bool UTrainingPlayerComponent::UsesStandardEliminationFeedback() const
{
    return !ActiveCQBSpawner.IsValid() || ActiveCQBSpawner->UsesStandardEliminationFeedback();
}

bool UTrainingPlayerComponent::IsCQBTrainingActive() const
{
    return ActiveCQBSpawner.IsValid() && ActiveCQBSpawner->IsTrainingActive();
}

bool UTrainingPlayerComponent::IsCQBTrainingActiveWith(const ACQBTrainingSpawner& Spawner) const
{
    return ActiveCQBSpawner.Get() == &Spawner;
}

bool UTrainingPlayerComponent::TryGetCQBRespawnTransform(FTransform& OutTransform) const
{
    if (!ActiveCQBSpawner.IsValid()
        || (!ActiveCQBSpawner->IsTrainingRunning() && !ActiveCQBSpawner->IsTrainingComplete()))
    {
        return false;
    }

    ACQBTrainingPlayerSpawnPoint* SpawnPoint = ActiveCQBSpawner->GetPlayerStartPoint();
    if (!IsValid(SpawnPoint))
    {
        SpawnPoint = ReturnCQBEntryPoint.Get();
    }
    if (!IsValid(SpawnPoint))
    {
        return false;
    }

    OutTransform = SpawnPoint->GetActorTransform();
    return true;
}

void UTrainingPlayerComponent::OpenCQBStartDialog(ACQBTrainingSpawner& Spawner)
{
    APlayerController* ParticipantPlayer = GetParticipantController();
    if (!ParticipantPlayer || !Spawner.CanInteract(*ParticipantPlayer))
    {
        return;
    }

    CancelCQBDialogRequest(PendingCQBRequestId);
    RequestSequence = RequestSequence == MAX_int32 ? 1 : RequestSequence + 1;
    PendingCQBRequestId = RequestSequence;
    PendingCQBSpawner = &Spawner;
    PendingCQBEndActor.Reset();
    ShowCQBDialog(ECQBTrainingDialogKind::Start, PendingCQBRequestId, Spawner.StartDialogWidgetClass);
}

void UTrainingPlayerComponent::OpenCQBCancelDialog(ACQBTrainingSpawner& Spawner)
{
    APlayerController* ParticipantPlayer = GetParticipantController();
    if (!ParticipantPlayer
        || !Spawner.CanInteract(*ParticipantPlayer)
        || !IsCQBTrainingActiveWith(Spawner))
    {
        return;
    }

    CancelCQBDialogRequest(PendingCQBRequestId);
    RequestSequence = RequestSequence == MAX_int32 ? 1 : RequestSequence + 1;
    PendingCQBRequestId = RequestSequence;
    PendingCQBSpawner = &Spawner;
    PendingCQBEndActor.Reset();
    ShowCQBDialog(ECQBTrainingDialogKind::Cancel, PendingCQBRequestId, Spawner.GetCancelDialogWidgetClass());
}

void UTrainingPlayerComponent::OpenCQBEndDialog(ACQBTrainingEndActor& EndActor)
{
    APlayerController* ParticipantPlayer = GetParticipantController();
    if (!ParticipantPlayer || !EndActor.CanInteract(*ParticipantPlayer))
    {
        return;
    }

    CancelCQBDialogRequest(PendingCQBRequestId);
    RequestSequence = RequestSequence == MAX_int32 ? 1 : RequestSequence + 1;
    PendingCQBRequestId = RequestSequence;
    PendingCQBEndActor = &EndActor;
    PendingCQBSpawner = EndActor.Spawner;
    ShowCQBDialog(ECQBTrainingDialogKind::End, PendingCQBRequestId, EndActor.EndDialogWidgetClass);
}

void UTrainingPlayerComponent::ShowCQBDialog(
    const ECQBTrainingDialogKind Kind, const int32 RequestId,
    TSubclassOf<UCQBTrainingDialogWidget> WidgetClass)
{
    CloseResults();
    ClearPendingRequest();
    CloseActiveDialog();
    CloseActiveCQBDialog();

    UShooterUIManagerSubsystem* UIManager = GetUIManager();
    if (!UIManager || !UIManager->IsUIInitialized() || !WidgetClass)
    {
        NotifyMessage(LOCTEXT("CQBUISetup",
            "CQB UI is not configured. Assign the CQB dialog widget class on the interaction actor."));
        CancelCQBDialogRequest(RequestId);
        return;
    }

    UCQBTrainingDialogWidget* NewDialog = Cast<UCQBTrainingDialogWidget>(
        UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, WidgetClass));
    if (!NewDialog)
    {
        NotifyMessage(LOCTEXT("CQBNoLayer", "Could not open the CQB training dialog."));
        CancelCQBDialogRequest(RequestId);
        return;
    }

    ActiveCQBDialog = NewDialog;
    NewDialog->InitializeDialog(*this, Kind, RequestId);
    if (WidgetOpenSound) UGameplayStatics::PlaySound2D(this, WidgetOpenSound);
}

void UTrainingPlayerComponent::DismissCQBDialog(const int32 RequestId)
{
    CancelCQBDialogRequest(RequestId);
    if (ActiveCQBDialog.IsValid() && ActiveCQBDialog->GetRequestId() == RequestId)
    {
        if (WidgetCloseSound) UGameplayStatics::PlaySound2D(this, WidgetCloseSound);
        CloseActiveCQBDialog();
    }
}

void UTrainingPlayerComponent::NotifyCQBDialogRemoved(UCQBTrainingDialogWidget& Dialog)
{
    CancelCQBDialogRequest(Dialog.GetRequestId());
    if (ActiveCQBDialog.Get() == &Dialog)
    {
        ActiveCQBDialog.Reset();
    }
    Dialog.DetachRequest();
}

void UTrainingPlayerComponent::CloseActiveCQBDialog()
{
    UCQBTrainingDialogWidget* Dialog = ActiveCQBDialog.Get();
    ActiveCQBDialog.Reset();
    if (!Dialog) return;

    Dialog->DetachRequest();
    if (UShooterUIManagerSubsystem* UIManager = GetUIManager())
    {
        UIManager->RemoveWidgetFromLayer(UITags::UI_Layer_Modal, Dialog);
    }
    else
    {
        Dialog->DeactivateWidget();
    }
}

void UTrainingPlayerComponent::ClearPendingCQBRequest()
{
    PendingCQBRequestId = 0;
    PendingCQBSpawner.Reset();
    PendingCQBEndActor.Reset();
    SuspendedCQBRequestId = 0;
    SuspendedCQBDialogClass = nullptr;
}

bool UTrainingPlayerComponent::ValidatePendingCQBRequest(const int32 RequestId, FText& OutFailure) const
{
    const UShooterInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UShooterInteractionComponent>();
    const AActor* Candidate = PendingCQBEndActor.IsValid()
        ? static_cast<const AActor*>(PendingCQBEndActor.Get())
        : static_cast<const AActor*>(PendingCQBSpawner.Get());
    if (RequestId <= 0 || RequestId != PendingCQBRequestId || !Candidate
        || !Interaction || !Interaction->CanReachInteractable(Candidate))
    {
        OutFailure = LOCTEXT("CQBExpired", "This CQB request is no longer valid. Interact again.");
        return false;
    }
    return true;
}

void UTrainingPlayerComponent::CancelCQBDialogRequest(const int32 RequestId)
{
    if (RequestId <= 0 || PendingCQBRequestId != RequestId)
    {
        return;
    }

    if (ACQBTrainingSpawner* Spawner = PendingCQBSpawner.Get())
    {
        Spawner->CancelPendingStart(RequestId);
    }
    ClearPendingCQBRequest();
}

void UTrainingPlayerComponent::ResolveCQBRequest(
    const int32 RequestId, const bool bSuccess, const FText& Failure)
{
    UCQBTrainingDialogWidget* Dialog = ActiveCQBDialog.Get();
    if (!Dialog || Dialog->GetRequestId() != RequestId) return;

    if (bSuccess)
    {
        CloseActiveCQBDialog();
    }
    else
    {
        Dialog->ReceiveRequestFailure(Failure);
    }
}

void UTrainingPlayerComponent::SuspendCQBDialogForClassSelection(const int32 RequestId)
{
    UCQBTrainingDialogWidget* Dialog = ActiveCQBDialog.Get();
    if (!Dialog || Dialog->GetRequestId() != RequestId) return;

    SuspendedCQBRequestId = RequestId;
    SuspendedCQBDialogClass = Dialog->GetClass();
    CloseActiveCQBDialog();
}

void UTrainingPlayerComponent::ResumeCQBStartDialogAfterClassSelection()
{
    if (SuspendedCQBRequestId <= 0 || !SuspendedCQBDialogClass) return;

    const int32 RequestId = SuspendedCQBRequestId;
    const TSubclassOf<UCQBTrainingDialogWidget> WidgetClass = SuspendedCQBDialogClass;
    SuspendedCQBRequestId = 0;
    SuspendedCQBDialogClass = nullptr;
    FText Failure;
    if (!ValidatePendingCQBRequest(RequestId, Failure) || IsCQBTrainingActive())
    {
        CancelCQBDialogRequest(RequestId);
        return;
    }
    ShowCQBDialog(ECQBTrainingDialogKind::Start, RequestId, WidgetClass);
}

void UTrainingPlayerComponent::SubmitCQBStart(const int32 RequestId)
{
    FText Failure;
    if (!ValidatePendingCQBRequest(RequestId, Failure) || !PendingCQBSpawner.IsValid())
    {
        ResolveCQBRequest(RequestId, false, Failure.IsEmpty()
            ? LOCTEXT("CQBStartMissing", "CQB spawner is unavailable.") : Failure);
        return;
    }

    APlayerController* Participant = GetParticipantController();
    if (!Participant || !PendingCQBSpawner->StartTrainingFor(*Participant, RequestId))
    {
        ResolveCQBRequest(RequestId, false,
            LOCTEXT("CQBStartFailed", "Could not start CQB training. Check the CQB spawner state and Trooper Class."));
    }
}

void UTrainingPlayerComponent::SubmitCQBClassSelection(const int32 RequestId)
{
    FText Failure;
    if (!ValidatePendingCQBRequest(RequestId, Failure)
        || PendingCQBEndActor.IsValid()
        || !PendingCQBSpawner.IsValid()
        || IsCQBTrainingActive())
    {
        ResolveCQBRequest(RequestId, false, Failure.IsEmpty()
            ? LOCTEXT("CQBClassSelectionUnavailable", "Class selection is only available before CQB starts.")
            : Failure);
        return;
    }

    ATrainingPlayerController* TrainingController = Cast<ATrainingPlayerController>(GetParticipantController());
    if (!TrainingController)
    {
        ResolveCQBRequest(RequestId, false,
            LOCTEXT("CQBClassControllerMissing", "Training class selection is unavailable for this controller."));
        return;
    }

    SuspendCQBDialogForClassSelection(RequestId);
    TrainingController->RequestTrainingClassSelection();
}

void UTrainingPlayerComponent::SubmitCQBCancel(const int32 RequestId)
{
    FText Failure;
    if (!ValidatePendingCQBRequest(RequestId, Failure)
        || !PendingCQBSpawner.IsValid()
        || PendingCQBEndActor.IsValid())
    {
        ResolveCQBRequest(RequestId, false, Failure.IsEmpty()
            ? LOCTEXT("CQBCancelExpired", "CQB cancellation is no longer available.") : Failure);
        return;
    }

    const bool bSuccess = IsCQBTrainingActiveWith(*PendingCQBSpawner)
        && PendingCQBSpawner->CancelTraining();
    if (bSuccess)
    {
        ClearPendingCQBRequest();
    }
    else
    {
        Failure = LOCTEXT("CQBCancelFailed", "Could not cancel the active CQB training session.");
    }
    ResolveCQBRequest(RequestId, bSuccess, Failure);
}

void UTrainingPlayerComponent::NotifyCQBStartRequestResolved(
    ACQBTrainingSpawner& Spawner,
    const int32 RequestId,
    const bool bSuccess,
    const FText& Failure)
{
    if (RequestId <= 0
        || PendingCQBRequestId != RequestId
        || PendingCQBSpawner.Get() != &Spawner)
    {
        return;
    }

    if (bSuccess)
    {
        ClearPendingCQBRequest();
    }
    ResolveCQBRequest(RequestId, bSuccess, Failure);
}

void UTrainingPlayerComponent::SubmitCQBReturnToOrigin(const int32 RequestId)
{
    FText Failure;
    if (!ValidatePendingCQBRequest(RequestId, Failure) || PendingCQBEndActor.IsValid()
        || IsCQBTrainingActive())
    {
        ResolveCQBRequest(RequestId, false, Failure.IsEmpty()
            ? LOCTEXT("CQBReturnWrongDialog", "Return is only available before CQB starts.") : Failure);
        return;
    }

    const bool bSuccess = TeleportToSavedPreTrainingPosition(Failure, true);
    if (bSuccess)
    {
        ClearPendingCQBRequest();
    }
    ResolveCQBRequest(RequestId, bSuccess, Failure);
}

void UTrainingPlayerComponent::SubmitCQBEndToStart(const int32 RequestId)
{
    FText Failure;
    if (!ValidatePendingCQBRequest(RequestId, Failure) || !PendingCQBSpawner.IsValid()
        || !PendingCQBEndActor.IsValid() || !PendingCQBSpawner->IsTrainingComplete())
    {
        ResolveCQBRequest(RequestId, false, Failure.IsEmpty()
            ? LOCTEXT("CQBEndUnavailable", "CQB completion is no longer available.") : Failure);
        return;
    }

    ACQBTrainingPlayerSpawnPoint* StartPoint = PendingCQBSpawner->GetPlayerStartPoint();
    if (!StartPoint) StartPoint = ReturnCQBEntryPoint.Get();
    if (!StartPoint)
    {
        ResolveCQBRequest(RequestId, false,
            LOCTEXT("CQBStartPointMissing", "Assign a CQB Training Player Spawn Point to the spawner."));
        return;
    }

    const FTransform Destination = StartPoint->GetActorTransform();
    const bool bSuccess = TeleportParticipant(Destination, Destination.Rotator());
    if (bSuccess)
    {
        PendingCQBSpawner->FinishTrainingAndReset();
        ClearPendingCQBRequest();
    }
    else
    {
        Failure = LOCTEXT("CQBStartBlocked", "The CQB start point is blocked.");
    }
    ResolveCQBRequest(RequestId, bSuccess, Failure);
}

void UTrainingPlayerComponent::SubmitCQBEndToOrigin(const int32 RequestId)
{
    FText Failure;
    if (!ValidatePendingCQBRequest(RequestId, Failure) || !PendingCQBSpawner.IsValid()
        || !PendingCQBEndActor.IsValid() || !PendingCQBSpawner->IsTrainingComplete())
    {
        ResolveCQBRequest(RequestId, false, Failure.IsEmpty()
            ? LOCTEXT("CQBEndOriginUnavailable", "CQB completion is no longer available.") : Failure);
        return;
    }

    const bool bSuccess = TeleportToSavedPreTrainingPosition(Failure, true);
    if (bSuccess)
    {
        PendingCQBSpawner->FinishTrainingAndReset();
        ClearPendingCQBRequest();
    }
    ResolveCQBRequest(RequestId, bSuccess, Failure);
}

void UTrainingPlayerComponent::NotifyCQBLoading(ACQBTrainingSpawner& Spawner)
{
    ActiveCQBSpawner = &Spawner;
}

void UTrainingPlayerComponent::NotifyCQBStopped(ACQBTrainingSpawner& Spawner)
{
    if (ActiveCQBSpawner.IsValid() && ActiveCQBSpawner.Get() != &Spawner)
    {
        return;
    }
    ActiveCQBSpawner.Reset();
    if (Presentation)
    {
        Presentation->StopCQBSession();
    }
}

void UTrainingPlayerComponent::NotifyCQBStarted(
    ACQBTrainingSpawner& Spawner, const int32 TotalTroopers,
    TSubclassOf<UTrainingSessionHUDWidget> HUDWidgetClass)
{
    ActiveCQBSpawner = &Spawner;
    if (Presentation)
    {
        Presentation->StartCQBSession(Spawner, TotalTroopers, HUDWidgetClass);
    }
}

void UTrainingPlayerComponent::NotifyCQBProgress(
    const int32 KilledTroopers, const int32 TotalTroopers, const bool bComplete)
{
    if (Presentation)
    {
        Presentation->UpdateCQBProgress(KilledTroopers, TotalTroopers, bComplete);
    }
}

#undef LOCTEXT_NAMESPACE
