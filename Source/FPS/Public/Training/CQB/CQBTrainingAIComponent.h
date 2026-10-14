#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/RogueCombatFeedbackPolicy.h"
#include "Training/CQB/CQBTrainingAwarenessTypes.h"
#include "Training/CQB/CQBTrainingTacticalTypes.h"
#include "CQBTrainingAIComponent.generated.h"

class AActor;
class ACharacter;
class ACQBTrainingSpawner;
class UAnimMontage;
class UCharacterMovementComponent;
class UNiagaraSystem;
class USkeletalMeshComponent;
class USoundBase;
class UStateTree;

USTRUCT(BlueprintType)
struct FPS_API FCQBPatrolRouteDefinition
{
    GENERATED_BODY()

    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "CQB|Patrol")
    FVector SpawnOrigin = FVector::ZeroVector;

    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "CQB|Patrol")
    bool bHasSpawnOrigin = false;

    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "CQB|Patrol")
    bool bPingPong = true;

    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "CQB|Patrol")
    TArray<FVector> PatrolLocations;
};

USTRUCT(BlueprintType)
struct FPS_API FCQBPatrolSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Patrol", meta = (ClampMin = "50.0", Units = "cm/s"))
    float PatrolSpeed = 220.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Patrol", meta = (ClampMin = "10.0", Units = "cm"))
    float AcceptanceRadius = 65.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Patrol", meta = (ClampMin = "0.05", Units = "s"))
    float MoveRetryDelaySeconds = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Patrol", meta = (ClampMin = "0.1", Units = "s"))
    float MoveFailureLogIntervalSeconds = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Patrol")
    FVector NavigationProjectionExtent = FVector(120.0f, 120.0f, 250.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Patrol", meta = (ClampMin = "30.0", ClampMax = "720.0", Units = "deg/s"))
    float PatrolTurnSpeedDegreesPerSecond = 260.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Patrol", meta = (ClampMin = "30.0", ClampMax = "720.0", Units = "deg/s"))
    float LookAroundTurnSpeedDegreesPerSecond = 180.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Patrol", meta = (ClampMin = "0.0", Units = "s"))
    float WaypointPauseMinSeconds = 0.65f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Patrol", meta = (ClampMin = "0.0", Units = "s"))
    float WaypointPauseMaxSeconds = 1.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Patrol", meta = (ClampMin = "0.0", ClampMax = "120.0", Units = "deg"))
    float LookAroundYawDegrees = 28.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Patrol", meta = (ClampMin = "0.5", ClampMax = "45.0", Units = "deg"))
    float DepartureFacingToleranceDegrees = 7.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Patrol", meta = (ClampMin = "50.0", Units = "cm/s"))
    float InvestigateSpeed = 260.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Patrol", meta = (ClampMin = "0.0", Units = "s"))
    float InvestigateLookSeconds = 0.4f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Patrol", meta = (ClampMin = "0.5", Units = "s"))
    float InvestigateTimeoutSeconds = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Patrol", meta = (ClampMin = "0.5", Units = "s"))
    float SearchDurationSeconds = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Patrol", meta = (ClampMin = "2", ClampMax = "4"))
    int32 SearchPointCount = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Patrol", meta = (ClampMin = "100.0", Units = "cm"))
    float SearchRadius = 350.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Patrol", meta = (ClampMin = "0.1", Units = "s"))
    float SearchScanIntervalSeconds = 0.75f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Patrol", meta = (ClampMin = "0.0", ClampMax = "160.0", Units = "deg"))
    float SearchScanYawDegrees = 65.0f;
};

USTRUCT(BlueprintType)
struct FPS_API FCQBPerceptionSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Perception", meta = (ClampMin = "100.0", Units = "cm"))
    float SightRadius = 2200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Perception", meta = (ClampMin = "100.0", Units = "cm"))
    float LoseSightRadius = 2500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Perception", meta = (ClampMin = "10.0", ClampMax = "180.0", Units = "deg"))
    float PeripheralVisionAngleDegrees = 75.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Perception", meta = (ClampMin = "0.1", Units = "s"))
    float SightMaxAgeSeconds = 1.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Perception", meta = (ClampMin = "100.0", Units = "cm"))
    float HearingRange = 600.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Perception", meta = (ClampMin = "0.0", Units = "cm"))
    float FootstepHearingRange = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Perception", meta = (ClampMin = "0.0", Units = "cm"))
    float LandingHearingRange = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Perception", meta = (ClampMin = "0.0", Units = "cm"))
    float GunshotHearingRange = 567.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Perception", meta = (ClampMin = "0.1", Units = "s"))
    float HearingMaxAgeSeconds = 2.5f;

    float GetMaximumHearingRange() const
    {
        return FMath::Max(100.0f, FMath::Max(FMath::Max(HearingRange, FootstepHearingRange),
            FMath::Max(LandingHearingRange, GunshotHearingRange)));
    }

    float GetHearingRangeForTag(const FName NoiseTag) const
    {
        if (NoiseTag == TEXT("Gunshot") || NoiseTag == TEXT("CQB_AI_Gunshot"))
        {
            return FMath::Max(0.0f, GunshotHearingRange);
        }
        if (NoiseTag == TEXT("Landing")) { return FMath::Max(0.0f, LandingHearingRange); }
        if (NoiseTag == TEXT("Footstep")) { return FMath::Max(0.0f, FootstepHearingRange); }
        return FMath::Max(0.0f, HearingRange);
    }
};

USTRUCT(BlueprintType)
struct FPS_API FCQBReactionSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Reaction", meta = (ClampMin = "0.0", Units = "s"))
    float ReactionMinSeconds = 0.45f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Reaction", meta = (ClampMin = "0.0", Units = "s"))
    float ReactionMaxSeconds = 0.75f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Reaction", meta = (ClampMin = "0.0", Units = "s"))
    float SameAngleMemorySeconds = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Reaction", meta = (ClampMin = "0.0", Units = "cm"))
    float SameAngleDistance = 160.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Reaction", meta = (ClampMin = "0.1", ClampMax = "1.0"))
    float RepeekReactionMultiplier = 0.65f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Reaction", meta = (ClampMin = "0.0", Units = "s"))
    float MinimumReactionSeconds = 0.18f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Reaction", meta = (ClampMin = "90.0", ClampMax = "1440.0", Units = "deg/s"))
    float CombatTurnSpeedDegreesPerSecond = 720.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Reaction", meta = (ClampMin = "0.5", ClampMax = "30.0", Units = "deg"))
    float FireFacingToleranceDegrees = 4.0f;
};

USTRUCT(BlueprintType)
struct FPS_API FCQBAccuracySettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Accuracy", meta = (ClampMin = "0.0", Units = "deg"))
    float InitialAimErrorDegrees = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Accuracy", meta = (ClampMin = "0.0", Units = "deg"))
    float SettledAimErrorDegrees = 0.9f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Accuracy", meta = (ClampMin = "0.01", Units = "s"))
    float AimSettleSeconds = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Accuracy", meta = (ClampMin = "0.0", Units = "deg"))
    float MovingPenaltyDegrees = 0.7f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Accuracy", meta = (ClampMin = "0.0", Units = "deg"))
    float HitReactionPenaltyDegrees = 1.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Accuracy", meta = (ClampMin = "0.0", Units = "s"))
    float HitReactionPenaltySeconds = 0.75f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Accuracy", meta = (ClampMin = "0.0", Units = "deg"))
    float DistancePenaltyPer10Meters = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Accuracy", meta = (ClampMin = "0.0", Units = "deg"))
    float BurstRecoilDegreesPerShot = 0.22f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Accuracy", meta = (Units = "cm"))
    float AimPointHeightOffset = 35.0f;
};

USTRUCT(BlueprintType)
struct FPS_API FCQBCombatSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Combat", meta = (ClampMin = "1.0"))
    float DamagePerShot = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Combat", meta = (ClampMin = "100.0", Units = "cm"))
    float MaxFireRange = 3500.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Combat", meta = (ClampMin = "50.0", Units = "cm/s"))
    float RepositionSpeed = 340.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Combat", meta = (ClampMin = "50.0", Units = "cm"))
    float RepositionAcceptanceRadius = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Combat", meta = (ClampMin = "0.5", Units = "s"))
    float DamageThreatMemorySeconds = 6.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Combat", meta = (ClampMin = "1", ClampMax = "10"))
    int32 BurstMinShots = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Combat", meta = (ClampMin = "1", ClampMax = "10"))
    int32 BurstMaxShots = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Combat", meta = (ClampMin = "0.02", Units = "s"))
    float ShotIntervalMinSeconds = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Combat", meta = (ClampMin = "0.02", Units = "s"))
    float ShotIntervalMaxSeconds = 0.18f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Combat", meta = (ClampMin = "0.0", Units = "s"))
    float BurstCooldownMinSeconds = 0.3f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Combat", meta = (ClampMin = "0.0", Units = "s"))
    float BurstCooldownMaxSeconds = 0.55f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Combat")
    FName MuzzleSocketName = TEXT("Muzzle");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Combat", meta = (ClampMin = "0.0"))
    float GunshotNoiseLoudness = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Combat", meta = (ClampMin = "0.0", Units = "cm"))
    float GunshotNoiseMaxRange = 567.0f;
};

UCLASS(ClassGroup = (Training), meta = (BlueprintSpawnableComponent, DisplayName = "CQB Training AI"))
class FPS_API UCQBTrainingAIComponent : public UActorComponent, public IRogueCombatFeedbackPolicy
{
    GENERATED_BODY()

public:
    UCQBTrainingAIComponent();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|AI")
    TObjectPtr<UStateTree> BehaviorStateTree;

    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "CQB|Patrol")
    FCQBPatrolRouteDefinition PatrolRoute;

    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "CQB|Rules")
    bool bSuppressStandardEliminationFeedback = true;

    UPROPERTY(Transient, VisibleInstanceOnly, BlueprintReadOnly, Category = "CQB|Optimization")
    float DeathCleanupSeconds = 1.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Settings")
    FCQBPatrolSettings PatrolSettings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Settings")
    FCQBPerceptionSettings PerceptionSettings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Settings")
    FCQBReactionSettings ReactionSettings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Settings")
    FCQBAccuracySettings AccuracySettings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Settings")
    FCQBCombatSettings CombatSettings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Settings")
    FCQBAwarenessSettings AwarenessSettings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Settings")
    FCQBTacticalSettings TacticalSettings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Presentation")
    TObjectPtr<USoundBase> FireSound;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Presentation")
    TObjectPtr<UNiagaraSystem> MuzzleEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CQB|Presentation")
    TObjectPtr<UAnimMontage> FireMontage;

    virtual bool UsesStandardEliminationFeedback() const override
    {
        return !bSuppressStandardEliminationFeedback;
    }

    void ConfigurePatrolRoute(const FVector& SpawnOrigin, const TArray<FVector>& InPatrolLocations, bool bPingPong);
    bool HasValidPatrolRoute() const;
    const FCQBPatrolRouteDefinition& GetPatrolRoute() const { return PatrolRoute; }

    bool IsAlive() const;
    UCharacterMovementComponent* GetCharacterMovement() const;
    USkeletalMeshComponent* GetMesh() const;
    FVector GetActorLocation() const;
    FVector GetVelocity() const;
    FVector GetPawnViewLocation() const;
    bool ResolveMuzzleTransform(FTransform& OutTransform) const;
    void PlayShotPresentation(const FVector& TraceStart, const FVector& TraceEnd, bool bHitSomething) const;
    void ApplyRuntimeOptimization();
    void PrepareForDeathCleanup(float CorpseLifetimeSeconds);

    void SetTrainingSpawner(ACQBTrainingSpawner* InSpawner);
    ACQBTrainingSpawner* GetTrainingSpawner() const;

private:
    ACharacter* GetOwnerCharacter() const;
    TWeakObjectPtr<ACQBTrainingSpawner> TrainingSpawner;
    bool bDeathCleanupStarted = false;
};
