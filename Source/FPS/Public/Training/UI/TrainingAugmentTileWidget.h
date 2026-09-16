#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Augment/AugmentTypes.h"
#include "Styling/SlateTypes.h"
#include "UObject/PrimaryAssetId.h"
#include "TrainingAugmentTileWidget.generated.h"

class UAugmentDefinition;
class UButton;
class UImage;
class UTextBlock;
class UShooterAugmentTierStyleSet;

DECLARE_MULTICAST_DELEGATE_OneParam(FTrainingAugmentTileClicked, FPrimaryAssetId);

UCLASS(Blueprintable)
class FPS_API UTrainingAugmentTileWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetDefinition(UAugmentDefinition& Definition);
    void SetSelection(bool bSelected, int32 ActiveIndex, bool bPending);
    FPrimaryAssetId GetDefinitionId() const { return DefinitionId; }
    FTrainingAugmentTileClicked OnClicked;

    UPROPERTY(EditDefaultsOnly, Category = "Training|Style")
    TObjectPtr<UShooterAugmentTierStyleSet> TierStyles;

    UPROPERTY(EditDefaultsOnly, Category = "Training|Style")
    FButtonStyle NormalStyle;
    UPROPERTY(EditDefaultsOnly, Category = "Training|Style")
    FButtonStyle SelectedStyle;
protected:
    virtual void NativeOnInitialized() override;
private:
    UFUNCTION() void HandleClicked();
    UPROPERTY(Transient, meta = (BindWidget)) TObjectPtr<UButton> Btn_Toggle;
    UPROPERTY(Transient, meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_Name;
    UPROPERTY(Transient, meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_Description;
    UPROPERTY(Transient, meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_Category;
    UPROPERTY(Transient, meta = (BindWidget)) TObjectPtr<UTextBlock> Txt_Selected;
    UPROPERTY(Transient, meta = (BindWidget)) TObjectPtr<UImage> Img_Icon;
    FPrimaryAssetId DefinitionId;
    FLinearColor TierAccent = FLinearColor::White;
    bool bSelectionInitialized = false;
    bool bLastSelected = false;
    int32 LastActiveIndex = INDEX_NONE;
};
