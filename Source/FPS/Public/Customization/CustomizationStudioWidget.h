#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CustomizationStudioWidget.generated.h"
class ACustomizationStudioController;
UCLASS(Blueprintable)
class FPS_API UCustomizationStudioWidget : public UUserWidget
{
 GENERATED_BODY()
protected:
 virtual TSharedRef<SWidget> RebuildWidget() override;
private:
 int32 Selected=0;
 bool bDragging=false;
 ACustomizationStudioController* Controller() const;
 FLinearColor CurrentColor() const;
 void SetColor(FLinearColor Color);
 TSharedRef<SWidget> HairMenu();
};
