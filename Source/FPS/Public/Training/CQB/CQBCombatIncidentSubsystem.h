#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CQBCombatIncidentSubsystem.generated.h"

class AActor;
class APawn;
class UCQBTrainingAwarenessComponent;

enum class ECQBCombatIncidentType : uint8
{
    AllyKilled,
};

struct FPS_API FCQBCombatIncident
{
    ECQBCombatIncidentType Type = ECQBCombatIncidentType::AllyKilled;
    FVector Origin = FVector::ZeroVector;
    // Victim toward the source of the lethal hit, with no shooter position or identity.
    FVector ThreatDirection = FVector::ZeroVector;
    double Timestamp = 0.0;
    TWeakObjectPtr<AActor> SourceActor;
    TWeakObjectPtr<AActor> Encounter;
};

UCLASS()
class FPS_API UCQBCombatIncidentSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;

    void RegisterListener(UCQBTrainingAwarenessComponent& Listener, APawn& Pawn,
        AActor& Encounter, float AwarenessRadius);
    void UnregisterListener(UCQBTrainingAwarenessComponent& Listener);
    void ReportIncident(const FCQBCombatIncident& Incident);

private:
    struct FListener
    {
        TWeakObjectPtr<UCQBTrainingAwarenessComponent> Awareness;
        TWeakObjectPtr<APawn> Pawn;
        TWeakObjectPtr<AActor> Encounter;
        float RadiusSquared = 0.0f;
    };

    TArray<FListener> Listeners;
};
