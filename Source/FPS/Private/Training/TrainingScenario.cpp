#include "Training/TrainingScenario.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Training/AI/TrainingCombatAI.h"
#include "Training/TrainingArena.h"
#include "Training/TrainingTarget.h"

void UTrainingScenario::Initialize(ATrainingArena& InArena, APlayerController& InParticipant,
    const FTrainingDistanceBand& InBand)
{
    Arena = &InArena;
    Participant = &InParticipant;
    DistanceBand = InBand;
}

ATrainingArena* UTrainingScenario::GetArena() const { return Arena.Get(); }
APlayerController* UTrainingScenario::GetParticipant() const { return Participant.Get(); }

void UTrainingScenario::TrackActor(AActor& SpawnedActor)
{
    OwnedActors.Add(&SpawnedActor);
    SpawnedActor.OnDestroyed.AddDynamic(this, &ThisClass::HandleActorDestroyed);
}

void UTrainingScenario::HandleActorDestroyed(AActor* DestroyedActor)
{
    OwnedActors.RemoveAll([DestroyedActor](const TWeakObjectPtr<AActor>& Entry)
    {
        return !Entry.IsValid() || Entry.Get() == DestroyedActor;
    });
}

void UTrainingScenario::Stop()
{
    // Empty ownership before Destroy() invokes EndPlay/delegates; no re-entrant array mutation.
    TArray<TWeakObjectPtr<AActor>> ActorsToDestroy = MoveTemp(OwnedActors);
    OwnedActors.Reset();
    for (const TWeakObjectPtr<AActor>& Entry : ActorsToDestroy)
    {
        if (AActor* SpawnedActor = Entry.Get())
        {
            SpawnedActor->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleActorDestroyed);
            SpawnedActor->Destroy();
        }
    }
    Arena.Reset();
    Participant.Reset();
}

void UTargetAcquisitionScenario::SpawnNext()
{
    ATrainingArena* ActiveArena = GetArena();
    APlayerController* ActiveParticipant = GetParticipant();
    if (!ActiveArena || !ActiveParticipant || !ActiveArena->TargetClass)
    {
        return;
    }
    FVector GroundPosition;
    if (!ActiveArena->FindGroundSpawn(DistanceBand, GroundPosition))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Training] Target spawn failed: no valid ground in %.0f-%.0f cm band around %s."),
            DistanceBand.MinCentimeters, DistanceBand.MaxCentimeters,
            *ActiveArena->GetCenterLocation().ToCompactString());
        return; // Retry on the next regular tick, never spawn at an invalid fallback location.
    }
    const ATrainingTarget* TargetDefaults = ActiveArena->TargetClass.GetDefaultObject();
    const FVector SpawnPosition = GroundPosition + FVector(0.0, 0.0, TargetDefaults->GetGroundOffsetCentimeters());
    const FRotator SpawnRotation(0.0, (ActiveArena->GetCenterLocation() - SpawnPosition).Rotation().Yaw, 0.0);
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = ActiveArena;
    SpawnParameters.Instigator = ActiveParticipant->GetPawn();
    // A target is intentionally placed just above the traced ground.  Using DontSpawnIfColliding
    // is too strict here because Unreal's spawn encroachment test can reject a valid placement
    // that is only touching/very close to the floor.  Allow the engine to make a small adjustment,
    // then validate the final radial distance again below.
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

    ATrainingTarget* SpawnedTarget = ActiveArena->GetWorld()->SpawnActor<ATrainingTarget>(
        ActiveArena->TargetClass, SpawnPosition, SpawnRotation, SpawnParameters);
    if (!SpawnedTarget)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Training] Target spawn failed: SpawnActor rejected class %s at %s. Check blocking geometry/collision."),
            *GetNameSafe(ActiveArena->TargetClass.Get()), *SpawnPosition.ToCompactString());
        return;
    }

    if (!DistanceBand.Contains(ActiveArena->GetCenterLocation(), SpawnedTarget->GetActorLocation()))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Training] Target spawn rejected after collision adjustment. Final=%s Center=%s Band=%.0f-%.0f cm."),
            *SpawnedTarget->GetActorLocation().ToCompactString(),
            *ActiveArena->GetCenterLocation().ToCompactString(),
            DistanceBand.MinCentimeters, DistanceBand.MaxCentimeters);
        SpawnedTarget->Destroy();
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[Training] Target spawned: %s at %s"),
        *GetNameSafe(SpawnedTarget), *SpawnedTarget->GetActorLocation().ToCompactString());
    TrackActor(*SpawnedTarget);
}

int32 UCombatResponseScenario::GetLivingAICount() const
{
    int32 LivingCount = 0;
    for (const TWeakObjectPtr<AActor>& Entry : OwnedActors)
    {
        const ATrainingCombatAI* TrainingPawn = Cast<ATrainingCombatAI>(Entry.Get());
        if (TrainingPawn && TrainingPawn->IsAlive())
        {
            ++LivingCount;
        }
    }
    return LivingCount;
}

void UCombatResponseScenario::SpawnNext()
{
    ATrainingArena* ActiveArena = GetArena();
    APlayerController* ActiveParticipant = GetParticipant();
    if (!ActiveArena || !ActiveParticipant || !ActiveArena->CombatAIClass
        || !TrainingRules::CanSpawnCombatAI(GetLivingAICount()))
    {
        return;
    }
    const ATrainingCombatAI* AIDefaults = ActiveArena->CombatAIClass.GetDefaultObject();
    FVector NavigationPosition;
    if (!ActiveArena->FindNavSpawn(DistanceBand, AIDefaults->GetNavAgentPropertiesRef(), NavigationPosition))
    {
        return;
    }
    const FVector SpawnPosition = NavigationPosition + FVector(0.0, 0.0,
        AIDefaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0f);
    const FRotator SpawnRotation(0.0, (ActiveArena->GetCenterLocation() - SpawnPosition).Rotation().Yaw, 0.0);
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = ActiveArena;
    SpawnParameters.Instigator = ActiveParticipant->GetPawn();
    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
    ATrainingCombatAI* SpawnedAI = ActiveArena->GetWorld()->SpawnActor<ATrainingCombatAI>(
        ActiveArena->CombatAIClass, SpawnPosition, SpawnRotation, SpawnParameters);
    if (SpawnedAI)
    {
        TrackActor(*SpawnedAI);
        if (!DistanceBand.Contains(ActiveArena->GetCenterLocation(), SpawnedAI->GetActorLocation())
            || !SpawnedAI->InitializeForTraining(*ActiveArena, *ActiveParticipant, DistanceBand))
        {
            SpawnedAI->Destroy();
        }
    }
}
