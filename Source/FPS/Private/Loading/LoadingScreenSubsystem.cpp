#include "Loading/LoadingScreenSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Engine.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/State/BaseRoundGameState.h"
#include "UI/Loading/LoadingScreenWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogLoadingScreen, Log, All);

namespace
{
	constexpr float MapEnteredProgress = 0.7f;
	constexpr float ReadyGateProgressRange = 0.25f;

	// 도미네이션 로딩 임계값
	constexpr float TravelStartProgress     = 0.20f;
	constexpr float EstimatedTravelMax      = 0.85f;
	constexpr float PreloadCompleteProgress = 0.90f;
	constexpr float WorldLoadedProgress     = 0.95f;
	constexpr float MatchReadyProgress      = 0.99f;

	// 추정 진행 속도. 초기 튜닝값
	constexpr float EstimatedTravelFastUntil   = 0.50f;
	constexpr float EstimatedTravelMediumUntil = 0.70f;
	constexpr float EstimatedTravelFastRate    = 0.050f;
	constexpr float EstimatedTravelMediumRate  = 0.025f;
	constexpr float EstimatedTravelSlowRate    = 0.010f;

	constexpr int32 LoadingScreenZOrder = 10000;

	// Task 0 Step 5.5 실측으로 조정하는 출발점
	constexpr float PreloadFinishWaitSeconds = 5.0f;
}

void ULoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	// Unreal 기본 로딩 비활성화
	if (GEngine)
	{
		GEngine->RegisterBeginStreamingPauseRenderingDelegate(nullptr);
		GEngine->RegisterEndStreamingPauseRenderingDelegate(nullptr);
	}
}

void ULoadingScreenSubsystem::Deinitialize()
{
	UnbindFromRoundGameState();
	HideLoadingScreen();

	Super::Deinitialize();
}

void ULoadingScreenSubsystem::ShowLoadingScreen(
	APlayerController* OwningPlayer,
	const TSubclassOf<ULoadingScreenWidget> WidgetClass,
	const float InitialProgress)
{
	if (!IsValid(OwningPlayer))
	{
		return;
	}

	bCompletionExitInFlight = false;

	// 이번 호출에서 새로 붙는지 판정할 기준
	const bool bWasAlreadyVisible = IsLoadingScreenVisible();

	if (!IsLoadingScreenVisible() && WidgetClass)
	{
		if (!ActiveLoadingWidget)
		{
			ActiveLoadingWidget =
				CreateWidget<ULoadingScreenWidget>(
					GetGameInstance(),
					WidgetClass);
		}
		else
		{
			RebindLoadingWidgetToCurrentWorld();
		}

		UGameViewportClient* ViewportClient = GetGameViewportClient();

		if (ViewportClient && ActiveLoadingWidget)
		{
			// 월드 위젯 트리가 아니라 뷰포트 오버레이에 직접 붙인다
			LoadingScreenSlateWidget = ActiveLoadingWidget->TakeWidget();

			ViewportClient->AddViewportWidgetContent(
				LoadingScreenSlateWidget.ToSharedRef(),
				LoadingScreenZOrder);

			ViewportClient->bDisableWorldRendering = true;
		}
	}

	if (ActiveLoadingWidget)
	{
		ActiveLoadingWidget->ApplyLoadingContext(PendingContext);
	}

	SetLoadingProgress(InitialProgress);

	// 보류됐더라도 새로 붙은 위젯은 현재 값을 받는다
	ApplyProgress(CurrentProgress);

	// 세션 최초 부착에서만. 같은 세션의 두 번째 Show 에서 스코프가 또 날아오면 안 된다
	if (!bWasAlreadyVisible && IsLoadingScreenVisible() && ActiveLoadingWidget)
	{
		ActiveLoadingWidget->BeginEnterAnimation();
	}

	SyncWithRoundGameState();
}

void ULoadingScreenSubsystem::SetLoadingProgress(const float Progress)
{
	if (bTravelProgressActive)
	{
		return;
	}

	ApplyProgress(FMath::Clamp(Progress, 0.0f, 1.0f));
}

void ULoadingScreenSubsystem::ApplyProgress(const float Progress)
{
	CurrentProgress = FMath::Max(CurrentProgress, Progress);

	if (ActiveLoadingWidget)
	{
		ActiveLoadingWidget->SetLoadingProgress(CurrentProgress);
	}
}

float ULoadingScreenSubsystem::ComputeDominationProgress() const
{
	// 계층 게이트. 앞 단계가 열리지 않았으면 뒤 단계는 화면에 반영하지 않는다
	if (!bPreloadComplete)
	{
		return EstimatedTravelProgress;
	}

	if (!bWorldLoaded)
	{
		return PreloadCompleteProgress;
	}

	if (!bMatchReady)
	{
		return WorldLoadedProgress;
	}

	return MatchReadyProgress;
}

void ULoadingScreenSubsystem::ApplyDominationProgress()
{
	ApplyProgress(ComputeDominationProgress());
}

void ULoadingScreenSubsystem::BeginTravelProgress()
{
	if (bTravelProgressActive)
	{
		return;
	}

	// Hide 를 거치지 않은 경로가 있어도 여기서 깨끗해진다
	bPreloadComplete = false;
	bWorldLoaded = false;
	bMatchReady = false;
	bPreloadWaitAbandoned = false;

	bTravelProgressActive = true;
	EstimatedTravelProgress = TravelStartProgress;

	ApplyDominationProgress();

	// 월드 타이머를 쓰지 않는다. travel 중 월드가 바뀐다
	EstimatedTravelTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::TickEstimatedTravelProgress));
}

bool ULoadingScreenSubsystem::TickEstimatedTravelProgress(const float DeltaTime)
{
	float Rate;
	if (EstimatedTravelProgress < EstimatedTravelFastUntil)
	{
		Rate = EstimatedTravelFastRate;
	}
	else if (EstimatedTravelProgress < EstimatedTravelMediumUntil)
	{
		Rate = EstimatedTravelMediumRate;
	}
	else
	{
		Rate = EstimatedTravelSlowRate;
	}

	EstimatedTravelProgress = FMath::Min(
		EstimatedTravelProgress + Rate * DeltaTime,
		EstimatedTravelMax);

	ApplyDominationProgress();

	if (EstimatedTravelProgress >= EstimatedTravelMax)
	{
		EstimatedTravelTickerHandle.Reset();
		return false;
	}

	return true;
}

void ULoadingScreenSubsystem::StopEstimatedTravelTicker()
{
	if (EstimatedTravelTickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(EstimatedTravelTickerHandle);
		EstimatedTravelTickerHandle.Reset();
	}
}

void ULoadingScreenSubsystem::NotifyPreloadComplete()
{
	if (!bTravelProgressActive || bPreloadComplete)
	{
		return;
	}

	bPreloadComplete = true;

	// 90% 상한해제
	StopEstimatedTravelTicker();
	ApplyDominationProgress();

	// 미뤄 둔 완료를 이제 실행한다. 게이트가 풀렸으므로 통과한다
	if (bFinishDeferredUntilPreloadEnd)
	{
		bFinishDeferredUntilPreloadEnd = false;
		ClearPreloadFinishTimeout();
		FinishLoadingScreen();
	}
}

void ULoadingScreenSubsystem::AbandonPreloadWait()
{
	if (!bTravelProgressActive || bPreloadWaitAbandoned || bPreloadComplete)
	{
		return;
	}

	bPreloadWaitAbandoned = true;

	// 미뤄 둔 완료를 이제 실행한다. 게이트가 풀렸으므로 통과한다
	if (bFinishDeferredUntilPreloadEnd)
	{
		bFinishDeferredUntilPreloadEnd = false;
		ClearPreloadFinishTimeout();
		FinishLoadingScreen();
	}
}

void ULoadingScreenSubsystem::NotifyWorldLoaded()
{
	if (!bTravelProgressActive || bWorldLoaded)
	{
		return;
	}

	// 순서가 어긋나도 저장한다. 반영은 게이트가 정한다
	bWorldLoaded = true;
	ApplyDominationProgress();
}

void ULoadingScreenSubsystem::NotifyMatchLoadReady()
{
	if (!bTravelProgressActive || bMatchReady)
	{
		return;
	}

	bMatchReady = true;
	ApplyDominationProgress();
}

bool ULoadingScreenSubsystem::IsTravelProgressActive() const
{
	return bTravelProgressActive;
}

bool ULoadingScreenSubsystem::CanFinishLoading() const
{
	return !bTravelProgressActive || bPreloadComplete || bPreloadWaitAbandoned;
}

void ULoadingScreenSubsystem::HideLoadingScreen()
{
	// 대기 중이었다면 여기서 취소된다. 즉시 Hide 가 항상 이긴다
	bCompletionExitInFlight = false;

	if (ActiveLoadingWidget)
	{
		ActiveLoadingWidget->ResetLoadingSession();
	}

	if (LoadingScreenSlateWidget.IsValid())
	{
		if (UGameViewportClient* ViewportClient = GetGameViewportClient())
		{
			ViewportClient->RemoveViewportWidgetContent(
				LoadingScreenSlateWidget.ToSharedRef());

			ViewportClient->bDisableWorldRendering = false;
		}

		LoadingScreenSlateWidget.Reset();
	}

	ActiveLoadingWidget = nullptr;

	// 위젯이 없는 경로에서도 세션은 끝
	UnbindFromRoundGameState();
	CurrentProgress = 0.0f;
	// 다음 세션이 이전 모드명을 물려받지 않게
	ClearPendingLoadingContext();

	StopEstimatedTravelTicker();

	bTravelProgressActive = false;
	bPreloadComplete = false;
	bWorldLoaded = false;
	bMatchReady = false;
	bPreloadWaitAbandoned = false;
	EstimatedTravelProgress = 0.0f;

	bFinishDeferredUntilPreloadEnd = false;
	ClearPreloadFinishTimeout();
}

void ULoadingScreenSubsystem::BindToRoundGameState(ABaseRoundGameState* RoundGameState)
{
	if (BoundRoundGameState == RoundGameState)
	{
		return;
	}

	UnbindFromRoundGameState();

	if (!IsValid(RoundGameState))
	{
		return;
	}

	BoundRoundGameState = RoundGameState;
	BoundRoundGameState->OnRoundStateChanged.AddUniqueDynamic(this, &ThisClass::HandleRoundStateChanged);
	SyncWithRoundGameState();
}

void ULoadingScreenSubsystem::UnbindFromRoundGameState()
{
	if (IsValid(BoundRoundGameState))
	{
		BoundRoundGameState->OnRoundStateChanged.RemoveDynamic(this, &ThisClass::HandleRoundStateChanged);
	}

	BoundRoundGameState = nullptr;
}

bool ULoadingScreenSubsystem::IsLoadingScreenVisible() const
{
	return LoadingScreenSlateWidget.IsValid();
}

void ULoadingScreenSubsystem::HandleRoundStateChanged()
{
	SyncWithRoundGameState();
}

void ULoadingScreenSubsystem::SyncWithRoundGameState()
{
	if (!IsValid(BoundRoundGameState))
	{
		return;
	}

	const EGameFlowPhase Phase = BoundRoundGameState->GetGameFlowPhase();
	if (Phase == EGameFlowPhase::WaitingForPlayers
		|| Phase == EGameFlowPhase::WaitingForPlayersReady)
	{
		const float FlowProgress = MapEnteredProgress + BoundRoundGameState->GetLoadingProgress() * ReadyGateProgressRange;
		SetLoadingProgress(FMath::Max(CurrentProgress, FlowProgress));
		return;
	}

	if (Phase == EGameFlowPhase::AugmentSelecting
		|| Phase == EGameFlowPhase::RoundInProgress)
	{
		SetLoadingProgress(1.0f);
		FinishLoadingScreen();
	}
}

void ULoadingScreenSubsystem::RebindLoadingWidgetToCurrentWorld()
{
	if (!IsValid(ActiveLoadingWidget))
	{
		return;
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		return;
	}

	// 캐시된 월드가 떠난 월드면 뷰포트를 못 찾음
	ActiveLoadingWidget->SetPlayerContext(
		FLocalPlayerContext(
			GameInstance->GetFirstGamePlayer(),
			GameInstance->GetWorld()));
}

UGameViewportClient* ULoadingScreenSubsystem::GetGameViewportClient() const
{
	const UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetGameViewportClient() : nullptr;
}

void ULoadingScreenSubsystem::SetPendingLoadingContext(const FShooterLoadingContext& InContext)
{
	PendingContext = InContext;
}

void ULoadingScreenSubsystem::ClearPendingLoadingContext()
{
	PendingContext = FShooterLoadingContext();
}

void ULoadingScreenSubsystem::FinishLoadingScreen()
{
	// 선로드가 끝나기 전에는 100% 로 가지 않음
	if (!CanFinishLoading())
	{
		DeferFinishUntilPreloadEnd();
		return;
	}

	ApplyProgress(1.0f);

	if (!ActiveLoadingWidget)
	{
		HideLoadingScreen();
		return;
	}

	if (bCompletionExitInFlight)
	{
		return;
	}

	bCompletionExitInFlight = true;

	// Exit 애니가 페이드하는 동안 뒤에 월드가 있어야 함. 없으면 검은색뜸
	if (UGameViewportClient* ViewportClient = GetGameViewportClient())
	{
		ViewportClient->bDisableWorldRendering = false;
	}

	FSimpleDelegate OnFinished;
	OnFinished.BindWeakLambda(this, [this]()
	{
		HideLoadingScreen();
	});

	ActiveLoadingWidget->BeginCompletionExit(OnFinished);
}

void ULoadingScreenSubsystem::DeferFinishUntilPreloadEnd()
{
	// 이미 기다리는 중이면 마감 시각을 다시 늘리지 않는다
	if (bFinishDeferredUntilPreloadEnd)
	{
		return;
	}

	bFinishDeferredUntilPreloadEnd = true;
	UE_LOG(LogLoadingScreen, Log, TEXT("[Loading] Finish deferred until pawn preload completes"));

	PreloadFinishTimeoutHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::HandlePreloadFinishTimeout),
		PreloadFinishWaitSeconds);
}

void ULoadingScreenSubsystem::ClearPreloadFinishTimeout()
{
	if (PreloadFinishTimeoutHandle.IsValid())
	{
		FTSTicker::RemoveTicker(PreloadFinishTimeoutHandle);
		PreloadFinishTimeoutHandle.Reset();
	}
}

bool ULoadingScreenSubsystem::HandlePreloadFinishTimeout(float DeltaTime)
{
	// 이미 끝난 핸들을 다시 지우지 않게 먼저 비움
	PreloadFinishTimeoutHandle.Reset();

	UE_LOG(LogLoadingScreen, Warning,
		TEXT("[Loading] Pawn preload did not finish within %.1fs - finishing anyway"),
		PreloadFinishWaitSeconds);

	// 완료가 아니다. 90% 를 열지 않음
	AbandonPreloadWait();
	return false;
}
