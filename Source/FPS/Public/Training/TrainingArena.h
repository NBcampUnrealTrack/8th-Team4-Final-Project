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

// Geometry/configuration only. Session state and all timers belong to the session subsystem.
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
    bool FindGroundSpawn(const FTrainingDistanceBand& Band, FVector& OutGroundPosition) const;
    bool FindNavSpawn(const FTrainingDistanceBand& Band, const FNavAgentProperties& AgentProperties,
        FVector& OutNavPosition) const;
    bool ProjectNavPosition(const FVector& Candidate, const FTrainingDistanceBand& Band,
        const FNavAgentProperties& AgentProperties, FVector& OutNavPosition) const;
    bool FindStrafeDestination(const FVector& CurrentPosition, const FTrainingDistanceBand& Band,
        const FNavAgentProperties& AgentProperties, bool bClockwise, FVector& OutNavPosition) const;

    void SetSessionState(ETrainingSessionState NewState);

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|Geometry")
    TObjectPtr<UArrowComponent> CenterPoint;

    // Arrow origin = pawn/capsule origin at arrival, NOT a ground contact point.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|Geometry")
    TObjectPtr<UArrowComponent> TrainingEntryPoint;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|Geometry")
    TObjectPtr<USphereComponent> MovementArea;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|Classes")
    TSubclassOf<ATrainingTarget> TargetClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|Classes")
    TSubclassOf<ATrainingCombatAI> CombatAIClass;

    // Disable only when the level supplies equivalent Pawn-only boundary collision.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|Geometry")
    bool bEnableMovementBoundary = true;

    UPROPERTY(EditAnywhere, Category = "Training|Placement", meta = (ClampMin = "1.0"))
    float GroundProbeUpCentimeters = 500.0f;

    UPROPERTY(EditAnywhere, Category = "Training|Placement", meta = (ClampMin = "1.0"))
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
