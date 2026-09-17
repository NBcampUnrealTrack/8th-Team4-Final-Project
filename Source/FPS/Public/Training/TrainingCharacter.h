#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Character.h"
#include "Interfaces/RogueDamageableTarget.h"
#include "TimerManager.h"
#include "Training/TrainingTypes.h"
#include "TrainingCharacter.generated.h"

class ATrainingCharacter;
class USoundBase;
class USoundAttenuation;
struct FGameplayEventData;
class UAbilitySystemComponent;

DECLARE_MULTICAST_DELEGATE_FourParams(FTrainingBotHit,
    ATrainingCharacter&, const AActor*, float, bool);
class UCombatHitZoneDefinition;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class URogueAugmentAttributeSet;
class URogueHealthComponent;
class URogueVitalAttributeSet;

UCLASS(Abstract)
class FPS_API ATrainingCharacter : public ACharacter, public IAbilitySystemInterface, public IRogueDamageableTarget
{
    GENERATED_BODY()

public:
    ATrainingCharacter();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void HandleDeath_Implementation(const FRogueDeathContext& Context) override;
    virtual void PlayDeathReaction_Implementation(const FRogueDeathReaction& Reaction) override;
    virtual UCombatHitZoneDefinition* GetCombatHitZoneDefinition_Implementation() const override;

    UFUNCTION(BlueprintPure, Category = "Training")
    bool IsAlive() const;

    void BeginDissolveRemoval();

    FTrainingBotHit OnTrainingHit;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Audio")
    TObjectPtr<USoundBase> SpawnSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Audio")
    TObjectPtr<USoundBase> DeathSound;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Audio")
    TObjectPtr<USoundAttenuation> BotSoundAttenuation;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Audio", meta = (ClampMin = "0.0", ClampMax = "4.0"))
    float BotSoundVolume = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Death")
    TObjectPtr<UMaterialInterface> DissolveMaterial;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Death")
    FName DissolveParameterName = TEXT("DissolveAmount");

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Death", meta = (ClampMin = "0.1", ClampMax = "10.0", Units = "s"))
    float DissolveDurationSeconds = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Death")
    float DissolveStartValue = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Death")
    float DissolveEndValue = 1.0f;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual float GetInitialTrainingHealth() const;
    virtual void StopTrainingMotion();
    virtual bool RemovesOnAnyAcceptedHit() const { return false; }

private:
    void HandleTrainingDamageTaken(FGameplayTag EventTag, const FGameplayEventData* Payload);
    FDelegateHandle TrainingDamageTakenHandle;
    bool bEliminationReported = false;

    void StartDissolvePresentation();
    void UpdateDissolvePresentation();
    float GetDissolveDuration() const;

    UFUNCTION()
    void OnRep_DissolveStartServerSeconds();

    UPROPERTY(VisibleAnywhere, Category = "Training|GAS")
    TObjectPtr<UAbilitySystemComponent> AbilitySystem;

    UPROPERTY()
    TObjectPtr<URogueVitalAttributeSet> VitalSet;

    UPROPERTY()
    TObjectPtr<URogueAugmentAttributeSet> AugmentSet;

    UPROPERTY(VisibleAnywhere, Category = "Training|GAS")
    TObjectPtr<URogueHealthComponent> HealthComponent;

    UPROPERTY(EditDefaultsOnly, Category = "Training|Damage")
    TObjectPtr<UCombatHitZoneDefinition> HitZoneDefinition;

    UPROPERTY(ReplicatedUsing = OnRep_DissolveStartServerSeconds)
    double DissolveStartServerSeconds = -1.0;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> DissolveInstance;

    FTimerHandle DissolveUpdateTimer;
    bool bDeathPresentationStarted = false;
};
