#include "Training/TrainingPlayerComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Player/TrainingPlayerController.h"
#include "Tags/UITags.h"
#include "Training/TrainingArena.h"
#include "Training/CQB/CQBTrainingEndActor.h"
#include "Training/CQB/CQBTrainingPlayerSpawnPoint.h"
#include "Training/CQB/CQBTrainingSpawner.h"
#include "Training/TrainingInteractionActor.h"
#include "Training/TrainingSessionSubsystem.h"
#include "Training/TrainingSettings.h"
#include "Training/UI/TrainingDialogWidget.h"
#include "Training/UI/CQBTrainingDialogWidget.h"
#include "Training/UI/TrainingSessionHUDWidget.h"
#include "Training/UI/TrainingResultsWidget.h"
#include "Training/UI/TrainingPlayerPresentation.h"
#include "UI/TrainingWidgetStyle.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"

#define LOCTEXT_NAMESPACE "TrainingPlayer"

UTrainingPlayerComponent::UTrainingPlayerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UTrainingPlayerComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!GetParticipantController())
    {
        return;
    }
    Presentation = NewObject<UTrainingPlayerPresentation>(this);
    if (Presentation)
    {
        Presentation->Initialize(*this);
    }
    WorldTearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &ThisClass::HandleWorldLeaving);
    SeamlessTravelHandle = FWorldDelegates::OnSeamlessTravelStart.AddUObject(this, &ThisClass::HandleSeamlessTravel);
}

int32 UTrainingPlayerComponent::AllocateRequestId()
{
    RequestSequence = RequestSequence == MAX_int32 ? 1 : RequestSequence + 1;
    return RequestSequence;
}

APlayerController* UTrainingPlayerComponent::GetParticipantController() const
{
    ATrainingPlayerController* Controller = Cast<ATrainingPlayerController>(GetOwner());
    return Controller && Controller->IsLocalController() && GetWorld()
        && GetWorld()->GetNetMode() == NM_Standalone ? Controller : nullptr;
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
    if (!ParticipantPlayer || IsAreaTransitionBlockingInput())
    {
        return;
    }

    const bool bEntrance = Interactable.InteractionRole == ETrainingInteractionRole::Entrance;
    ATrainingArena* RequestedArena = Interactable.Arena;
    if ((bEntrance && !RequestedArena && !Interactable.CQBEntryPoint) || (!bEntrance && !RequestedArena))
    {
        return;
    }

    UTrainingSessionSubsystem* Sessions = GetWorld()->GetSubsystem<UTrainingSessionSubsystem>();
    if (!Sessions)
    {
        return;
    }

    if (IsCQBTrainingActive())
    {
        NotifyMessage(LOCTEXT("CQBActive", "End CQB training before entering the Training Arena."));
        return;
    }

    const bool bSessionActive = Sessions->GetState() != ETrainingSessionState::Idle;
    ETrainingDialogKind Kind = ETrainingDialogKind::EnterArena;
    if (bEntrance)
    {
        if (bSessionActive)
        {
            NotifyMessage(LOCTEXT("ArenaBusy", "A training session is already active. End it before entering again."));
            return;
        }
    }
    else if (bSessionActive)
    {
        if (Sessions->GetActiveArena() != RequestedArena)
        {
            NotifyMessage(LOCTEXT("OtherArena", "End the current training session before using another arena."));
            return;
        }
        Kind = ETrainingDialogKind::EndSession;
    }
    else
    {
        if (!ParticipantPlayer->GetPawn() || !RequestedArena->IsInMovementArea(*ParticipantPlayer->GetPawn()))
        {
            NotifyMessage(LOCTEXT("NotInside", "Enter the central training area first."));
            return;
        }
        Kind = ETrainingDialogKind::SelectMode;
    }

    CancelCQBDialogRequest(PendingCQBRequestId);
    CloseActiveCQBDialog();
    PendingRequestId = AllocateRequestId();
    PendingSessionId = Sessions->GetSessionId();
    PendingKind = Kind;
    PendingInteractable = &Interactable;
    ShowDialog(Kind, PendingRequestId, Interactable.InteractionSound, Interactable.InteractionSoundVolume,
        Interactable.DialogWidgetClass, BuildLoadoutSnapshot());
}

void UTrainingPlayerComponent::ShowDialog(const ETrainingDialogKind Kind, const int32 RequestId,
    USoundBase* InteractionSound, const float SoundVolume, TSubclassOf<UTrainingDialogWidget> WidgetClassOverride,
    const FTrainingLoadoutSnapshot& LoadoutSnapshot)
{
    CloseResults();
    CloseActiveDialog();
    UShooterUIManagerSubsystem* UIManager = GetUIManager();
    TSubclassOf<UTrainingDialogWidget> WidgetClass = WidgetClassOverride;
    if (!WidgetClass)
    {
        if (Kind == ETrainingDialogKind::SelectMode)
        {
            WidgetClass = ModeSelectionWidgetClass;
        }
        else if (Kind == ETrainingDialogKind::EndSession)
        {
            WidgetClass = EndSessionWidgetClass ? EndSessionWidgetClass : ConfirmationWidgetClass;
        }
        else
        {
            WidgetClass = ConfirmationWidgetClass;
        }
    }
    if (!UIManager || !UIManager->IsUIInitialized() || !WidgetClass)
    {
        NotifyMessage(LOCTEXT("UISetup", "Training UI is not configured: initialize the existing PrimaryGameLayout and assign both Training widget classes."));
        CancelDialogRequest(RequestId);
        return;
    }
    UTrainingDialogWidget* NewDialog = Cast<UTrainingDialogWidget>(
        UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, WidgetClass));
    if (!NewDialog)
    {
        NotifyMessage(LOCTEXT("NoLayer", "The existing PrimaryGameLayout could not open the Training modal."));
        CancelDialogRequest(RequestId);
        return;
    }
    ActiveDialog = NewDialog;


    NewDialog->SetAppliedLoadout(LoadoutSnapshot);
    NewDialog->InitializeDialog(*this, Kind, RequestId);
    USoundBase* OpenSound = InteractionSound ? InteractionSound : WidgetOpenSound.Get();
    if (OpenSound) { UGameplayStatics::PlaySound2D(this, OpenSound, InteractionSound ? FMath::Max(0.0f, SoundVolume) : 1.0f); }
}

bool UTrainingPlayerComponent::ValidatePendingRequest(const int32 RequestId, FText& OutFailure) const
{
    const ATrainingInteractionActor* Interactable = PendingInteractable.Get();
    const bool bHasDestination = Interactable && (Interactable->InteractionRole == ETrainingInteractionRole::Entrance
        ? (IsValid(Interactable->Arena) || IsValid(Interactable->CQBEntryPoint))
        : IsValid(Interactable->Arena));
    if (RequestId <= 0 || RequestId != PendingRequestId || !bHasDestination
        || !CanReachPendingTrainingActor(Interactable))
    {
        OutFailure = LOCTEXT("Expired", "This request is no longer valid. Close the dialog and interact again.");
        return false;
    }
    return true;
}

void UTrainingPlayerComponent::SubmitConfirmation(const int32 RequestId)
{
    FText Failure;
    if (!ValidatePendingRequest(RequestId, Failure))
    {
        ResolveDialogRequest(RequestId, false, Failure);
        return;
    }
    APlayerController* ParticipantPlayer = GetParticipantController();
    UTrainingSessionSubsystem* Sessions = GetWorld()->GetSubsystem<UTrainingSessionSubsystem>();
    bool bSuccess = false;
    if (PendingKind == ETrainingDialogKind::EnterArena)
    {
        SubmitTrainingDestination(RequestId, ETrainingDestination::Arena);
        return;
    }
    else if (PendingKind == ETrainingDialogKind::EndSession && ParticipantPlayer && Sessions)
    {
        bSuccess = Sessions->RequestEndSession(PendingSessionId);
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
    ResolveDialogRequest(RequestId, bSuccess, Failure);
}

bool UTrainingPlayerComponent::ResolveModePreview(const ETrainingMode Mode,
    FTrainingModeConfig& OutConfig) const
{
    const ATrainingInteractionActor* Interactable = PendingInteractable.Get();
    return TrainingModeDefinitions::Resolve(Mode, OutConfig, Interactable ? Interactable->Arena.Get() : nullptr);
}

void UTrainingPlayerComponent::SubmitMode(const int32 RequestId, const ETrainingMode Mode,
    const ETrainingDistancePreset Preset, const float CustomMeters)
{
    FText Failure;
    if (!ValidatePendingRequest(RequestId, Failure))
    {
        ResolveDialogRequest(RequestId, false, Failure);
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
    ResolveDialogRequest(RequestId, bSuccess, Failure);
}


void UTrainingPlayerComponent::ResolveDialogRequest(const int32 RequestId,
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

void UTrainingPlayerComponent::NotifyMessage(const FText& Message)
{
    UE_LOG(LogTemp, Warning, TEXT("[Training] %s"), *Message.ToString());
    OnTrainingMessage.Broadcast(Message);
}

void UTrainingPlayerComponent::NotifySessionPhase(const int32 SessionId,
    const ETrainingFeedbackPhase Phase, const double PhaseStartSeconds,
    const ETrainingMode Mode, const float DurationSeconds)
{
    APlayerController* LocalController = GetParticipantController();
    if (!LocalController || !LocalController->IsLocalController())
    {
        return;
    }

    if (Phase == ETrainingFeedbackPhase::Stopped)
    {
        if (Presentation && Presentation->GetSessionId() == SessionId)
        {
            ClearPendingRequest();
            LastStoppedSessionId = SessionId;
            Presentation->HandlePhase(SessionId, Phase, PhaseStartSeconds, Mode, DurationSeconds);
            CloseActiveDialog();
        }
        return;
    }

    if (Phase == ETrainingFeedbackPhase::Countdown)
    {
        CloseResults();
        LastStoppedSessionId = 0;
    }

    if (Presentation)
    {
        Presentation->HandlePhase(SessionId, Phase, PhaseStartSeconds, Mode, DurationSeconds);
    }
}

void UTrainingPlayerComponent::ClearPendingRequest()
{
    PendingInteractable.Reset();
    PendingRequestId = 0;
    PendingSessionId = 0;
}

void UTrainingPlayerComponent::CancelDialogRequest(const int32 RequestId)
{
    if (PendingRequestId == RequestId)
    {
        ClearPendingRequest();
    }
}

void UTrainingPlayerComponent::DismissDialog(const int32 RequestId)
{
    CancelDialogRequest(RequestId);
    if (ActiveDialog.IsValid() && ActiveDialog->GetRequestId() == RequestId)
    {
        if (WidgetCloseSound) { UGameplayStatics::PlaySound2D(this, WidgetCloseSound); }
        CloseActiveDialog();
    }
}

void UTrainingPlayerComponent::NotifyDialogRemoved(UTrainingDialogWidget& Dialog)
{
    CancelDialogRequest(Dialog.GetRequestId());
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
    ResetTrainingState();
}

void UTrainingPlayerComponent::HandleParticipantDeath()
{
    CancelAreaTransition();
    CancelCQBDialogRequest(PendingCQBRequestId);
    ClearPendingCQBRequest();
    ClearPendingRequest();
    LastStoppedSessionId = 0;
    CloseActiveDialog();
    CloseActiveCQBDialog();
    CloseResults();
    if (Presentation)
    {
        Presentation->Clear(true);
    }
}

void UTrainingPlayerComponent::ResetTrainingState()
{
    if (APlayerController* ParticipantPlayer = GetParticipantController())
    {
        if (UWorld* World = GetWorld())
        {
            if (UTrainingSessionSubsystem* Sessions = World->GetSubsystem<UTrainingSessionSubsystem>())
            {
                Sessions->EndSessionForParticipant(ParticipantPlayer);
            }
        }
    }
    if (ACQBTrainingSpawner* Spawner = ActiveCQBSpawner.Get())
    {
        Spawner->CancelTraining();
    }
    HandleParticipantDeath();
    if (Presentation)
    {
        Presentation->Clear();
    }
    ActiveCQBSpawner.Reset();
    ClearSavedTrainingPosition();
}

void UTrainingPlayerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    FWorldDelegates::OnWorldBeginTearDown.Remove(WorldTearDownHandle);
    FWorldDelegates::OnSeamlessTravelStart.Remove(SeamlessTravelHandle);
    WorldTearDownHandle.Reset();
    SeamlessTravelHandle.Reset();
    HandleWorldLeaving(GetWorld());
    Presentation = nullptr;
    Super::EndPlay(EndPlayReason);
}

#undef LOCTEXT_NAMESPACE
