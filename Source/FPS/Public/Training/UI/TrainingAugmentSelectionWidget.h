#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Augment/AugmentTypes.h"
#include "GameplayTagContainer.h"
#include "UObject/PrimaryAssetId.h"
#include "TrainingAugmentSelectionWidget.generated.h"

class UAugmentRegistrySubsystem;
class UTrainingPlayerComponent;
class UTrainingAugmentTileWidget;
class UButton;
class UTextBlock;
class UVerticalBox;
class UScrollBox;
class UWeaponInventoryComponent;
class UAbilitySystemComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FTrainingAugmentApplyRequested, const TArray<FPrimaryAssetId>&);
DECLARE_MULTICAST_DELEGATE(FTrainingAugmentBackRequested);

UCLASS(Blueprintable)
class FPS_API UTrainingAugmentSelectionWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void BeginSelection(UTrainingPlayerComponent& InBridge, const TArray<FPrimaryAssetId>& AppliedIds);
    void EndSelection();
    void SetPending(bool bInPending);
    void ShowFailure(const FText& Failure);
    UWidget* GetInitialFocusTarget() const;
    FTrainingAugmentApplyRequested OnApplyRequested;
    FTrainingAugmentBackRequested OnBackRequested;

    UPROPERTY(EditDefaultsOnly, Category = "Training|UI")
    TSubclassOf<UTrainingAugmentTileWidget> TileWidgetClass;
protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;
private:
    UFUNCTION() void HandleRegistryReady();
    UFUNCTION() void HandleEligibilityChanged();
    void HandleUnlockTagChanged(FGameplayTag Tag, int32 NewCount);
    void ClearCatalog();
    void ResetCatalogScroll();

    bool HasTierCategories() const;
    UButton* GetTierButton(bool bActive, EAugmentTier Tier) const;
    void SetVisibleTier(bool bActive, EAugmentTier Tier);
    void RefreshTierView();
    void RefreshTierButtons();
    void ChooseInitialTiers();
    UFUNCTION() void HandleActiveCommon();
    UFUNCTION() void HandleActiveRare();
    UFUNCTION() void HandleActiveEpic();
    UFUNCTION() void HandleActiveLegendary();
    UFUNCTION() void HandlePassiveCommon();
    UFUNCTION() void HandlePassiveRare();
    UFUNCTION() void HandlePassiveEpic();
    UFUNCTION() void HandlePassiveLegendary();
    UFUNCTION() void HandleApply();
    UFUNCTION() void HandleBack();
    void HandleTileClicked(FPrimaryAssetId Id);
    void BuildCatalog();
    void RefreshSelection();
    UAugmentRegistrySubsystem* GetRegistry() const;

    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UScrollBox> Scroll_Active;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UScrollBox> Scroll_Passive;
    UPROPERTY(Transient, meta = (BindWidget)) TObjectPtr<UVerticalBox> List_Active;
    UPROPERTY(Transient, meta = (BindWidget)) TObjectPtr<UVerticalBox> List_Passive;
    UPROPERTY(Transient, meta = (BindWidget)) TObjectPtr<UButton> Btn_Apply;
    UPROPERTY(Transient, meta = (BindWidget)) TObjectPtr<UButton> Btn_Back;
    UPROPERTY(Transient, meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_Count;
    UPROPERTY(Transient, meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_Selection;
    UPROPERTY(Transient, meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_Status;
    UPROPERTY(Transient, meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_Apply;
    UPROPERTY(Transient, meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_ActiveCount;
    UPROPERTY(Transient, meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_PassiveCount;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_ActiveCommon;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_ActiveRare;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_ActiveEpic;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_ActiveLegendary;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_PassiveCommon;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_PassiveRare;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_PassiveEpic;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_PassiveLegendary;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_ActiveTierInfo;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_PassiveTierInfo;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_ActiveTierEmpty;
    UPROPERTY(Transient, meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_PassiveTierEmpty;

    UPROPERTY(Transient) TArray<TObjectPtr<UTrainingAugmentTileWidget>> Tiles;
    UPROPERTY(Transient) TArray<FPrimaryAssetId> SelectedIds;
    TWeakObjectPtr<UTrainingPlayerComponent> Bridge;
    TWeakObjectPtr<UAugmentRegistrySubsystem> BoundRegistry;
    TWeakObjectPtr<UWeaponInventoryComponent> BoundWeapons;
    TWeakObjectPtr<UAbilitySystemComponent> BoundAbilitySystem;
    FDelegateHandle UnlockChangedHandle;
    TSet<FPrimaryAssetId> CatalogIds;
    TMap<EAugmentTier, int32> ActiveTierCounts;
    TMap<EAugmentTier, int32> PassiveTierCounts;
    EAugmentTier ActiveTier = EAugmentTier::Common;
    EAugmentTier PassiveTier = EAugmentTier::Common;
    bool bChooseInitialTiers = true;
    bool bPending = false;
    bool bOpen = false;
    bool bCatalogBuilt = false;
};
