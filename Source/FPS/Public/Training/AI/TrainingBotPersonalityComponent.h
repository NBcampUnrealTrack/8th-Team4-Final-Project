#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "Training/AI/TrainingBotPersonalityTypes.h"
#include "TrainingBotPersonalityComponent.generated.h"

class APawn;

UCLASS(ClassGroup = (Training), meta = (BlueprintSpawnableComponent))
class FPS_API UTrainingBotPersonalityComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UTrainingBotPersonalityComponent();

    void BeginTraining();
    void EndTraining();

    UFUNCTION(BlueprintPure, Category = "Training|Personality")
    ETrainingBotPersonality GetActivePersonality() const { return ActivePersonality; }

    UFUNCTION(BlueprintPure, Category = "Training|Personality")
    bool HasParticipant() const { return ParticipantPawn.IsValid(); }

    UFUNCTION(BlueprintPure, Category = "Training|Personality")
    bool IsBeingAimedAt() const { return AimLatch.IsAimedAt(); }

    bool ReverseStrafeDirection();
    bool ChooseEscapeClockwise() const;
    void StartTrickSequence();
    bool GetTrickClockwise() const { return bTrickClockwise; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Personality")
    ETrainingBotPersonality Personality = ETrainingBotPersonality::Random;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Personality", meta = (ClampMin = "0.05", ClampMax = "1.0", Units = "s"))
    float ReactionSeconds = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Personality", meta = (ClampMin = "0.02", ClampMax = "0.25", Units = "s"))
    float ObservationIntervalSeconds = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Personality|Strafe", meta = (ClampMin = "0.1", Units = "s"))
    float StrafeDirectionSeconds = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Personality|Strafe", meta = (ClampMin = "0.0", Units = "s"))
    float StrafeDirectionVariance = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Personality", meta = (ClampMin = "0.0", Units = "cm"))
    float AimEnterPadding = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Personality", meta = (ClampMin = "0.0", Units = "cm"))
    float AimExitPadding = 45.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Personality|Coward", meta = (ClampMin = "0.0", Units = "cm"))
    float ApproachStepCentimeters = 65.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Personality|Coward", meta = (ClampMin = "0.0", Units = "cm"))
    float EscapeStepCentimeters = 100.0f;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void Observe();

    FTrainingAimLatch AimLatch;
    TWeakObjectPtr<APawn> ParticipantPawn;
    float AimTangentOffset = 0.0f;
    bool bStrafeClockwise = false;
    bool bTrickClockwise = false;
    double LastObservationSeconds = 0.0;
    FTimerHandle ObservationTimer;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Training|Personality", meta = (AllowPrivateAccess = "true"))
    ETrainingBotPersonality ActivePersonality = ETrainingBotPersonality::Random;
};
