#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "Training/TrainingTypes.h"
#include "Training/TrainingModeDefinition.h"
#include "TrainingSessionSubsystem.generated.h"

class AActor;
class APlayerController;
class ATrainingArena;
class UTrainingScenario;
class ATrainingCharacter;
class UHitscanResolverSubsystem;
struct FHitscanShotRequest;

UCLASS()
class FPS_API UTrainingSessionSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Deinitialize() override;

    bool StartSession(ATrainingArena& InArena, APlayerController& InParticipant, ETrainingMode Mode,
        ETrainingDistancePreset Preset, float CustomMeters, FText& OutFailure);
    bool RequestEndSession(APlayerController& RequestingPlayer, int32 ExpectedSessionId);
    void EndSessionForParticipant(const APlayerController* LeavingParticipant);
    void EndSessionForArena(const ATrainingArena* EndingArena);

    ETrainingSessionState GetState() const { return State; }
    ATrainingArena* GetActiveArena() const;
    APlayerController* GetParticipant() const;
    int32 GetSessionId() const { return SessionId; }

private:
    void BeginActiveSession();
    bool IsInsideScoringWindow() const;
    bool IsParticipantActor(const AActor* Candidate) const;
    void HandleBotSpawned(ATrainingCharacter& Bot);
    void HandleBotHit(ATrainingCharacter& Bot, const AActor* InstigatorActor, float Damage, bool bKilled);
    void HandleShotResolved(const FHitscanShotRequest& Shot, const TArray<TWeakObjectPtr<AActor>>& AppliedTargets);
    void UnbindShotObserver();
    void HandleSpawnTick();
    void StopSession(bool bImmediate);
    void NotifyParticipant(ETrainingFeedbackPhase Phase, double PhaseStartServerSeconds);
    void EndSession();

    UPROPERTY(Transient)
    TObjectPtr<UTrainingScenario> ActiveScenario;

    TWeakObjectPtr<ATrainingArena> Arena;
    TWeakObjectPtr<APlayerController> Participant;
    ETrainingSessionState State = ETrainingSessionState::Idle;
    ETrainingMode ActiveMode = ETrainingMode::TargetAcquisition;
    FTrainingModeConfig ActiveModeConfig;
    FTimerHandle CountdownTimer;
    FTimerHandle SpawnTimer;
    FTimerHandle DurationTimer;
    FTrainingSessionStats Stats;
    TWeakObjectPtr<UHitscanResolverSubsystem> ShotResolver;
    FDelegateHandle ShotResolvedHandle;
    float SessionDurationSeconds = 0.0f;
    double SessionStartSeconds = 0.0;
    int32 SessionId = 0;
    bool bStopping = false;
};
