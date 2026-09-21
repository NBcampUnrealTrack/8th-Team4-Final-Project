#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "LoadingScreenSubsystem.generated.h"

class ABaseRoundGameState;
class APlayerController;
class SWidget;
class UGameViewportClient;
class ULoadingScreenWidget;

UCLASS()
class FPS_API ULoadingScreenSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
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

	UGameViewportClient* GetGameViewportClient() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<ULoadingScreenWidget> ActiveLoadingWidget;

	// 뷰포트에 직접 붙인 Slate 본체, 월드 교체에 휩쓸리지 않는다
	TSharedPtr<SWidget> LoadingScreenSlateWidget;

	UPROPERTY(Transient)
	TObjectPtr<ABaseRoundGameState> BoundRoundGameState;

	float CurrentProgress = 0.0f;
};
