#include "GameMode/Manager/DominationSandstormManager.h"

#include "GameMode/Settings/DominationSandstormSettings.h"
#include "GameMode/State/DominationGameState.h"

namespace
{
	float RandomDuration(const float A, const float B)
	{
		return FMath::FRandRange(FMath::Min(A, B), FMath::Max(A, B));
	}
}

bool UDominationSandstormManager::Initialize(
	ADominationGameMode* InOwnerGameMode, UDominationSandstormSettings* InSettings)
{
	Shutdown();
	if (!InSettings || !InitializeManager(InOwnerGameMode))
	{
		return false;
	}
	Settings = InSettings;
	GetDominationGameState()->SetSandstormSettings(Settings);
	return true;
}

void UDominationSandstormManager::Shutdown()
{
	StopStormFlow();
	Settings = nullptr;
	Super::Shutdown();
}

void UDominationSandstormManager::StartScheduling()
{
	if (!Settings || bRunning) { return; }
	bRunning = true;
#if !UE_BUILD_SHIPPING
	if (Settings->bDisableStormScheduling) { return; }
	if (Settings->bStartImmediately)
	{
		BeginPhase(EDominationSandstormPhase::Warning, Settings->WarningDuration);
		return;
	}
#endif
	ScheduleNextStorm(true);
}

void UDominationSandstormManager::StopStormFlow()
{
	bRunning = false;
	if (FTimerManager* TimerManager = GetTimerManager())
	{
		TimerManager->ClearTimer(PhaseTimer);
	}
	if (ADominationGameState* GameState = GetDominationGameState())
	{
		GameState->SetSandstormState(FDominationSandstormState());
	}
}

void UDominationSandstormManager::ForceStorm()
{
#if !UE_BUILD_SHIPPING
	if (bRunning)
	{
		BeginPhase(EDominationSandstormPhase::Warning, Settings->WarningDuration);
	}
#endif
}

void UDominationSandstormManager::ScheduleNextStorm(const bool bInitialStorm)
{
#if !UE_BUILD_SHIPPING
	if (Settings->bDisableStormScheduling)
	{
		GetTimerManager()->ClearTimer(PhaseTimer);
		GetDominationGameState()->SetSandstormState(FDominationSandstormState());
		return;
	}
#endif
	const float Delay = bInitialStorm
		? RandomDuration(Settings->InitialDelayMin, Settings->InitialDelayMax)
		: RandomDuration(Settings->RepeatDelayMin, Settings->RepeatDelayMax);
	BeginPhase(EDominationSandstormPhase::Inactive, Delay);
}

void UDominationSandstormManager::AdvancePhase()
{
	if (!bRunning) { return; }
	using EPhase = EDominationSandstormPhase;
	switch (GetDominationGameState()->GetSandstormState().Phase)
	{
	case EPhase::Inactive:
		BeginPhase(EPhase::Warning, Settings->WarningDuration);
		break;
	case EPhase::Warning:
		BeginPhase(EPhase::FadeIn, Settings->FadeInDuration);
		break;
	case EPhase::FadeIn:
		BeginPhase(EPhase::Peak, RandomDuration(Settings->PeakDurationMin, Settings->PeakDurationMax));
		break;
	case EPhase::Peak:
		BeginPhase(EPhase::FadeOut, Settings->FadeOutDuration);
		break;
	case EPhase::FadeOut:
		ScheduleNextStorm(false);
		break;
	}
}

void UDominationSandstormManager::BeginPhase(const EDominationSandstormPhase Phase, const float Duration)
{
	ADominationGameState* GameState = GetDominationGameState();
	FDominationSandstormState State;
	State.Phase = Phase;
	// 0초 타이머는 해제 의미이므로 다음 단계가 멈추지 않도록 최소 간격을 둔다.
	State.PhaseDuration = FMath::Max(0.01f, Duration);
	State.PhaseEndServerTime = GameState->GetServerWorldTimeSeconds() + State.PhaseDuration;
	GetTimerManager()->SetTimer(PhaseTimer, this, &ThisClass::AdvancePhase, State.PhaseDuration, false);
	GameState->SetSandstormState(State);
}
