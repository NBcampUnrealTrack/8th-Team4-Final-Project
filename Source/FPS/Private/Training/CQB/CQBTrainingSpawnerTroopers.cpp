#include "Training/CQB/CQBTrainingSpawner.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Training/CQB/CQBTrainingAIComponent.h"
#include "Training/CQB/CQBTrainingAIController.h"
#include "Training/CQB/CQBTrainingEndActor.h"
#include "Training/CQB/CQBTrainingPatrolPoint.h"
#include "Training/CQB/CQBTrainingSpawnPoint.h"

#define LOCTEXT_NAMESPACE "CQBTrainingSpawner"

DEFINE_LOG_CATEGORY_STATIC(LogCQBTrainingSpawnerTroopers, Log, All);

bool ACQBTrainingSpawner::CollectSpawnPoints()
{
    OrderedSpawnPoints.Reset();

    UWorld* World = GetWorld();
    if (!World)
    {
        return false;
    }

    TArray<ACQBTrainingSpawnPoint*> FoundPoints;
    for (TActorIterator<ACQBTrainingSpawnPoint> It(World); It; ++It)
    {
        ACQBTrainingSpawnPoint* Point = *It;
        if (IsValid(Point) && Point->SpawnGroup == SpawnGroup)
        {
            FoundPoints.Add(Point);
        }
    }

    FoundPoints.Sort([](const ACQBTrainingSpawnPoint& A, const ACQBTrainingSpawnPoint& B)
    {
        if (A.SpawnOrder != B.SpawnOrder)
        {
            return A.SpawnOrder < B.SpawnOrder;
        }
        return A.GetName() < B.GetName();
    });

    if (FoundPoints.IsEmpty())
    {
        UE_LOG(LogCQBTrainingSpawnerTroopers, Error,
            TEXT("%s: No CQBTrainingSpawnPoint found for group '%s'."),
            *GetName(), *SpawnGroup.ToString());
        return false;
    }

    TSet<int32> UsedOrders;
    bool bValid = true;
    for (int32 PointIndex = 0; PointIndex < FoundPoints.Num(); ++PointIndex)
    {
        ACQBTrainingSpawnPoint* Point = FoundPoints[PointIndex];
        if (!IsValid(Point))
        {
            bValid = false;
            continue;
        }

        if (UsedOrders.Contains(Point->SpawnOrder))
        {
            bValid = false;
            UE_LOG(LogCQBTrainingSpawnerTroopers, Error,
                TEXT("%s: Duplicate SpawnOrder %d in group '%s'."),
                *GetName(), Point->SpawnOrder, *SpawnGroup.ToString());
            continue;
        }

        if (Point->SpawnOrder != PointIndex)
        {
            bValid = false;
            UE_LOG(LogCQBTrainingSpawnerTroopers, Error,
                TEXT("%s: SpawnOrder sequence must be contiguous from 0. Expected %d but found %d on %s."),
                *GetName(), PointIndex, Point->SpawnOrder, *GetNameSafe(Point));
            continue;
        }

        bool bPatrolValid = !Point->PatrolPoints.IsEmpty();
        for (const ACQBTrainingPatrolPoint* PatrolPoint : Point->PatrolPoints)
        {
            bPatrolValid &= IsValid(PatrolPoint);
        }
        if (!bPatrolValid)
        {
            bValid = false;
            UE_LOG(LogCQBTrainingSpawnerTroopers, Error,
                TEXT("%s: SpawnOrder %d has an empty or invalid Patrol Points list."),
                *GetName(), Point->SpawnOrder);
            continue;
        }

        UsedOrders.Add(Point->SpawnOrder);
        OrderedSpawnPoints.Add(Point);

        UE_LOG(LogCQBTrainingSpawnerTroopers, Log,
            TEXT("SpawnPoint order=%d actor=%s patrol=%d"),
            Point->SpawnOrder, *GetNameSafe(Point), Point->PatrolPoints.Num());
    }

    if (!bValid)
    {
        OrderedSpawnPoints.Reset();
    }
    return bValid && !OrderedSpawnPoints.IsEmpty();
}

void ACQBTrainingSpawner::StartLoadedTraining(UClass& LoadedTrooperClass)
{
    if (bStopping || SessionState != ECQBTrainingSessionState::Loading)
    {
        return;
    }
    if (!Participant.IsValid() || !Participant->GetPawn() || OrderedSpawnPoints.IsEmpty())
    {
        AbortPendingStart(LOCTEXT("StartContextLost", "The CQB start context is no longer available."));
        return;
    }

    const uint32 StartGeneration = SessionGeneration;
    NextSpawnIndex = 0;
    KilledTroopers = 0;
    CountedDeadTroopers.Reset();
    SpawnedTroopers.Reset();

    const bool bSpawned = SpawnUntilFull(LoadedTrooperClass);
    if (SessionGeneration != StartGeneration || SessionState != ECQBTrainingSessionState::Loading)
    {
        return;
    }
    if (!bSpawned)
    {
        AbortPendingStart(LOCTEXT("InitialSpawnFailed",
            "CQB could not create the initial troopers. Check the Trooper Class, StateTree and navigation setup."));
        return;
    }

    SessionState = ECQBTrainingSessionState::Running;
    NotifyParticipantStarted();
    if (SessionGeneration != StartGeneration || SessionState != ECQBTrainingSessionState::Running)
    {
        return;
    }
    ResolvePendingStart(true, FText::GetEmpty());

    UE_LOG(LogCQBTrainingSpawnerTroopers, Log,
        TEXT("%s: CQB training started. Initial alive=%d, spawn points=%d, next index=%d."),
        *GetName(), CountAliveTroopers(), OrderedSpawnPoints.Num(), NextSpawnIndex);
}

bool ACQBTrainingSpawner::SpawnUntilFull(UClass& LoadedTrooperClass)
{
    const uint32 SpawnGeneration = SessionGeneration;
    int32 AliveCount = CountAliveTroopers();
    const int32 ConcurrentLimit = FMath::Clamp(MaxConcurrentTroopers, 1, 32);

    while (AliveCount < ConcurrentLimit && NextSpawnIndex < OrderedSpawnPoints.Num())
    {
        ACQBTrainingSpawnPoint* SpawnPoint = OrderedSpawnPoints[NextSpawnIndex].Get();
        if (!IsValid(SpawnPoint))
        {
            UE_LOG(LogCQBTrainingSpawnerTroopers, Error,
                TEXT("%s: Spawn point at runtime index %d became invalid."),
                *GetName(), NextSpawnIndex);
            return false;
        }

        APawn* SpawnedPawn = SpawnFromPoint(*SpawnPoint, LoadedTrooperClass);
        if (!SpawnedPawn)
        {
            if (SessionGeneration != SpawnGeneration || !IsTrainingActive())
            {
                return false;
            }
            UE_LOG(LogCQBTrainingSpawnerTroopers, Error,
                TEXT("%s: SpawnOrder %d failed. The designed route will not be skipped."),
                *GetName(), SpawnPoint->SpawnOrder);
            return false;
        }

        SpawnedTroopers.Add(SpawnedPawn);
        SpawnedPawn->OnDestroyed.AddDynamic(this, &ThisClass::HandleTrooperDestroyed);
        ++NextSpawnIndex;
        ++AliveCount;
    }

    return true;
}

int32 ACQBTrainingSpawner::CountAliveTroopers() const
{
    int32 AliveCount = 0;
    for (const TWeakObjectPtr<APawn>& PawnPtr : SpawnedTroopers)
    {
        const APawn* SpawnedPawn = PawnPtr.Get();
        if (!IsValid(SpawnedPawn))
        {
            continue;
        }

        const UCQBTrainingAIComponent* CQBConfig =
            SpawnedPawn->FindComponentByClass<UCQBTrainingAIComponent>();
        if (CQBConfig && CQBConfig->IsAlive())
        {
            ++AliveCount;
        }
    }
    return AliveCount;
}

APawn* ACQBTrainingSpawner::SpawnFromPoint(
    ACQBTrainingSpawnPoint& SpawnPoint, UClass& LoadedTrooperClass)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return nullptr;
    }

    const uint32 SpawnGeneration = SessionGeneration;
    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    APawn* SpawnedPawn = World->SpawnActor<APawn>(
        &LoadedTrooperClass, SpawnPoint.GetActorTransform(), SpawnParams);
    if (!IsValid(SpawnedPawn))
    {
        UE_LOG(LogCQBTrainingSpawnerTroopers, Warning,
            TEXT("%s: Failed to spawn order %d at %s."),
            *GetName(), SpawnPoint.SpawnOrder, *SpawnPoint.GetActorLocation().ToString());
        return nullptr;
    }

    if (bStopping || SessionGeneration != SpawnGeneration || !IsTrainingActive())
    {
        DestroyTrooper(*SpawnedPawn);
        return nullptr;
    }

    SpawnedPawn->SetReplicates(false);
    SpawnedPawn->SetReplicateMovement(false);

    UCQBTrainingAIComponent* CQBConfig =
        SpawnedPawn->FindComponentByClass<UCQBTrainingAIComponent>();
    if (!CQBConfig)
    {
        UE_LOG(LogCQBTrainingSpawnerTroopers, Error,
            TEXT("%s: %s has no CQB Training AI component."),
            *GetName(), *GetNameSafe(SpawnedPawn));
        DestroyTrooper(*SpawnedPawn);
        return nullptr;
    }

    TArray<FVector> PatrolLocations;
    PatrolLocations.Reserve(SpawnPoint.PatrolPoints.Num());
    for (ACQBTrainingPatrolPoint* PatrolPoint : SpawnPoint.PatrolPoints)
    {
        if (IsValid(PatrolPoint))
        {
            PatrolLocations.Add(PatrolPoint->GetActorLocation());
        }
    }

    CQBConfig->ConfigurePatrolRoute(
        SpawnPoint.GetActorLocation(),
        PatrolLocations,
        SpawnPoint.bPingPongPatrol);
    CQBConfig->SetTrainingSpawner(this);
    CQBConfig->bSuppressStandardEliminationFeedback = bSuppressStandardEliminationFeedback;
    CQBConfig->DeathCleanupSeconds = CorpseLifetimeSeconds;

    ACQBTrainingAIController* CQBController =
        Cast<ACQBTrainingAIController>(SpawnedPawn->GetController());
    if (!CQBController)
    {
        if (AController* ExistingController = SpawnedPawn->GetController())
        {
            ExistingController->UnPossess();
            if (IsValid(ExistingController) && !ExistingController->IsActorBeingDestroyed())
            {
                ExistingController->Destroy();
            }
        }

        if (!IsValid(SpawnedPawn))
        {
            return nullptr;
        }
        if (bStopping || SessionGeneration != SpawnGeneration || !IsTrainingActive())
        {
            DestroyTrooper(*SpawnedPawn);
            return nullptr;
        }

        CQBController = World->SpawnActor<ACQBTrainingAIController>(
            ACQBTrainingAIController::StaticClass(),
            SpawnedPawn->GetActorLocation(),
            SpawnedPawn->GetActorRotation());

        if (IsValid(CQBController) && IsValid(SpawnedPawn))
        {
            CQBController->Possess(SpawnedPawn);
        }
    }

    if (!IsValid(SpawnedPawn))
    {
        if (IsValid(CQBController))
        {
            CQBController->Destroy();
        }
        return nullptr;
    }
    if (bStopping || SessionGeneration != SpawnGeneration || !IsTrainingActive())
    {
        DestroyTrooper(*SpawnedPawn);
        return nullptr;
    }

    const bool bControllerReady = IsValid(CQBController) && CQBController->RefreshFromPawnConfiguration();
    if (!IsValid(SpawnedPawn))
    {
        if (IsValid(CQBController))
        {
            CQBController->Destroy();
        }
        return nullptr;
    }
    if (!bControllerReady || bStopping || SessionGeneration != SpawnGeneration || !IsTrainingActive())
    {
        UE_LOG(LogCQBTrainingSpawnerTroopers, Error,
            TEXT("%s: CQB controller initialization failed or the session was cancelled for %s."),
            *GetName(), *GetNameSafe(SpawnedPawn));
        DestroyTrooper(*SpawnedPawn);
        return nullptr;
    }

    UE_LOG(LogCQBTrainingSpawnerTroopers, Log,
        TEXT("Spawned order=%d pawn=%s patrol=%d"),
        SpawnPoint.SpawnOrder, *GetNameSafe(SpawnedPawn), CQBConfig->GetPatrolRoute().PatrolLocations.Num());

    return SpawnedPawn;
}

void ACQBTrainingSpawner::DestroyTrooper(APawn& Trooper)
{
    Trooper.OnDestroyed.RemoveDynamic(this, &ThisClass::HandleTrooperDestroyed);
    if (UCQBTrainingAIComponent* Config = Trooper.FindComponentByClass<UCQBTrainingAIComponent>())
    {
        Config->SetTrainingSpawner(nullptr);
    }

    if (AController* Controller = Trooper.GetController())
    {
        if (ACQBTrainingAIController* CQBController = Cast<ACQBTrainingAIController>(Controller))
        {
            CQBController->StopCQBBrain();
        }
        Controller->UnPossess();
        if (IsValid(Controller) && !Controller->IsActorBeingDestroyed())
        {
            Controller->Destroy();
        }
    }

    Trooper.SetLifeSpan(0.0f);
    Trooper.Destroy();
}

void ACQBTrainingSpawner::DestroySpawnedTroopersOnly()
{
    TArray<TWeakObjectPtr<APawn>> Troopers = MoveTemp(SpawnedTroopers);
    for (const TWeakObjectPtr<APawn>& PawnPtr : Troopers)
    {
        if (APawn* SpawnedPawn = PawnPtr.Get())
        {
            DestroyTrooper(*SpawnedPawn);
        }
    }
}

void ACQBTrainingSpawner::HandleTrooperDestroyed(AActor* DestroyedActor)
{
    APawn* Trooper = Cast<APawn>(DestroyedActor);
    const TWeakObjectPtr<APawn> TrooperRef(Trooper);
    const bool bDeathWasCounted = CountedDeadTroopers.Remove(TrooperRef) > 0;
    SpawnedTroopers.RemoveAll([TrooperRef](const TWeakObjectPtr<APawn>& Entry)
    {
        return !Entry.IsValid() || Entry == TrooperRef;
    });

    if (!bStopping && !bDeathWasCounted && SessionState == ECQBTrainingSessionState::Running)
    {
        UE_LOG(LogCQBTrainingSpawnerTroopers, Warning,
            TEXT("%s: A live CQB trooper was removed; resetting the incomplete session."), *GetName());
        DespawnSpawnedTroopers();
    }
}

void ACQBTrainingSpawner::NotifyTrooperDeath(APawn& Trooper)
{
    const TWeakObjectPtr<APawn> TrooperRef(&Trooper);
    if (bStopping || SessionState != ECQBTrainingSessionState::Running
        || !SpawnedTroopers.Contains(TrooperRef) || CountedDeadTroopers.Contains(TrooperRef))
    {
        return;
    }

    if (UCQBTrainingAIComponent* Config = Trooper.FindComponentByClass<UCQBTrainingAIComponent>())
    {
        Config->PrepareForDeathCleanup(CorpseLifetimeSeconds);
    }

    RegisterTrooperDeath(Trooper);
}

void ACQBTrainingSpawner::RegisterTrooperDeath(APawn& Trooper)
{
    const uint32 DeathGeneration = SessionGeneration;
    CountedDeadTroopers.Add(&Trooper);
    KilledTroopers = FMath::Min(KilledTroopers + 1, OrderedSpawnPoints.Num());
    NotifyParticipantProgress();
    if (SessionGeneration != DeathGeneration || SessionState != ECQBTrainingSessionState::Running)
    {
        return;
    }

    const bool bRefilled = RefillAfterDeath();
    if (SessionGeneration != DeathGeneration || SessionState != ECQBTrainingSessionState::Running)
    {
        return;
    }
    if (!bRefilled)
    {
        UE_LOG(LogCQBTrainingSpawnerTroopers, Error,
            TEXT("%s: Refill failed. CQB session is being reset."), *GetName());
        DespawnSpawnedTroopers();
        return;
    }

    if (NextSpawnIndex >= OrderedSpawnPoints.Num()
        && KilledTroopers >= OrderedSpawnPoints.Num()
        && CountAliveTroopers() == 0)
    {
        CompleteTraining();
    }
}

bool ACQBTrainingSpawner::RefillAfterDeath()
{
    if (SessionState != ECQBTrainingSessionState::Running
        || NextSpawnIndex >= OrderedSpawnPoints.Num())
    {
        return true;
    }

    UClass* LoadedClass = TrooperClass.Get();
    if (!LoadedClass)
    {
        UE_LOG(LogCQBTrainingSpawnerTroopers, Error,
            TEXT("%s: CQB trooper class was unloaded during an active session."),
            *GetName());
        return false;
    }

    return SpawnUntilFull(*LoadedClass);
}

void ACQBTrainingSpawner::CompleteTraining()
{
    if (SessionState != ECQBTrainingSessionState::Running)
    {
        return;
    }

    SessionState = ECQBTrainingSessionState::Completed;

    if (IsValid(EndActor))
    {
        EndActor->Spawner = this;
        EndActor->SetEndEnabled(true);
    }

    NotifyParticipantProgress();
}


#undef LOCTEXT_NAMESPACE
