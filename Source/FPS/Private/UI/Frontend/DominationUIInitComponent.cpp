#include "UI/Frontend/DominationUIInitComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameMode/State/DominationGameState.h"
#include "Player/BaseTeamPlayerState.h"
#include "Player/DominationPlayerState.h"
#include "Player/ShooterPlayerController.h"
#include "Preload/DominationPreloadSubsystem.h"
#include "Tags/UITags.h"
#include "TimerManager.h"
#include "UI/InGame/ShooterMatchOutcomeScreen.h"
#include "UI/OutGame/MatchResult/ShooterMatchResultSubsystem.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"
#include "UI/Transition/ScreenTransitionSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogDominationUIInit, Log, All);

void UDominationUIInitComponent::BeginPlay()
{
    Super::BeginPlay();

    // travel 이벤트를 못 받은 경로 대비. 
    const AShooterPlayerController* PlayerController =
        Cast<AShooterPlayerController>(GetOwner());
    UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    if (IsValid(PlayerController)
        && PlayerController->IsLocalController()
        && IsValid(GameInstance))
    {
        if (UDominationPreloadSubsystem* PreloadSubsystem =
            GameInstance->GetSubsystem<UDominationPreloadSubsystem>())
        {
            PreloadSubsystem->EnsurePreloadStarted(GetWorld());
        }
    }
}

void UDominationUIInitComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(MatchExitTransitionTimer);
    }

    UnbindFromGameState();
    Super::EndPlay(EndPlayReason);
}

bool UDominationUIInitComponent::PrepareModeBindings()
{
    ADominationGameState* ModeState = GetWorld()
        ? GetWorld()->GetGameState<ADominationGameState>() : nullptr;
    if (!IsValid(ModeState)) { return false; }
    BindToGameState(ModeState);
    return true;
}

bool UDominationUIInitComponent::CanInitializeCombatHUD(const ADominationPlayerState&) const
{
    return BoundGameState.IsValid()
        && (BoundGameState->GetDominationPhase() == EDominationMatchPhase::InProgress
            || BoundGameState->GetDominationPhase() == EDominationMatchPhase::MatchEnded);
}

bool UDominationUIInitComponent::ShouldPresentClassSelection(const ADominationPlayerState& ClassState) const
{
    if (!BoundGameState.IsValid()) { return false; }
    const EDominationMatchPhase Phase = BoundGameState->GetDominationPhase();
    return (Phase == EDominationMatchPhase::ClassSelecting
            && ClassState.GetParticipationState() == EPlayerParticipationState::WaitingForMatchStart)
        || (Phase == EDominationMatchPhase::InProgress
            && ClassState.GetParticipationState() == EPlayerParticipationState::WaitingForNextSpawn
            && !ClassState.HasSelectedCombatClass());
}

bool UDominationUIInitComponent::IsAugmentModeActive() const
{
    return BoundGameState.IsValid()
        && BoundGameState->GetDominationPhase() == EDominationMatchPhase::InProgress;
}

void UDominationUIInitComponent::HandleDominationFlowStateChanged()
{
	RefreshCombatUI();
	TryCaptureMatchResult();
	TryPresentMatchEnd();

	if (IsTeamMatchUIReady())
	{
		NotifyModeUIStateChanged();
	}
}

void UDominationUIInitComponent::HandleMatchStateChanged()
{
	TryCaptureMatchResult();
	TryPresentMatchEnd();
	
	// 새 매치가 InProgress 로 진입하는 순간, 이전 매치에서 남은
	// 증강 선택 보류 상태와 진입 연출 이력을 초기화한다.
	if (!BoundGameState.IsValid())
	{
		return;
	}
	
	const EDominationMatchPhase Phase = BoundGameState->GetDominationPhase();
	if (Phase == LastObservedMatchPhase)
	{
		return;
	}
	LastObservedMatchPhase = Phase;

	// 진입 순간에만 초기화
	if (Phase != EDominationMatchPhase::InProgress)
	{
		return;                     
	}

	ResetAugmentPresentationHistory();
	LastObservedRewardRound = 0;
}

void UDominationUIInitComponent::HandleRewardRoundChanged(EGameTeam Team, int32 NewScore)
{
	const int32 Round = BoundGameState.IsValid()
		? BoundGameState->GetCurrentRoundNumber()
		: 0;
	if (Round == LastObservedRewardRound)
	{
		return;
	}
	
	LastObservedRewardRound = Round;
	
	SetAugmentSelectDismissed(false);
}

void UDominationUIInitComponent::TryCaptureMatchResult()
{
	if (BoundGameState.IsValid()
		&& BoundGameState->GetDominationPhase() == EDominationMatchPhase::MatchEnded
		&& BoundGameState->GetMatchWinner() != EGameTeam::None)
	{
		UGameInstance* GameInstance = GetWorld()
			? GetWorld()->GetGameInstance()
			: nullptr;
		UShooterMatchResultSubsystem* MatchResultSubsystem = IsValid(GameInstance)
			? GameInstance->GetSubsystem<UShooterMatchResultSubsystem>()
			: nullptr;
		if (IsValid(MatchResultSubsystem))
		{
			MatchResultSubsystem->CaptureSnapshot(BoundGameState.Get());
		}
	}
}

void UDominationUIInitComponent::TryPresentMatchEnd()
{
	if (!BoundGameState.IsValid()
		|| BoundGameState->GetDominationPhase() != EDominationMatchPhase::MatchEnded
		|| BoundGameState->GetMatchWinner() == EGameTeam::None)
	{
		return;
	}

	SuspendScreenPresentation();
	TryPresentMatchOutcome();

	// 연출 성공과 무관하게 예약
	TryScheduleMatchExitTransition();
}

void UDominationUIInitComponent::TryPresentMatchOutcome()
{
	if (IsValid(MatchOutcomeScreen) || !MatchOutcomeScreenClass)
	{
		return;
	}

	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	const ABaseTeamPlayerState* PlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<ABaseTeamPlayerState>()
		: nullptr;
	const EGameTeam LocalTeam = IsValid(PlayerState) ? PlayerState->GetTeam() : EGameTeam::None;
	if (LocalTeam == EGameTeam::None)
	{
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	if (!IsValid(UIManager))
	{
		return;
	}

	MatchOutcomeScreen = Cast<UShooterMatchOutcomeScreen>(
		UIManager->PushWidgetToLayer(UITags::UI_Layer_Menu, MatchOutcomeScreenClass));
	if (!IsValid(MatchOutcomeScreen))
	{
		return;
	}

	// 내 팀 점수가 먼저
	const bool bLocalIsTeamA = (LocalTeam == EGameTeam::TeamA);
	const int32 TeamAScore = BoundGameState->GetTeamAScore();
	const int32 TeamBScore = BoundGameState->GetTeamBScore();

	MatchOutcomeScreen->ShowOutcome(
		LocalTeam == BoundGameState->GetMatchWinner(),
		bLocalIsTeamA ? TeamAScore : TeamBScore,
		bLocalIsTeamA ? TeamBScore : TeamAScore);
}

void UDominationUIInitComponent::TryScheduleMatchExitTransition()
{
	UWorld* World = GetWorld();
	if (bMatchExitTransitionAttempted
		|| !IsValid(World)
		|| World->GetTimerManager().IsTimerActive(MatchExitTransitionTimer))
	{
		return;
	}

	// 서버가 복제한 종료 시각 기준
	const float Remaining = BoundGameState->GetPhaseTimeRemaining();
	if (Remaining <= MatchExitCoverLeadSeconds)
	{
		BeginMatchExitTransition(true);
		return;
	}

	World->GetTimerManager().SetTimer(
		MatchExitTransitionTimer,
		FTimerDelegate::CreateUObject(this, &ThisClass::BeginMatchExitTransition, false),
		Remaining - MatchExitCoverLeadSeconds,
		false);
}

void UDominationUIInitComponent::BeginMatchExitTransition(const bool bInstant)
{
	bMatchExitTransitionAttempted = true;

	UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UScreenTransitionSubsystem* Transition = IsValid(GameInstance)
		? GameInstance->GetSubsystem<UScreenTransitionSubsystem>()
		: nullptr;

	if (!Transition || !Transition->BeginTransition(FSimpleDelegate(), bInstant))
	{
		UE_LOG(LogDominationUIInit, Warning, TEXT("Match exit transition could not start."));
	}
}

void UDominationUIInitComponent::BindToGameState(
	ADominationGameState* DominationGameState)
{
	if (BoundGameState.Get() == DominationGameState)
	{
		return;
	}

	UnbindFromGameState();
	BoundGameState = DominationGameState;
	DominationGameState->OnDominationFlowStateChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleDominationFlowStateChanged);
	DominationGameState->OnMatchStateChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleMatchStateChanged);
	DominationGameState->OnTeamScoreChanged.AddUniqueDynamic(
		this, 
		&ThisClass::HandleRewardRoundChanged);
}

void UDominationUIInitComponent::UnbindFromGameState()
{
	if (BoundGameState.IsValid())
	{
		BoundGameState->OnDominationFlowStateChanged.RemoveAll(this);
		BoundGameState->OnMatchStateChanged.RemoveAll(this);
		BoundGameState->OnTeamScoreChanged.RemoveAll(this);
	}
	BoundGameState.Reset();
}

bool UDominationUIInitComponent::IsSurrenderRequestAllowed() const
{
	return BoundGameState.IsValid()
		&& BoundGameState->GetDominationPhase() == EDominationMatchPhase::InProgress;
}

