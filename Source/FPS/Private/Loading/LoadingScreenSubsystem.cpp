#include "Loading/LoadingScreenSubsystem.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFlow/Round/BaseRoundGameState.h"
#include "UI/Loading/LoadingScreenWidget.h"

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

	if (!ActiveLoadingWidget && WidgetClass)
	{
		ActiveLoadingWidget = CreateWidget<ULoadingScreenWidget>(OwningPlayer, WidgetClass);
		if (ActiveLoadingWidget)
		{
			ActiveLoadingWidget->AddToViewport(10000);
		}
	}

	SetLoadingProgress(InitialProgress);
	SyncWithRoundGameState();
}

void ULoadingScreenSubsystem::SetLoadingProgress(const float Progress)
{
	CurrentProgress = FMath::Clamp(Progress, 0.0f, 1.0f);

	if (ActiveLoadingWidget)
	{
		ActiveLoadingWidget->SetLoadingProgress(CurrentProgress);
	}
}

void ULoadingScreenSubsystem::HideLoadingScreen()
{
	if (ActiveLoadingWidget)
	{
		ActiveLoadingWidget->RemoveFromParent();
		ActiveLoadingWidget = nullptr;
	}
}

void ULoadingScreenSubsystem::BindToRoundGameState(ABaseRoundGameState* RoundGameState)
{
	if (!IsValid(RoundGameState) || BoundRoundGameState == RoundGameState)
	{
		return;
	}

	UnbindFromRoundGameState();
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
	return ActiveLoadingWidget != nullptr;
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

	SetLoadingProgress(FMath::Max(CurrentProgress, BoundRoundGameState->GetLoadingProgress()));

	const EGameFlowPhase Phase = BoundRoundGameState->GetGameFlowPhase();
	if (Phase == EGameFlowPhase::AugmentSelecting
		|| Phase == EGameFlowPhase::RoundInProgress)
	{
		HideLoadingScreen();
	}
}
