#include "Training/TrainingScenario.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Training/AI/TrainingCombatAI.h"
#include "Training/TrainingCharacter.h"
#include "Training/TrainingModeDefinition.h"
#include "Training/TrainingArena.h"
#include "Training/TrainingTarget.h"

#define LOCTEXT_NAMESPACE "TrainingScenario"

namespace
{
const ATrainingCharacter* ValidateBotClass(UClass* BotClass, FText& OutFailure)
{
    if (!BotClass || BotClass->HasAnyClassFlags(CLASS_Abstract))
    {
        OutFailure = LOCTEXT("BotClass", "Assign the bot Blueprint required by this Training Mode to the Training Arena.");
        return nullptr;
    }
    const ATrainingCharacter* Defaults = Cast<ATrainingCharacter>(BotClass->GetDefaultObject());
    if (!Defaults || !Defaults->GetMesh()->GetSkeletalMeshAsset())
    {
        OutFailure = LOCTEXT("BotMesh", "Assign a Skeletal Mesh to the training bot Blueprint.");
        return nullptr;
    }
    if (!Defaults->DissolveMaterial)
    {
        OutFailure = LOCTEXT("BotMaterial", "Assign a Dissolve Material to the training bot Blueprint.");
        return nullptr;
    }
    return Defaults;
}
}

ETrainingBotSource UTargetAcquisitionScenario::GetBotSource() const { return ETrainingBotSource::Target; }
ETrainingBotSource UCombatResponseScenario::GetBotSource() const { return ETrainingBotSource::CombatAI; }

bool UTargetAcquisitionScenario::ValidateConfiguration(const ATrainingArena& InArena,
    const FTrainingDistanceBand& InBand, FText& OutFailure) const
{
    if (!ValidateBotClass(InArena.TargetClass.Get(), OutFailure)) { return false; }
    FVector ProbePosition;
    if (!InArena.FindGroundSpawn(InBand, ProbePosition))
    {
        OutFailure = LOCTEXT("TargetGround", "No valid WorldStatic floor in this distance band. Check the arena center, floor collision and ground probe height.");
        return false;
    }
    return true;
}

bool UCombatResponseScenario::ValidateConfiguration(const ATrainingArena& InArena,
    const FTrainingDistanceBand& InBand, FText& OutFailure) const
{
    const ATrainingCombatAI* Defaults = Cast<ATrainingCombatAI>(ValidateBotClass(InArena.CombatAIClass.Get(), OutFailure));
    if (!Defaults) { return false; }
    if (!Defaults->HasConfiguredBehavior())
    {
        OutFailure = LOCTEXT("CombatTree", "Assign a Training Combat AI Controller Blueprint with a compiled Training State Tree to the Combat AI.");
        return false;
    }
    FVector ProbePosition;
    if (!InArena.FindNavSpawn(InBand, Defaults->GetNavAgentPropertiesRef(), ProbePosition))
    {
        OutFailure = LOCTEXT("CombatNavigation", "No valid NavMesh in the selected distance band. Add/build NavMeshBoundsVolume over the combat training floor.");
        return false;
    }
    return true;
}

void UTrainingScenario::Initialize(ATrainingArena& InArena, APlayerController& InParticipant,
    const FTrainingDistanceBand& InBand, const FTrainingModeConfig& InModeConfig)
{
    Arena = &InArena;
    Participant = &InParticipant;
    DistanceBand = InBand;
    MaxLivingActors = FMath::Max(0, InModeConfig.MaxLivingActors);
    bStopped = false;
}

ATrainingArena* UTrainingScenario::GetArena() const { return Arena.Get(); }
APlayerController* UTrainingScenario::GetParticipant() const { return Participant.Get(); }

void UTrainingScenario::TrackActor(ATrainingCharacter& SpawnedActor)
{
    if (bStopped || !IsValid(&SpawnedActor))
    {
        if (IsValid(&SpawnedActor)) { SpawnedActor.Destroy(); }
        return;
    }
    OwnedActors.Add(&SpawnedActor);
    SpawnedActor.OnDestroyed.AddDynamic(this, &ThisClass::HandleActorDestroyed);
    SpawnedActor.OnTrainingHit.AddUObject(this, &ThisClass::HandleBotHit);
    OnBotSpawned.Broadcast(SpawnedActor);
}

bool UTrainingScenario::OwnsActor(const AActor* Candidate) const
{
    return Candidate && OwnedActors.ContainsByPredicate([Candidate](const TWeakObjectPtr<ATrainingCharacter>& Entry)
    {
        return Entry.Get() == Candidate;
    });
}

void UTrainingScenario::HandleBotHit(ATrainingCharacter& Bot, const AActor* InstigatorActor,
    const float Damage, const bool bKilled)
{
    if (!bStopped) { OnBotHit.Broadcast(Bot, InstigatorActor, Damage, bKilled); }
}

void UTrainingScenario::HandleActorDestroyed(AActor* DestroyedActor)
{
    if (ATrainingCharacter* Bot = Cast<ATrainingCharacter>(DestroyedActor))
    {
        Bot->OnTrainingHit.RemoveAll(this);
    }
    OwnedActors.RemoveAll([DestroyedActor](const TWeakObjectPtr<ATrainingCharacter>& Entry)
    {
        return !Entry.IsValid() || Entry.Get() == DestroyedActor;
    });
}

void UTrainingScenario::Stop()
{
    bStopped = true;
    OnBotSpawned.Clear();
    OnBotHit.Clear();
    TArray<TWeakObjectPtr<ATrainingCharacter>> ActorsToRemove = MoveTemp(OwnedActors);
    for (const TWeakObjectPtr<ATrainingCharacter>& Entry : ActorsToRemove)
    {
        if (ATrainingCharacter* Bot = Entry.Get())
        {
            Bot->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleActorDestroyed);
            Bot->OnTrainingHit.RemoveAll(this);
            Bot->Destroy();
        }
    }
    Arena.Reset();
    Participant.Reset();
}

bool UCombatResponseScenario::SpawnCombatBot()
{
    ATrainingArena* ActiveArena = GetArena();
    APlayerController* ActiveParticipant = GetParticipant();
    if (!ActiveArena || !ActiveParticipant || !ActiveParticipant->GetPawn() || !ActiveArena->CombatAIClass)
    {
        return false;
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
        return false;
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
    if (!IsValid(SpawnedBot))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Training] %s spawn failed: SpawnActor rejected %s at %s."),
            ModeLabel, *GetNameSafe(BotClass.Get()), *SpawnPosition.ToCompactString());
        return false;
    }
    if (GetArena() != ActiveArena || GetParticipant() != ActiveParticipant || !ActiveParticipant->GetPawn())
    {
        SpawnedBot->Destroy();
        return false;
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
        return false;
    }
    if (!SpawnedBot->InitializeForTraining(*ActiveArena, *ActiveParticipant, DistanceBand))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Training] %s could not start StateTree on %s. Check controller, schema and Context bindings."),
            ModeLabel, *GetNameSafe(SpawnedBot));
        SpawnedBot->Destroy();
        return false;
    }
    TrackActor(*SpawnedBot);
    return OwnsActor(SpawnedBot);
}

bool UTargetAcquisitionScenario::SpawnNext()
{
    if (!HasSpawnCapacity()) { return true; }
    ATrainingArena* ActiveArena = GetArena();
    APlayerController* ActiveParticipant = GetParticipant();
    if (!ActiveArena || !ActiveParticipant || !ActiveParticipant->GetPawn() || !ActiveArena->TargetClass)
    {
        return false;
    }
    FVector GroundPosition;
    if (!ActiveArena->FindGroundSpawn(DistanceBand, GroundPosition))
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Training] Target spawn failed: no valid WorldStatic ground in %.0f-%.0f cm around %s. StateTree/NavMesh are NOT used by targets."),
            DistanceBand.MinCentimeters, DistanceBand.MaxCentimeters,
            *ActiveArena->GetCenterLocation().ToCompactString());
        return false;
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
    if (!IsValid(SpawnedTarget))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Training] Target spawn failed: collision at %s."),
            *SpawnPosition.ToCompactString());
        return false;
    }
    if (GetArena() != ActiveArena || GetParticipant() != ActiveParticipant || !ActiveParticipant->GetPawn())
    {
        SpawnedTarget->Destroy();
        return false;
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
        return false;
    }
    TrackActor(*SpawnedTarget);
    return OwnsActor(SpawnedTarget);
}

bool UTrainingScenario::HasSpawnCapacity() const
{
    int32 LivingCount = 0;
    for (const TWeakObjectPtr<ATrainingCharacter>& Entry : OwnedActors)
    {
        const ATrainingCharacter* TrainingPawn = Entry.Get();
        if (TrainingPawn && TrainingPawn->IsAlive())
        {
            ++LivingCount;
        }
    }
    return MaxLivingActors <= 0 || LivingCount < MaxLivingActors;
}

bool UCombatResponseScenario::SpawnNext()
{
    if (HasSpawnCapacity())
    {
        return SpawnCombatBot();
    }
    return true;
}

#undef LOCTEXT_NAMESPACE
