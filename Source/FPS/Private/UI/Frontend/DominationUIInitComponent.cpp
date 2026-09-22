#include "UI/Frontend/DominationUIInitComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameMode/State/DominationGameState.h"
#include "Player/DominationPlayerState.h"
#include "Player/ShooterPlayerController.h"
#include "Preload/DominationPreloadSubsystem.h"
#include "UI/OutGame/MatchResult/ShooterMatchResultSubsystem.h"

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

	if (IsTeamMatchUIReady())
	{
		NotifyModeUIStateChanged();
	}
}

void UDominationUIInitComponent::HandleMatchStateChanged()
{
	TryCaptureMatchResult();
	
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

