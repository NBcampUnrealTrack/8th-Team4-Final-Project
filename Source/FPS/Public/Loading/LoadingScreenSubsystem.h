#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UObject/SoftObjectPtr.h"
#include "LoadingScreenSubsystem.generated.h"

class ABaseRoundGameState;
class APlayerController;
class SWidget;
class UGameViewportClient;
class ULoadingScreenWidget;
class UTexture2D;

// travel 을 넘겨야 하는 최소 상태
USTRUCT()
struct FShooterLoadingContext
{
	GENERATED_BODY()

	UPROPERTY()
	FText ModeName;

	UPROPERTY()
	TSoftObjectPtr<UTexture2D> Thumbnail;

	bool HasModeName() const { return !ModeName.IsEmpty(); }
};

/*
 * 로딩 상태
 * 90% MetaHuman PreLoad
 * 95% World
 * 99% Player Ready
 * 뒷 배경은 Room ModeSelect의 Thumbnail에서 가져온다
 * 모두 로딩되어도 Loading Display가 100% 아니면 끝나지않는다.
*/

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

	// 맵 이동마다 시작하고 실제 준비 완료 또는 취소 시 종료한다
	void BeginTravelProgress();
	void NotifyPreloadComplete();

	// 취소는 완료로 취급하지 않는다. 화면 제거는 호출자가 명시적으로 처리한다
	void AbandonPreloadWait();

	void NotifyWorldLoaded();
	void NotifyMatchLoadReady();

	bool IsTravelProgressActive() const;

	// 맵 이동 중에는 선로드·월드·플레이어 준비가 모두 완료되어야 한다
	bool CanFinishLoading() const;

	// 로비가 살아 있을 때 캐시해 두는 표시값
	void SetPendingLoadingContext(const FShooterLoadingContext& InContext);
	void ClearPendingLoadingContext();

	// 정상 완료. 100% 를 보여준 뒤 로딩 제거
	void FinishLoadingScreen();

private:
	UFUNCTION()
	void HandleRoundStateChanged();

	void SyncWithRoundGameState();

	void ApplyProgress(float Progress);

	// 계층 게이트. 앞 단계가 열려야 다음 단계가 화면에 반영된다
	float ComputeTravelProgress() const;
	void ApplyTravelProgress();

	bool TickEstimatedTravelProgress(float DeltaTime);
	void StopEstimatedTravelTicker();

	// 뷰포트 조작 전 위젯 월드 재바인딩
	void RebindLoadingWidgetToCurrentWorld();

	// 실제 준비 이벤트가 모두 도착하면 보류된 정상 완료를 실행한다.
	void TryFinishDeferredLoading();

	UGameViewportClient* GetGameViewportClient() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<ULoadingScreenWidget> ActiveLoadingWidget;

	// 뷰포트에 직접 붙인 Slate 본체, 월드 교체에 휩쓸리지 않는다
	TSharedPtr<SWidget> LoadingScreenSlateWidget;

	UPROPERTY(Transient)
	TWeakObjectPtr<ABaseRoundGameState> BoundRoundGameState;

	bool bTravelProgressActive = false;

	// 실제 완료 이벤트로만 켜진다.
	bool bPreloadComplete = false;
	bool bWorldLoaded = false;
	bool bMatchReady = false;

	// 취소된 이동의 늦은 완료 이벤트를 무시한다.
	bool bPreloadWaitAbandoned = false;

	// UX 용 추정치
	float EstimatedTravelProgress = 0.0f;

	FTSTicker::FDelegateHandle EstimatedTravelTickerHandle;

	// 완료 대기를 위젯에 맡긴 상태. 즉시 Hide 가 항상 이긴다
	bool bCompletionExitInFlight = false;

	// 실제 준비가 끝날 때까지 미뤄 둔 완료
	bool bFinishDeferredUntilReady = false;

	UPROPERTY(Transient)
	FShooterLoadingContext PendingContext;

	float CurrentProgress = 0.0f;
};
