#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "Interfaces/RogueHitscanReactiveTarget.h"
#include "TrainingPopUpTarget.generated.h"

class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;
class USoundBase;

UENUM(BlueprintType)
enum class ETrainingPopUpTargetState : uint8
{
    Up,
    Falling,
    Down,
    Rising
};

UCLASS(Blueprintable)
class FPS_API ATrainingPopUpTarget : public AActor, public IRogueHitscanReactiveTarget
{
    GENERATED_BODY()

public:
    ATrainingPopUpTarget();

    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual void HandleHitscanImpact(AActor* InstigatorActor, const FVector& ImpactPoint,
        const FVector& ShotDirection) override;

    UFUNCTION(BlueprintCallable, Category = "Training|PopUpTarget")
    void KnockDown();

    UFUNCTION(BlueprintCallable, Category = "Training|PopUpTarget")
    void ResetTarget();

    UFUNCTION(BlueprintPure, Category = "Training|PopUpTarget")
    bool IsTargetUp() const { return TargetState == ETrainingPopUpTargetState::Up; }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Components")
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Components")
    TObjectPtr<UStaticMeshComponent> BaseMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Components")
    TObjectPtr<USceneComponent> TargetPivot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Components")
    TObjectPtr<UStaticMeshComponent> TargetMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Components")
    TObjectPtr<UBoxComponent> HitBox;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Motion")
    FRotator DownRotation = FRotator(-90.0f, 0.0f, 0.0f);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Motion",
        meta = (ClampMin = "0.01", Units = "s"))
    float FallDurationSeconds = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Motion",
        meta = (ClampMin = "0.0", Units = "s"))
    float DownHoldSeconds = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Motion",
        meta = (ClampMin = "0.01", Units = "s"))
    float RiseDurationSeconds = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Audio")
    TObjectPtr<USoundBase> HitSound;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Audio",
        meta = (ClampMin = "0.0"))
    float HitSoundVolume = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|PopUpTarget|Audio",
        meta = (ClampMin = "0.01"))
    float HitSoundPitch = 1.0f;

protected:
    virtual void BeginPlay() override;
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    UFUNCTION()
    void OnRep_TargetState();

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastPlayHitSound(FVector_NetQuantize ImpactPoint);

    void SetTargetState(ETrainingPopUpTargetState NewState);
    void UpdateTargetRotation();
    void ConfigureTargetComponents();
    void ConfigureHitBoxCollision();
    void SetHitBoxActive(bool bActive);
    void BeginRise();

    UPROPERTY(ReplicatedUsing = OnRep_TargetState, VisibleInstanceOnly,
        Category = "Training|PopUpTarget|State")
    ETrainingPopUpTargetState TargetState = ETrainingPopUpTargetState::Up;

    FRotator RaisedRotation = FRotator::ZeroRotator;
    float MotionAlpha = 0.0f;
    FTimerHandle ResetTimer;
};
