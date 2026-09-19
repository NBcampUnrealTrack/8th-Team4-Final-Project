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
#include "Training/UI/TrainingSessionHUDWidget.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"

#define LOCTEXT_NAMESPACE "TrainingPlayer"

bool UTrainingPlayerComponent::UsesStandardEliminationFeedback() const
{
    return !ActiveCQBSpawner.IsValid() || ActiveCQBSpawner->UsesStandardEliminationFeedback();
}

bool UTrainingPlayerComponent::IsCQBTrainingActive() const
{
    return ActiveCQBSpawner.IsValid();
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

void UTrainingPlayerComponent::SubmitCQBStart(const int32 RequestId)
{
    ServerStartCQB(RequestId);
}

void UTrainingPlayerComponent::SubmitCQBCancel(const int32 RequestId)
{
    ServerCancelActiveCQB(RequestId);
}

void UTrainingPlayerComponent::SubmitCQBClassSelection(const int32 RequestId)
{
    ServerOpenCQBClassSelection(RequestId);
}

void UTrainingPlayerComponent::SubmitCQBReturnToOrigin(const int32 RequestId)
{
    ServerReturnFromCQBStart(RequestId);
}

void UTrainingPlayerComponent::SubmitCQBEndToStart(const int32 RequestId)
{
    ServerFinishCQBToStart(RequestId);
}

void UTrainingPlayerComponent::SubmitCQBEndToOrigin(const int32 RequestId)
{
    ServerFinishCQBToOrigin(RequestId);
}

void UTrainingPlayerComponent::OpenCQBStartDialog(ACQBTrainingSpawner& Spawner)
{
    APlayerController* ParticipantPlayer = GetParticipantController();
    if (!ParticipantPlayer || !ParticipantPlayer->HasAuthority() || !Spawner.CanInteract(*ParticipantPlayer))
    {
        return;
    }

    RequestSequence = RequestSequence == MAX_int32 ? 1 : RequestSequence + 1;
    PendingCQBRequestId = RequestSequence;
    PendingCQBSpawner = &Spawner;
    PendingCQBEndActor.Reset();
    ClientOpenCQBDialog(ECQBTrainingDialogKind::Start, PendingCQBRequestId, Spawner.StartDialogWidgetClass);
}

void UTrainingPlayerComponent::OpenCQBCancelDialog(ACQBTrainingSpawner& Spawner)
{
    APlayerController* ParticipantPlayer = GetParticipantController();
    if (!ParticipantPlayer || !ParticipantPlayer->HasAuthority()
        || !Spawner.CanInteract(*ParticipantPlayer)
        || !IsCQBTrainingActiveWith(Spawner))
    {
        return;
    }

    RequestSequence = RequestSequence == MAX_int32 ? 1 : RequestSequence + 1;
    PendingCQBRequestId = RequestSequence;
    PendingCQBSpawner = &Spawner;
    PendingCQBEndActor.Reset();
    ClientOpenCQBDialog(ECQBTrainingDialogKind::Cancel, PendingCQBRequestId, Spawner.GetCancelDialogWidgetClass());
}

void UTrainingPlayerComponent::OpenCQBEndDialog(ACQBTrainingEndActor& EndActor)
{
    APlayerController* ParticipantPlayer = GetParticipantController();
    if (!ParticipantPlayer || !ParticipantPlayer->HasAuthority() || !EndActor.CanInteract(*ParticipantPlayer))
    {
        return;
    }

    RequestSequence = RequestSequence == MAX_int32 ? 1 : RequestSequence + 1;
    PendingCQBRequestId = RequestSequence;
    PendingCQBEndActor = &EndActor;
    PendingCQBSpawner = EndActor.Spawner;
    ClientOpenCQBDialog(ECQBTrainingDialogKind::End, PendingCQBRequestId, EndActor.EndDialogWidgetClass);
}

void UTrainingPlayerComponent::ClientOpenCQBDialog_Implementation(
    const ECQBTrainingDialogKind Kind, const int32 RequestId,
    TSubclassOf<UCQBTrainingDialogWidget> WidgetClass)
{
    CloseResults();
    CloseActiveDialog();
    CloseActiveCQBDialog();

    UShooterUIManagerSubsystem* UIManager = GetUIManager();
    if (!UIManager || !UIManager->IsUIInitialized() || !WidgetClass)
    {
        ClientNotify_Implementation(LOCTEXT("CQBUISetup",
            "CQB UI is not configured. Assign the CQB dialog widget class on the interaction actor."));
        ServerCancelCQB(RequestId);
        return;
    }

    UCQBTrainingDialogWidget* NewDialog = Cast<UCQBTrainingDialogWidget>(
        UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, WidgetClass));
    if (!NewDialog)
    {
        ClientNotify_Implementation(LOCTEXT("CQBNoLayer", "Could not open the CQB training dialog."));
        ServerCancelCQB(RequestId);
        return;
    }

    ActiveCQBDialog = NewDialog;
    NewDialog->InitializeDialog(*this, Kind, RequestId);
    if (WidgetOpenSound) UGameplayStatics::PlaySound2D(this, WidgetOpenSound);
}

void UTrainingPlayerComponent::DismissCQBDialog(const int32 RequestId)
{
    ServerCancelCQB(RequestId);
    if (ActiveCQBDialog.IsValid() && ActiveCQBDialog->GetRequestId() == RequestId)
    {
        if (WidgetCloseSound) UGameplayStatics::PlaySound2D(this, WidgetCloseSound);
        CloseActiveCQBDialog();
    }
}

void UTrainingPlayerComponent::NotifyCQBDialogRemoved(UCQBTrainingDialogWidget& Dialog)
{
    ServerCancelCQB(Dialog.GetRequestId());
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

void UTrainingPlayerComponent::ServerCancelCQB_Implementation(const int32 RequestId)
{
    if (PendingCQBRequestId != RequestId)
    {
        return;
    }

    if (APlayerController* Participant = GetParticipantController())
    {
        if (ACQBTrainingSpawner* Spawner = PendingCQBSpawner.Get())
        {
            Spawner->CancelPendingStart(*Participant, RequestId);
        }
    }
    ClearPendingCQBRequest();
}

void UTrainingPlayerComponent::ClientCQBRequestResult_Implementation(
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

void UTrainingPlayerComponent::ClientSuspendCQBDialogForClassSelection_Implementation(const int32 RequestId)
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
    ClientOpenCQBDialog_Implementation(ECQBTrainingDialogKind::Start, RequestId, WidgetClass);
}

void UTrainingPlayerComponent::ServerStartCQB_Implementation(const int32 RequestId)
{
    FText Failure;
    if (!ValidatePendingCQBRequest(RequestId, Failure) || !PendingCQBSpawner.IsValid())
    {
        ClientCQBRequestResult(RequestId, false, Failure.IsEmpty()
            ? LOCTEXT("CQBStartMissing", "CQB spawner is unavailable.") : Failure);
        return;
    }

    APlayerController* Participant = GetParticipantController();
    if (!Participant || !PendingCQBSpawner->StartTrainingFor(*Participant, RequestId))
    {
        ClientCQBRequestResult(RequestId, false,
            LOCTEXT("CQBStartFailed", "Could not start CQB training. Check the CQB spawner state and Trooper Class."));
    }
}

void UTrainingPlayerComponent::ServerOpenCQBClassSelection_Implementation(const int32 RequestId)
{
    FText Failure;
    if (!ValidatePendingCQBRequest(RequestId, Failure)
        || PendingCQBEndActor.IsValid()
        || !PendingCQBSpawner.IsValid()
        || IsCQBTrainingActive())
    {
        ClientCQBRequestResult(RequestId, false, Failure.IsEmpty()
            ? LOCTEXT("CQBClassSelectionUnavailable", "Class selection is only available before CQB starts.")
            : Failure);
        return;
    }

    ATrainingPlayerController* TrainingController = Cast<ATrainingPlayerController>(GetParticipantController());
    if (!TrainingController)
    {
        ClientCQBRequestResult(RequestId, false,
            LOCTEXT("CQBClassControllerMissing", "Training class selection is unavailable for this controller."));
        return;
    }

    ClientSuspendCQBDialogForClassSelection(RequestId);
    TrainingController->RequestTrainingClassSelection();
}

void UTrainingPlayerComponent::ServerCancelActiveCQB_Implementation(const int32 RequestId)
{
    FText Failure;
    if (!ValidatePendingCQBRequest(RequestId, Failure)
        || !PendingCQBSpawner.IsValid()
        || PendingCQBEndActor.IsValid())
    {
        ClientCQBRequestResult(RequestId, false, Failure.IsEmpty()
            ? LOCTEXT("CQBCancelExpired", "CQB cancellation is no longer available.") : Failure);
        return;
    }

    APlayerController* Participant = GetParticipantController();
    const bool bSuccess = Participant
        && IsCQBTrainingActiveWith(*PendingCQBSpawner)
        && PendingCQBSpawner->CancelTrainingFor(*Participant);
    if (bSuccess)
    {
        ClearPendingCQBRequest();
    }
    else
    {
        Failure = LOCTEXT("CQBCancelFailed", "Could not cancel the active CQB training session.");
    }
    ClientCQBRequestResult(RequestId, bSuccess, Failure);
}

void UTrainingPlayerComponent::NotifyCQBStartRequestResolved(
    ACQBTrainingSpawner& Spawner,
    const int32 RequestId,
    const bool bSuccess,
    const FText& Failure)
{
    if (!GetOwner()->HasAuthority()
        || RequestId <= 0
        || PendingCQBRequestId != RequestId
        || PendingCQBSpawner.Get() != &Spawner)
    {
        return;
    }

    if (bSuccess)
    {
        ClearPendingCQBRequest();
    }
    ClientCQBRequestResult(RequestId, bSuccess, Failure);
}

void UTrainingPlayerComponent::ServerReturnFromCQBStart_Implementation(const int32 RequestId)
{
    FText Failure;
    if (!ValidatePendingCQBRequest(RequestId, Failure) || PendingCQBEndActor.IsValid())
    {
        ClientCQBRequestResult(RequestId, false, Failure.IsEmpty()
            ? LOCTEXT("CQBReturnWrongDialog", "Return is only available before CQB starts.") : Failure);
        return;
    }

    const bool bSuccess = TeleportToSavedPreTrainingPosition(Failure, true);
    if (bSuccess)
    {
        ClearPendingCQBRequest();
    }
    ClientCQBRequestResult(RequestId, bSuccess, Failure);
}

void UTrainingPlayerComponent::ServerFinishCQBToStart_Implementation(const int32 RequestId)
{
    FText Failure;
    if (!ValidatePendingCQBRequest(RequestId, Failure) || !PendingCQBSpawner.IsValid()
        || !PendingCQBEndActor.IsValid() || !PendingCQBSpawner->IsTrainingComplete())
    {
        ClientCQBRequestResult(RequestId, false, Failure.IsEmpty()
            ? LOCTEXT("CQBEndUnavailable", "CQB completion is no longer available.") : Failure);
        return;
    }

    ACQBTrainingPlayerSpawnPoint* StartPoint = PendingCQBSpawner->GetPlayerStartPoint();
    if (!StartPoint) StartPoint = ReturnCQBEntryPoint.Get();
    if (!StartPoint)
    {
        ClientCQBRequestResult(RequestId, false,
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
    ClientCQBRequestResult(RequestId, bSuccess, Failure);
}

void UTrainingPlayerComponent::ServerFinishCQBToOrigin_Implementation(const int32 RequestId)
{
    FText Failure;
    if (!ValidatePendingCQBRequest(RequestId, Failure) || !PendingCQBSpawner.IsValid()
        || !PendingCQBEndActor.IsValid() || !PendingCQBSpawner->IsTrainingComplete())
    {
        ClientCQBRequestResult(RequestId, false, Failure.IsEmpty()
            ? LOCTEXT("CQBEndOriginUnavailable", "CQB completion is no longer available.") : Failure);
        return;
    }

    const bool bSuccess = TeleportToSavedPreTrainingPosition(Failure, true);
    if (bSuccess)
    {
        PendingCQBSpawner->FinishTrainingAndReset();
        ClearPendingCQBRequest();
    }
    ClientCQBRequestResult(RequestId, bSuccess, Failure);
}

void UTrainingPlayerComponent::NotifyCQBStarted(
    ACQBTrainingSpawner& Spawner, const int32 TotalTroopers, TSubclassOf<UTrainingSessionHUDWidget> HUDWidgetClass)
{
    if (!GetOwner()->HasAuthority())
    {
        return;
    }
    ActiveCQBSpawner = &Spawner;
    ClientCQBStarted(&Spawner, TotalTroopers, HUDWidgetClass);
}

void UTrainingPlayerComponent::NotifyCQBProgress(
    const int32 KilledTroopers, const int32 TotalTroopers, const bool bComplete)
{
    if (GetOwner()->HasAuthority()) ClientCQBProgress(KilledTroopers, TotalTroopers, bComplete);
}

void UTrainingPlayerComponent::NotifyCQBStopped(ACQBTrainingSpawner& Spawner)
{
    if (!GetOwner()->HasAuthority())
    {
        return;
    }
    if (!ActiveCQBSpawner.IsValid() || ActiveCQBSpawner.Get() == &Spawner)
    {
        ActiveCQBSpawner.Reset();
    }
    ClientCQBStopped();
}

void UTrainingPlayerComponent::ClientCQBStarted_Implementation(
    ACQBTrainingSpawner* Spawner, const int32 TotalTroopers,
    TSubclassOf<UTrainingSessionHUDWidget> HUDWidgetClass)
{
    APlayerController* LocalController = GetParticipantController();
    if (!LocalController || !LocalController->IsLocalController()) return;

    ActiveCQBSpawner = Spawner;

    if (UTrainingSessionHUDWidget* Existing = CQBHUDWidget.Get())
    {
        Existing->RemoveFromParent();
        CQBHUDWidget.Reset();
    }

    if (!HUDWidgetClass)
    {
        ClientNotify_Implementation(LOCTEXT("CQBHUDMissing",
            "CQB HUD is not configured. Assign HUD Widget Class on BP_CQBTrainingSpawner."));
        return;
    }

    UTrainingSessionHUDWidget* HUD = CreateWidget<UTrainingSessionHUDWidget>(LocalController, HUDWidgetClass);
    if (!HUD) return;
    CQBHUDWidget = HUD;
    HUD->AddToPlayerScreen(GetDefault<UTrainingSettings>()->SessionHUDZOrder);
    HUD->StartCQBSession(TotalTroopers);
}

void UTrainingPlayerComponent::ClientCQBProgress_Implementation(
    const int32 KilledTroopers, const int32 TotalTroopers, const bool bComplete)
{
    if (UTrainingSessionHUDWidget* HUD = CQBHUDWidget.Get())
    {
        HUD->PresentCQBKill(KilledTroopers, TotalTroopers);
        if (bComplete) HUD->FinishCQBSession();
    }
}

void UTrainingPlayerComponent::ClientCQBStopped_Implementation()
{
    ActiveCQBSpawner.Reset();
    if (UTrainingSessionHUDWidget* HUD = CQBHUDWidget.Get())
    {
        HUD->RemoveFromParent();
    }
    CQBHUDWidget.Reset();
}

#undef LOCTEXT_NAMESPACE
