#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Training/TrainingTypes.h"
#include "TrainingScenario.generated.h"

class AActor;
class APlayerController;
class ATrainingArena;

// Owns spawned actors, not session timers or UI. Dead AI stays here until destroyed.
UCLASS(Abstract)
class FPS_API UTrainingScenario : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(ATrainingArena& InArena, APlayerController& InParticipant,
        const FTrainingDistanceBand& InBand);
    virtual void SpawnNext() PURE_VIRTUAL(UTrainingScenario::SpawnNext, );
    void Stop();

protected:
    void TrackActor(AActor& SpawnedActor);
    ATrainingArena* GetArena() const;
    APlayerController* GetParticipant() const;
    FTrainingDistanceBand DistanceBand;
    TArray<TWeakObjectPtr<AActor>> OwnedActors;

private:
    UFUNCTION()
    void HandleActorDestroyed(AActor* DestroyedActor);

    TWeakObjectPtr<ATrainingArena> Arena;
    TWeakObjectPtr<APlayerController> Participant;
};

UCLASS()
class FPS_API UTargetAcquisitionScenario : public UTrainingScenario
{
    GENERATED_BODY()
public:
    virtual void SpawnNext() override;
};

UCLASS()
class FPS_API UCombatResponseScenario : public UTrainingScenario
{
    GENERATED_BODY()
public:
    virtual void SpawnNext() override;
    int32 GetLivingAICount() const;
};
