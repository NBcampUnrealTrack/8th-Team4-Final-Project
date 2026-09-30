#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "FirstPersonAppearanceComponent.generated.h"

class USkeletalMesh;
class USkeletalMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
struct FCustomizationAppearance;

UCLASS(ClassGroup=(Customization))
class FPS_API UFirstPersonAppearanceComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UFirstPersonAppearanceComponent();
    void RefreshFromPlayerState();
    void SetPresentationVisible(bool bVisible);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float Delta, ELevelTick Tick, FActorComponentTickFunction* Function) override;
private:
    void Restore();
    void UpdateColors(const FCustomizationAppearance* Appearance);
    void UpdateVisibility();
    UPROPERTY(EditDefaultsOnly, Category="Appearance") TMap<FName, TSoftObjectPtr<USkeletalMesh>> ClassVisuals;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Visual;
    UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Driver;
    UPROPERTY(Transient) TObjectPtr<USkeletalMesh> AppliedMesh;
    UPROPERTY(Transient) TObjectPtr<USkeletalMesh> AppliedVisual;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Skin;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Top;
    UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> Gloves;
    int32 SkinSlot = INDEX_NONE;
    int32 TopSlot = INDEX_NONE;
    int32 GlovesSlot = INDEX_NONE;
    FLinearColor OriginalTint = FLinearColor::White;
    float OriginalValue = 1.f;
    bool bOriginalOwnerNoSee = false;
    bool bPresentationVisible = true;
    EVisibilityBasedAnimTickOption OriginalTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
};
