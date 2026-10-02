#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Customization/ReplicatedAppearance.h"
#include "UObject/SoftObjectPath.h"
#include "GameplayAppearanceComponent.generated.h"

class UGameplayAppearanceConfig;
class UCustomizationStudioConfig;
class UCustomizableObjectInstance;
class USkeletalMeshComponent;
class UGroomComponent;
class UPrimitiveComponent;
class UAppearanceBoneDependencyComponent;

UCLASS(ClassGroup=(Customization))
class FPS_API UGameplayAppearanceComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UGameplayAppearanceComponent();
    void RefreshFromPlayerState();
    void RequestAppearance(const FReplicatedAppearance& Data);
    void AppendPreloadAssetPaths(FName ClassId, TArray<FSoftObjectPath>& OutPaths) const;
    uint32 GetAppliedRevision() const { return AppliedRevision; }
    USkeletalMeshComponent* GetReadyBody() const;
    const FString& GetFailure() const { return Failure; }
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float Delta, ELevelTick Tick, FActorComponentTickFunction* Function) override;
private:
    void InitializeAuthoredBoneDependencies();
    void StartUpdate();
    void EnsureVisualComponents();
    void RefreshLODSynchronization();
    FName ResolveClassId() const;
    UPROPERTY(Transient) TObjectPtr<UGameplayAppearanceConfig> Config;
    UPROPERTY(Transient) TObjectPtr<UCustomizationStudioConfig> Studio;
    UPROPERTY(Transient) TObjectPtr<UCustomizableObjectInstance> Pending;
    UPROPERTY(Transient) TObjectPtr<UCustomizableObjectInstance> Live;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Face;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Outfit;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> BindingSurface;
    UPROPERTY(Transient) TObjectPtr<UAppearanceBoneDependencyComponent> DriverBoneDependency;
    UPROPERTY(Transient) TObjectPtr<UAppearanceBoneDependencyComponent> BodyBoneDependency;
    UPROPERTY(Transient) TArray<TObjectPtr<UGroomComponent>> Grooms;
    UPROPERTY(Transient) TArray<TObjectPtr<UPrimitiveComponent>> AuthoredVisuals;
    FReplicatedAppearance Desired;
    uint32 RequestSerial = 0;
    uint32 AppliedRevision = 0;
    bool bBusy = false;
    bool bQueued = false;
    FString Failure;
};
