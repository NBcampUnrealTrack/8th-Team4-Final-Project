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
    void BeginSearchScan();

    TWeakObjectPtr<UCQBTrainingAIComponent> ConfigRef;
    TWeakObjectPtr<UCQBTrainingAwarenessComponent> AwarenessRef;
    TWeakObjectPtr<UCQBTrainingLocomotionComponent> LocomotionRef;

    double StateEnterTime = 0.0;
    bool bMovePrepared = false;

    bool bSearchScanStarted = false;
    int32 SearchScanStep = 0;
    double NextSearchScanTime = 0.0;
    float SearchBaseYaw = 0.0f;
    float SearchLookTargetYaw = 0.0f;
};
