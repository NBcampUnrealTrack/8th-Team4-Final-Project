#include "Training/CQB/CQBCombatIncidentSubsystem.h"

#include "GameFramework/Pawn.h"
#include "Training/CQB/CQBTrainingAwarenessComponent.h"

void UCQBCombatIncidentSubsystem::Deinitialize()
{
    Listeners.Reset();
    Super::Deinitialize();
}

void UCQBCombatIncidentSubsystem::RegisterListener(
    UCQBTrainingAwarenessComponent& Listener, APawn& Pawn, AActor& Encounter,
    const float AwarenessRadius)
{
    UnregisterListener(Listener);
    FListener& Entry = Listeners.AddDefaulted_GetRef();
    Entry.Awareness = &Listener;
    Entry.Pawn = &Pawn;
    Entry.Encounter = &Encounter;
    Entry.RadiusSquared = FMath::Square(FMath::Max(0.0f, AwarenessRadius));
}

void UCQBCombatIncidentSubsystem::UnregisterListener(UCQBTrainingAwarenessComponent& Listener)
{
    Listeners.RemoveAllSwap([&Listener](const FListener& Entry)
    {
        return !Entry.Awareness.IsValid() || Entry.Awareness.Get() == &Listener;
    });
}

void UCQBCombatIncidentSubsystem::ReportIncident(const FCQBCombatIncident& Incident)
{
    if (!Incident.Encounter.IsValid())
    {
        return;
    }

    Listeners.RemoveAllSwap([](const FListener& Entry)
    {
        return !Entry.Awareness.IsValid() || !Entry.Pawn.IsValid() || !Entry.Encounter.IsValid();
    });

    for (const FListener& Entry : Listeners)
    {
        if (Entry.Encounter != Incident.Encounter || Entry.Pawn.Get() == Incident.SourceActor.Get()
            || FVector::DistSquared(Entry.Pawn->GetActorLocation(), Incident.Origin) > Entry.RadiusSquared)
        {
            continue;
        }

        Entry.Awareness->RecordCombatIncident(Incident);
    }
}
