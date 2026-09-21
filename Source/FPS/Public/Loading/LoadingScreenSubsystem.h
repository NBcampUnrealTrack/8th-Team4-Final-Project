#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LoadingScreenSubsystem.generated.h"

class ABaseRoundGameState;
class APlayerController;
class ULoadingScreenWidget;

UCLASS()
class FPS_API ULoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	void ShowLoadingScreen(APlayerController* OwningPlayer, TSubclassOf<ULoadingScreenWidget> WidgetClass, float InitialProgress);
	void SetLoadingProgress(float Progress);
	void HideLoadingScreen();

	void BindToRoundGameState(ABaseRoundGameState* RoundGameState);
	void UnbindFromRoundGameState();

	bool IsLoadingScreenVisible() const;

private:
	UFUNCTION()
	void HandleRoundStateChanged();

	void SyncWithRoundGameState();

	// 뷰포트 조작 전 위젯 월드 재바인딩
	void RebindLoadingWidgetToCurrentWorld();

private:
	UPROPERTY(Transient)
	TObjectPtr<ULoadingScreenWidget> ActiveLoadingWidget;

	UPROPERTY(Transient)
	TObjectPtr<ABaseRoundGameState> BoundRoundGameState;

	float CurrentProgress = 0.0f;
};
