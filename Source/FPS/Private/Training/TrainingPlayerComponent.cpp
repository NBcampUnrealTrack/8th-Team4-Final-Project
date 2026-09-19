#include "Training/TrainingPlayerComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Input/ShooterInteractionComponent.h"
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
    SetIsReplicatedByDefault(true);
}

void UTrainingPlayerComponent::BeginPlay()
{
    Super::BeginPlay();
    Presentation = NewObject<UTrainingPlayerPresentation>(this);
    if (Presentation)
    {
        Presentation->Initialize(*this);
    }
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
    if (!ParticipantPlayer || !ParticipantPlayer->HasAuthority())
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

    const bool bSessionActive = Sessions->GetState() != ETrainingSessionState::Idle;
    ETrainingDialogKind Kind = ETrainingDialogKind::EnterArena;
    if (bEntrance)
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
    ClientOpenDialog(Kind, PendingRequestId, Interactable.InteractionSound, Interactable.InteractionSoundVolume,
        Interactable.DialogWidgetClass, BuildLoadoutSnapshot());
}

void UTrainingPlayerComponent::ClientOpenDialog_Implementation(const ETrainingDialogKind Kind, const int32 RequestId,
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
    ClientLoadout = LoadoutSnapshot;
    bHasClientLoadout = true;
    NewDialog->SetAppliedLoadout(LoadoutSnapshot);
    NewDialog->InitializeDialog(*this, Kind, RequestId);
    USoundBase* OpenSound = InteractionSound ? InteractionSound : WidgetOpenSound.Get();
    if (OpenSound) { UGameplayStatics::PlaySound2D(this, OpenSound, InteractionSound ? FMath::Max(0.0f, SoundVolume) : 1.0f); }
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

void UTrainingPlayerComponent::ReturnToPreTrainingPosition(const int32 RequestId)
{
    ServerReturnToPreTrainingPosition(RequestId);
}

void UTrainingPlayerComponent::SubmitTrainingDestination(const int32 RequestId, const ETrainingDestination Destination)
{
    ServerEnterTrainingDestination(RequestId, Destination);
}

void UTrainingPlayerComponent::SavePreTrainingPosition(
    APawn& ParticipantPawn, APlayerController& ParticipantPlayer)
{
    PreTrainingTransform = ParticipantPawn.GetActorTransform();
    PreTrainingControlRotation = ParticipantPlayer.GetControlRotation();
    bHasPreTrainingTransform = true;
}

bool UTrainingPlayerComponent::TeleportParticipant(
    const FTransform& Destination, const FRotator& TargetControlRotation)
{
    APlayerController* ParticipantPlayer = GetParticipantController();
    APawn* ParticipantPawn = ParticipantPlayer ? ParticipantPlayer->GetPawn() : nullptr;
    if (!ParticipantPlayer || !ParticipantPawn) return false;

    const bool bSuccess = ParticipantPawn->TeleportTo(Destination.GetLocation(), Destination.Rotator());
    if (!bSuccess) return false;

    if (ACharacter* ParticipantCharacter = Cast<ACharacter>(ParticipantPawn))
    {
        ParticipantCharacter->GetCharacterMovement()->StopMovementImmediately();
    }
    ParticipantPlayer->SetControlRotation(TargetControlRotation);
    ParticipantPlayer->ClientSetRotation(TargetControlRotation, true);
    ParticipantPawn->ForceNetUpdate();
    return true;
}

bool UTrainingPlayerComponent::TeleportToSavedPreTrainingPosition(
    FText& OutFailure, const bool bClearSavedPosition)
{
    if (!bHasPreTrainingTransform)
    {
        OutFailure = LOCTEXT("CQBOriginMissing", "No saved pre-training position is available.");
        return false;
    }

    const bool bSuccess = TeleportParticipant(PreTrainingTransform, PreTrainingControlRotation);
    if (!bSuccess)
    {
        OutFailure = LOCTEXT("CQBOriginBlocked", "The saved return position is blocked.");
        return false;
    }

    if (bClearSavedPosition)
    {
        ReturnArena.Reset();
        ReturnCQBEntryPoint.Reset();
        PreTrainingTransform = FTransform::Identity;
        PreTrainingControlRotation = FRotator::ZeroRotator;
        bHasPreTrainingTransform = false;
    }
    return true;
}

void UTrainingPlayerComponent::ServerEnterTrainingDestination_Implementation(
    const int32 RequestId, const ETrainingDestination Destination)
{
    FText Failure;
    if (!ValidatePendingRequest(RequestId, Failure) || PendingKind != ETrainingDialogKind::EnterArena)
    {
        ClientRequestResult(RequestId, false, Failure.IsEmpty()
            ? LOCTEXT("DestinationExpired", "Training destination request expired.") : Failure);
        return;
    }

    APlayerController* ParticipantPlayer = GetParticipantController();
    APawn* ParticipantPawn = ParticipantPlayer ? ParticipantPlayer->GetPawn() : nullptr;
    UTrainingSessionSubsystem* Sessions = GetWorld()->GetSubsystem<UTrainingSessionSubsystem>();
    if (!ParticipantPlayer || !ParticipantPawn || !Sessions || Sessions->GetState() != ETrainingSessionState::Idle)
    {
        ClientRequestResult(RequestId, false,
            LOCTEXT("DestinationBusy", "Training is currently unavailable."));
        return;
    }

    FTransform DestinationTransform;
    ATrainingArena* RequestedArena = PendingInteractable->Arena;
    ACQBTrainingPlayerSpawnPoint* CQBEntry = PendingInteractable->CQBEntryPoint;
    if (Destination == ETrainingDestination::Arena)
    {
        if (!RequestedArena)
        {
            ClientRequestResult(RequestId, false,
                LOCTEXT("ArenaDestinationMissing", "Training Arena is not assigned to the entrance actor."));
            return;
        }
        DestinationTransform = RequestedArena->GetEntryTransform();
    }
    else
    {
        if (!CQBEntry)
        {
            ClientRequestResult(RequestId, false,
                LOCTEXT("CQBDestinationMissing", "CQB Entry Point is not assigned to the entrance actor."));
            return;
        }
        DestinationTransform = CQBEntry->GetActorTransform();
    }

    const FTransform PreviousTransform = ParticipantPawn->GetActorTransform();
    const FRotator PreviousControlRotation = ParticipantPlayer->GetControlRotation();
    const bool bSuccess = TeleportParticipant(DestinationTransform, DestinationTransform.Rotator());
    if (bSuccess)
    {
        PreTrainingTransform = PreviousTransform;
        PreTrainingControlRotation = PreviousControlRotation;
        bHasPreTrainingTransform = true;
        if (Destination == ETrainingDestination::Arena)
        {
            ReturnArena = RequestedArena;
            ReturnCQBEntryPoint.Reset();
        }
        else
        {
            ReturnArena.Reset();
            ReturnCQBEntryPoint = CQBEntry;
        }
        ClearPendingRequest();
    }
    else
    {
        Failure = LOCTEXT("DestinationBlocked", "The selected training entry point is blocked.");
    }
    ClientRequestResult(RequestId, bSuccess, Failure);
}

bool UTrainingPlayerComponent::ValidatePendingRequest(const int32 RequestId, FText& OutFailure) const
{
    const UShooterInteractionComponent* Interaction = GetOwner()->FindComponentByClass<UShooterInteractionComponent>();
    const ATrainingInteractionActor* Interactable = PendingInteractable.Get();
    const bool bHasDestination = Interactable && (Interactable->InteractionRole == ETrainingInteractionRole::Entrance
        ? (IsValid(Interactable->Arena) || IsValid(Interactable->CQBEntryPoint))
        : IsValid(Interactable->Arena));
    if (RequestId <= 0 || RequestId != PendingRequestId || !bHasDestination
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
    if (PendingKind == ETrainingDialogKind::EnterArena)
    {
        ServerEnterTrainingDestination_Implementation(RequestId, ETrainingDestination::Arena);
        return;
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


void UTrainingPlayerComponent::ServerReturnToPreTrainingPosition_Implementation(const int32 RequestId)
{
    FText Failure;
    if (!ValidatePendingRequest(RequestId, Failure))
    {
        ClientRequestResult(RequestId, false, Failure);
        return;
    }

    APlayerController* ParticipantPlayer = GetParticipantController();
    ATrainingArena* RequestedArena = PendingInteractable.IsValid() ? PendingInteractable->Arena.Get() : nullptr;
    UTrainingSessionSubsystem* Sessions = GetWorld()->GetSubsystem<UTrainingSessionSubsystem>();
    bool bSuccess = false;

    if (PendingKind != ETrainingDialogKind::SelectMode)
    {
        Failure = LOCTEXT("ReturnStartWrongDialog", "Return to start is only available from the training mode selection screen.");
    }
    else if (!ParticipantPlayer || !RequestedArena || !Sessions)
    {
        Failure = LOCTEXT("ReturnStartUnavailable", "The training return position is no longer available.");
    }
    else if (Sessions->GetState() != ETrainingSessionState::Idle)
    {
        Failure = LOCTEXT("ReturnStartSessionActive", "End the active training session before returning to the start position.");
    }
    else if (!bHasPreTrainingTransform || ReturnArena.Get() != RequestedArena)
    {
        Failure = LOCTEXT("ReturnStartMissing", "No saved pre-training position is available. Leave and enter the Training Arena again.");
    }
    else if (APawn* ParticipantPawn = ParticipantPlayer->GetPawn())
    {
        bSuccess = ParticipantPawn->TeleportTo(
            PreTrainingTransform.GetLocation(), PreTrainingTransform.Rotator());
        if (bSuccess)
        {
            if (ACharacter* ParticipantCharacter = Cast<ACharacter>(ParticipantPawn))
            {
                ParticipantCharacter->GetCharacterMovement()->StopMovementImmediately();
            }
            ParticipantPlayer->SetControlRotation(PreTrainingControlRotation);
            ParticipantPlayer->ClientSetRotation(PreTrainingControlRotation, true);
            ParticipantPawn->ForceNetUpdate();

            ReturnArena.Reset();
            PreTrainingTransform = FTransform::Identity;
            PreTrainingControlRotation = FRotator::ZeroRotator;
            bHasPreTrainingTransform = false;
        }
        else
        {
            Failure = LOCTEXT("ReturnStartBlocked", "The saved pre-training position is blocked.");
        }
    }

    if (bSuccess)
    {
        ClearPendingRequest();
    }
    else if (Failure.IsEmpty())
    {
        Failure = LOCTEXT("ReturnStartFailed", "Could not return to the saved pre-training position.");
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

void UTrainingPlayerComponent::NotifySessionPhase(const int32 SessionId,
    const ETrainingFeedbackPhase Phase, const double PhaseStartServerSeconds,
    const ETrainingMode Mode, const float DurationSeconds)
{
    const APlayerController* ParticipantPlayer = GetParticipantController();
    if (ParticipantPlayer && ParticipantPlayer->HasAuthority())
    {
        if (Phase == ETrainingFeedbackPhase::Stopped) { ClearPendingRequest(); }
        ClientSessionPhase(SessionId, Phase, PhaseStartServerSeconds, Mode, DurationSeconds);
    }
}

void UTrainingPlayerComponent::ClientSessionPhase_Implementation(const int32 SessionId,
    const ETrainingFeedbackPhase Phase, const double PhaseStartServerSeconds,
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
            LastStoppedSessionId = SessionId;
            Presentation->HandlePhase(SessionId, Phase, PhaseStartServerSeconds, Mode, DurationSeconds);
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
        Presentation->HandlePhase(SessionId, Phase, PhaseStartServerSeconds, Mode, DurationSeconds);
    }
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
        if (WidgetCloseSound) { UGameplayStatics::PlaySound2D(this, WidgetCloseSound); }
        CloseActiveDialog();
    }
}

void UTrainingPlayerComponent::NotifyDialogRemoved(UTrainingDialogWidget& Dialog)
{
    ServerCancel(Dialog.GetRequestId());
    if (ActiveDialog.Get() == &Dialog)
    {
        ActiveDialog.Reset();
        bHasClientLoadout = false;
    }
    Dialog.DetachRequest();
}

void UTrainingPlayerComponent::CloseActiveDialog()
{
    UTrainingDialogWidget* Dialog = ActiveDialog.Get();
    ActiveDialog.Reset();
    bHasClientLoadout = false;
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
    CloseActiveCQBDialog();
    CloseResults();
    if (UTrainingSessionHUDWidget* HUD = CQBHUDWidget.Get())
    {
        HUD->RemoveFromParent();
    }
    CQBHUDWidget.Reset();
    if (Presentation)
    {
        Presentation->Clear();
    }
    ClearPendingRequest();
    ClearPendingCQBRequest();
    ActiveCQBSpawner.Reset();
    ReturnArena.Reset();
    ReturnCQBEntryPoint.Reset();
    PreTrainingTransform = FTransform::Identity;
    PreTrainingControlRotation = FRotator::ZeroRotator;
    bHasPreTrainingTransform = false;
    LastStoppedSessionId = 0;
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
