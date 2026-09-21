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
	SyncWithRoundGameState();
}

void ULoadingScreenSubsystem::SetLoadingProgress(const float Progress)
{
	CurrentProgress = FMath::Max(CurrentProgress, FMath::Clamp(Progress, 0.0f, 1.0f));

	if (ActiveLoadingWidget)
	{
		ActiveLoadingWidget->SetLoadingProgress(CurrentProgress);
	}
}

void ULoadingScreenSubsystem::HideLoadingScreen()
{
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
