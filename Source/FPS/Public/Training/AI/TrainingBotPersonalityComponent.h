#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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

    float GetCopyTangentialVelocity(bool bDelayed) const;
    bool ChooseEscapeClockwise() const;
    void StartTrickSequence();
    bool GetTrickClockwise() const { return bTrickClockwise; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Personality")
    ETrainingBotPersonality Personality = ETrainingBotPersonality::Random;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Personality", meta = (ClampMin = "0.05", ClampMax = "1.0", Units = "s"))
    float ReactionSeconds = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Personality", meta = (ClampMin = "0.05", ClampMax = "1.0", Units = "s"))
    float CopyDelaySeconds = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Personality", meta = (ClampMin = "1.0", Units = "cm/s"))
    float CopyMovementDeadZone = 25.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Personality", meta = (ClampMin = "0.0", Units = "cm"))
    float AimEnterPadding = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Personality", meta = (ClampMin = "0.0", Units = "cm"))
    float AimExitPadding = 45.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Personality")
    bool bLogPersonalityOnSpawn = false;

protected:
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

private:
    void Observe(float DeltaSeconds);

    FTrainingVelocityHistory VelocityHistory;
    FTrainingAimLatch AimLatch;
    TWeakObjectPtr<APawn> ParticipantPawn;
    float ObservationSeconds = 0.0f;
    float AimTangentOffset = 0.0f;
    bool bTrickClockwise = false;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Training|Personality", meta = (AllowPrivateAccess = "true"))
    ETrainingBotPersonality ActivePersonality = ETrainingBotPersonality::Random;
};
