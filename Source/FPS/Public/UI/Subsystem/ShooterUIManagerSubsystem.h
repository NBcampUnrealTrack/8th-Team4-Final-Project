// ShooterUIManagerSubsystem
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameplayTagContainer.h"
#include "ShooterUIManagerSubsystem.generated.h"

class UShooterPrimaryGameLayout;
class UCommonActivatableWidget;
class APlayerController;

/**
 * RootLayout 생성, Push/Pop을 띄우려는 쪽과 중계
 */
UCLASS()
class FPS_API UShooterUIManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()
	
public:
	// RootLayout 을 만들어 뷰포트에 올림
	void InitializeUI(APlayerController* OwningController, TSubclassOf<UShooterPrimaryGameLayout> RootLayoutClass);

	bool IsUIInitialized() const;
	
	UFUNCTION(BlueprintCallable, Category = "UI")
	UCommonActivatableWidget* PushWidgetToLayer(FGameplayTag LayerTag, TSubclassOf<UCommonActivatableWidget> WidgetClass);

	UFUNCTION(BlueprintCallable, Category = "UI")
	void PopWidgetFromLayer(FGameplayTag LayerTag);
	
	UFUNCTION(BlueprintCallable, Category = "UI")
	void ClearLayer(FGameplayTag LayerTag);
	
protected:
	UPROPERTY(Transient)
	TObjectPtr<UShooterPrimaryGameLayout> RootLayoutInstance; 
};
