#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
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

	// 도미네이션 전용. travel 시작에 켜고 세션 종료에 끈다
	void BeginTravelProgress();
	void NotifyPreloadComplete();

	// 취소·해제·타임아웃. 게이트만 풀고 90% 는 열지 않는다
	void AbandonPreloadWait();

	void NotifyWorldLoaded();
	void NotifyMatchLoadReady();

	bool IsTravelProgressActive() const;

	// 더 기다릴 대상이 있나. 도미네이션이 아니면 항상 참
	bool CanFinishLoading() const;

private:
	UFUNCTION()
	void HandleRoundStateChanged();

	void SyncWithRoundGameState();

	void ApplyProgress(float Progress);

	// 계층 게이트. 앞 단계가 열려야 다음 단계가 화면에 반영된다
	float ComputeDominationProgress() const;
	void ApplyDominationProgress();

	bool TickEstimatedTravelProgress(float DeltaTime);
	void StopEstimatedTravelTicker();

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

	bool bTravelProgressActive = false;

	// 실제 완료 이벤트로만 켜진다.
	bool bPreloadComplete = false;
	bool bWorldLoaded = false;
	bool bMatchReady = false;

	// 취소·해제·타임아웃. 
	bool bPreloadWaitAbandoned = false;

	// UX 용 추정치
	float EstimatedTravelProgress = 0.0f;

	FTSTicker::FDelegateHandle EstimatedTravelTickerHandle;

	float CurrentProgress = 0.0f;
};
