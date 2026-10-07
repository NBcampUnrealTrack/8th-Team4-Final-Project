#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Customization/ReplicatedAppearance.h"
#include "Customization/CustomizationStudioActor.h"
#include "UObject/SoftObjectPath.h"
#include "GameplayAppearanceComponent.generated.h"

class UGameplayAppearanceConfig;
class UCustomizationStudioConfig;
class UCustomizableObjectInstance;
class USkeletalMeshComponent;
class UGroomComponent;
class UPrimitiveComponent;
class UAppearanceBoneDependencyComponent;
struct FStreamableHandle;

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
    // A valid previous body is still usable while a newer request is generating.
    // Previews must wait for this, not merely a nonzero applied revision.
    bool IsLatestAppearanceApplied() const;
    USkeletalMeshComponent* GetReadyBody() const;
    const FString& GetFailure() const { return Failure; }
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float Delta, ELevelTick Tick, FActorComponentTickFunction* Function) override;
private:
    void InitializeAuthoredBoneDependencies();
    void StartUpdate();
    void BeginMutableUpdate(uint32 Serial, FReplicatedAppearance Snapshot);
    void FinishRequest(uint32 Serial);
    void EnsureVisualComponents();
    void RefreshLODSynchronization();
    void RefreshRenderVisibility();
    FName ResolveClassId() const;
    UPROPERTY(Transient) TObjectPtr<UGameplayAppearanceConfig> Config;
    UPROPERTY(Transient) TObjectPtr<UCustomizationStudioConfig> Studio;
    UPROPERTY(Transient) TObjectPtr<UCustomizableObjectInstance> Pending;
    UPROPERTY(Transient) TObjectPtr<UCustomizableObjectInstance> Live;
    UPROPERTY(Transient) TArray<FStudioGroomChoice> PendingGrooms;
    UPROPERTY(Transient) TObjectPtr<USkeletalMesh> PendingSourceFace;
    TSharedPtr<FStreamableHandle> GroomLoadHandle;
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
    uint32 ActiveRequest = 0;
    uint32 AppliedRequestSerial = 0;
    uint32 AppliedRevision = 0;
    bool bBusy = false;
    bool bQueued = false;
    bool bEndingPlay = false;
    // A collection (killcam) can hide a component without changing its visible flag.
    // Recreate only on its next visible edge, never continuously while hidden.
    uint8 PendingRenderRecovery = 0;
    FString Failure;
};
