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
enum class ETrainingBotSource : uint8;

DECLARE_MULTICAST_DELEGATE_OneParam(FTrainingBotSpawned, ATrainingCharacter&);

UCLASS(Abstract)
class FPS_API UTrainingScenario : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(ATrainingArena& InArena, APlayerController& InParticipant,
        const FTrainingDistanceBand& InBand, const FTrainingModeConfig& InModeConfig);
    virtual ETrainingBotSource GetBotSource() const PURE_VIRTUAL(UTrainingScenario::GetBotSource, return ETrainingBotSource(););
    virtual bool ValidateConfiguration(const ATrainingArena& InArena,
        const FTrainingDistanceBand& InBand, FText& OutFailure) const PURE_VIRTUAL(UTrainingScenario::ValidateConfiguration, return false;);
    virtual bool SpawnNext() PURE_VIRTUAL(UTrainingScenario::SpawnNext, return false;);
    void Stop();
    bool OwnsActor(const AActor* Candidate) const;
    FTrainingBotSpawned OnBotSpawned;
    FTrainingBotHit OnBotHit;

protected:
    void TrackActor(ATrainingCharacter& SpawnedActor);
    bool HasSpawnCapacity() const;
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
    bool bStopped = true;
};

UCLASS()
class FPS_API UTargetAcquisitionScenario : public UTrainingScenario
{
    GENERATED_BODY()
public:
    virtual ETrainingBotSource GetBotSource() const override;
    virtual bool ValidateConfiguration(const ATrainingArena& InArena,
        const FTrainingDistanceBand& InBand, FText& OutFailure) const override;
    virtual bool SpawnNext() override;
};

UCLASS()
class FPS_API UCombatResponseScenario : public UTrainingScenario
{
    GENERATED_BODY()
public:
    virtual ETrainingBotSource GetBotSource() const override;
    virtual bool ValidateConfiguration(const ATrainingArena& InArena,
        const FTrainingDistanceBand& InBand, FText& OutFailure) const override;
    virtual bool SpawnNext() override;

private:
    bool SpawnCombatBot();
};
