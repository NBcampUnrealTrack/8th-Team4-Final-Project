#include "Training/TrainingSessionSubsystem.h"

#include "Components/SkeletalMeshComponent.h"
#include "Combat/Hitscan/HitscanResolverSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Training/AI/TrainingCombatAI.h"
#include "Training/TrainingArena.h"
#include "Training/TrainingCharacter.h"
#include "Training/TrainingPlayerComponent.h"
#include "Training/TrainingScenario.h"
#include "Training/TrainingSettings.h"
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
    StopSession(true);
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
        OutFailure = LOCTEXT("Busy", "Another training session is already active or counting down.");
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

    UClass* BotClass = nullptr;
    switch (Mode)
    {
    case ETrainingMode::TargetAcquisition: BotClass = InArena.TargetClass.Get(); break;
    case ETrainingMode::CombatResponse: BotClass = InArena.CombatAIClass.Get(); break;
    default:
        OutFailure = LOCTEXT("Mode", "The selected training mode is invalid.");
        return false;
    }
    if (!BotClass || BotClass->HasAnyClassFlags(CLASS_Abstract))
    {
        OutFailure = LOCTEXT("BotClass", "Assign a concrete Target / Combat AI Blueprint to the Training Arena.");
        return false;
    }
    const ATrainingCharacter* BotDefaults = Cast<ATrainingCharacter>(BotClass->GetDefaultObject());
    if (!BotDefaults || !BotDefaults->GetMesh()->GetSkeletalMeshAsset())
    {
        OutFailure = LOCTEXT("BotMesh", "Assign a Skeletal Mesh (for example Manny) to the training bot. Recreate the old static Target Blueprint.");
        return false;
    }
    if (!BotDefaults->DissolveMaterial)
    {
        OutFailure = LOCTEXT("BotMaterial", "Assign a Dissolve Material to the Target / Combat AI Blueprint.");
        return false;
    }
    FVector ProbePosition;
    if (Mode == ETrainingMode::TargetAcquisition)
    {
        if (!InArena.FindGroundSpawn(Band, ProbePosition))
        {
            OutFailure = LOCTEXT("TargetGround", "No valid WorldStatic floor in this distance band. Check the arena center, floor collision and ground probe height.");
            return false;
        }
    }
    else
    {
        const ATrainingCombatAI* CombatDefaults = Cast<ATrainingCombatAI>(BotDefaults);
        if (!CombatDefaults || !CombatDefaults->HasConfiguredBehavior())
        {
            OutFailure = LOCTEXT("CombatTree", "Assign a Training Combat AI Controller Blueprint with a compiled Strafe State Tree to the Combat AI.");
            return false;
        }
        if (!InArena.FindNavSpawn(Band, CombatDefaults->GetNavAgentPropertiesRef(), ProbePosition))
        {
            OutFailure = LOCTEXT("CombatNavigation", "No valid NavMesh in the selected distance band. Add/build NavMeshBoundsVolume over the combat training floor.");
            return false;
        }
    }

    ActiveScenario = Mode == ETrainingMode::TargetAcquisition
        ? static_cast<UTrainingScenario*>(NewObject<UTargetAcquisitionScenario>(this))
        : static_cast<UTrainingScenario*>(NewObject<UCombatResponseScenario>(this));
    Arena = &InArena;
    Participant = &InParticipant;
    PendingMode = Mode;
    Stats = FTrainingSessionStats();
    const UTrainingSettings* Settings = GetDefault<UTrainingSettings>();
    SessionDurationSeconds = Mode == ETrainingMode::TargetAcquisition
        ? FMath::Max(0.0f, Settings->TargetDurationSeconds)
        : (FMath::IsFinite(InArena.CombatDurationSeconds) ? FMath::Max(0.0f, InArena.CombatDurationSeconds) : 0.0f);
    SessionId = SessionId == MAX_int32 ? 1 : SessionId + 1;
    SessionStartSeconds = 0.0;
    ActiveScenario->Initialize(InArena, InParticipant, Band);
    ActiveScenario->OnBotSpawned.AddUObject(this, &ThisClass::HandleBotSpawned);
    ActiveScenario->OnBotHit.AddUObject(this, &ThisClass::HandleBotHit);
    State = ETrainingSessionState::Countdown;
    InArena.SetSessionState(State);

    const float CountdownSeconds = Settings->GetCountdownSeconds();
    NotifyParticipant(ETrainingFeedbackPhase::Countdown, GetWorld()->GetTimeSeconds());
    if (CountdownSeconds <= KINDA_SMALL_NUMBER)
    {
        BeginActiveSession();
    }
    else
    {
        GetWorld()->GetTimerManager().SetTimer(CountdownTimer, this, &ThisClass::BeginActiveSession,
            CountdownSeconds, false);
    }
    return true;
}

void UTrainingSessionSubsystem::BeginActiveSession()
{
    if (bStopping || State != ETrainingSessionState::Countdown)
    {
        return;
    }
    if (!Arena.IsValid() || !Participant.IsValid() || !ActiveScenario)
    {
        StopSession(true);
        return;
    }
    State = PendingMode == ETrainingMode::TargetAcquisition
        ? ETrainingSessionState::TargetAcquisitionActive : ETrainingSessionState::CombatResponseActive;
    SessionStartSeconds = GetWorld()->GetTimeSeconds();
    ShotResolver = GetWorld()->GetSubsystem<UHitscanResolverSubsystem>();
    if (ShotResolver.IsValid())
    {
        ShotResolvedHandle = ShotResolver->OnServerShotResolved.AddUObject(this, &ThisClass::HandleShotResolved);
    }
    Arena->SetSessionState(State);
    NotifyParticipant(ETrainingFeedbackPhase::Started, SessionStartSeconds);

    FTimerManager& Timers = GetWorld()->GetTimerManager();
    if (State == ETrainingSessionState::CombatResponseActive)
    {
        Timers.SetTimer(SpawnTimer, this, &ThisClass::HandleSpawnTick,
            FMath::Max(0.05f, GetDefault<UTrainingSettings>()->CombatSpawnIntervalSeconds), true);
    }
    if (SessionDurationSeconds > 0.0f)
    {
        Timers.SetTimer(DurationTimer, this, &ThisClass::EndSession, SessionDurationSeconds, false);
    }
    HandleSpawnTick();
}

void UTrainingSessionSubsystem::HandleSpawnTick()
{
    if (bStopping || !TrainingRules::IsRunningState(State))
    {
        return;
    }
    if (!Arena.IsValid() || !Participant.IsValid() || !ActiveScenario)
    {
        StopSession(true);
        return;
    }
    const double ElapsedSeconds = GetWorld()->GetTimeSeconds() - SessionStartSeconds;
    if (SessionDurationSeconds > 0.0f && ElapsedSeconds >= SessionDurationSeconds)
    {
        EndSession();
        return;
    }
    if (State == ETrainingSessionState::TargetAcquisitionActive)
    {
        const float SpawnInterval = TrainingRules::GetTargetSpawnIntervalSeconds(
            static_cast<float>(ElapsedSeconds), SessionDurationSeconds);
        GetWorld()->GetTimerManager().SetTimer(SpawnTimer, this, &ThisClass::HandleSpawnTick,
            SpawnInterval, false);
    }
    ActiveScenario->SpawnNext();
}

void UTrainingSessionSubsystem::NotifyParticipant(const ETrainingFeedbackPhase Phase,
    const double PhaseStartServerSeconds)
{
    if (APlayerController* ParticipantPlayer = Participant.Get())
    {
        if (UTrainingPlayerComponent* Bridge = ParticipantPlayer->FindComponentByClass<UTrainingPlayerComponent>())
        {
            Bridge->NotifySessionPhase(SessionId, Phase, PhaseStartServerSeconds, PendingMode, SessionDurationSeconds);
        }
    }
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
        StopSession(true);
    }
}

void UTrainingSessionSubsystem::EndSessionForArena(const ATrainingArena* EndingArena)
{
    if (EndingArena && Arena.Get() == EndingArena)
    {
        StopSession(true);
    }
}

void UTrainingSessionSubsystem::EndSession()
{
    StopSession(false);
}

bool UTrainingSessionSubsystem::IsInsideScoringWindow() const
{
    return !bStopping && TrainingRules::IsRunningState(State) && ActiveScenario && Participant.IsValid()
        && (SessionDurationSeconds <= 0.0f
            || GetWorld()->GetTimeSeconds() - SessionStartSeconds < SessionDurationSeconds);
}

bool UTrainingSessionSubsystem::IsParticipantActor(const AActor* Candidate) const
{
    const APlayerController* Player = Participant.Get();
    if (!Candidate || !Player) { return false; }
    const APawn* SourcePawn = Cast<APawn>(Candidate);
    return Candidate == Player || Candidate == Player->PlayerState
        || (SourcePawn && SourcePawn->GetController() == Player);
}

void UTrainingSessionSubsystem::HandleBotSpawned(ATrainingCharacter& Bot)
{
    if (!IsInsideScoringWindow()) { return; }
    if (UTrainingPlayerComponent* Bridge = Participant->FindComponentByClass<UTrainingPlayerComponent>())
    {
        Bridge->NotifyBotSound(SessionId, Bot.SpawnSound, Bot.BotSoundAttenuation,
            Bot.GetActorLocation(), Bot.BotSoundVolume);
    }
}

void UTrainingSessionSubsystem::HandleBotHit(ATrainingCharacter& Bot, const AActor* InstigatorActor,
    const float Damage, const bool bKilled)
{
    if (!IsInsideScoringWindow() || !ActiveScenario->OwnsActor(&Bot)) { return; }
    UTrainingPlayerComponent* Bridge = Participant->FindComponentByClass<UTrainingPlayerComponent>();
    if (Bridge && bKilled)
    {
        Bridge->NotifyBotSound(SessionId, Bot.DeathSound, Bot.BotSoundAttenuation,
            Bot.GetActorLocation(), Bot.BotSoundVolume);
    }
    if (!IsParticipantActor(InstigatorActor)) { return; }
    if (bKilled) { ++Stats.Kills; }
    if (Bridge)
    {
        Bridge->NotifySessionHit(SessionId, Stats.Kills,
            PendingMode == ETrainingMode::CombatResponse ? Damage : 0.0f,
            bKilled, Bot.GetActorLocation());
    }
}

void UTrainingSessionSubsystem::HandleShotResolved(const FHitscanShotRequest& Shot,
    const TArray<TWeakObjectPtr<AActor>>& AppliedTargets)
{
    if (!IsInsideScoringWindow() || !IsParticipantActor(Shot.InstigatorActor)) { return; }
    const bool bHitTrainingBot = AppliedTargets.ContainsByPredicate([this](const TWeakObjectPtr<AActor>& Target)
    {
        return ActiveScenario->OwnsActor(Target.Get());
    });
    Stats.RecordShot(bHitTrainingBot);
}

void UTrainingSessionSubsystem::UnbindShotObserver()
{
    if (ShotResolver.IsValid() && ShotResolvedHandle.IsValid())
    {
        ShotResolver->OnServerShotResolved.Remove(ShotResolvedHandle);
    }
    ShotResolvedHandle.Reset();
    ShotResolver.Reset();
}

void UTrainingSessionSubsystem::StopSession(const bool bImmediate)
{
    if (bStopping)
    {
        return;
    }
    bStopping = true;
    const bool bShowResult = !bImmediate && TrainingRules::IsRunningState(State);
    FTrainingSessionResult Result;
    Result.SessionId = SessionId;
    Result.Mode = PendingMode;
    Result.Stats = Stats;
    if (bShowResult && GetWorld())
    {
        Result.ElapsedSeconds = static_cast<float>(FMath::Max(0.0, GetWorld()->GetTimeSeconds() - SessionStartSeconds));
        Result.bCompleted = SessionDurationSeconds > 0.0f && Result.ElapsedSeconds >= SessionDurationSeconds;
        if (SessionDurationSeconds > 0.0f) { Result.ElapsedSeconds = FMath::Min(Result.ElapsedSeconds, SessionDurationSeconds); }
    }
    TWeakObjectPtr<UTrainingPlayerComponent> ResultBridge = Participant.IsValid()
        ? Participant->FindComponentByClass<UTrainingPlayerComponent>() : nullptr;
    UnbindShotObserver();
    if (UWorld* ActiveWorld = GetWorld())
    {
        ActiveWorld->GetTimerManager().ClearTimer(CountdownTimer);
        ActiveWorld->GetTimerManager().ClearTimer(SpawnTimer);
        ActiveWorld->GetTimerManager().ClearTimer(DurationTimer);
    }
    if (State != ETrainingSessionState::Idle)
    {
        NotifyParticipant(ETrainingFeedbackPhase::Stopped, 0.0);
    }
    if (ActiveScenario)
    {
        ActiveScenario->Stop(bImmediate);
        ActiveScenario = nullptr;
    }
    if (ATrainingArena* PreviousArena = Arena.Get())
    {
        PreviousArena->SetSessionState(ETrainingSessionState::Idle);
    }
    Arena.Reset();
    Participant.Reset();
    SessionStartSeconds = 0.0;
    PendingMode = ETrainingMode::TargetAcquisition;
    State = ETrainingSessionState::Idle;
    SessionDurationSeconds = 0.0f;
    Stats = FTrainingSessionStats();
    bStopping = false;
    if (bShowResult && ResultBridge.IsValid()) { ResultBridge->NotifySessionFinished(Result); }
}

#undef LOCTEXT_NAMESPACE
