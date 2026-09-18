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
 bool CanSave() const {return !Account.IsEmpty()&&Studio!=nullptr;}
private:
 UPROPERTY() TObjectPtr<UCustomizationStudioWidget> Screen;
 FString Service,Account;
 float AccountPoll=0;
 void RefreshAccount();
};
