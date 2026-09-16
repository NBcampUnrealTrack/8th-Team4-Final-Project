#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "UObject/Object.h"
#include "PlayerRespawnProtection.generated.h"

class AShooterPlayerState;
class UAbilitySystemComponent;
class UWorld;
struct FGameplayEventData;

// Mode-independent protection. It owns no respawn location, score, or match rule.
UCLASS()
class FPS_API UPlayerRespawnProtection : public UObject
{
    GENERATED_BODY()
public:
    void Initialize(UWorld* InWorld, float InDuration);
    void BeginProtection(AShooterPlayerState* PlayerState);
    void EndProtection(AShooterPlayerState* PlayerState);
    void ClearAll();
    void Shutdown();
private:
    struct FActiveRespawnProtection
    {
        TWeakObjectPtr<UAbilitySystemComponent> AbilitySystem;
        FTimerHandle ExpirationTimer;
        FDelegateHandle WeaponFiredDelegate;
        FGameplayTagContainer WeaponFiredEventTags;
    };
    void HandleProtectionExpired(TWeakObjectPtr<AShooterPlayerState> PlayerState);
    void HandleProtectedPlayerWeaponFired(FGameplayTag EventTag,
        const FGameplayEventData* Payload, TWeakObjectPtr<AShooterPlayerState> PlayerState);
    void ReleaseProtection(FActiveRespawnProtection& Protection);
    FTimerManager* GetTimerManager() const;
    TWeakObjectPtr<UWorld> BoundWorld;
    TMap<TWeakObjectPtr<AShooterPlayerState>, FActiveRespawnProtection> ActiveRespawnProtections;
    float ProtectionDuration = 2.0f;
};
