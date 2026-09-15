#include "Training/TrainingPlayerComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Input/ShooterInteractionComponent.h"
#include "Tags/UITags.h"
#include "Training/TrainingArena.h"
#include "Training/TrainingInteractionActor.h"
#include "Training/TrainingSessionSubsystem.h"
#include "Training/UI/TrainingDialogWidget.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"

#define LOCTEXT_NAMESPACE "TrainingPlayer"

UTrainingPlayerComponent::UTrainingPlayerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UTrainingPlayerComponent::BeginPlay()
{
    Super::BeginPlay();
    // Existing game modes use seamless travel; a controller can survive its old world.
    WorldTearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &ThisClass::HandleWorldLeaving);
    SeamlessTravelHandle = FWorldDelegates::OnSeamlessTravelStart.AddUObject(this, &ThisClass::HandleSeamlessTravel);
}

APlayerController* UTrainingPlayerComponent::GetParticipantController() const
{
    return Cast<APlayerController>(GetOwner());
}

UShooterUIManagerSubsystem* UTrainingPlayerComponent::GetUIManager() const
{
    const APlayerController* ParticipantPlayer = GetParticipantController();
    UGameInstance* GameInstance = ParticipantPlayer ? ParticipantPlayer->GetGameInstance() : nullptr;
    return GameInstance ? GameInstance->GetSubsystem<UShooterUIManagerSubsystem>() : nullptr;
}

void UTrainingPlayerComponent::OpenDialogFor(ATrainingInteractionActor& Interactable)
{
    APlayerController* ParticipantPlayer = GetParticipantController();
    ATrainingArena* RequestedArena = Interactable.Arena;
    if (!ParticipantPlayer || !ParticipantPlayer->HasAuthority() || !RequestedArena)
    {
        return;
    }
    UTrainingSessionSubsystem* Sessions = GetWorld()->GetSubsystem<UTrainingSessionSubsystem>();
    if (!Sessions)
    {
        return;
    }
    const bool bSessionActive = Sessions->GetState() != ETrainingSessionState::Idle;
    ETrainingDialogKind Kind = ETrainingDialogKind::EnterArena;
    if (Interactable.InteractionRole == ETrainingInteractionRole::Entrance)
    {
        if (bSessionActive)
        {
            ClientNotify(LOCTEXT("ArenaBusy", "A training session is already active. End it before entering again."));
            return;
        }
    }
    else if (bSessionActive)
    {
        if (Sessions->GetParticipant() != ParticipantPlayer || Sessions->GetActiveArena() != RequestedArena)
        {
            ClientNotify(LOCTEXT("OtherPlayer", "Another player is using the Training Arena."));
            return;
        }
        Kind = ETrainingDialogKind::EndSession;
    }
    else
    {
        if (!ParticipantPlayer->GetPawn() || !RequestedArena->IsInMovementArea(*ParticipantPlayer->GetPawn()))
        {
            ClientNotify(LOCTEXT("NotInside", "Enter the central training area first."));
            return;
        }
        Kind = ETrainingDialogKind::SelectMode;
    }

    RequestSequence = RequestSequence == MAX_int32 ? 1 : RequestSequence + 1;
    PendingRequestId = RequestSequence;
    PendingSessionId = Sessions->GetSessionId();
    PendingKind = Kind;
    PendingInteractable = &Interactable;
    ClientOpenDialog(Kind, PendingRequestId);
}

void UTrainingPlayerComponent::ClientOpenDialog_Implementation(const ETrainingDialogKind Kind, const int32 RequestId)
{
    CloseActiveDialog();
    UShooterUIManagerSubsystem* UIManager = GetUIManager();
    TSubclassOf<UTrainingDialogWidget> WidgetClass = Kind == ETrainingDialogKind::SelectMode
        ? ModeSelectionWidgetClass : ConfirmationWidgetClass;
    if (Kind == ETrainingDialogKind::EndSession && EndSessionWidgetClass)
    {
        WidgetClass = EndSessionWidgetClass;
    }
    if (!UIManager || !UIManager->IsUIInitialized() || !WidgetClass)
    {
        ClientNotify_Implementation(LOCTEXT("UISetup", "Training UI is not configured: initialize the existing PrimaryGameLayout and assign both Training widget classes."));
        ServerCancel(RequestId);
        return;
    }
    UTrainingDialogWidget* NewDialog = Cast<UTrainingDialogWidget>(
        UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, WidgetClass));
    if (!NewDialog)
    {
        ClientNotify_Implementation(LOCTEXT("NoLayer", "The existing PrimaryGameLayout could not open the Training modal."));
        ServerCancel(RequestId);
        return;
    }
    ActiveDialog = NewDialog;
    NewDialog->InitializeDialog(*this, Kind, RequestId);
}

void UTrainingPlayerComponent::SubmitConfirmation(const int32 RequestId)
{
    ServerConfirm(RequestId);
}

void UTrainingPlayerComponent::SubmitMode(const int32 RequestId, const ETrainingMode Mode,
    const ETrainingDistancePreset Preset, const float CustomMeters)
{
    ServerStartMode(RequestId, Mode, Preset, CustomMeters);
}

bool UTrainingPlayerComponent::ValidatePendingRequest(const int32 RequestId, FText& OutFailure) const
{
    const UShooterInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UShooterInteractionComponent>();
    const ATrainingInteractionActor* Interactable = PendingInteractable.Get();
    if (RequestId <= 0 || RequestId != PendingRequestId || !Interactable || !Interactable->Arena
        || !Interaction || !Interaction->CanReachInteractable(Interactable))
    {
        OutFailure = LOCTEXT("Expired", "This request is no longer valid. Close the dialog and interact again.");
        return false;
    }
    return true;
}

void UTrainingPlayerComponent::ServerConfirm_Implementation(const int32 RequestId)
{
    FText Failure;
    if (!ValidatePendingRequest(RequestId, Failure))
    {
        ClientRequestResult(RequestId, false, Failure);
        return;
    }
    APlayerController* ParticipantPlayer = GetParticipantController();
    UTrainingSessionSubsystem* Sessions = GetWorld()->GetSubsystem<UTrainingSessionSubsystem>();
    ATrainingArena* RequestedArena = PendingInteractable->Arena;
    bool bSuccess = false;
    if (PendingKind == ETrainingDialogKind::EnterArena && ParticipantPlayer && Sessions)
    {
        if (Sessions->GetState() != ETrainingSessionState::Idle)
        {
            Failure = LOCTEXT("NowBusy", "A training session has started. Interact again when it is idle.");
        }
        else if (APawn* ParticipantPawn = ParticipantPlayer->GetPawn())
        {
            const FTransform EntryTransform = RequestedArena->GetEntryTransform();
            bSuccess = ParticipantPawn->TeleportTo(EntryTransform.GetLocation(), EntryTransform.Rotator());
            if (bSuccess)
            {
                if (ACharacter* ParticipantCharacter = Cast<ACharacter>(ParticipantPawn))
                {
                    ParticipantCharacter->GetCharacterMovement()->StopMovementImmediately();
                }
                ParticipantPlayer->SetControlRotation(EntryTransform.Rotator());
                ParticipantPlayer->ClientSetRotation(EntryTransform.Rotator(), true);
                ParticipantPawn->ForceNetUpdate();
            }
            else
            {
                Failure = LOCTEXT("BlockedEntry", "TrainingEntryPoint is blocked. Check capsule clearance at the entry transform.");
            }
        }
    }
    else if (PendingKind == ETrainingDialogKind::EndSession && ParticipantPlayer && Sessions)
    {
        bSuccess = Sessions->RequestEndSession(*ParticipantPlayer, PendingSessionId);
        if (!bSuccess)
        {
            Failure = LOCTEXT("ChangedSession", "That session has already ended or changed. Close the dialog and interact again.");
        }
    }
    if (bSuccess)
    {
        ClearPendingRequest();
    }
    else if (Failure.IsEmpty())
    {
        Failure = LOCTEXT("InvalidConfirmation", "This confirmation can no longer be applied.");
    }
    ClientRequestResult(RequestId, bSuccess, Failure);
}

void UTrainingPlayerComponent::ServerStartMode_Implementation(const int32 RequestId, const ETrainingMode Mode,
    const ETrainingDistancePreset Preset, const float CustomMeters)
{
    FText Failure;
    if (!ValidatePendingRequest(RequestId, Failure))
    {
        ClientRequestResult(RequestId, false, Failure);
        return;
    }
    UTrainingSessionSubsystem* Sessions = GetWorld()->GetSubsystem<UTrainingSessionSubsystem>();
    APlayerController* ParticipantPlayer = GetParticipantController();
    const bool bSuccess = PendingKind == ETrainingDialogKind::SelectMode && Sessions && ParticipantPlayer
        && Sessions->StartSession(*PendingInteractable->Arena, *ParticipantPlayer, Mode, Preset, CustomMeters, Failure);
    if (bSuccess)
    {
        ClearPendingRequest();
    }
    else if (Failure.IsEmpty())
    {
        Failure = LOCTEXT("InvalidSelection", "This mode selection is no longer valid.");
    }
    ClientRequestResult(RequestId, bSuccess, Failure);
}

void UTrainingPlayerComponent::ClientRequestResult_Implementation(const int32 RequestId,
    const bool bSuccess, const FText& Failure)
{
    UTrainingDialogWidget* Dialog = ActiveDialog.Get();
    if (!Dialog || Dialog->GetRequestId() != RequestId)
    {
        return;
    }
    if (bSuccess)
    {
        CloseActiveDialog();
    }
    else
    {
        Dialog->ReceiveRequestFailure(Failure);
    }
}

void UTrainingPlayerComponent::ClientNotify_Implementation(const FText& Message)
{
    UE_LOG(LogTemp, Warning, TEXT("[Training] %s"), *Message.ToString());
    OnTrainingMessage.Broadcast(Message);
}

void UTrainingPlayerComponent::ClearPendingRequest()
{
    PendingInteractable.Reset();
    PendingRequestId = 0;
    PendingSessionId = 0;
}

void UTrainingPlayerComponent::ServerCancel_Implementation(const int32 RequestId)
{
    if (PendingRequestId == RequestId)
    {
        ClearPendingRequest();
    }
}

void UTrainingPlayerComponent::DismissDialog(const int32 RequestId)
{
    ServerCancel(RequestId);
    if (ActiveDialog.IsValid() && ActiveDialog->GetRequestId() == RequestId)
    {
        CloseActiveDialog();
    }
}

void UTrainingPlayerComponent::NotifyDialogRemoved(UTrainingDialogWidget& Dialog)
{
    // Called from destruction: do not re-enter RemoveWidgetFromLayer while it is removing this widget.
    ServerCancel(Dialog.GetRequestId());
    if (ActiveDialog.Get() == &Dialog)
    {
        ActiveDialog.Reset();
    }
    Dialog.DetachRequest();
}

void UTrainingPlayerComponent::CloseActiveDialog()
{
    UTrainingDialogWidget* Dialog = ActiveDialog.Get();
    ActiveDialog.Reset();
    if (Dialog)
    {
        Dialog->DetachRequest(); // Deactivation must not issue another cancel for a newer request.
        if (UShooterUIManagerSubsystem* UIManager = GetUIManager())
        {
            UIManager->RemoveWidgetFromLayer(UITags::UI_Layer_Modal, Dialog);
        }
        else
        {
            Dialog->DeactivateWidget();
        }
    }
}

void UTrainingPlayerComponent::HandleSeamlessTravel(UWorld* DepartingWorld, const FString&)
{
    HandleWorldLeaving(DepartingWorld);
}

void UTrainingPlayerComponent::HandleWorldLeaving(UWorld* DepartingWorld)
{
    if (!DepartingWorld || DepartingWorld != GetWorld())
    {
        return;
    }
    if (APlayerController* ParticipantPlayer = GetParticipantController())
    {
        if (ParticipantPlayer->HasAuthority())
        {
            if (UTrainingSessionSubsystem* Sessions = DepartingWorld->GetSubsystem<UTrainingSessionSubsystem>())
            {
                Sessions->EndSessionForParticipant(ParticipantPlayer);
            }
        }
    }
    CloseActiveDialog();
    ClearPendingRequest();
}

void UTrainingPlayerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    FWorldDelegates::OnWorldBeginTearDown.Remove(WorldTearDownHandle);
    FWorldDelegates::OnSeamlessTravelStart.Remove(SeamlessTravelHandle);
    WorldTearDownHandle.Reset();
    SeamlessTravelHandle.Reset();
    HandleWorldLeaving(GetWorld());
    Super::EndPlay(EndPlayReason);
}

#undef LOCTEXT_NAMESPACE
