#include "Training/TrainingScenario.h"

#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Training/AI/TrainingCombatAI.h"
#include "Training/TrainingCharacter.h"
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
    if (ATrainingCharacter* Bot = Cast<ATrainingCharacter>(&SpawnedActor))
    {
        Bot->OnTrainingHit.AddUObject(this, &ThisClass::HandleBotHit);
        OnBotSpawned.Broadcast(*Bot);
    }
}

bool UTrainingScenario::OwnsActor(const AActor* Candidate) const
{
    return Candidate && OwnedActors.ContainsByPredicate([Candidate](const TWeakObjectPtr<AActor>& Entry)
    {
        return Entry.Get() == Candidate;
    });
}

void UTrainingScenario::HandleBotHit(ATrainingCharacter& Bot, const AActor* InstigatorActor,
    const float Damage, const bool bKilled)
{
    OnBotHit.Broadcast(Bot, InstigatorActor, Damage, bKilled);
}

void UTrainingScenario::HandleActorDestroyed(AActor* DestroyedActor)
{
    if (ATrainingCharacter* Bot = Cast<ATrainingCharacter>(DestroyedActor))
    {
        Bot->OnTrainingHit.RemoveAll(this);
    }
    OwnedActors.RemoveAll([DestroyedActor](const TWeakObjectPtr<AActor>& Entry)
    {
        return !Entry.IsValid() || Entry.Get() == DestroyedActor;
    });
}

void UTrainingScenario::Stop(const bool bImmediate)
{
    OnBotSpawned.Clear();
    OnBotHit.Clear();
    TArray<TWeakObjectPtr<AActor>> ActorsToRemove = MoveTemp(OwnedActors);
    OwnedActors.Reset();
    for (const TWeakObjectPtr<AActor>& Entry : ActorsToRemove)
    {
        if (AActor* SpawnedActor = Entry.Get())
        {
            SpawnedActor->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleActorDestroyed);
            ATrainingCharacter* TrainingBot = Cast<ATrainingCharacter>(SpawnedActor);
            if (TrainingBot) { TrainingBot->OnTrainingHit.RemoveAll(this); }
            if (!bImmediate && TrainingBot)
            {
                TrainingBot->BeginDissolveRemoval();
            }
            else
            {
                SpawnedActor->Destroy();
            }
        }
    }
    Arena.Reset();
    Participant.Reset();
}

void UCombatResponseScenario::SpawnCombatBot()
{
    ATrainingArena* ActiveArena = GetArena();
    APlayerController* ActiveParticipant = GetParticipant();
    if (!ActiveArena || !ActiveParticipant || !ActiveArena->CombatAIClass)
    {
        return;
    }
    const TSubclassOf<ATrainingCombatAI> BotClass = ActiveArena->CombatAIClass;
    const TCHAR* ModeLabel = TEXT("Combat AI");
    const ATrainingCombatAI* BotDefaults = BotClass.GetDefaultObject();
    FVector NavigationPosition;
    if (!ActiveArena->FindNavSpawn(DistanceBand, BotDefaults->GetNavAgentPropertiesRef(), NavigationPosition))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Training] %s spawn failed: no valid NavMesh in %.0f-%.0f cm band around %s."),
            ModeLabel, DistanceBand.MinCentimeters, DistanceBand.MaxCentimeters,
            *ActiveArena->GetCenterLocation().ToCompactString());
        return;
    }
    const FVector SpawnPosition = NavigationPosition + FVector(0.0, 0.0,
        BotDefaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0f);
    const FRotator SpawnRotation(0.0, (ActiveArena->GetCenterLocation() - SpawnPosition).Rotation().Yaw, 0.0);
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = ActiveArena;
    SpawnParameters.Instigator = ActiveParticipant->GetPawn();
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
    ATrainingCombatAI* SpawnedBot = ActiveArena->GetWorld()->SpawnActor<ATrainingCombatAI>(
        BotClass, SpawnPosition, SpawnRotation, SpawnParameters);
    if (!SpawnedBot)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Training] %s spawn failed: SpawnActor rejected %s at %s."),
            ModeLabel, *GetNameSafe(BotClass.Get()), *SpawnPosition.ToCompactString());
        return;
    }
    FVector ValidatedNavigation;
    if (!DistanceBand.Contains(ActiveArena->GetCenterLocation(), SpawnedBot->GetActorLocation())
        || !ActiveArena->ProjectNavPosition(SpawnedBot->GetActorLocation() - FVector(0.0, 0.0,
                SpawnedBot->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), DistanceBand,
            SpawnedBot->GetNavAgentPropertiesRef(), ValidatedNavigation)
        || FVector::DistSquared2D(SpawnedBot->GetActorLocation(), ValidatedNavigation) > FMath::Square(25.0))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Training] %s spawn rejected after collision/NavMesh adjustment: %s."),
            ModeLabel, *SpawnedBot->GetActorLocation().ToCompactString());
        SpawnedBot->Destroy();
        return;
    }
    if (!SpawnedBot->InitializeForTraining(*ActiveArena, *ActiveParticipant, DistanceBand))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Training] %s could not start StateTree on %s. Check controller, schema and Context bindings."),
            ModeLabel, *GetNameSafe(SpawnedBot));
        SpawnedBot->Destroy();
        return;
    }
    TrackActor(*SpawnedBot);
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
            TEXT("[Training] Target spawn failed: no valid WorldStatic ground in %.0f-%.0f cm around %s. StateTree/NavMesh are NOT used by targets."),
            DistanceBand.MinCentimeters, DistanceBand.MaxCentimeters,
            *ActiveArena->GetCenterLocation().ToCompactString());
        return;
    }
    const ATrainingTarget* TargetDefaults = ActiveArena->TargetClass.GetDefaultObject();
    const FVector SpawnPosition = GroundPosition + FVector(0.0, 0.0,
        TargetDefaults->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 2.0f);
    const FRotator SpawnRotation(0.0, (ActiveArena->GetCenterLocation() - SpawnPosition).Rotation().Yaw, 0.0);
    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Owner = ActiveArena;
    SpawnParameters.Instigator = ActiveParticipant->GetPawn();
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
    ATrainingTarget* SpawnedTarget = ActiveArena->GetWorld()->SpawnActor<ATrainingTarget>(
        ActiveArena->TargetClass, SpawnPosition, SpawnRotation, SpawnParameters);
    if (!SpawnedTarget)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Training] Target spawn failed: collision at %s."),
            *SpawnPosition.ToCompactString());
        return;
    }
    FVector AdjustedGround;
    const FVector FinalPosition = SpawnedTarget->GetActorLocation();
    const float HalfHeight = SpawnedTarget->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    if (!ActiveArena->ProjectGroundPosition(FinalPosition, DistanceBand, AdjustedGround)
        || FMath::Abs(FinalPosition.Z - HalfHeight - AdjustedGround.Z) > 5.0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Training] Target rejected after collision adjustment: %s."),
            *FinalPosition.ToCompactString());
        SpawnedTarget->Destroy();
        return;
    }
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
    if (GetArena())
    {
        if (TrainingRules::CanSpawnCombatAI(GetLivingAICount()))
        {
            SpawnCombatBot();
        }
    }
}
