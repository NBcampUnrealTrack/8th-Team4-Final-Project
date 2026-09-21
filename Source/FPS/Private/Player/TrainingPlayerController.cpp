#include "Player/TrainingPlayerController.h"

#include "Character/ShooterCharacter.h"
#include "Camera/PlayerCameraManager.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameMode/TrainingGameMode.h"
#include "Input/ShooterInteractionComponent.h"
#include "Input/ShooterInputConfig.h"
#include "Player/DominationPlayerState.h"
#include "Training/TrainingPlayerComponent.h"
#include "Training/TrainingSessionSubsystem.h"
#include "Training/TrainingSettings.h"
#include "UI/Frontend/TrainingUIInitComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogTrainingController, Log, All);

ATrainingPlayerController::ATrainingPlayerController()
{
    UIInitComponent = CreateDefaultSubobject<UTrainingUIInitComponent>(TEXT("TrainingUIInitComponent"));
}

void ATrainingPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (!GetWorld() || GetWorld()->GetNetMode() != NM_Standalone || !IsLocalController())
    {
        return;
    }

    const UTrainingSettings* Settings = GetDefault<UTrainingSettings>();
    UShooterInteractionComponent* Interaction = FindComponentByClass<UShooterInteractionComponent>();
    if (!Settings || !Interaction)
    {
        return;
    }

    Interaction->SetInteractionPromptZOrder(Settings->InteractionPromptZOrder);
    Interaction->SetInteractionPromptWidgetClass(Settings->InteractionPromptWidgetClass.LoadSynchronous());
}

void ATrainingPlayerController::OnUnPossess()
{
    if (UWorld* World = GetWorld())
    {
        if (UTrainingSessionSubsystem* Session = World->GetSubsystem<UTrainingSessionSubsystem>())
        {
            Session->EndSessionForParticipant(this);
        }
    }
    Super::OnUnPossess();
}

bool ATrainingPlayerController::CanProcessGameplayInput() const
{
    const ADominationPlayerState* ClassState = GetPlayerState<ADominationPlayerState>();
    return GetWorld() && GetWorld()->GetNetMode() == NM_Standalone
        && IsLocalController() && Super::CanProcessGameplayInput()
        && IsValid(ClassState) && ClassState->HasSelectedCombatClass()
        && ClassState->GetParticipationState() == EPlayerParticipationState::Active
        && ClassState->IsAlive() && IsValid(GetPawn<AShooterCharacter>());
}

void ATrainingPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
    if (EnhancedInput && InputConfig && InputConfig->ToggleAugmentSelectAction)
    {
        EnhancedInput->BindAction(InputConfig->ToggleAugmentSelectAction, ETriggerEvent::Started,
            this, &ThisClass::Input_ToggleAugmentSelect);
    }
}

void ATrainingPlayerController::Input_ToggleAugmentSelect()
{
    UIInitComponent->ToggleAugmentSelect();
}

float ATrainingPlayerController::GetCombatClassSelectionTimeRemaining() const
{
    return -1.0f;
}

void ATrainingPlayerController::RequestTrainingClassSelection()
{
    if (!GetWorld() || GetWorld()->GetNetMode() != NM_Standalone || !IsLocalController())
    {
        return;
    }

    const ADominationPlayerState* ClassState = GetPlayerState<ADominationPlayerState>();
    const UTrainingSessionSubsystem* Sessions = GetWorld()
        ? GetWorld()->GetSubsystem<UTrainingSessionSubsystem>()
        : nullptr;

    if (!IsValid(ClassState)
        || !ClassState->HasSelectedCombatClass()
        || !ClassState->IsAlive()
        || ClassState->GetParticipationState() != EPlayerParticipationState::Active)
    {
        CombatClassSelectionFailed(NSLOCTEXT("TrainingGameMode", "ClassChangeUnavailable",
            "Class selection is not available right now."));
        return;
    }

    const UTrainingPlayerComponent* TrainingBridge = FindComponentByClass<UTrainingPlayerComponent>();
    if (!Sessions || Sessions->GetState() != ETrainingSessionState::Idle
        || (TrainingBridge && TrainingBridge->IsCQBTrainingActive()))
    {
        CombatClassSelectionFailed(NSLOCTEXT("TrainingGameMode", "ClassChangeDuringSession",
            "End the current training session before changing class."));
        return;
    }

    OpenTrainingClassSelection();
}

void ATrainingPlayerController::OpenTrainingClassSelection()
{
    if (!IsValid(UIInitComponent) || !UIInitComponent->OpenClassSelectionForTraining())
    {
        UE_LOG(LogTrainingController, Error, TEXT("Could not open the Training class selection screen."));
    }
}

void ATrainingPlayerController::SelectCombatClass(const EDominationCombatClass RequestedClass)
{
    ATrainingGameMode* TrainingMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ATrainingGameMode>() : nullptr;
    FText Failure;
    if (!IsValid(TrainingMode))
    {
        CombatClassSelectionFailed(NSLOCTEXT("TrainingGameMode", "WrongGameMode",
            "This controller requires TrainingGameMode. Check the level's GameMode Override."));
        return;
    }

    const ADominationPlayerState* ClassState = GetPlayerState<ADominationPlayerState>();
    const bool bChangingActiveClass = IsValid(ClassState)
        && ClassState->HasSelectedCombatClass()
        && ClassState->GetParticipationState() == EPlayerParticipationState::Active;
    const bool bSuccess = bChangingActiveClass
        ? TrainingMode->RequestTrainingCombatClass(this, RequestedClass, Failure)
        : TrainingMode->RequestCombatClass(this, RequestedClass, Failure);

    if (!bSuccess)
    {
        CombatClassSelectionFailed(Failure);
    }
    else if (bChangingActiveClass)
    {
        TrainingClassSelectionSucceeded();
    }
}

void ATrainingPlayerController::TrainingClassSelectionSucceeded()
{
    if (IsValid(UIInitComponent))
    {
        UIInitComponent->CloseClassSelectionForTraining();
    }
    if (UTrainingPlayerComponent* TrainingUI = FindComponentByClass<UTrainingPlayerComponent>())
    {
        TrainingUI->ResumeCQBStartDialogAfterClassSelection();
    }
}

void ATrainingPlayerController::SetPreviewCombatClass(const EDominationCombatClass RequestedClass)
{
    ATrainingGameMode* TrainingMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ATrainingGameMode>() : nullptr;
    if (IsValid(TrainingMode))
    {
        TrainingMode->RequestPreviewCombatClass(this, RequestedClass);
    }
}

void ATrainingPlayerController::CombatClassSelectionFailed(const FText& Reason)
{
    UE_LOG(LogTrainingController, Error, TEXT("Training class selection: %s"), *Reason.ToString());
    if (IsValid(UIInitComponent))
    {
        UIInitComponent->NotifyClassSelectionRequestFailed(Reason);
    }
    if (UTrainingPlayerComponent* TrainingUI = FindComponentByClass<UTrainingPlayerComponent>())
    {
        TrainingUI->OnTrainingMessage.Broadcast(Reason);
    }
}

void ATrainingPlayerController::BeginRespawnFade(const float FadeOutSeconds)
{
    if (PlayerCameraManager)
    {
        PlayerCameraManager->StartCameraFade(
            0.0f, 1.0f, FMath::Max(0.0f, FadeOutSeconds), FLinearColor::Black, false, true);
    }
}

void ATrainingPlayerController::FinishRespawnFade(const float FadeInSeconds)
{
    if (PlayerCameraManager)
    {
        PlayerCameraManager->StartCameraFade(
            1.0f, 0.0f, FMath::Max(0.0f, FadeInSeconds), FLinearColor::Black, false, false);
    }
}
