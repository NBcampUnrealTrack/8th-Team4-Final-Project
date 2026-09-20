#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Perception/AIPerceptionTypes.h"
#include "TimerManager.h"
#include "CQBTrainingAIController.generated.h"

class UAIPerceptionComponent;
class UAISenseConfig_Hearing;
class UAISenseConfig_Sight;
class UCQBTrainingAIComponent;
class UCQBTrainingAwarenessComponent;
class UCQBTrainingCombatComponent;
class UCQBTrainingInvestigationComponent;
class UCQBTrainingLocomotionComponent;
class UCQBTrainingPatrolComponent;
class URogueHealthComponent;
class UStateTreeAIComponent;

UENUM(BlueprintType)
enum class ECQBAIBehaviorState : uint8
{
    Patrol      UMETA(DisplayName = "Patrol"),
    Investigate UMETA(DisplayName = "Investigate"),
    Reaction    UMETA(DisplayName = "Reaction"),
    Engage      UMETA(DisplayName = "Engage"),
    Search      UMETA(DisplayName = "Search"),
};

UCLASS()
class FPS_API ACQBTrainingAIController : public AAIController
{
    GENERATED_BODY()

public:
    ACQBTrainingAIController();
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CQB|AI")
    TObjectPtr<UAIPerceptionComponent> CQBPerceptionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CQB|AI")
    TObjectPtr<UStateTreeAIComponent> CQBStateTreeComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CQB|AI")
    TObjectPtr<UCQBTrainingAwarenessComponent> AwarenessComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CQB|AI")
    TObjectPtr<UCQBTrainingLocomotionComponent> LocomotionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CQB|AI")
    TObjectPtr<UCQBTrainingPatrolComponent> PatrolComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CQB|AI")
    TObjectPtr<UCQBTrainingInvestigationComponent> InvestigationComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CQB|AI")
    TObjectPtr<UCQBTrainingCombatComponent> CombatComponent;

    bool StartCQBBrain();
    void StopCQBBrain();
    bool RefreshFromPawnConfiguration();
    bool IsCombatTargetValid(const AActor* Actor) const;

    UFUNCTION(BlueprintPure, Category = "CQB|AI")
    bool IsTargetVisible() const;

    UFUNCTION(BlueprintPure, Category = "CQB|AI")
    AActor* GetCurrentTarget() const;

    UCQBTrainingAwarenessComponent* GetAwarenessComponent() const { return AwarenessComponent; }
    UCQBTrainingLocomotionComponent* GetLocomotionComponent() const { return LocomotionComponent; }
    UCQBTrainingPatrolComponent* GetPatrolComponent() const { return PatrolComponent; }
    UCQBTrainingInvestigationComponent* GetInvestigationComponent() const { return InvestigationComponent; }
    UCQBTrainingCombatComponent* GetCombatComponent() const { return CombatComponent; }

protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;

private:
    UFUNCTION()
    void HandleTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

    void HandlePawnDamageTaken(const AActor* InstigatorActor, FVector Direction);

    UFUNCTION()
    void HandlePawnDeathStateChanged(bool bIsDead);

    UCQBTrainingAIComponent* GetCQBComponent() const;
    bool IsControlledPawnAlive() const;
    void ConfigurePerceptionFromPawn();
    bool IsHearingStimulusInRange(const FAIStimulus& Stimulus) const;

    void BindHealthEvents();
    void UnbindHealthEvents();
    void BeginDeathShutdown();
    void FinalizeDeathShutdown();

    UPROPERTY()
    TObjectPtr<UAISenseConfig_Sight> SightConfig;

    UPROPERTY()
    TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

    UPROPERTY(Transient)
    TObjectPtr<URogueHealthComponent> BoundHealthComponent;

    FTimerHandle DeathShutdownTimer;
    bool bDeathShutdownStarted = false;
};
