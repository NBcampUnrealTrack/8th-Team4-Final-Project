#include "UI/InGame/HUD/BaseBombGameFlowWidget.h"

#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "GameMode/BombGameState.h"
#include "Actor/DataJack.h"
#include "Player/BaseTeamPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogBombGameFlowWidget, Log, All);

UBaseBombGameFlowWidget::UBaseBombGameFlowWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	RoundEndTitleText = NSLOCTEXT("BombGameFlow", "RoundEndTitleText", "라운드 종료");
	MatchEndTitleText = NSLOCTEXT("BombGameFlow", "MatchEndTitleText", "경기 종료");
	VictoryText = NSLOCTEXT("BombGameFlow", "VictoryText", "W I N");
	DefeatText = NSLOCTEXT("BombGameFlow", "DefeatText", "D E F E A T");
}

void UBaseBombGameFlowWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BindToReplicatedState();
	RefreshResult();
}

void UBaseBombGameFlowWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(StateBindingTimerHandle);
	}

	UnbindFromGameState();
	bHasInitializedBindings = false;
	CachedResultType = EBombResultType::None;
	CachedResult = EBombMatchResult::None;
	Super::NativeDestruct();
}

int32 UBaseBombGameFlowWidget::GetCurrentRoundNumber() const
{
	const ABombGameState* BombGameState = GetBombGameState();
	return IsValid(BombGameState) ? BombGameState->GetCurrentRoundNumber() : 0;
}

EGameFlowPhase UBaseBombGameFlowWidget::GetGameFlowPhase() const
{
	const ABombGameState* BombGameState = GetBombGameState();
	return IsValid(BombGameState) ? BombGameState->GetGameFlowPhase() : EGameFlowPhase::WaitingForPlayers;
}

float UBaseBombGameFlowWidget::GetRoundTimeRemaining() const
{
	const ABombGameState* BombGameState = GetBombGameState();
	return IsValid(BombGameState) ? BombGameState->GetRoundTimeRemaining() : 0.0f;
}

FText UBaseBombGameFlowWidget::GetRoundTimeText() const
{
	if (GetGameFlowPhase() == EGameFlowPhase::AugmentSelecting)
	{
		return GetPhaseTimeText();
	}

	return FormatTime(GetRoundTimeRemaining());
}

float UBaseBombGameFlowWidget::GetPhaseTimeRemaining() const
{
	const ABombGameState* BombGameState = GetBombGameState();
	return IsValid(BombGameState) ? BombGameState->GetPhaseTimeRemaining() : 0.0f;
}

FText UBaseBombGameFlowWidget::GetPhaseTimeText() const
{
	return FormatTime(GetPhaseTimeRemaining());
}

EGameTeam UBaseBombGameFlowWidget::GetRoundWinner() const
{
	const ABombGameState* BombGameState = GetBombGameState();
	return IsValid(BombGameState) ? BombGameState->GetRoundWinner() : EGameTeam::None;
}

ERoundEndReason UBaseBombGameFlowWidget::GetRoundEndReason() const
{
	const ABombGameState* BombGameState = GetBombGameState();
	return IsValid(BombGameState) ? BombGameState->GetRoundEndReason() : ERoundEndReason::None;
}

EGameTeam UBaseBombGameFlowWidget::GetMatchWinner() const
{
	const ABombGameState* BombGameState = GetBombGameState();
	return IsValid(BombGameState) ? BombGameState->GetMatchWinner() : EGameTeam::None;
}

EBombResultType UBaseBombGameFlowWidget::GetResultType() const
{
	switch (GetGameFlowPhase())
	{
	case EGameFlowPhase::RoundEnding:
		return GetRoundWinner() != EGameTeam::None ? EBombResultType::Round : EBombResultType::None;
	case EGameFlowPhase::MatchEnded:
		return GetMatchWinner() != EGameTeam::None ? EBombResultType::Match : EBombResultType::None;
	default:
		return EBombResultType::None;
	}
}

EGameTeam UBaseBombGameFlowWidget::GetLocalPlayerTeam() const
{
	const ABaseTeamPlayerState* LocalPlayerState = GetLocalTeamPlayerState();
	return IsValid(LocalPlayerState) ? LocalPlayerState->GetTeam() : EGameTeam::None;
}

EGameTeam UBaseBombGameFlowWidget::GetWinningTeamForCurrentResult() const
{
	switch (GetResultType())
	{
	case EBombResultType::Round:
		return GetRoundWinner();
	case EBombResultType::Match:
		return GetMatchWinner();
	default:
		return EGameTeam::None;
	}
}

EBombMatchResult UBaseBombGameFlowWidget::GetLocalResult() const
{
	const EGameTeam LocalTeam = GetLocalPlayerTeam();
	const EGameTeam WinningTeam = GetWinningTeamForCurrentResult();

	if (LocalTeam == EGameTeam::None || WinningTeam == EGameTeam::None)
	{
		return EBombMatchResult::None;
	}

	return LocalTeam == WinningTeam ? EBombMatchResult::Victory : EBombMatchResult::Defeat;
}

bool UBaseBombGameFlowWidget::ShouldShowResult() const
{
	return GetResultType() != EBombResultType::None && GetLocalResult() != EBombMatchResult::None;
}

FText UBaseBombGameFlowWidget::GetResultTitleText() const
{
	switch (GetResultType())
	{
	case EBombResultType::Round:
		return RoundEndTitleText;
	case EBombResultType::Match:
		return MatchEndTitleText;
	default:
		return FText::GetEmpty();
	}
}

FText UBaseBombGameFlowWidget::GetResultText() const
{
	switch (GetLocalResult())
	{
	case EBombMatchResult::Victory:
		return VictoryText.IsEmpty() ? NSLOCTEXT("BombGameFlow", "DefaultVictoryText", "W I N") : VictoryText;
	case EBombMatchResult::Defeat:
	{
		const FText DefaultDefeatText = NSLOCTEXT("BombGameFlow", "DefaultDefeatText", "D E F E A T");
		return DefeatText.IsEmpty() || DefeatText.ToString() == VictoryText.ToString() ? DefaultDefeatText : DefeatText;
	}
	default:
		return FText::GetEmpty();
	}
}

ESlateVisibility UBaseBombGameFlowWidget::GetResultVisibility() const
{
	return ShouldShowResult() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
}

EBombMatchResult UBaseBombGameFlowWidget::GetLocalMatchResult() const
{
	if (GetResultType() != EBombResultType::Match)
	{
		return EBombMatchResult::None;
	}

	return GetLocalResult();
}

bool UBaseBombGameFlowWidget::ShouldShowMatchResult() const
{
	return GetLocalMatchResult() != EBombMatchResult::None;
}

FText UBaseBombGameFlowWidget::GetMatchResultText() const
{
	return GetResultType() == EBombResultType::Match ? GetResultText() : FText::GetEmpty();
}

ESlateVisibility UBaseBombGameFlowWidget::GetMatchResultVisibility() const
{
	return ShouldShowMatchResult() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
}

int32 UBaseBombGameFlowWidget::GetMaxRounds() const
{
	const ABombGameState* BombGameState = GetBombGameState();
	return IsValid(BombGameState) ? BombGameState->GetMaxRounds() : 0;
}

int32 UBaseBombGameFlowWidget::GetTeamAScore() const
{
	const ABombGameState* BombGameState = GetBombGameState();
	return IsValid(BombGameState) ? BombGameState->GetTeamAScore() : 0;
}

int32 UBaseBombGameFlowWidget::GetTeamBScore() const
{
	const ABombGameState* BombGameState = GetBombGameState();
	return IsValid(BombGameState) ? BombGameState->GetTeamBScore() : 0;
}

int32 UBaseBombGameFlowWidget::GetLocalTeamScore() const
{
	const EGameTeam LocalTeam = GetLocalPlayerTeam();
	if (LocalTeam == EGameTeam::None)
	{
		return 0;
	}

	return LocalTeam == EGameTeam::TeamA ? GetTeamAScore() : GetTeamBScore();
}

int32 UBaseBombGameFlowWidget::GetEnemyTeamScore() const
{
	const EGameTeam LocalTeam = GetLocalPlayerTeam();
	if (LocalTeam == EGameTeam::None)
	{
		return 0;
	}

	return LocalTeam == EGameTeam::TeamA ? GetTeamBScore() : GetTeamAScore();
}

ADataJack* UBaseBombGameFlowWidget::GetActiveDataJack() const
{
	if (BoundDataJack.IsValid())
	{
		return BoundDataJack.Get();
	}

	const ABombGameState* BombGameState = GetBombGameState();
	return IsValid(BombGameState) ? BombGameState->GetActiveDataJack() : nullptr;
}

EDataJackState UBaseBombGameFlowWidget::GetDataJackState() const
{
	const ADataJack* DataJack = GetActiveDataJack();
	return IsValid(DataJack) ? DataJack->GetDataJackState() : EDataJackState::Idle;
}

float UBaseBombGameFlowWidget::GetDataJackCountdownRemaining() const
{
	const ADataJack* DataJack = GetActiveDataJack();
	return IsValid(DataJack) ? DataJack->GetDetonationTimeRemaining() : 0.0f;
}

FText UBaseBombGameFlowWidget::GetDataJackCountdownText() const
{
	return FormatTime(GetDataJackCountdownRemaining());
}

float UBaseBombGameFlowWidget::GetDataJackCountdownProgress() const
{
	const ADataJack* DataJack = GetActiveDataJack();
	return IsValid(DataJack) ? DataJack->GetDetonationProgress() : 0.0f;
}

float UBaseBombGameFlowWidget::GetDataJackInteractionTimeRemaining() const
{
	const ADataJack* DataJack = GetActiveDataJack();
	return IsValid(DataJack) ? DataJack->GetInteractionTimeRemaining() : 0.0f;
}

FText UBaseBombGameFlowWidget::GetDataJackInteractionTimeText() const
{
	return FormatTime(GetDataJackInteractionTimeRemaining());
}

float UBaseBombGameFlowWidget::GetDataJackInteractionProgress() const
{
	const ADataJack* DataJack = GetActiveDataJack();
	return IsValid(DataJack) ? DataJack->GetInteractionProgress() : 0.0f;
}

FVector UBaseBombGameFlowWidget::GetDataJackWorldLocation() const
{
	const ADataJack* DataJack = GetActiveDataJack();
	return IsValid(DataJack) ? DataJack->GetTrackedWorldLocation() : FVector::ZeroVector;
}

FName UBaseBombGameFlowWidget::GetPlantedSiteId() const
{
	const ADataJack* DataJack = GetActiveDataJack();
	return IsValid(DataJack) ? DataJack->GetPlantedSiteId() : NAME_None;
}

bool UBaseBombGameFlowWidget::ShouldShowRoundTimer() const
{
	return GetGameFlowPhase() == EGameFlowPhase::RoundInProgress
		&& !ShouldShowDataJackCountdown();
}

bool UBaseBombGameFlowWidget::ShouldShowDataJackCountdown() const
{
	if (GetGameFlowPhase() != EGameFlowPhase::RoundInProgress)
	{
		return false;
	}

	const EDataJackState ObjectiveState = GetDataJackState();
	return ObjectiveState == EDataJackState::Planted || ObjectiveState == EDataJackState::Defusing;
}

bool UBaseBombGameFlowWidget::ShouldShowDataJackMinimapIcon() const
{
	if (GetGameFlowPhase() != EGameFlowPhase::RoundInProgress)
	{
		return false;
	}

	const ADataJack* DataJack = GetActiveDataJack();
	if (!IsValid(DataJack))
	{
		return false;
	}

	switch (DataJack->GetDataJackState())
	{
	case EDataJackState::Idle:
		return IsLocalPlayerAttacker();
	case EDataJackState::Carried:
	case EDataJackState::Planting:
		return IsDataJackCarriedByLocalTeam();
	case EDataJackState::Planted:
	case EDataJackState::Defusing:
		return true;
	default:
		return false;
	}
}

bool UBaseBombGameFlowWidget::ShouldShowDataJackWorldPing() const
{
	if (GetGameFlowPhase() != EGameFlowPhase::RoundInProgress)
	{
		return false;
	}

	const ADataJack* DataJack = GetActiveDataJack();
	return IsValid(DataJack)
		&& IsLocalPlayerAttacker()
		&& DataJack->GetDataJackState() == EDataJackState::Idle
		&& DataJack->HasBeenDropped();
}

bool UBaseBombGameFlowWidget::DoesLocalPlayerCarryDataJack() const
{
	const ADataJack* DataJack = GetActiveDataJack();
	return IsValid(DataJack) && DataJack->IsCarriedBy(GetLocalTeamPlayerState());
}

bool UBaseBombGameFlowWidget::IsDataJackCarriedByLocalTeam() const
{
	const ADataJack* DataJack = GetActiveDataJack();
	const ABaseTeamPlayerState* LocalPlayerState = GetLocalTeamPlayerState();
	const ABaseTeamPlayerState* CarrierPlayerState = IsValid(DataJack) ? DataJack->GetCarrierPlayerState() : nullptr;
	return IsValid(LocalPlayerState)
		&& IsValid(CarrierPlayerState)
		&& LocalPlayerState->GetTeam() != EGameTeam::None
		&& LocalPlayerState->GetTeam() == CarrierPlayerState->GetTeam();
}

bool UBaseBombGameFlowWidget::IsLocalPlayerInteractingWithDataJack() const
{
	const ADataJack* DataJack = GetActiveDataJack();
	return IsValid(DataJack) && DataJack->IsInteractingPlayer(GetLocalTeamPlayerState());
}

void UBaseBombGameFlowWidget::HandleRoundStateChanged()
{
	NotifyGameFlowChanged();
}

void UBaseBombGameFlowWidget::HandleTeamScoreChanged(const EGameTeam, const int32)
{
	NotifyGameFlowChanged();
}

void UBaseBombGameFlowWidget::HandleActiveDataJackChanged(ADataJack* NewActiveDataJack)
{
	BindToActiveDataJack(NewActiveDataJack);
	NotifyGameFlowChanged();
}

void UBaseBombGameFlowWidget::HandleLocalPlayerTeamChanged(const EGameTeam, const EGameTeam)
{
	NotifyGameFlowChanged();
}

void UBaseBombGameFlowWidget::HandleDataJackChanged()
{
	NotifyGameFlowChanged();
}

void UBaseBombGameFlowWidget::BindToReplicatedState()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	ABombGameState* BombGameState = World->GetGameState<ABombGameState>();
	if (!IsValid(BombGameState))
	{
		if (!World->GetTimerManager().IsTimerActive(StateBindingTimerHandle))
		{
			World->GetTimerManager().SetTimer(StateBindingTimerHandle, this, &UBaseBombGameFlowWidget::BindToReplicatedState, 0.25f, true);
		}
		return;
	}

	const bool bBindingChanged = BoundGameState.Get() != BombGameState;
	if (bBindingChanged)
	{
		UnbindFromGameState();
		BoundGameState = BombGameState;
		BombGameState->OnRoundStateChanged.AddDynamic(this, &UBaseBombGameFlowWidget::HandleRoundStateChanged);
		BombGameState->OnTeamScoreChanged.AddDynamic(this, &UBaseBombGameFlowWidget::HandleTeamScoreChanged);
		BombGameState->OnActiveDataJackChanged.AddDynamic(this, &UBaseBombGameFlowWidget::HandleActiveDataJackChanged);
		BindToActiveDataJack(BombGameState->GetActiveDataJack());
	}

	ABaseTeamPlayerState* LocalPlayerState = GetLocalTeamPlayerState();
	const bool bLocalPlayerBindingChanged = BindToLocalPlayerState(LocalPlayerState);
	if (IsValid(LocalPlayerState))
	{
		World->GetTimerManager().ClearTimer(StateBindingTimerHandle);
	}
	else if (!World->GetTimerManager().IsTimerActive(StateBindingTimerHandle))
	{
		World->GetTimerManager().SetTimer(StateBindingTimerHandle, this, &UBaseBombGameFlowWidget::BindToReplicatedState, 0.25f, true);
	}

	if (bBindingChanged || bLocalPlayerBindingChanged || !bHasInitializedBindings)
	{
		bHasInitializedBindings = true;
		NotifyGameFlowChanged();
	}
}

void UBaseBombGameFlowWidget::NotifyGameFlowChanged()
{
	RefreshResult();
	BP_OnGameFlowChanged();
}

void UBaseBombGameFlowWidget::RefreshResult()
{
	const EBombResultType NewResultType = GetResultType();
	const EBombMatchResult NewResult = GetLocalResult();
	const FText ResultTitleText = GetResultTitleText();
	const FText ResultText = GetResultText();

	UWidget* ResultPanel = Panel_MatchResult.Get();
	if (IsValid(ResultPanel))
	{
		ResultPanel->SetVisibility(GetResultVisibility());
	}

	UTextBlock* ResultTitleTextBlock = Text_ResultTitle.Get();
	if (IsValid(ResultTitleTextBlock))
	{
		ResultTitleTextBlock->SetText(ResultTitleText);
	}

	UTextBlock* ResultTextBlock = Text_MatchResult.Get();
	if (IsValid(ResultTextBlock))
	{
		ResultTextBlock->SetText(ResultText);
	}

	if (CachedResultType == NewResultType && CachedResult == NewResult)
	{
		return;
	}

	UE_LOG(
		LogBombGameFlowWidget,
		Log,
		TEXT("Result changed. Owner=%s LocalTeam=%d WinningTeam=%d ResultType=%d LocalResult=%d Text=%s"),
		*GetNameSafe(GetOwningPlayer()),
		static_cast<int32>(GetLocalPlayerTeam()),
		static_cast<int32>(GetWinningTeamForCurrentResult()),
		static_cast<int32>(NewResultType),
		static_cast<int32>(NewResult),
		*ResultText.ToString());

	const EBombResultType PreviousResultType = CachedResultType;
	CachedResultType = NewResultType;
	CachedResult = NewResult;
	BP_OnResultChanged(NewResultType, NewResult, ResultTitleText, ResultText);

	if (NewResultType == EBombResultType::Match || PreviousResultType == EBombResultType::Match)
	{
		BP_OnMatchResultChanged(GetLocalMatchResult(), GetMatchResultText());
	}
}

void UBaseBombGameFlowWidget::BindToActiveDataJack(ADataJack* NewActiveDataJack)
{
	if (BoundDataJack.Get() == NewActiveDataJack)
	{
		return;
	}

	UnbindFromDataJack();
	BoundDataJack = NewActiveDataJack;
	if (IsValid(NewActiveDataJack))
	{
		NewActiveDataJack->OnDataJackChanged.AddDynamic(this, &UBaseBombGameFlowWidget::HandleDataJackChanged);
	}
}

bool UBaseBombGameFlowWidget::BindToLocalPlayerState(ABaseTeamPlayerState* NewLocalPlayerState)
{
	if (BoundLocalPlayerState.Get() == NewLocalPlayerState)
	{
		return false;
	}

	UnbindFromLocalPlayerState();
	BoundLocalPlayerState = NewLocalPlayerState;
	if (IsValid(NewLocalPlayerState))
	{
		NewLocalPlayerState->OnPlayerTeamChanged.AddDynamic(this, &UBaseBombGameFlowWidget::HandleLocalPlayerTeamChanged);
	}

	return true;
}

void UBaseBombGameFlowWidget::UnbindFromGameState()
{
	UnbindFromDataJack();
	UnbindFromLocalPlayerState();

	if (ABombGameState* BombGameState = BoundGameState.Get())
	{
		BombGameState->OnRoundStateChanged.RemoveDynamic(this, &UBaseBombGameFlowWidget::HandleRoundStateChanged);
		BombGameState->OnTeamScoreChanged.RemoveDynamic(this, &UBaseBombGameFlowWidget::HandleTeamScoreChanged);
		BombGameState->OnActiveDataJackChanged.RemoveDynamic(this, &UBaseBombGameFlowWidget::HandleActiveDataJackChanged);
	}

	BoundGameState.Reset();
}

void UBaseBombGameFlowWidget::UnbindFromDataJack()
{
	if (ADataJack* DataJack = BoundDataJack.Get())
	{
		DataJack->OnDataJackChanged.RemoveDynamic(this, &UBaseBombGameFlowWidget::HandleDataJackChanged);
	}

	BoundDataJack.Reset();
}

void UBaseBombGameFlowWidget::UnbindFromLocalPlayerState()
{
	if (ABaseTeamPlayerState* LocalPlayerState = BoundLocalPlayerState.Get())
	{
		LocalPlayerState->OnPlayerTeamChanged.RemoveDynamic(this, &UBaseBombGameFlowWidget::HandleLocalPlayerTeamChanged);
	}

	BoundLocalPlayerState.Reset();
}

FText UBaseBombGameFlowWidget::FormatTime(const float TimeSeconds) const
{
	const int32 TotalSeconds = FMath::Max(0, FMath::CeilToInt(TimeSeconds));
	const int32 Minutes = TotalSeconds / 60;
	const int32 Seconds = TotalSeconds % 60;
	return FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds));
}

ABombGameState* UBaseBombGameFlowWidget::GetBombGameState() const
{
	if (BoundGameState.IsValid())
	{
		return BoundGameState.Get();
	}

	return GetWorld() != nullptr ? GetWorld()->GetGameState<ABombGameState>() : nullptr;
}

ABaseTeamPlayerState* UBaseBombGameFlowWidget::GetLocalTeamPlayerState() const
{
	APlayerController* OwningPlayerController = GetOwningPlayer();
	return IsValid(OwningPlayerController) ? OwningPlayerController->GetPlayerState<ABaseTeamPlayerState>() : nullptr;
}


bool UBaseBombGameFlowWidget::IsLocalPlayerAttacker() const
{
	const ABaseTeamPlayerState* LocalPlayerState = GetLocalTeamPlayerState();
	const ABombGameState* BombGameState = GetBombGameState();
	return IsValid(LocalPlayerState)
		&& IsValid(BombGameState)
		&& BombGameState->GetTeamRole(LocalPlayerState->GetTeam()) == ETeamRole::Attackers;
}
