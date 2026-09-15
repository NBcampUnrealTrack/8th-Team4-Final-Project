#include "Training/TrainingSessionSubsystem.h"

#include "Engine/World.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Training/AI/TrainingCombatAI.h"
#include "Training/TrainingArena.h"
#include "Training/TrainingScenario.h"
#include "Training/TrainingTarget.h"

#define LOCTEXT_NAMESPACE "TrainingSession"

bool UTrainingSessionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* OuterWorld = Cast<UWorld>(Outer);
    return Super::ShouldCreateSubsystem(Outer) && OuterWorld
        && (OuterWorld->WorldType == EWorldType::Game || OuterWorld->WorldType == EWorldType::PIE)
        && OuterWorld->GetNetMode() != NM_Client;
}

void UTrainingSessionSubsystem::Deinitialize()
{
    EndSession();
    Super::Deinitialize();
}

ATrainingArena* UTrainingSessionSubsystem::GetActiveArena() const { return Arena.Get(); }
APlayerController* UTrainingSessionSubsystem::GetParticipant() const { return Participant.Get(); }

bool UTrainingSessionSubsystem::StartSession(ATrainingArena& InArena, APlayerController& InParticipant,
    const ETrainingMode Mode, const ETrainingDistancePreset Preset, const float CustomMeters, FText& OutFailure)
{
    OutFailure = FText::GetEmpty();
    if (GetWorld()->GetNetMode() == NM_Client || InArena.GetWorld() != GetWorld()
        || InParticipant.GetWorld() != GetWorld())
    {
        OutFailure = LOCTEXT("WrongWorld", "Training must be started by the server in this level.");
        return false;
    }
    if (State != ETrainingSessionState::Idle || bStopping)
    {
        OutFailure = LOCTEXT("Busy", "Another training session is already active.");
        return false;
    }
    const APawn* ParticipantPawn = InParticipant.GetPawn();
    if (!ParticipantPawn || !InArena.IsInMovementArea(*ParticipantPawn))
    {
        OutFailure = LOCTEXT("Outside", "Enter the central training area before starting.");
        return false;
    }
    FTrainingDistanceBand Band;
    if (!TrainingRules::ResolveDistanceBand(Preset, CustomMeters, Band))
    {
        OutFailure = LOCTEXT("Distance", "The selected training distance is invalid.");
        return false;
    }
    switch (Mode)
    {
    case ETrainingMode::TargetAcquisition:
        if (!InArena.TargetClass || InArena.TargetClass->HasAnyClassFlags(CLASS_Abstract))
        {
            OutFailure = LOCTEXT("TargetClass", "Assign a concrete Training Target Blueprint to the arena.");
            return false;
        }
        if (!InArena.TargetClass.GetDefaultObject()->DisplayMesh->GetStaticMesh())
        {
            OutFailure = LOCTEXT("TargetMesh", "Assign a Static Mesh to the Training Target Blueprint's DisplayMesh component.");
            return false;
        }
        ActiveScenario = NewObject<UTargetAcquisitionScenario>(this);
        State = ETrainingSessionState::TargetAcquisitionActive;
        break;
    case ETrainingMode::CombatResponse:
        if (!InArena.CombatAIClass || InArena.CombatAIClass->HasAnyClassFlags(CLASS_Abstract)
            || !InArena.CombatAIClass.GetDefaultObject()->HasConfiguredBehavior())
        {
            OutFailure = LOCTEXT("AIClass", "Assign a Combat AI Blueprint with a Training AI Controller and compiled StateTree.");
            return false;
        }
        if (!InArena.CombatAIClass.GetDefaultObject()->GetMesh()->GetSkeletalMeshAsset()
            || !InArena.CombatAIClass.GetDefaultObject()->GetMesh()->GetPhysicsAsset()
            || !InArena.CombatAIClass.GetDefaultObject()->DissolveMaterial)
        {
            OutFailure = LOCTEXT("AIVisuals", "Assign a Skeletal Mesh, Physics Asset and DissolveMaterial to the Combat AI Blueprint.");
            return false;
        }
        ActiveScenario = NewObject<UCombatResponseScenario>(this);
        State = ETrainingSessionState::CombatResponseActive;
        break;
    default:
        OutFailure = LOCTEXT("Mode", "The selected training mode is invalid.");
        return false;
    }

    Arena = &InArena;
    Participant = &InParticipant;
    SessionId = SessionId == MAX_int32 ? 1 : SessionId + 1;
    SessionStartSeconds = GetWorld()->GetTimeSeconds();
    ActiveScenario->Initialize(InArena, InParticipant, Band);
    InArena.SetSessionState(State);

    FTimerManager& Timers = GetWorld()->GetTimerManager();
    const bool bTargets = Mode == ETrainingMode::TargetAcquisition;
    const float Interval = bTargets ? TrainingRules::TargetIntervalSeconds : TrainingRules::CombatIntervalSeconds;
    Timers.SetTimer(SpawnTimer, this, &ThisClass::HandleSpawnTick, Interval, true);
    if (bTargets)
    {
        Timers.SetTimer(DurationTimer, this, &ThisClass::EndSession,
            TrainingRules::TargetDurationSeconds, false);
    }
    HandleSpawnTick(); // t=0, followed by the normal interval. Death never schedules a replacement.
    return true;
}

void UTrainingSessionSubsystem::HandleSpawnTick()
{
    if (bStopping || State == ETrainingSessionState::Idle)
    {
        return;
    }
    // A destroyed controller means disconnect/leave. Pawn death/replacement deliberately does not.
    if (!Arena.IsValid() || !Participant.IsValid() || !ActiveScenario)
    {
        EndSession();
        return;
    }
    if (State == ETrainingSessionState::TargetAcquisitionActive
        && GetWorld()->GetTimeSeconds() - SessionStartSeconds >= TrainingRules::TargetDurationSeconds)
    {
        EndSession(); // Do not create an extra target at the 60-second deadline.
        return;
    }
    ActiveScenario->SpawnNext();
}

bool UTrainingSessionSubsystem::RequestEndSession(APlayerController& RequestingPlayer,
    const int32 ExpectedSessionId)
{
    if (Participant.Get() != &RequestingPlayer || ExpectedSessionId != SessionId
        || State == ETrainingSessionState::Idle)
    {
        return false;
    }
    EndSession();
    return true;
}

void UTrainingSessionSubsystem::EndSessionForParticipant(const APlayerController* LeavingParticipant)
{
    if (LeavingParticipant && Participant.Get() == LeavingParticipant)
    {
        EndSession();
    }
}

void UTrainingSessionSubsystem::EndSessionForArena(const ATrainingArena* EndingArena)
{
    if (EndingArena && Arena.Get() == EndingArena)
    {
        EndSession();
    }
}

void UTrainingSessionSubsystem::EndSession()
{
    if (bStopping)
    {
        return;
    }
    bStopping = true;
    if (UWorld* ActiveWorld = GetWorld())
    {
        ActiveWorld->GetTimerManager().ClearTimer(SpawnTimer);
        ActiveWorld->GetTimerManager().ClearTimer(DurationTimer);
    }
    if (ActiveScenario)
    {
        ActiveScenario->Stop();
        ActiveScenario = nullptr;
    }
    if (ATrainingArena* PreviousArena = Arena.Get())
    {
        PreviousArena->SetSessionState(ETrainingSessionState::Idle);
    }
    Arena.Reset();
    Participant.Reset();
    SessionStartSeconds = 0.0;
    State = ETrainingSessionState::Idle;
    bStopping = false;
}

#undef LOCTEXT_NAMESPACE
