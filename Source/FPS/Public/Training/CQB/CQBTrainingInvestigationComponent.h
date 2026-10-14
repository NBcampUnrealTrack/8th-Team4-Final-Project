#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CQBTrainingInvestigationComponent.generated.h"

class APawn;
class UCQBTrainingAIComponent;
class UCQBTrainingAwarenessComponent;
class UCQBTrainingLocomotionComponent;

UCLASS(ClassGroup = (Training))
class FPS_API UCQBTrainingInvestigationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCQBTrainingInvestigationComponent();

    void Configure(UCQBTrainingAIComponent& InConfig,
        UCQBTrainingAwarenessComponent& InAwareness,
        UCQBTrainingLocomotionComponent& InLocomotion);
    void ResetRuntimeState();

    void EnterInvestigate();
    void TickInvestigate(float DeltaTime);
    void ExitInvestigate();

    void EnterSearch();
    void TickSearch(float DeltaTime);
    void ExitSearch();

private:
    UCQBTrainingAIComponent* GetConfig() const;
    UCQBTrainingAwarenessComponent* GetAwareness() const;
    UCQBTrainingLocomotionComponent* GetLocomotion() const;
    APawn* GetControlledPawn() const;
    void BuildSearchPlan();
    void MoveToSearchPoint();
    void BeginPointInspection();
    void FinishSearch();

    TWeakObjectPtr<UCQBTrainingAIComponent> ConfigRef;
    TWeakObjectPtr<UCQBTrainingAwarenessComponent> AwarenessRef;
    TWeakObjectPtr<UCQBTrainingLocomotionComponent> LocomotionRef;

    double StateEnterTime = 0.0;
    uint32 ActiveEvidenceRevision = 0;
    bool bMoveAttempted = false;
    bool bMovePrepared = false;

    uint32 PlannedEvidenceRevision = 0;
    bool bSearchPlanBuilt = false;
    TArray<FVector> SearchPoints;
    FVector SearchCenter = FVector::ZeroVector;
    FVector SearchForward = FVector::ForwardVector;
    int32 SearchPointIndex = 0;
    double MoveDeadline = 0.0;
    bool bInspectingPoint = false;
    double InspectionStartTime = 0.0;
    float InspectionBaseYaw = 0.0f;
};
