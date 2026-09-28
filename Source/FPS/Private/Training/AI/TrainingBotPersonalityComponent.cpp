#include "Training/AI/TrainingBotPersonalityComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Training/AI/TrainingCombatAI.h"
#include "Training/AI/TrainingCombatAIController.h"
#include "Training/TrainingArena.h"

UTrainingBotPersonalityComponent::UTrainingBotPersonalityComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UTrainingBotPersonalityComponent::BeginTraining()
{
    EndTraining();
    const ATrainingCombatAIController* TrainingController = Cast<ATrainingCombatAIController>(GetOwner());
    if (!TrainingController)
    {
        return;
    }
    ActivePersonality = Personality;
    if (!TrainingPersonality::IsConcretePersonality(ActivePersonality))
    {
        const ETrainingBotPersonality Choices[] = {
            ETrainingBotPersonality::Copycat,
            ETrainingBotPersonality::Coward,
            ETrainingBotPersonality::Trickster
        };
        ActivePersonality = Choices[FMath::RandHelper(static_cast<int32>(UE_ARRAY_COUNT(Choices)))];
    }
    bStrafeClockwise = (FMath::RandHelper(2) != 0);
    StartTrickSequence();

    if (UWorld* World = GetWorld())
    {
        LastObservationSeconds = World->GetTimeSeconds();
        Observe();
        FTimerManagerTimerParameters TimerParameters;
        TimerParameters.bLoop = true;
        TimerParameters.bMaxOncePerFrame = true;
        World->GetTimerManager().SetTimer(
            ObservationTimer, this, &ThisClass::Observe,
            FMath::Clamp(ObservationIntervalSeconds, 0.02f, 0.25f), TimerParameters);
    }
}

void UTrainingBotPersonalityComponent::EndTraining()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ObservationTimer);
    }
    LastObservationSeconds = 0.0;
    AimLatch.Reset();
    ParticipantPawn.Reset();
    bStrafeClockwise = false;
    AimTangentOffset = 0.0f;
    ActivePersonality = ETrainingBotPersonality::Random;
}

void UTrainingBotPersonalityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    EndTraining();
    Super::EndPlay(EndPlayReason);
}

void UTrainingBotPersonalityComponent::Observe()
{
    UWorld* World = GetWorld();
    if (!World)
    {
        EndTraining();
        return;
    }

    const double Now = World->GetTimeSeconds();
    const float DeltaSeconds = LastObservationSeconds > 0.0
        ? static_cast<float>(FMath::Max(0.0, Now - LastObservationSeconds)) : 0.0f;
    LastObservationSeconds = Now;
    const ATrainingCombatAIController* TrainingController = Cast<ATrainingCombatAIController>(GetOwner());
    const ATrainingCombatAI* TrainingBot = TrainingController
        ? Cast<ATrainingCombatAI>(TrainingController->GetPawn()) : nullptr;
    if (!TrainingController || !TrainingBot || !TrainingBot->IsAlive())
    {
        EndTraining();
        return;
    }
    const APlayerController* Player = TrainingBot->GetParticipant();
    APawn* CurrentParticipantPawn = Player ? Player->GetPawn() : nullptr;
    if (ParticipantPawn.Get() != CurrentParticipantPawn)
    {
        ParticipantPawn = CurrentParticipantPawn;
        AimLatch.Reset();
        AimTangentOffset = 0.0f;
    }
    if (!IsValid(CurrentParticipantPawn))
    {
        ParticipantPawn.Reset();
        AimLatch.Reset();
        return;
    }

    bool bAimObserved = false;
    const ATrainingArena* Arena = TrainingBot->GetTrainingArena();
    const UCapsuleComponent* Capsule = TrainingBot->GetCapsuleComponent();
    if (Arena && Capsule)
    {
        FVector Eye;
        FRotator ViewRotation;
        Player->GetPlayerViewPoint(Eye, ViewRotation);
        const float Padding = AimLatch.IsAimedAt()
            ? FMath::Max(AimEnterPadding, AimExitPadding) : FMath::Max(0.0f, AimEnterPadding);
        FVector ClosestAimPoint;
        bAimObserved = TrainingPersonality::IsAimNearCapsule(Eye, ViewRotation.Vector(),
            TrainingBot->GetActorLocation(), Capsule->GetScaledCapsuleHalfHeight(),
            Capsule->GetScaledCapsuleRadius(), Padding, ClosestAimPoint);
        if (bAimObserved)
        {
            FCollisionQueryParams Params(SCENE_QUERY_STAT(TrainingPersonalitySight), false);
            Params.AddIgnoredActor(CurrentParticipantPawn);
            Params.AddIgnoredActor(TrainingBot);
            FHitResult Obstruction;
            FVector SightPoint = TrainingBot->GetActorLocation();
            const float AxisHalfHeight = Capsule->GetScaledCapsuleHalfHeight() - Capsule->GetScaledCapsuleRadius();
            SightPoint.Z += FMath::Clamp(ClosestAimPoint.Z - SightPoint.Z,
                -static_cast<double>(AxisHalfHeight), static_cast<double>(AxisHalfHeight));
            bAimObserved = !World->LineTraceSingleByChannel(Obstruction,
                Eye, SightPoint, ECC_Visibility, Params);
            AimTangentOffset = TrainingPersonality::TangentialProjection(
                ClosestAimPoint - TrainingBot->GetActorLocation(), Arena->GetCenterLocation(),
                TrainingBot->GetActorLocation());
        }
    }
    AimLatch.Update(bAimObserved, DeltaSeconds, FMath::Clamp(ReactionSeconds, 0.05f, 1.0f));
}

bool UTrainingBotPersonalityComponent::ReverseStrafeDirection()
{
    bStrafeClockwise = !bStrafeClockwise;
    return bStrafeClockwise;
}

bool UTrainingBotPersonalityComponent::ChooseEscapeClockwise() const
{
    return FMath::Abs(AimTangentOffset) > 1.0f ? AimTangentOffset < 0.0f : (FMath::RandHelper(2) != 0);
}

void UTrainingBotPersonalityComponent::StartTrickSequence()
{
    bTrickClockwise = (FMath::RandHelper(2) != 0);
}
