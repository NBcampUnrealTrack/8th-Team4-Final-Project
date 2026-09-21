// ShooterOutGameUIInitComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/Layout/ShooterPrimaryGameLayout.h"
#include "UI/OutGame/ShooterOutGameTypes.h"
#include "ShooterOutGameUIInitComponent.generated.h"

class UCommonActivatableWidget;
class ULoadingScreenSubsystem;
class UShooterOutGameSubsystem;
class UShooterUIManagerSubsystem;

/**
 * OutGame의 UIInitComp
 * RootLayout 생성 → 배경은 Game 레이어 → 첫 화면은 Menu 레이어. 
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class FPS_API UShooterOutGameUIInitComponent : public UActorComponent
{
	GENERATED_BODY()

protected:
	virtual void BeginPlay() override;

private:
	UShooterUIManagerSubsystem* GetUIManager() const;
	UShooterOutGameSubsystem* GetOutGameSubsystem() const;
	ULoadingScreenSubsystem* GetLoadingScreenSubsystem() const;

	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UShooterPrimaryGameLayout> RootLayoutClass;
	// 비워 두면 배경을 올리지 않는다
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UCommonActivatableWidget> BackdropWidgetClass;
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UCommonActivatableWidget> InitialWidgetClass;

	// 비워 두면 인트로 없이 첫 화면
	UPROPERTY(EditAnywhere, Category = "UI")
	TSubclassOf<UCommonActivatableWidget> StartupWidgetClass;
	UPROPERTY(EditAnywhere, Category = "UI")
	EShooterOutGameMusicState MusicState = EShooterOutGameMusicState::None;
};
