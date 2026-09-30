#include "Training/CQB/CQBTrainingAIController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/StateTreeAIComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GAS/RogueHealthComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISense.h"
#include "Perception/AISense_Hearing.h"
#include "Perception/AISense_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Player/ShooterPlayerState.h"
#include "StateTree.h"
#include "Tags/GameplayTags.h"
#include "TimerManager.h"
#include "Training/CQB/CQBTrainingAIComponent.h"
#include "Training/CQB/CQBTrainingAwarenessComponent.h"
#include "Training/CQB/CQBCombatIncidentSubsystem.h"
#include "Training/CQB/CQBTrainingCombatComponent.h"
#include "Training/CQB/CQBTrainingInvestigationComponent.h"
#include "Training/CQB/CQBTrainingLocomotionComponent.h"
#include "Training/CQB/CQBTrainingPatrolComponent.h"
#include "Training/CQB/CQBTrainingSpawner.h"

DEFINE_LOG_CATEGORY_STATIC(LogCQBTrainingAI, Log, All);

ACQBTrainingAIController::ACQBTrainingAIController()
{
    PrimaryActorTick.bCanEverTick = false;
    bStartAILogicOnPossess = false;
    bAllowStrafe = false;
    bWantsPlayerState = true;

    CQBPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("CQBPerception"));
    SetPerceptionComponent(*CQBPerceptionComponent);

    const FCQBPerceptionSettings DefaultPerception;
    SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("CQBSightConfig"));
    SightConfig->SightRadius = DefaultPerception.SightRadius;
    SightConfig->LoseSightRadius = DefaultPerception.LoseSightRadius;
    SightConfig->PeripheralVisionAngleDegrees = DefaultPerception.PeripheralVisionAngleDegrees;
    SightConfig->DetectionByAffiliation.bDetectEnemies = true;
    SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
    SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
    SightConfig->SetMaxAge(DefaultPerception.SightMaxAgeSeconds);

    HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("CQBHearingConfig"));
    HearingConfig->HearingRange = DefaultPerception.GetMaximumHearingRange();
    HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
    HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
    HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
    HearingConfig->SetMaxAge(DefaultPerception.HearingMaxAgeSeconds);

    CQBPerceptionComponent->ConfigureSense(*SightConfig);
    CQBPerceptionComponent->ConfigureSense(*HearingConfig);
    CQBPerceptionComponent->SetDominantSense(UAISense_Sight::StaticClass());

    CQBStateTreeComponent = CreateDefaultSubobject<UStateTreeAIComponent>(TEXT("CQBStateTree"));
    CQBStateTreeComponent->SetStartLogicAutomatically(false);
    CQBStateTreeComponent->PrimaryComponentTick.bStartWithTickEnabled = false;
    BrainComponent = CQBStateTreeComponent;

    AwarenessComponent = CreateDefaultSubobject<UCQBTrainingAwarenessComponent>(TEXT("CQBAwareness"));
    LocomotionComponent = CreateDefaultSubobject<UCQBTrainingLocomotionComponent>(TEXT("CQBLocomotion"));
    PatrolComponent = CreateDefaultSubobject<UCQBTrainingPatrolComponent>(TEXT("CQBPatrol"));
    InvestigationComponent = CreateDefaultSubobject<UCQBTrainingInvestigationComponent>(TEXT("CQBInvestigation"));
    CombatComponent = CreateDefaultSubobject<UCQBTrainingCombatComponent>(TEXT("CQBCombat"));
}

void ACQBTrainingAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearAllTimersForObject(this);
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DeathShutdownTimer);
    }
    UnbindHealthEvents();
    StopCQBBrain();
    if (CQBPerceptionComponent)
    {
        CQBPerceptionComponent->OnTargetPerceptionUpdated.RemoveDynamic(
            this, &ThisClass::HandleTargetPerceptionUpdated);
        CQBPerceptionComponent->ForgetAll();
        CQBPerceptionComponent->Deactivate();
    }
    Super::EndPlay(EndPlayReason);
}

void ACQBTrainingAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);

    bDeathShutdownStarted = false;
    if (CQBPerceptionComponent)
    {
        CQBPerceptionComponent->Activate(true);
        CQBPerceptionComponent->SetComponentTickEnabled(true);
        CQBPerceptionComponent->OnTargetPerceptionUpdated.RemoveDynamic(
            this, &ThisClass::HandleTargetPerceptionUpdated);
        CQBPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(
            this, &ThisClass::HandleTargetPerceptionUpdated);
    }

    if (AwarenessComponent)
    {
        AwarenessComponent->ResetRuntimeState();
    }
    if (LocomotionComponent)
    {
        LocomotionComponent->ResetRuntimeState();
    }
    if (PatrolComponent)
    {
        PatrolComponent->ResetRuntimeState();
    }
    if (InvestigationComponent)
    {
        InvestigationComponent->ResetRuntimeState();
    }
    if (CombatComponent)
    {
        CombatComponent->ResetRuntimeState();
    }
}

void ACQBTrainingAIController::OnUnPossess()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DeathShutdownTimer);
    }
    UnbindHealthEvents();
    if (CQBPerceptionComponent)
    {
        CQBPerceptionComponent->OnTargetPerceptionUpdated.RemoveDynamic(
            this, &ThisClass::HandleTargetPerceptionUpdated);
    }

    StopCQBBrain();
    if (AwarenessComponent)
    {
        AwarenessComponent->ResetRuntimeState();
    }

    Super::OnUnPossess();

    if (!IsActorBeingDestroyed())
    {
        Destroy();
    }
}

bool ACQBTrainingAIController::StartCQBBrain()
{
    UCQBTrainingAIComponent* Config = GetCQBComponent();
    if (!Config || !Config->IsAlive() || !CQBStateTreeComponent)
    {
        return false;
    }

    AwarenessComponent->StopListening();
    CQBStateTreeComponent->StopLogic(TEXT("CQB brain restart"));
    CQBStateTreeComponent->SetComponentTickEnabled(false);
    UStateTree* StateTree = Config->BehaviorStateTree;
    if (!IsValid(StateTree) || !StateTree->IsReadyToRun())
    {
        UE_LOG(LogCQBTrainingAI, Warning,
            TEXT("%s: BehaviorStateTree is missing or not compiled."),
            *GetNameSafe(Config));
        return false;
    }

    CQBStateTreeComponent->SetStateTree(StateTree);
    CQBStateTreeComponent->SetComponentTickEnabled(true);
    CQBStateTreeComponent->StartLogic();
    if (!CQBStateTreeComponent->IsRunning())
    {
        StopCQBBrain();
        UE_LOG(LogCQBTrainingAI, Warning,
            TEXT("%s: StateTree failed to start."), *GetNameSafe(Config));
        return false;
    }
    AwarenessComponent->StartListening();
    return true;
}

void ACQBTrainingAIController::StopCQBBrain()
{
    AwarenessComponent->StopListening();
    if (CQBStateTreeComponent)
    {
        CQBStateTreeComponent->StopLogic(TEXT("CQB stopped"));
        CQBStateTreeComponent->SetComponentTickEnabled(false);
    }
    if (PatrolComponent)
    {
        PatrolComponent->ExitPatrol();
    }
    if (CombatComponent)
    {
        CombatComponent->ExitCombat();
    }
    if (InvestigationComponent)
    {
        InvestigationComponent->ResetRuntimeState();
    }
    if (LocomotionComponent)
    {
        LocomotionComponent->Stop();
    }
}

bool ACQBTrainingAIController::RefreshFromPawnConfiguration()
{
    UCQBTrainingAIComponent* Config = GetCQBComponent();
    if (!Config || !Config->HasValidPatrolRoute()
        || !AwarenessComponent || !LocomotionComponent || !PatrolComponent
        || !InvestigationComponent || !CombatComponent)
    {
        UE_LOG(LogCQBTrainingAI, Error,
            TEXT("%s: CQB runtime components or patrol route are invalid."),
            *GetNameSafe(GetPawn()));
        return false;
    }

    BindHealthEvents();
    if (!BoundHealthComponent || !Config->IsAlive())
    {
        UE_LOG(LogCQBTrainingAI, Error,
            TEXT("%s: CQB health state is not ready."), *GetNameSafe(GetPawn()));
        return false;
    }

    Config->ApplyRuntimeOptimization();
    ConfigurePerceptionFromPawn();

    if (UCharacterMovementComponent* Movement = Config->GetCharacterMovement())
    {
        Movement->MaxWalkSpeed = FMath::Max(50.0f, Config->PatrolSettings.PatrolSpeed);
        Movement->bUseControllerDesiredRotation = false;
        Movement->bOrientRotationToMovement = false;
        Movement->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
    }

    AwarenessComponent->Configure(*Config);
    LocomotionComponent->Configure(Config->PatrolSettings);
    InvestigationComponent->Configure(*Config, *AwarenessComponent, *LocomotionComponent);
    CombatComponent->Configure(*Config, *AwarenessComponent, *LocomotionComponent);

    if (!PatrolComponent->ConfigureRoute(
        Config->GetPatrolRoute(), Config->PatrolSettings, *LocomotionComponent))
    {
        UE_LOG(LogCQBTrainingAI, Error,
            TEXT("%s: Failed to configure CQB patrol route."), *GetNameSafe(GetPawn()));
        return false;
    }

    return StartCQBBrain();
}

bool ACQBTrainingAIController::IsTargetVisible() const
{
    return AwarenessComponent
        && AwarenessComponent->HasVisibleTarget()
        && IsCombatTargetValid(AwarenessComponent->GetTargetActor());
}

AActor* ACQBTrainingAIController::GetCurrentTarget() const
{
    return AwarenessComponent ? AwarenessComponent->GetTargetActor() : nullptr;
}

bool ACQBTrainingAIController::IsCombatTargetValid(const AActor* Actor) const
{
    const APawn* TargetPawn = Cast<APawn>(Actor);
    if (!TargetPawn || !TargetPawn->IsPlayerControlled())
    {
        return false;
    }

    const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(
        const_cast<AActor*>(Actor));
    return !ASC || !ASC->HasMatchingGameplayTag(RogueTags::State_Combat_IsDead);
}

UCQBTrainingAIComponent* ACQBTrainingAIController::GetCQBComponent() const
{
    APawn* ControlledPawn = GetPawn();
    return ControlledPawn
        ? ControlledPawn->FindComponentByClass<UCQBTrainingAIComponent>()
        : nullptr;
}

bool ACQBTrainingAIController::IsControlledPawnAlive() const
{
    const UCQBTrainingAIComponent* Config = GetCQBComponent();
    return Config && Config->IsAlive();
}

void ACQBTrainingAIController::ConfigurePerceptionFromPawn()
{
    const UCQBTrainingAIComponent* Config = GetCQBComponent();
    if (!Config || !CQBPerceptionComponent || !SightConfig || !HearingConfig)
    {
        return;
    }

    const FCQBPerceptionSettings& Settings = Config->PerceptionSettings;
    SightConfig->SightRadius = FMath::Max(100.0f, Settings.SightRadius);
    SightConfig->LoseSightRadius = FMath::Max(SightConfig->SightRadius, Settings.LoseSightRadius);
    SightConfig->PeripheralVisionAngleDegrees = FMath::Clamp(
        Settings.PeripheralVisionAngleDegrees, 10.0f, 180.0f);
    SightConfig->SetMaxAge(FMath::Max(0.1f, Settings.SightMaxAgeSeconds));

    // 개별 소리 범위가 전체 감지 반경에서 먼저 잘리지 않도록 한다.
    HearingConfig->HearingRange = Settings.GetMaximumHearingRange();
    HearingConfig->SetMaxAge(FMath::Max(0.1f, Settings.HearingMaxAgeSeconds));

    CQBPerceptionComponent->ConfigureSense(*SightConfig);
    CQBPerceptionComponent->ConfigureSense(*HearingConfig);
    CQBPerceptionComponent->RequestStimuliListenerUpdate();
}

bool ACQBTrainingAIController::IsHearingStimulusInRange(const FAIStimulus& Stimulus) const
{
    const UCQBTrainingAIComponent* Config = GetCQBComponent();
    const APawn* ControlledPawn = GetPawn();
    if (!Config || !ControlledPawn)
    {
        return false;
    }

    const float MaxRange = Config->PerceptionSettings.GetHearingRangeForTag(Stimulus.Tag);

    if (MaxRange <= 0.0f)
    {
        return false;
    }

    const FVector NoiseLocation = Stimulus.StimulusLocation;
    return FVector::DistSquared(ControlledPawn->GetActorLocation(), NoiseLocation)
        <= FMath::Square(MaxRange);
}

void ACQBTrainingAIController::HandleTargetPerceptionUpdated(
    AActor* Actor,
    const FAIStimulus Stimulus)
{
    if (!AwarenessComponent || !IsControlledPawnAlive() || !IsValid(Actor))
    {
        return;
    }

    UCQBTrainingAIComponent* Config = GetCQBComponent();
    UWorld* World = GetWorld();
    if (!Config || !World)
    {
        return;
    }

    const double Now = World->GetTimeSeconds();
    if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
    {
        if (Stimulus.WasSuccessfullySensed())
        {
            if (IsCombatTargetValid(Actor))
            {
                const FVector SeenLocation = Stimulus.StimulusLocation.IsNearlyZero()
                    ? Actor->GetActorLocation()
                    : Stimulus.StimulusLocation;
                AwarenessComponent->RecordSightAcquired(
                    *Actor, SeenLocation, Config->ReactionSettings, Now);
            }
        }
        else if (AwarenessComponent->GetTargetActor() == Actor)
        {
            AwarenessComponent->RecordSightLost(
                *Actor,
                AwarenessComponent->GetLastSeenLocation(),
                Config->PatrolSettings.SearchDurationSeconds,
                Now);
        }
        return;
    }

    if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>()
        && Stimulus.WasSuccessfullySensed()
        && IsCombatTargetValid(Actor)
        && IsHearingStimulusInRange(Stimulus))
    {
        const FVector NoiseLocation = Stimulus.StimulusLocation;
        AwarenessComponent->RecordNoiseHeard(
            *Actor,
            NoiseLocation,
            Config->PatrolSettings.InvestigateTimeoutSeconds,
            Now);
    }
}

void ACQBTrainingAIController::BindHealthEvents()
{
    UnbindHealthEvents();
    if (AShooterPlayerState* ShooterPlayerState = GetPlayerState<AShooterPlayerState>())
    {
        BoundHealthComponent = ShooterPlayerState->GetHealthComponent();
        if (BoundHealthComponent)
        {
            BoundHealthComponent->OnDamageTakenNative.AddUObject(
                this, &ThisClass::HandlePawnDamageTaken);
            BoundHealthComponent->OnDeathStateChanged.AddDynamic(
                this, &ThisClass::HandlePawnDeathStateChanged);
        }
    }
}

void ACQBTrainingAIController::UnbindHealthEvents()
{
    if (!BoundHealthComponent)
    {
        return;
    }

    BoundHealthComponent->OnDamageTakenNative.RemoveAll(this);
    BoundHealthComponent->OnDeathStateChanged.RemoveDynamic(
        this, &ThisClass::HandlePawnDeathStateChanged);
    BoundHealthComponent = nullptr;
}

void ACQBTrainingAIController::HandlePawnDamageTaken(
    const AActor* InstigatorActor,
    const FVector Direction)
{
    if (!IsControlledPawnAlive())
    {
        return;
    }

    if (CombatComponent)
    {
        CombatComponent->NotifyPawnHit();
    }

    if (!AwarenessComponent || !IsCombatTargetValid(InstigatorActor))
    {
        return;
    }

    UCQBTrainingAIComponent* CurrentConfig = GetCQBComponent();
    UWorld* World = GetWorld();
    if (!CurrentConfig || !World)
    {
        return;
    }

    AwarenessComponent->RecordDirectionalDamage(
        CurrentConfig->GetActorLocation(),
        Direction,
        CurrentConfig->CombatSettings.DamageThreatMemorySeconds,
        CurrentConfig->PatrolSettings.SearchDurationSeconds,
        World->GetTimeSeconds());
}

void ACQBTrainingAIController::HandlePawnDeathStateChanged(const bool bIsDead)
{
    if (bIsDead && !bDeathShutdownStarted)
    {
        UCQBTrainingAIComponent* Config = GetCQBComponent();
        APawn* Victim = GetPawn();
        if (Config && Victim && BoundHealthComponent && Config->GetTrainingSpawner())
        {
            FCQBCombatIncident Incident;
            Incident.Origin = Victim->GetActorLocation();
            // The normal damage delegate arrives after death; use this lethal effect's retained direction.
            Incident.ThreatDirection = -FVector(BoundHealthComponent->GetDeathReaction().Direction);
            Incident.Timestamp = GetWorld()->GetTimeSeconds();
            Incident.SourceActor = Victim;
            Incident.Encounter = Config->GetTrainingSpawner();
            GetWorld()->GetSubsystem<UCQBCombatIncidentSubsystem>()->ReportIncident(Incident);
        }
        BeginDeathShutdown();
    }
}

void ACQBTrainingAIController::BeginDeathShutdown()
{
    if (bDeathShutdownStarted)
    {
        return;
    }
    bDeathShutdownStarted = true;

    StopCQBBrain();
    if (AwarenessComponent)
    {
        AwarenessComponent->ResetRuntimeState();
    }

    if (CQBPerceptionComponent)
    {
        CQBPerceptionComponent->ForgetAll();
        CQBPerceptionComponent->SetComponentTickEnabled(false);
        CQBPerceptionComponent->Deactivate();
    }

    APawn* ControlledPawn = GetPawn();
    if (IsValid(ControlledPawn))
    {
        UCQBTrainingAIComponent* Config = GetCQBComponent();
        ACQBTrainingSpawner* Spawner = Config ? Config->GetTrainingSpawner() : nullptr;
        if (IsValid(Spawner))
        {
            Spawner->NotifyTrooperDeath(*ControlledPawn);
        }
        else if (Config)
        {
            UE_LOG(LogCQBTrainingAI, Warning,
                TEXT("%s: CQB spawner reference is missing during death cleanup."),
                *GetNameSafe(ControlledPawn));
            Config->PrepareForDeathCleanup(Config->DeathCleanupSeconds);
        }
    }

    if (!IsActorBeingDestroyed())
    {
        if (UWorld* World = GetWorld())
        {
            DeathShutdownTimer = World->GetTimerManager().SetTimerForNextTick(
                FTimerDelegate::CreateUObject(this, &ThisClass::FinalizeDeathShutdown));
        }
    }
}

void ACQBTrainingAIController::FinalizeDeathShutdown()
{
    if (GetPawn())
    {
        UnPossess();
    }
    if (!IsActorBeingDestroyed())
    {
        Destroy();
    }
}
