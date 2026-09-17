#include "Training/AI/TrainingCombatAI.h"

#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Training/AI/TrainingCombatAIController.h"
#include "Training/TrainingArena.h"

ATrainingCombatAI::ATrainingCombatAI()
{
    AIControllerClass = ATrainingCombatAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::Disabled;
    GetCharacterMovement()->bUseControllerDesiredRotation = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0, 540.0, 0.0);
    GetCharacterMovement()->MaxWalkSpeed = 180.0f;
    GetCharacterMovement()->bUseRVOAvoidance = false;
}

bool ATrainingCombatAI::HasConfiguredBehavior() const
{
    const ATrainingCombatAIController* DefaultAIController = AIControllerClass
        ? Cast<ATrainingCombatAIController>(AIControllerClass->GetDefaultObject()) : nullptr;
    return DefaultAIController && DefaultAIController->HasConfiguredStateTree();
}

bool ATrainingCombatAI::InitializeForTraining(ATrainingArena& InArena,
    APlayerController& InParticipant, const FTrainingDistanceBand& InBand)
{
    if (!HasAuthority() || !IsAlive())
    {
        return false;
    }
    TrainingArena = &InArena;
    Participant = &InParticipant;
    DistanceBand = InBand;
    SpawnDefaultController();
    ATrainingCombatAIController* TrainingController = Cast<ATrainingCombatAIController>(GetController());
    return TrainingController && TrainingController->StartTrainingBrain();
}

ATrainingArena* ATrainingCombatAI::GetTrainingArena() const { return TrainingArena.Get(); }
APlayerController* ATrainingCombatAI::GetParticipant() const { return Participant.Get(); }

void ATrainingCombatAI::StopTrainingMotion()
{
    if (ATrainingCombatAIController* TrainingController = Cast<ATrainingCombatAIController>(GetController()))
    {
        TrainingController->StopTrainingBrain();
    }
    Super::StopTrainingMotion();
}

void ATrainingCombatAI::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HasAuthority())
    {
        if (ATrainingCombatAIController* TrainingController = Cast<ATrainingCombatAIController>(GetController()))
        {
            TrainingController->UnPossess();
            TrainingController->Destroy();
        }
    }
    TrainingArena.Reset();
    Participant.Reset();
    Super::EndPlay(EndPlayReason);
}
