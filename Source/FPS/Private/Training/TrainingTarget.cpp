#include "Training/TrainingTarget.h"

#include "GameFramework/CharacterMovementComponent.h"

ATrainingTarget::ATrainingTarget()
{
    AIControllerClass = nullptr;
    AutoPossessAI = EAutoPossessAI::Disabled;
    AutoPossessPlayer = EAutoReceiveInput::Disabled;
    GetCharacterMovement()->DefaultLandMovementMode = MOVE_None;
    GetCharacterMovement()->DefaultWaterMovementMode = MOVE_None;
    GetCharacterMovement()->bRunPhysicsWithNoController = false;
}

void ATrainingTarget::PostInitializeComponents()
{
    AIControllerClass = nullptr;
    AutoPossessAI = EAutoPossessAI::Disabled;
    AutoPossessPlayer = EAutoReceiveInput::Disabled;
    Super::PostInitializeComponents();
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    GetCharacterMovement()->SetComponentTickEnabled(false);
}

void ATrainingTarget::SpawnDefaultController()
{
}
