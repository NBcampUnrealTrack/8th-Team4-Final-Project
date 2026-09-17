#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Training/TrainingTypes.h"
#include "Training/TrainingCharacter.h"
#include "TrainingScenario.generated.h"

class AActor;
class APlayerController;
class ATrainingArena;
struct FTrainingModeConfig;

DECLARE_MULTICAST_DELEGATE_OneParam(FTrainingBotSpawned, ATrainingCharacter&);

UCLASS(Abstract)
class FPS_API UTrainingScenario : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(ATrainingArena& InArena, APlayerController& InParticipant,
        const FTrainingDistanceBand& InBand, const FTrainingModeConfig& InModeConfig);
    virtual void SpawnNext() PURE_VIRTUAL(UTrainingScenario::SpawnNext, );
    void Stop(bool bImmediate = false);
    bool OwnsActor(const AActor* Candidate) const;
    FTrainingBotSpawned OnBotSpawned;
    FTrainingBotHit OnBotHit;

protected:
    void TrackActor(ATrainingCharacter& SpawnedActor);
    ATrainingArena* GetArena() const;
    APlayerController* GetParticipant() const;
    FTrainingDistanceBand DistanceBand;
    int32 MaxLivingActors = 0;
    TArray<TWeakObjectPtr<ATrainingCharacter>> OwnedActors;

private:
    void HandleBotHit(ATrainingCharacter& Bot, const AActor* InstigatorActor, float Damage, bool bKilled);

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

private:
    void SpawnCombatBot();
};
