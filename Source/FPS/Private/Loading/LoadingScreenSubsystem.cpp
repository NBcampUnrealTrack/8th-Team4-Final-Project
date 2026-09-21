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

	SetLoadingProgress(InitialProgress);

	// 보류됐더라도 새로 붙은 위젯은 현재 값을 받는다
	ApplyProgress(CurrentProgress);

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
}

void ULoadingScreenSubsystem::AbandonPreloadWait()
{
	if (!bTravelProgressActive || bPreloadWaitAbandoned || bPreloadComplete)
	{
		return;
	}

	bPreloadWaitAbandoned = true;
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

	StopEstimatedTravelTicker();

	bTravelProgressActive = false;
	bPreloadComplete = false;
	bWorldLoaded = false;
	bMatchReady = false;
	bPreloadWaitAbandoned = false;
	EstimatedTravelProgress = 0.0f;
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
		HideLoadingScreen();
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
