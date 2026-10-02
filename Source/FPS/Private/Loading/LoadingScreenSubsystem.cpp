#include "Loading/LoadingScreenSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/State/BaseRoundGameState.h"
#include "UI/Loading/LoadingScreenWidget.h"
#include "UI/OutGame/ShooterOutGameSubsystem.h"
#include "UI/ShooterUISettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogLoadingScreen, Log, All);

namespace
{
	constexpr float MapEnteredProgress = 0.7f;
	constexpr float ReadyGateProgressRange = 0.25f;

	// 맵 이동의 실제 완료 단계
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

	// 서버는 그리지 않는다
	if (!IsRunningDedicatedServer())
	{
		LoadingBackground = GetDefault<UShooterUISettings>()->LoadingBackground.LoadSynchronous();
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

	if (IsValid(ActiveLoadingWidget))
	{
		RebindLoadingWidgetToCurrentWorld();
		ActiveLoadingWidget->ApplyLoadingContext(PendingContext);
	}

	SetLoadingProgress(InitialProgress);

	// 보류됐더라도 새로 붙은 위젯은 현재 값을 받는다
	ApplyProgress(CurrentProgress);

	// 세션 최초 부착에서만. 같은 세션의 두 번째 Show 에서 스코프가 또 날아오면 안 된다
	if (!bWasAlreadyVisible && IsLoadingScreenVisible() && ActiveLoadingWidget)
	{
		ActiveLoadingWidget->BeginEnterAnimation();

		if (UShooterOutGameSubsystem* OutGame = GetGameInstance()->GetSubsystem<UShooterOutGameSubsystem>())
		{
			OutGame->SetMusicState(EShooterOutGameMusicState::None);
		}
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

float ULoadingScreenSubsystem::ComputeTravelProgress() const
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

void ULoadingScreenSubsystem::ApplyTravelProgress()
{
	ApplyProgress(ComputeTravelProgress());
}

void ULoadingScreenSubsystem::BeginTravelProgress()
{
	if (bTravelProgressActive && !bPreloadWaitAbandoned && !bCompletionExitInFlight)
	{
		return;
	}

	// 이전 표시값과 종료 애니메이션은 새 이동에 넘기지 않는다.
	const bool bRestartVisibleSession = IsLoadingScreenVisible()
		&& (CurrentProgress > TravelStartProgress || bCompletionExitInFlight || bPreloadWaitAbandoned);
	const FShooterLoadingContext LoadingContext = PendingContext;
	const TSubclassOf<ULoadingScreenWidget> WidgetClass = IsValid(ActiveLoadingWidget)
		? ActiveLoadingWidget->GetClass()
		: nullptr;
	if (bRestartVisibleSession)
	{
		HideLoadingScreen();
		PendingContext = LoadingContext;
	}
	else
	{
		StopEstimatedTravelTicker();
	}

	bCompletionExitInFlight = false;
	bFinishDeferredUntilReady = false;
	bPreloadComplete = false;
	bWorldLoaded = false;
	bMatchReady = false;
	bPreloadWaitAbandoned = false;
	bTravelProgressActive = true;
	EstimatedTravelProgress = TravelStartProgress;
	CurrentProgress = 0.0f;
	ApplyTravelProgress();

	if (bRestartVisibleSession)
	{
		UGameInstance* GameInstance = GetGameInstance();
		ShowLoadingScreen(GameInstance ? GameInstance->GetFirstLocalPlayerController() : nullptr,
			WidgetClass, TravelStartProgress);
	}

	// 월드가 바뀌어도 추정 표시를 유지한다. 이 값으로 완료를 판단하지 않는다.
	EstimatedTravelTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &ThisClass::TickEstimatedTravelProgress));
}

bool ULoadingScreenSubsystem::TickEstimatedTravelProgress(const float DeltaTime)
{
	if (!bTravelProgressActive || bPreloadWaitAbandoned)
	{
		EstimatedTravelTickerHandle.Reset();
		return false;
	}

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

	ApplyTravelProgress();

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
	if (!bTravelProgressActive || bPreloadWaitAbandoned || bPreloadComplete)
	{
		return;
	}

	bPreloadComplete = true;
	StopEstimatedTravelTicker();
	ApplyTravelProgress();
	TryFinishDeferredLoading();
}

void ULoadingScreenSubsystem::AbandonPreloadWait()
{
	if (!bTravelProgressActive || bPreloadWaitAbandoned)
	{
		return;
	}

	// 취소된 선로드는 준비 완료가 아니다. 실패 경로에서 화면을 직접 걷는다.
	bPreloadWaitAbandoned = true;
	bFinishDeferredUntilReady = false;
	StopEstimatedTravelTicker();
}

void ULoadingScreenSubsystem::NotifyWorldLoaded()
{
	if (!bTravelProgressActive || bPreloadWaitAbandoned || bWorldLoaded)
	{
		return;
	}

	// 순서가 어긋나도 저장한다. 반영은 게이트가 정한다.
	bWorldLoaded = true;
	ApplyTravelProgress();
	TryFinishDeferredLoading();
}

void ULoadingScreenSubsystem::NotifyMatchLoadReady()
{
	if (!bTravelProgressActive || bPreloadWaitAbandoned || bMatchReady)
	{
		return;
	}

	bMatchReady = true;
	ApplyTravelProgress();
	TryFinishDeferredLoading();
}

bool ULoadingScreenSubsystem::IsTravelProgressActive() const
{
	return bTravelProgressActive;
}

bool ULoadingScreenSubsystem::CanFinishLoading() const
{
	return !bTravelProgressActive
		|| (!bPreloadWaitAbandoned && bPreloadComplete && bWorldLoaded && bMatchReady);
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

	bFinishDeferredUntilReady = false;
}

void ULoadingScreenSubsystem::BindToRoundGameState(ABaseRoundGameState* RoundGameState)
{
	if (BoundRoundGameState.Get() == RoundGameState)
	{
		return;
	}

	UnbindFromRoundGameState();

	if (!IsValid(RoundGameState))
	{
		return;
	}

	BoundRoundGameState = RoundGameState;
	RoundGameState->OnRoundStateChanged.AddUniqueDynamic(this, &ThisClass::HandleRoundStateChanged);
	SyncWithRoundGameState();
}

void ULoadingScreenSubsystem::UnbindFromRoundGameState()
{
	if (ABaseRoundGameState* RoundGameState = BoundRoundGameState.Get())
	{
		RoundGameState->OnRoundStateChanged.RemoveDynamic(this, &ThisClass::HandleRoundStateChanged);
	}

	BoundRoundGameState.Reset();
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
	const ABaseRoundGameState* RoundGameState = BoundRoundGameState.Get();
	if (!IsValid(RoundGameState))
	{
		return;
	}

	const EGameFlowPhase Phase = RoundGameState->GetGameFlowPhase();
	if (Phase == EGameFlowPhase::WaitingForPlayers
		|| Phase == EGameFlowPhase::WaitingForPlayersReady)
	{
		const float FlowProgress = MapEnteredProgress + RoundGameState->GetLoadingProgress() * ReadyGateProgressRange;
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

	ULocalPlayer* LocalPlayer = GameInstance->GetFirstGamePlayer();
	UWorld* CurrentWorld = GameInstance->GetWorld();
	if (!IsValid(LocalPlayer) || !IsValid(CurrentWorld))
	{
		return;
	}

	// 같은 위젯이 맵 이동을 넘겨도 오디오와 뷰포트는 현재 월드를 사용한다.
	ActiveLoadingWidget->SetPlayerContext(FLocalPlayerContext(LocalPlayer, CurrentWorld));
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
	// 실제 준비 세 단계가 끝나기 전에는 정상 완료를 표시하지 않는다.
	if (!CanFinishLoading())
	{
		if (!bPreloadWaitAbandoned && !bFinishDeferredUntilReady)
		{
			bFinishDeferredUntilReady = true;
			UE_LOG(LogLoadingScreen, Log,
				TEXT("[Loading] Finish deferred. Preload=%d World=%d Ready=%d"),
				bPreloadComplete, bWorldLoaded, bMatchReady);
		}
		return;
	}

	bFinishDeferredUntilReady = false;
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

void ULoadingScreenSubsystem::TryFinishDeferredLoading()
{
	if (bFinishDeferredUntilReady && CanFinishLoading())
	{
		bFinishDeferredUntilReady = false;
		FinishLoadingScreen();
	}
}
