#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "CustomizationStudioWidget.generated.h"
class ACustomizationStudioController;
UCLASS(Blueprintable)
class FPS_API UCustomizationStudioWidget : public UUserWidget
{
 GENERATED_BODY()
public:
 UPROPERTY(EditDefaultsOnly,Category="Mutable Level") bool bUseMutableLevelLayout=false;
 UPROPERTY(EditDefaultsOnly,Category="Mutable Level") TObjectPtr<class UFont> BodyFont;
 UPROPERTY(EditDefaultsOnly,Category="Mutable Level") TObjectPtr<class UFont> HeadingFont;
 UPROPERTY(EditDefaultsOnly,Category="Mutable Level") FButtonStyle ActionStyle;
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
private:
 int32 Selected=0;
 bool bDragging=false;
 ACustomizationStudioController* Controller() const;
 FLinearColor CurrentColor() const;
 void SetColor(FLinearColor Color);
 TSharedRef<SWidget> HairMenu();
 TSharedRef<SWidget> BuildMutableLevelLayout();
 TSharedRef<SWidget> BuildMutableColorPopup();
 TWeakPtr<SWidget> ColorPopup;
 FString ColorInputError;
 bool bConfirmReset=false;
 FButtonStyle PrimaryActionStyle;
};
