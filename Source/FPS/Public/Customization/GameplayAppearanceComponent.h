#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Customization/ReplicatedAppearance.h"
#include "GameplayAppearanceComponent.generated.h"

class UGameplayAppearanceConfig;
class UCustomizationStudioConfig;
class UCustomizableObjectInstance;
class USkeletalMeshComponent;
class UGroomComponent;
class UPrimitiveComponent;
class USkeletalMesh;
struct FStreamableHandle;

UCLASS(ClassGroup=(Customization))
class FPS_API UGameplayAppearanceComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UGameplayAppearanceComponent();
    void RefreshFromPlayerState();
    void RequestAppearance(const FReplicatedAppearance& Data);
    uint32 GetAppliedRevision() const { return AppliedRevision; }
    const FString& GetFailure() const { return Failure; }
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float Delta, ELevelTick Tick, FActorComponentTickFunction* Function) override;
private:
    void StartUpdate();
    void LoadAppearanceAssets(uint32 Serial);
    void GenerateAppearance(uint32 Serial);
    bool ContinueRequest(uint32 Serial);
    void FailUpdate(const FString& Reason);
    void EnsureVisualComponents();
    FName ResolveClassId() const;
    UPROPERTY(Transient) TObjectPtr<UGameplayAppearanceConfig> Config;
    UPROPERTY(Transient) TObjectPtr<UCustomizationStudioConfig> Studio;
    UPROPERTY(Transient) TObjectPtr<USkeletalMesh> PendingSourceFace;
    TSharedPtr<FStreamableHandle> AssetLoadHandle;
    UPROPERTY(Transient) TObjectPtr<UCustomizableObjectInstance> Pending;
    UPROPERTY(Transient) TObjectPtr<UCustomizableObjectInstance> Live;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Body;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Face;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Outfit;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> BindingSurface;
    UPROPERTY(Transient) TArray<TObjectPtr<UGroomComponent>> Grooms;
    UPROPERTY(Transient) TArray<TObjectPtr<UPrimitiveComponent>> AuthoredVisuals;
    FReplicatedAppearance Desired;
    uint32 RequestSerial = 0;
    uint32 AppliedRevision = 0;
    bool bBusy = false;
    bool bQueued = false;
    bool bEndingPlay = false;
    FString Failure;
};
