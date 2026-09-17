#include "Player/TrainingPlayerController.h"

#include "Character/ShooterCharacter.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameMode/TrainingGameMode.h"
#include "Input/ShooterInputConfig.h"
#include "Player/DominationPlayerState.h"
#include "Training/TrainingPlayerComponent.h"
#include "Training/TrainingSessionSubsystem.h"
#include "UI/Frontend/TrainingUIInitComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogTrainingController, Log, All);

ATrainingPlayerController::ATrainingPlayerController()
{
    UIInitComponent = CreateDefaultSubobject<UTrainingUIInitComponent>(TEXT("TrainingUIInitComponent"));
}

bool ATrainingPlayerController::CanProcessGameplayInput() const
{
    const ADominationPlayerState* ClassState = GetPlayerState<ADominationPlayerState>();
    return Super::CanProcessGameplayInput()
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

void ATrainingPlayerController::SelectCombatClass(const EDominationCombatClass RequestedClass)
{
    ServerSelectCombatClass(RequestedClass);
}

void ATrainingPlayerController::SetPreviewCombatClass(const EDominationCombatClass RequestedClass)
{
    ServerSetPreviewCombatClass(RequestedClass);
}

float ATrainingPlayerController::GetCombatClassSelectionTimeRemaining() const
{
    return -1.0f;
}

void ATrainingPlayerController::RequestTrainingClassSelection()
{
    if (!HasAuthority())
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
        ClientCombatClassSelectionFailed(NSLOCTEXT("TrainingGameMode", "ClassChangeUnavailable",
            "Class selection is not available right now."));
        return;
    }

    if (!Sessions || Sessions->GetState() != ETrainingSessionState::Idle)
    {
        ClientCombatClassSelectionFailed(NSLOCTEXT("TrainingGameMode", "ClassChangeDuringSession",
            "End the current training session before changing class."));
        return;
    }

    ClientOpenTrainingClassSelection();
}

void ATrainingPlayerController::ClientOpenTrainingClassSelection_Implementation()
{
    if (!IsValid(UIInitComponent) || !UIInitComponent->OpenClassSelectionForTraining())
    {
        UE_LOG(LogTrainingController, Error, TEXT("Could not open the Training class selection screen."));
    }
}

void ATrainingPlayerController::ServerSelectCombatClass_Implementation(const EDominationCombatClass RequestedClass)
{
    ATrainingGameMode* TrainingMode = GetWorld()
        ? GetWorld()->GetAuthGameMode<ATrainingGameMode>() : nullptr;
    FText Failure;
    if (!IsValid(TrainingMode))
    {
        ClientCombatClassSelectionFailed(NSLOCTEXT("TrainingGameMode", "WrongGameMode",
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
        ClientCombatClassSelectionFailed(Failure);
    }
    else if (bChangingActiveClass)
    {
        ClientTrainingClassSelectionSucceeded();
    }
}

void ATrainingPlayerController::ClientTrainingClassSelectionSucceeded_Implementation()
{
    if (IsValid(UIInitComponent))
    {
        UIInitComponent->CloseClassSelectionForTraining();
    }
}

void ATrainingPlayerController::ServerSetPreviewCombatClass_Implementation(const EDominationCombatClass RequestedClass)
{
    if (ATrainingGameMode* TrainingMode = GetWorld()->GetAuthGameMode<ATrainingGameMode>())
    {
        TrainingMode->RequestPreviewCombatClass(this, RequestedClass);
    }
}

void ATrainingPlayerController::ClientCombatClassSelectionFailed_Implementation(const FText& Reason)
{
    UE_LOG(LogTrainingController, Error, TEXT("Training class selection: %s"), *Reason.ToString());
    UIInitComponent->NotifyClassSelectionRequestFailed(Reason);
    if (UTrainingPlayerComponent* TrainingUI = FindComponentByClass<UTrainingPlayerComponent>())
    {
        TrainingUI->OnTrainingMessage.Broadcast(Reason);
    }
}
