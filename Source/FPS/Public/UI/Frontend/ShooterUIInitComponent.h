// ShooterUIInitComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "CommonActivatableWidget.h"
#include "UI/Layout/ShooterPrimaryGameLayout.h"
#include "ShooterUIInitComponent.generated.h"

// UI 초기화 트리거
UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class FPS_API UShooterUIInitComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;
	
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UShooterPrimaryGameLayout> RootLayoutClass;
	
	// 이 Controller가 뜰 때 처음 띄울 화면
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UCommonActivatableWidget> InitialWidgetClass;
	// 처음 띄울 화면 Tag
	UPROPERTY(EditAnywhere, Category = "UI", meta = (Categories = "UI.Layer"))
	FGameplayTag InitialWidgetLayer;
};
