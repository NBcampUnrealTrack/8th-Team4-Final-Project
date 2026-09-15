#include "Training/AI/TrainingBotPersonalityComponent.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Training/AI/TrainingCombatAI.h"
#include "Training/AI/TrainingCombatAIController.h"
#include "Training/TrainingArena.h"
#include "UObject/Class.h"

UTrainingBotPersonalityComponent::UTrainingBotPersonalityComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false;
    PrimaryComponentTick.TickInterval = 0.05f;
}

void UTrainingBotPersonalityComponent::BeginTraining()
{
    EndTraining();
    const ATrainingCombatAIController* TrainingController = Cast<ATrainingCombatAIController>(GetOwner());
    if (!TrainingController || !TrainingController->HasAuthority())
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
    StartTrickSequence();
    Observe(0.0f);
    SetComponentTickEnabled(true);
    if (bLogPersonalityOnSpawn)
    {
        UE_LOG(LogTemp, Log, TEXT("[TrainingPersonality] %s -> %s"),
            *GetNameSafe(TrainingController->GetPawn()),
            *UEnum::GetValueAsString(ActivePersonality));
    }
}

void UTrainingBotPersonalityComponent::EndTraining()
{
    SetComponentTickEnabled(false);
    VelocityHistory.Reset();
    AimLatch.Reset();
    ParticipantPawn.Reset();
    ObservationSeconds = 0.0f;
    AimTangentOffset = 0.0f;
    ActivePersonality = ETrainingBotPersonality::Random;
}

void UTrainingBotPersonalityComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    Observe(DeltaTime);
}

void UTrainingBotPersonalityComponent::Observe(const float DeltaSeconds)
{
    const ATrainingCombatAIController* TrainingController = Cast<ATrainingCombatAIController>(GetOwner());
    const ATrainingCombatAI* TrainingBot = TrainingController
        ? Cast<ATrainingCombatAI>(TrainingController->GetPawn()) : nullptr;
    if (!TrainingController || !TrainingController->HasAuthority() || !TrainingBot || !TrainingBot->IsAlive())
    {
        EndTraining();
        return;
    }
    ObservationSeconds += FMath::Max(0.0f, DeltaSeconds);
    const APlayerController* Player = TrainingBot->GetParticipant();
    APawn* CurrentParticipantPawn = Player ? Player->GetPawn() : nullptr;
    if (ParticipantPawn.Get() != CurrentParticipantPawn)
    {
        ParticipantPawn = CurrentParticipantPawn;
        VelocityHistory.Reset();
        AimLatch.Reset();
        AimTangentOffset = 0.0f;
    }
    if (!IsValid(CurrentParticipantPawn))
    {
        ParticipantPawn.Reset();
        VelocityHistory.Reset();
        AimLatch.Reset();
        return;
    }
    VelocityHistory.Add(ObservationSeconds, CurrentParticipantPawn->GetVelocity());

    bool bAimObserved = false;
    const ATrainingArena* Arena = TrainingBot->GetTrainingArena();
    const UCapsuleComponent* Capsule = TrainingBot->GetCapsuleComponent();
    if (Arena && Capsule)
    {
        FVector Eye = CurrentParticipantPawn->GetPawnViewLocation();
        FRotator ViewRotation = Player->GetControlRotation();
        if (Player->IsLocalController())
        {
            Player->GetPlayerViewPoint(Eye, ViewRotation);
        }
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
            bAimObserved = !GetWorld()->LineTraceSingleByChannel(Obstruction,
                Eye, SightPoint, ECC_Visibility, Params);
            AimTangentOffset = TrainingPersonality::TangentialVelocity(
                ClosestAimPoint - TrainingBot->GetActorLocation(), Arena->GetCenterLocation(),
                TrainingBot->GetActorLocation());
        }
    }
    AimLatch.Update(bAimObserved, DeltaSeconds, FMath::Clamp(ReactionSeconds, 0.05f, 1.0f));
}

float UTrainingBotPersonalityComponent::GetCopyTangentialVelocity(const bool bDelayed) const
{
    const ATrainingCombatAIController* TrainingController = Cast<ATrainingCombatAIController>(GetOwner());
    const ATrainingCombatAI* TrainingBot = TrainingController
        ? Cast<ATrainingCombatAI>(TrainingController->GetPawn()) : nullptr;
    const ATrainingArena* Arena = TrainingBot ? TrainingBot->GetTrainingArena() : nullptr;
    if (!ParticipantPawn.IsValid() || !TrainingBot || !Arena)
    {
        return 0.0f;
    }
    FVector SampleVelocity;
    const float Delay = bDelayed ? FMath::Clamp(CopyDelaySeconds, 0.05f, 1.0f) : 0.0f;
    if (!VelocityHistory.TryGetAtOrBefore(ObservationSeconds - Delay, SampleVelocity))
    {
        return 0.0f;
    }
    return TrainingPersonality::TangentialVelocity(SampleVelocity,
        Arena->GetCenterLocation(), TrainingBot->GetActorLocation());
}

bool UTrainingBotPersonalityComponent::ChooseEscapeClockwise() const
{
    return FMath::Abs(AimTangentOffset) > 1.0f ? AimTangentOffset < 0.0f : (FMath::RandHelper(2) != 0);
}

void UTrainingBotPersonalityComponent::StartTrickSequence()
{
    bTrickClockwise = (FMath::RandHelper(2) != 0);
}
