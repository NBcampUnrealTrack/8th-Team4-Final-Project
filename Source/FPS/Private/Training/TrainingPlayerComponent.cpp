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

bool UTrainingPlayerComponent::UsesStandardEliminationFeedback() const
{
    return !ActiveCQBSpawner.IsValid() || ActiveCQBSpawner->UsesStandardEliminationFeedback();
}

bool UTrainingPlayerComponent::IsCQBTrainingActive() const
{
    return ActiveCQBSpawner.IsValid();
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

void UTrainingPlayerComponent::SubmitCQBStart(const int32 RequestId)
{
    ServerStartCQB(RequestId);
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
    ClientCQBStarted(TotalTroopers, HUDWidgetClass);
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
    const int32 TotalTroopers, TSubclassOf<UTrainingSessionHUDWidget> HUDWidgetClass)
{
    APlayerController* LocalController = GetParticipantController();
    if (!LocalController || !LocalController->IsLocalController()) return;

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
    if (UTrainingSessionHUDWidget* HUD = CQBHUDWidget.Get())
    {
        HUD->RemoveFromParent();
    }
    CQBHUDWidget.Reset();
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

void UTrainingPlayerComponent::ApplyButtonSounds(UUserWidget& Widget) const
{
    TrainingWidgetStyle::ApplyButtonSounds(Widget, ButtonHoverSound, ButtonClickSound);
}

void UTrainingPlayerComponent::NotifySessionHit(const int32 SessionId, const int32 Kills,
    const float Damage, const bool bKilled, const FVector& WorldPosition)
{
    if (GetOwner()->HasAuthority()) { ClientSessionHit(SessionId, Kills, Damage, bKilled, WorldPosition); }
}

void UTrainingPlayerComponent::ClientSessionHit_Implementation(const int32 SessionId, const int32 Kills,
    const float Damage, const bool bKilled, const FVector WorldPosition)
{
    if (Presentation)
    {
        Presentation->HandleHit(SessionId, Kills, Damage, bKilled, WorldPosition);
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
    if (Presentation)
    {
        Presentation->PresentStandaloneDamage(Damage, bKilled, WorldPosition);
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
    if (Presentation)
    {
        Presentation->PlayBotSound(SessionId, Sound, Attenuation, WorldPosition, Volume);
    }
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
    if (!ResultsWidgetClass)
    {
        ClientNotify_Implementation(LOCTEXT("ResultsClassMissing", "Training Results Widget Class is not configured. Assign the existing WBP in the Training Player Component."));
        return;
    }
    UTrainingResultsWidget* NewResults = Cast<UTrainingResultsWidget>(
        UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, ResultsWidgetClass));
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
