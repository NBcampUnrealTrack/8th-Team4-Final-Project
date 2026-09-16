#include "Player/TrainingPlayerController.h"

#include "Character/ShooterCharacter.h"
#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameMode/TrainingGameMode.h"
#include "Input/ShooterInputConfig.h"
#include "Player/DominationPlayerState.h"
#include "Training/TrainingPlayerComponent.h"
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
    if (!TrainingMode->RequestCombatClass(this, RequestedClass, Failure))
    {
        ClientCombatClassSelectionFailed(Failure);
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
