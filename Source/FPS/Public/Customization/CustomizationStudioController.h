#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Customization/CustomizationProfile.h"
#include "CustomizationStudioController.generated.h"
class ACustomizationStudioActor;
class UCustomizationStudioWidget;
UCLASS()
class FPS_API ACustomizationStudioController : public APlayerController
{
 GENERATED_BODY()
public:
 ACustomizationStudioController();
 virtual void BeginPlay() override;
 virtual void Tick(float DeltaSeconds) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 UPROPERTY(EditDefaultsOnly,Category="Studio") TSubclassOf<UCustomizationStudioWidget> WidgetClass;
 UPROPERTY(BlueprintReadOnly,Category="Studio") TObjectPtr<ACustomizationStudioActor> Studio;
 FCustomizationAppearance Draft;
 FString Status,AccountLabel;
 bool bDirty=false;
 void Changed();
 void SelectBody(const FString& Type);
 void SaveAppearance();
 void LoadAppearance();
 void ResetAppearance();
 bool CanSave() const;
 // Modal color editing previews Draft but cannot save until confirmed.
 void BeginColorEdit(int32 Part);
 void EndColorEdit(bool bAccept);
 bool IsColorEditOpen() const {return ColorEditPart!=INDEX_NONE;}
 int32 GetColorEditPart() const {return ColorEditPart;}
 FLinearColor GetEditingColor() const;
 FLinearColor GetPreviousColor() const;
 void SetEditingColor(FLinearColor Color);
 void RestoreEditingPart();
private:
 int32 ColorEditPart=INDEX_NONE;
 FCustomizationAppearance BeforeColorEdit;
 bool bDirtyBeforeColorEdit=false;
 FString StatusBeforeColorEdit;
 UPROPERTY() TObjectPtr<UCustomizationStudioWidget> Screen;
 FString Service,Account;
 float AccountPoll=0;
 void RefreshAccount();
};
