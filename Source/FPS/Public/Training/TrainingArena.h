#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Training/TrainingTypes.h"
#include "TrainingArena.generated.h"

class ATrainingTarget;
class ATrainingCombatAI;
class UArrowComponent;
class UBoxComponent;
class USphereComponent;
struct FNavAgentProperties;

UCLASS()
class FPS_API ATrainingArena : public AActor
{
    GENERATED_BODY()

public:
    ATrainingArena();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "Training")
    FVector GetCenterLocation() const;

    UFUNCTION(BlueprintPure, Category = "Training")
    FTransform GetEntryTransform() const;

    UFUNCTION(BlueprintPure, Category = "Training")
    ETrainingSessionState GetSessionState() const { return SessionState; }

    bool IsInMovementArea(const AActor& Participant) const;
    bool ProjectGroundPosition(const FVector& Candidate, const FTrainingDistanceBand& Band,
        FVector& OutGroundPosition) const;
    bool FindGroundSpawn(const FTrainingDistanceBand& Band, FVector& OutGroundPosition) const;
    bool FindNavSpawn(const FTrainingDistanceBand& Band, const FNavAgentProperties& AgentProperties,
        FVector& OutNavPosition) const;
    bool ProjectNavPosition(const FVector& Candidate, const FTrainingDistanceBand& Band,
        const FNavAgentProperties& AgentProperties, FVector& OutNavPosition) const;
    bool FindStrafeDestination(const FVector& CurrentPosition, const FTrainingDistanceBand& Band,
        const FNavAgentProperties& AgentProperties, bool bClockwise, FVector& OutNavPosition) const;

    bool FindBotStepDestination(const FVector& CurrentPosition, const FTrainingDistanceBand& Band,
        const FNavAgentProperties& AgentProperties, float SignedArcCentimeters,
        float RadialStepCentimeters, FVector& OutNavPosition) const;

    void SetSessionState(ETrainingSessionState NewState);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|Rules", meta = (ClampMin = "0.0", Units = "s"))
    float CombatDurationSeconds = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|Geometry")
    TObjectPtr<UArrowComponent> CenterPoint;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|Geometry")
    TObjectPtr<UArrowComponent> TrainingEntryPoint;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|Geometry")
    TObjectPtr<USphereComponent> MovementArea;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|Classes")
    TSubclassOf<ATrainingTarget> TargetClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|Classes")
    TSubclassOf<ATrainingCombatAI> CombatAIClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|Geometry")
    bool bEnableMovementBoundary = true;

    UPROPERTY(EditAnywhere, Category = "Training|Placement", meta = (ClampMin = "1.0", Units = "cm"))
    float GroundProbeUpCentimeters = 500.0f;

    UPROPERTY(EditAnywhere, Category = "Training|Placement", meta = (ClampMin = "1.0", Units = "cm"))
    float GroundProbeDownCentimeters = 2000.0f;

    UPROPERTY(EditAnywhere, Category = "Training|Placement")
    FVector NavigationProjectionExtent = FVector(100.0, 100.0, 300.0);

protected:
    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    FVector MakeRadialCandidate(const FTrainingDistanceBand& Band) const;

    UPROPERTY(VisibleAnywhere, Category = "Training|Geometry")
    TArray<TObjectPtr<UBoxComponent>> MovementBoundary;

    UPROPERTY(Replicated)
    ETrainingSessionState SessionState = ETrainingSessionState::Idle;
};
