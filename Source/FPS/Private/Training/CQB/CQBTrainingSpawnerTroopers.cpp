#include "Training/CQB/CQBTrainingSpawner.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
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
    if (!HasAuthority()
        || SessionState != ECQBTrainingSessionState::Loading
        || OrderedSpawnPoints.IsEmpty())
    {
        return;
    }

    NextSpawnIndex = 0;
    KilledTroopers = 0;
    CountedDeathIndices.Reset();
    SpawnedTroopers.Reset();

    if (!SpawnUntilFull(LoadedTrooperClass))
    {
        AbortPendingStart(LOCTEXT("InitialSpawnFailed",
            "CQB could not create the initial troopers. Check the Trooper Class, StateTree and navigation setup."));
        return;
    }

    SessionState = ECQBTrainingSessionState::Running;
    NotifyParticipantStarted();
    ResolvePendingStart(true, FText::GetEmpty());
    ForceNetUpdate();

    UE_LOG(LogCQBTrainingSpawnerTroopers, Log,
        TEXT("%s: CQB training started. Initial alive=%d, spawn points=%d, next index=%d."),
        *GetName(), CountAliveTroopers(), OrderedSpawnPoints.Num(), NextSpawnIndex);
}

bool ACQBTrainingSpawner::SpawnUntilFull(UClass& LoadedTrooperClass)
{
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
            UE_LOG(LogCQBTrainingSpawnerTroopers, Error,
                TEXT("%s: SpawnOrder %d failed. The designed route will not be skipped."),
                *GetName(), SpawnPoint->SpawnOrder);
            return false;
        }

        SpawnedTroopers.Add(SpawnedPawn);
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

    FActorSpawnParameters SpawnParams;
    SpawnParams.Owner = this;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    APawn* SpawnedPawn = World->SpawnActor<APawn>(
        &LoadedTrooperClass, SpawnPoint.GetActorTransform(), SpawnParams);
    if (!SpawnedPawn)
    {
        UE_LOG(LogCQBTrainingSpawnerTroopers, Warning,
            TEXT("%s: Failed to spawn order %d at %s."),
            *GetName(), SpawnPoint.SpawnOrder, *SpawnPoint.GetActorLocation().ToString());
        return nullptr;
    }

    UCQBTrainingAIComponent* CQBConfig =
        SpawnedPawn->FindComponentByClass<UCQBTrainingAIComponent>();
    if (!CQBConfig)
    {
        UE_LOG(LogCQBTrainingSpawnerTroopers, Error,
            TEXT("%s: %s has no CQB Training AI component."),
            *GetName(), *GetNameSafe(SpawnedPawn));
        SpawnedPawn->Destroy();
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

        CQBController = World->SpawnActor<ACQBTrainingAIController>(
            ACQBTrainingAIController::StaticClass(),
            SpawnedPawn->GetActorLocation(),
            SpawnedPawn->GetActorRotation());

        if (CQBController)
        {
            CQBController->Possess(SpawnedPawn);
        }
    }

    if (!CQBController || !CQBController->RefreshFromPawnConfiguration())
    {
        UE_LOG(LogCQBTrainingSpawnerTroopers, Error,
            TEXT("%s: CQB controller or StateTree initialization failed for %s."),
            *GetName(), *GetNameSafe(SpawnedPawn));

        if (CQBController)
        {
            CQBController->StopCQBBrain();
            CQBController->UnPossess();
        }
        SpawnedPawn->Destroy();
        return nullptr;
    }

    UE_LOG(LogCQBTrainingSpawnerTroopers, Log,
        TEXT("Spawned order=%d pawn=%s patrol=%d"),
        SpawnPoint.SpawnOrder, *GetNameSafe(SpawnedPawn), CQBConfig->GetPatrolRoute().PatrolLocations.Num());

    return SpawnedPawn;
}

void ACQBTrainingSpawner::DestroySpawnedTroopersOnly()
{
    for (const TWeakObjectPtr<APawn>& PawnPtr : SpawnedTroopers)
    {
        APawn* SpawnedPawn = PawnPtr.Get();
        if (!IsValid(SpawnedPawn))
        {
            continue;
        }

        if (AController* Controller = SpawnedPawn->GetController())
        {
            if (ACQBTrainingAIController* CQBController = Cast<ACQBTrainingAIController>(Controller))
            {
                CQBController->StopCQBBrain();
                CQBController->UnPossess();
            }
            else
            {
                Controller->UnPossess();
                if (IsValid(Controller) && !Controller->IsActorBeingDestroyed())
                {
                    Controller->Destroy();
                }
            }
        }

        SpawnedPawn->SetLifeSpan(0.0f);
        SpawnedPawn->Destroy();
    }
    SpawnedTroopers.Reset();
}

void ACQBTrainingSpawner::NotifyTrooperDeath(APawn& Trooper)
{
    if (!HasAuthority() || SessionState != ECQBTrainingSessionState::Running)
    {
        return;
    }

    const int32 TrooperIndex = SpawnedTroopers.IndexOfByPredicate(
        [&Trooper](const TWeakObjectPtr<APawn>& Entry)
        {
            return Entry.Get() == &Trooper;
        });

    if (TrooperIndex == INDEX_NONE || CountedDeathIndices.Contains(TrooperIndex))
    {
        return;
    }

    if (UCQBTrainingAIComponent* Config = Trooper.FindComponentByClass<UCQBTrainingAIComponent>())
    {
        Config->PrepareForDeathCleanup(CorpseLifetimeSeconds);
    }

    RegisterTrooperDeath(TrooperIndex);
}

void ACQBTrainingSpawner::RegisterTrooperDeath(const int32 TrooperIndex)
{
    if (SessionState != ECQBTrainingSessionState::Running
        || !SpawnedTroopers.IsValidIndex(TrooperIndex)
        || CountedDeathIndices.Contains(TrooperIndex))
    {
        return;
    }

    CountedDeathIndices.Add(TrooperIndex);
    KilledTroopers = FMath::Min(KilledTroopers + 1, OrderedSpawnPoints.Num());
    NotifyParticipantProgress();

    if (!RefillAfterDeath())
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

    if (EndActor)
    {
        EndActor->Spawner = this;
        EndActor->SetEndEnabled(true);
    }

    NotifyParticipantProgress();
    ForceNetUpdate();
}


#undef LOCTEXT_NAMESPACE
