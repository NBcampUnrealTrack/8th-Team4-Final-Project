#include "Training/TrainingPlayerComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "Input/ShooterInteractionComponent.h"
#include "Tags/UITags.h"
#include "Training/TrainingArena.h"
#include "Training/TrainingInteractionActor.h"
#include "Training/TrainingSessionSubsystem.h"
#include "Training/TrainingSettings.h"
#include "Training/UI/TrainingDamageFeedbackWidget.h"
#include "Training/UI/TrainingDialogWidget.h"
#include "Training/UI/TrainingStartFeedbackWidget.h"
#include "Training/UI/TrainingSessionHUDWidget.h"
#include "Training/UI/TrainingResultsWidget.h"
#include "UI/TrainingWidgetLayout.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
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
    ClientOpenDialog(Kind, PendingRequestId, Interactable.InteractionSound, Interactable.InteractionSoundVolume, BuildLoadoutSnapshot());
}

void UTrainingPlayerComponent::ClientOpenDialog_Implementation(const ETrainingDialogKind Kind, const int32 RequestId,
    USoundBase* InteractionSound, const float SoundVolume, const FTrainingLoadoutSnapshot& LoadoutSnapshot)
{
    CloseResults();
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
            const FTransform PreviousTransform = ParticipantPawn->GetActorTransform();
            const FRotator PreviousControlRotation = ParticipantPlayer->GetControlRotation();
            const FTransform EntryTransform = RequestedArena->GetEntryTransform();
            bSuccess = ParticipantPawn->TeleportTo(EntryTransform.GetLocation(), EntryTransform.Rotator());
            if (bSuccess)
            {
                ReturnArena = RequestedArena;
                PreTrainingTransform = PreviousTransform;
                PreTrainingControlRotation = PreviousControlRotation;
                bHasPreTrainingTransform = true;

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
    if (!LocalController || !LocalController->IsLocalController()) { return; }
    if (Phase == ETrainingFeedbackPhase::Stopped)
    {
        if (SessionId == FeedbackSessionId)
        {
            LastStoppedSessionId = SessionId;
            ClearSessionFeedback();
            CloseActiveDialog();
        }
        return;
    }
    if (Phase == ETrainingFeedbackPhase::Countdown)
    {
        ClearSessionFeedback();
        CloseResults();
        LastStoppedSessionId = 0;
        FeedbackSessionId = SessionId;
        FeedbackMode = Mode;
    }
    else if (FeedbackSessionId != SessionId)
    {
        return;
    }
    if (Phase == ETrainingFeedbackPhase::Started)
    {
        if (!SessionHUDWidget)
        {
            UClass* HUDClass = SessionHUDWidgetClass ? SessionHUDWidgetClass.Get() : UTrainingSessionHUDWidget::StaticClass();
            SessionHUDWidget = CreateWidget<UTrainingSessionHUDWidget>(LocalController, HUDClass);
        }
        if (SessionHUDWidget)
        {
            if (!SessionHUDWidget->IsInViewport())
            {
                SessionHUDWidget->AddToPlayerScreen(GetDefault<UTrainingSettings>()->SessionHUDZOrder);
            }
            SessionHUDWidget->StartSession(Mode, PhaseStartServerSeconds, DurationSeconds);
        }
    }
    if (!StartFeedbackWidget)
    {
        UClass* FeedbackClass = StartFeedbackWidgetClass
            ? StartFeedbackWidgetClass.Get() : UTrainingStartFeedbackWidget::StaticClass();
        StartFeedbackWidget = CreateWidget<UTrainingStartFeedbackWidget>(LocalController, FeedbackClass);
    }
    if (!StartFeedbackWidget)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Training] Could not create the start feedback widget."));
        return;
    }
    if (!StartFeedbackWidget->IsInViewport()
        && !StartFeedbackWidget->AddToPlayerScreen(GetDefault<UTrainingSettings>()->StartFeedbackZOrder))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Training] Could not add start feedback to the local player's viewport."));
        return;
    }
    const AGameStateBase* WorldGameState = GetWorld()->GetGameState();
    const float Elapsed = WorldGameState
        ? static_cast<float>(FMath::Max(0.0, WorldGameState->GetServerWorldTimeSeconds() - PhaseStartServerSeconds))
        : 0.0f;
    if (Phase == ETrainingFeedbackPhase::Countdown)
    {
        StartFeedbackWidget->ShowCountdown(Elapsed);
    }
    else
    {
        StartFeedbackWidget->ShowStarted(Elapsed);
    }
}

void UTrainingPlayerComponent::ClearSessionFeedback()
{
    if (SessionHUDWidget)
    {
        SessionHUDWidget->RemoveFromParent();
        SessionHUDWidget = nullptr;
    }
    if (StartFeedbackWidget)
    {
        StartFeedbackWidget->ClearFeedback();
        StartFeedbackWidget->RemoveFromParent();
        StartFeedbackWidget = nullptr;
    }
    FeedbackSessionId = 0;
}

void UTrainingPlayerComponent::ClearTrainingDamageFeedback()
{
    if (DamageFeedbackWidget)
    {
        DamageFeedbackWidget->RemoveFromParent();
        DamageFeedbackWidget = nullptr;
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
    CloseResults();
    ClearSessionFeedback();
    ClearTrainingDamageFeedback();
    ClearPendingRequest();
    ReturnArena.Reset();
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
    Super::EndPlay(EndPlayReason);
}

void UTrainingPlayerComponent::ApplyButtonSounds(UUserWidget& Widget) const
{
    TrainingWidgetLayout::ApplyButtonSounds(Widget, ButtonHoverSound, ButtonClickSound);
}

void UTrainingPlayerComponent::NotifySessionHit(const int32 SessionId, const int32 Kills,
    const float Damage, const bool bKilled, const FVector& WorldPosition)
{
    if (GetOwner()->HasAuthority()) { ClientSessionHit(SessionId, Kills, Damage, bKilled, WorldPosition); }
}

void UTrainingPlayerComponent::ClientSessionHit_Implementation(const int32 SessionId, const int32 Kills,
    const float Damage, const bool bKilled, const FVector WorldPosition)
{
    if (SessionId != FeedbackSessionId)
    {
        return;
    }

    if (SessionHUDWidget)
    {
        SessionHUDWidget->PresentHit(Kills, bKilled);
    }

    if (FeedbackMode == ETrainingMode::CombatResponse)
    {
        PresentDamageFeedback(Damage, bKilled, WorldPosition);
    }
}

void UTrainingPlayerComponent::NotifyTrainingDamage(const float Damage, const bool bKilled,
    const FVector& WorldPosition)
{
    if (GetOwner()->HasAuthority() && FMath::IsFinite(Damage) && Damage > 0.0f)
    {
        ClientTrainingDamage(Damage, bKilled, WorldPosition);
    }
}

void UTrainingPlayerComponent::ClientTrainingDamage_Implementation(const float Damage, const bool bKilled,
    const FVector WorldPosition)
{
    APlayerController* LocalController = GetParticipantController();
    if (!LocalController || !LocalController->IsLocalController())
    {
        return;
    }

    PresentDamageFeedback(Damage, bKilled, WorldPosition);
}

UTrainingDamageFeedbackWidget* UTrainingPlayerComponent::EnsureDamageFeedbackWidget()
{
    APlayerController* LocalController = GetParticipantController();
    if (!LocalController || !LocalController->IsLocalController())
    {
        return nullptr;
    }

    if (!DamageFeedbackWidget)
    {
        UClass* FeedbackClass = DamageFeedbackWidgetClass
            ? DamageFeedbackWidgetClass.Get()
            : UTrainingDamageFeedbackWidget::StaticClass();
        DamageFeedbackWidget = CreateWidget<UTrainingDamageFeedbackWidget>(LocalController, FeedbackClass);
    }

    if (DamageFeedbackWidget && !DamageFeedbackWidget->IsInViewport())
    {
        DamageFeedbackWidget->AddToPlayerScreen(GetDefault<UTrainingSettings>()->DamageFeedbackZOrder);
    }

    return DamageFeedbackWidget;
}

void UTrainingPlayerComponent::PresentDamageFeedback(const float Damage, const bool bKilled,
    const FVector& WorldPosition)
{
    if (!FMath::IsFinite(Damage) || Damage <= 0.0f)
    {
        return;
    }

    if (UTrainingDamageFeedbackWidget* Feedback = EnsureDamageFeedbackWidget())
    {
        Feedback->PresentDamage(Damage, bKilled, WorldPosition);
    }
}

void UTrainingPlayerComponent::NotifyBotSound(const int32 SessionId, USoundBase* Sound,
    USoundAttenuation* Attenuation, const FVector& WorldPosition, const float Volume)
{
    if (GetOwner()->HasAuthority() && Sound && Volume > 0.0f)
    {
        ClientPlayBotSound(SessionId, Sound, Attenuation, WorldPosition, Volume);
    }
}

void UTrainingPlayerComponent::ClientPlayBotSound_Implementation(const int32 SessionId, USoundBase* Sound,
    USoundAttenuation* Attenuation, const FVector WorldPosition, const float Volume)
{
    APlayerController* LocalController = GetParticipantController();
    if (!LocalController || !LocalController->IsLocalController() || SessionId != FeedbackSessionId || !Sound) { return; }
    if (!Attenuation)
    {
        if (!DefaultBotAttenuation)
        {
            DefaultBotAttenuation = NewObject<USoundAttenuation>(this);
            FSoundAttenuationSettings& Settings = DefaultBotAttenuation->Attenuation;
            Settings.bAttenuate = true;
            Settings.bSpatialize = true;
            Settings.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
            Settings.bEnableListenerFocus = false;
            Settings.bEnableOcclusion = false;
            Settings.bApplyNormalizationToStereoSounds = true;
            Settings.SpatializationAlgorithm = SPATIALIZATION_Default;
            Settings.AttenuationShape = EAttenuationShape::Sphere;
            Settings.AttenuationShapeExtents = FVector(1000.0, 0.0, 0.0);
            Settings.FalloffDistance = 6000.0f;
            Settings.NonSpatializedRadiusStart = 0.0f;
            Settings.NonSpatializedRadiusEnd = 0.0f;
            Settings.StereoSpread = 0.0f;
        }
        Attenuation = DefaultBotAttenuation;
    }
    UGameplayStatics::PlaySoundAtLocation(this, Sound, WorldPosition, FRotator::ZeroRotator,
        FMath::Clamp(Volume, 0.0f, 4.0f), 1.0f, 0.0f, Attenuation);
}

void UTrainingPlayerComponent::NotifySessionFinished(const FTrainingSessionResult& Result)
{
    if (GetOwner()->HasAuthority()) { ClientSessionFinished(Result); }
}

void UTrainingPlayerComponent::ClientSessionFinished_Implementation(const FTrainingSessionResult& Result)
{
    APlayerController* LocalController = GetParticipantController();
    if (!LocalController || !LocalController->IsLocalController() || Result.SessionId != LastStoppedSessionId) { return; }
    CloseActiveDialog();
    CloseResults();
    UShooterUIManagerSubsystem* UIManager = GetUIManager();
    if (!UIManager || !UIManager->IsUIInitialized())
    {
        ClientNotify_Implementation(LOCTEXT("ResultsNoLayout", "Training results require the existing PrimaryGameLayout modal layer."));
        return;
    }
    UClass* ResultClass = ResultsWidgetClass ? ResultsWidgetClass.Get() : UTrainingResultsWidget::StaticClass();
    UTrainingResultsWidget* NewResults = Cast<UTrainingResultsWidget>(
        UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, ResultClass));
    if (!NewResults)
    {
        ClientNotify_Implementation(LOCTEXT("ResultsFailed", "Could not create Training Results. Check Results Widget Class and the Modal layer."));
        return;
    }
    ResultsWidget = NewResults;
    NewResults->InitializeResults(*this, Result);
    if (WidgetOpenSound) { UGameplayStatics::PlaySound2D(this, WidgetOpenSound); }
}

void UTrainingPlayerComponent::DismissResults(UTrainingResultsWidget& Widget)
{
    if (ResultsWidget.Get() == &Widget) { CloseResults(true); }
}

void UTrainingPlayerComponent::NotifyResultsRemoved(UTrainingResultsWidget& Widget)
{
    if (ResultsWidget.Get() == &Widget) { ResultsWidget.Reset(); }
    Widget.DetachBridge();
}

void UTrainingPlayerComponent::CloseResults(const bool bPlayCloseSound)
{
    UTrainingResultsWidget* CurrentResults = ResultsWidget.Get();
    ResultsWidget.Reset();
    if (!CurrentResults) { return; }
    CurrentResults->DetachBridge();
    if (bPlayCloseSound && WidgetCloseSound) { UGameplayStatics::PlaySound2D(this, WidgetCloseSound); }
    if (UShooterUIManagerSubsystem* UIManager = GetUIManager())
    {
        UIManager->RemoveWidgetFromLayer(UITags::UI_Layer_Modal, CurrentResults);
    }
    else { CurrentResults->DeactivateWidget(); }
}

#undef LOCTEXT_NAMESPACE
