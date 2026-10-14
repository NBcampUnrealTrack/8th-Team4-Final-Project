#include "Training/TrainingSessionSubsystem.h"

#include "Combat/Hitscan/HitscanResolverSubsystem.h"
#include "GameFramework/PlayerState.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameMode/TrainingGameMode.h"
#include "GameFramework/PlayerController.h"
#include "Training/TrainingArena.h"
#include "Training/TrainingCharacter.h"
#include "Training/TrainingPlayerComponent.h"
#include "Training/TrainingScenario.h"
#include "Training/TrainingSettings.h"
#include "Training/TrainingModeDefinition.h"

#define LOCTEXT_NAMESPACE "TrainingSession"

bool UTrainingSessionSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* OuterWorld = Cast<UWorld>(Outer);
    return Super::ShouldCreateSubsystem(Outer) && OuterWorld
        && (OuterWorld->WorldType == EWorldType::Game || OuterWorld->WorldType == EWorldType::PIE)
        && OuterWorld->GetNetMode() == NM_Standalone;
}

void UTrainingSessionSubsystem::Deinitialize()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearAllTimersForObject(this);
    }
    CountdownTimer.Invalidate();
    SpawnTimer.Invalidate();
    DurationTimer.Invalidate();
    StopSession(true);
    Super::Deinitialize();
}

ATrainingArena* UTrainingSessionSubsystem::GetActiveArena() const { return Arena.Get(); }
APlayerController* UTrainingSessionSubsystem::GetParticipant() const { return Participant.Get(); }

bool UTrainingSessionSubsystem::StartSession(ATrainingArena& InArena, APlayerController& InParticipant,
    const ETrainingMode Mode, const ETrainingDistancePreset Preset, const float CustomMeters, FText& OutFailure)
{
    OutFailure = FText::GetEmpty();
    UWorld* World = GetWorld();
    const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
    if (!World || World->GetNetMode() != NM_Standalone || !World->GetAuthGameMode<ATrainingGameMode>()
        || !GameInstance || GameInstance->GetLocalPlayers().Num() != 1
        || !InParticipant.IsLocalPlayerController() || InArena.GetWorld() != World
        || InParticipant.GetWorld() != World)
    {
        OutFailure = LOCTEXT("WrongWorld", "Training requires a standalone Training level with one local player.");
        return false;
    }
    if (State != ETrainingSessionState::Idle || bStopping)
    {
        OutFailure = LOCTEXT("Busy", "Training is already active or counting down.");
        return false;
    }
    const UTrainingPlayerComponent* Bridge = InParticipant.FindComponentByClass<UTrainingPlayerComponent>();
    if (Bridge && Bridge->IsCQBTrainingActive())
    {
        OutFailure = LOCTEXT("CQBActive", "End CQB training before starting an arena session.");
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

    FTrainingModeConfig ModeConfig;
    if (!TrainingModeDefinitions::Resolve(Mode, ModeConfig, &InArena))
    {
        OutFailure = LOCTEXT("ModeDefinition", "The selected training mode has no valid mode definition.");
        return false;
    }

    const UTrainingScenario* ScenarioDefaults = ModeConfig.ScenarioClass.GetDefaultObject();
    if (!ScenarioDefaults->ValidateConfiguration(InArena, Band, OutFailure))
    {
        return false;
    }

    ActiveScenario = NewObject<UTrainingScenario>(this, ModeConfig.ScenarioClass);
    if (!ActiveScenario)
    {
        OutFailure = LOCTEXT("ScenarioCreate", "Could not create the selected Training Scenario.");
        return false;
    }

    Arena = &InArena;
    Participant = &InParticipant;
    ActiveMode = Mode;
    ActiveModeConfig = ModeConfig;
    Stats = FTrainingSessionStats();
    const UTrainingSettings* Settings = GetDefault<UTrainingSettings>();
    SessionDurationSeconds = ActiveModeConfig.DurationSeconds;
    SessionId = SessionId == MAX_int32 ? 1 : SessionId + 1;
    SessionStartSeconds = 0.0;
    ActiveScenario->Initialize(InArena, InParticipant, Band, ActiveModeConfig);
    ActiveScenario->OnBotSpawned.AddUObject(this, &ThisClass::HandleBotSpawned);
    ActiveScenario->OnBotHit.AddUObject(this, &ThisClass::HandleBotHit);
    State = ETrainingSessionState::Countdown;

    const float CountdownSeconds = Settings->GetCountdownSeconds();
    const int32 StartingSessionId = SessionId;
    NotifyParticipant(ETrainingFeedbackPhase::Countdown, World->GetTimeSeconds());
    if (bStopping || State != ETrainingSessionState::Countdown || SessionId != StartingSessionId)
    {
        return true;
    }
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
    if (!Arena.IsValid() || !Participant.IsValid() || !Participant->GetPawn() || !ActiveScenario)
    {
        StopSession(true);
        return;
    }
    State = ETrainingSessionState::Active;
    SessionStartSeconds = GetWorld()->GetTimeSeconds();
    ShotResolver = GetWorld()->GetSubsystem<UHitscanResolverSubsystem>();
    if (ShotResolver.IsValid())
    {
        ShotResolvedHandle = ShotResolver->OnServerShotResolved.AddUObject(this, &ThisClass::HandleShotResolved);
    }
    const int32 StartingSessionId = SessionId;
    NotifyParticipant(ETrainingFeedbackPhase::Started, SessionStartSeconds);
    if (bStopping || State != ETrainingSessionState::Active || SessionId != StartingSessionId)
    {
        return;
    }

    if (!ActiveScenario->SpawnNext())
    {
        if (State == ETrainingSessionState::Active && SessionId == StartingSessionId)
        {
            TWeakObjectPtr<UTrainingPlayerComponent> FailureBridge = Participant.IsValid()
                ? Participant->FindComponentByClass<UTrainingPlayerComponent>() : nullptr;
            StopSession(true);
            if (FailureBridge.IsValid())
            {
                FailureBridge->NotifyMessage(LOCTEXT("FirstSpawnFailed",
                    "Training could not start. Check the bot Blueprint, floor collision and navigation."));
            }
        }
        return;
    }
    if (bStopping || State != ETrainingSessionState::Active || SessionId != StartingSessionId)
    {
        return;
    }

    FTimerManager& Timers = GetWorld()->GetTimerManager();
    if (SessionDurationSeconds > 0.0f)
    {
        Timers.SetTimer(DurationTimer, this, &ThisClass::EndSession, SessionDurationSeconds, false);
    }
    Timers.SetTimer(SpawnTimer, this, &ThisClass::HandleSpawnTick,
        ActiveModeConfig.GetSpawnInterval(0.0f), false);
}

void UTrainingSessionSubsystem::HandleSpawnTick()
{
    if (bStopping || !TrainingRules::IsRunningState(State))
    {
        return;
    }
    if (!Arena.IsValid() || !Participant.IsValid() || !Participant->GetPawn() || !ActiveScenario)
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
    const int32 SpawningSessionId = SessionId;
    ActiveScenario->SpawnNext();
    if (bStopping || State != ETrainingSessionState::Active || SessionId != SpawningSessionId)
    {
        return;
    }

    const float SpawnInterval = ActiveModeConfig.GetSpawnInterval(static_cast<float>(ElapsedSeconds));
    GetWorld()->GetTimerManager().SetTimer(SpawnTimer, this, &ThisClass::HandleSpawnTick,
        SpawnInterval, false);
}

void UTrainingSessionSubsystem::NotifyParticipant(const ETrainingFeedbackPhase Phase,
    const double PhaseStartSeconds)
{
    if (APlayerController* ParticipantPlayer = Participant.Get())
    {
        if (UTrainingPlayerComponent* Bridge = ParticipantPlayer->FindComponentByClass<UTrainingPlayerComponent>())
        {
            Bridge->NotifySessionPhase(SessionId, Phase, PhaseStartSeconds, ActiveMode, SessionDurationSeconds);
        }
    }
}

bool UTrainingSessionSubsystem::RequestEndSession(const int32 ExpectedSessionId)
{
    if (bStopping || ExpectedSessionId != SessionId || State == ETrainingSessionState::Idle)
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
            ActiveModeConfig.bShowDamageFeedback ? Damage : 0.0f,
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

void UTrainingSessionSubsystem::StopSession(const bool bSuppressResult)
{
    if (bStopping)
    {
        return;
    }
    TGuardValue<bool> StopGuard(bStopping, true);
    const bool bHadSession = State != ETrainingSessionState::Idle;
    const bool bShowResult = !bSuppressResult && TrainingRules::IsRunningState(State);
    FTrainingSessionResult Result;
    Result.SessionId = SessionId;
    Result.Mode = ActiveMode;
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
    if (ActiveScenario)
    {
        ActiveScenario->Stop();
        ActiveScenario = nullptr;
    }
    Arena.Reset();
    Participant.Reset();
    SessionStartSeconds = 0.0;
    ActiveMode = ETrainingMode::TargetAcquisition;
    ActiveModeConfig = FTrainingModeConfig();
    State = ETrainingSessionState::Idle;
    SessionDurationSeconds = 0.0f;
    Stats = FTrainingSessionStats();
    if (bHadSession && ResultBridge.IsValid())
    {
        ResultBridge->NotifySessionPhase(Result.SessionId, ETrainingFeedbackPhase::Stopped,
            0.0, Result.Mode, 0.0f);
    }
    if (bShowResult && ResultBridge.IsValid()) { ResultBridge->NotifySessionFinished(Result); }
}

#undef LOCTEXT_NAMESPACE
