#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "TimerManager.h"
#include "Training/TrainingTypes.h"
#include "TrainingSessionSubsystem.generated.h"

class APlayerController;
class ATrainingArena;
class UTrainingScenario;

// One authoritative session per UWorld, shared by every arena/participant in that world.
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
    void HandleSpawnTick();
    void EndSession();

    UPROPERTY(Transient)
    TObjectPtr<UTrainingScenario> ActiveScenario;

    TWeakObjectPtr<ATrainingArena> Arena;
    TWeakObjectPtr<APlayerController> Participant;
    ETrainingSessionState State = ETrainingSessionState::Idle;
    FTimerHandle SpawnTimer;
    FTimerHandle DurationTimer;
    double SessionStartSeconds = 0.0;
    int32 SessionId = 0;
    bool bStopping = false;
};
