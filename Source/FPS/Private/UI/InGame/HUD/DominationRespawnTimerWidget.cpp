#include "UI/InGame/HUD/DominationRespawnTimerWidget.h"

#include "Components/Widget.h"
#include "Engine/World.h"
#include "Player/DominationPlayerState.h"

namespace
{
	// 플레이어 상태가 늦게 복제되는 경우 다시 시도하는 간격 (초)
	constexpr float BindRetryIntervalSeconds = 0.5f;
}

bool UDominationRespawnTimerWidget::IsWaitingForRespawn() const
{
	const ADominationPlayerState* PlayerState = GetBoundPlayerState();
	return IsValid(PlayerState)
		&& !PlayerState->IsAlive()
		&& PlayerState->GetRespawnSecondsRemaining() > 0.0f;
}

int32 UDominationRespawnTimerWidget::GetRespawnSecondsRemaining() const
{
	const ADominationPlayerState* PlayerState = GetBoundPlayerState();
	return IsValid(PlayerState) ? FMath::CeilToInt(PlayerState->GetRespawnSecondsRemaining()) : 0;
}

int32 UDominationRespawnTimerWidget::GetRespawnReductionSeconds() const
{
	const ADominationPlayerState* PlayerState = GetBoundPlayerState();
	return IsValid(PlayerState) ? FMath::RoundToInt(PlayerState->GetRespawnReductionSeconds()) : 0;
}

void UDominationRespawnTimerWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 시작할 때는 숨겨 둔다. 대기가 시작되면 Refresh가 보여 준다.
	if (IsValid(TimerRoot))
	{
		TimerRoot->SetVisibility(ESlateVisibility::Collapsed);
	}

	TryBindPlayerState();
}

void UDominationRespawnTimerWidget::NativeDestruct()
{
	UnbindPlayerState();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RefreshTimerHandle);
		World->GetTimerManager().ClearTimer(BindRetryTimerHandle);
	}

	Super::NativeDestruct();
}

void UDominationRespawnTimerWidget::TryBindPlayerState()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	ADominationPlayerState* PlayerState = GetOwningPlayerState<ADominationPlayerState>();
	if (!IsValid(PlayerState))
	{
		// 플레이어 상태가 아직 복제되지 않았으면 잠시 뒤 다시 시도한다.
		if (!World->GetTimerManager().IsTimerActive(BindRetryTimerHandle))
		{
			World->GetTimerManager().SetTimer(
				BindRetryTimerHandle, this, &ThisClass::TryBindPlayerState, BindRetryIntervalSeconds, true);
		}
		return;
	}

	World->GetTimerManager().ClearTimer(BindRetryTimerHandle);
	if (BoundPlayerState.Get() == PlayerState)
	{
		return;
	}

	UnbindPlayerState();
	BoundPlayerState = PlayerState;
	PlayerState->OnRespawnTimerChanged.AddUniqueDynamic(this, &ThisClass::HandleRespawnStateChanged);
	PlayerState->OnPlayerAliveStateChanged.AddUniqueDynamic(this, &ThisClass::HandleAliveStateChanged);
	Refresh();
}

void UDominationRespawnTimerWidget::UnbindPlayerState()
{
	if (ADominationPlayerState* PlayerState = BoundPlayerState.Get())
	{
		PlayerState->OnRespawnTimerChanged.RemoveDynamic(this, &ThisClass::HandleRespawnStateChanged);
		PlayerState->OnPlayerAliveStateChanged.RemoveDynamic(this, &ThisClass::HandleAliveStateChanged);
	}

	BoundPlayerState.Reset();
}

void UDominationRespawnTimerWidget::HandleRespawnStateChanged()
{
	Refresh();
}

void UDominationRespawnTimerWidget::HandleAliveStateChanged(bool)
{
	Refresh();
}

void UDominationRespawnTimerWidget::Refresh()
{
	const bool bWaiting = IsWaitingForRespawn();

	if (IsValid(TimerRoot))
	{
		const ESlateVisibility NewVisibility = bWaiting
			? ESlateVisibility::HitTestInvisible
			: ESlateVisibility::Collapsed;
		if (TimerRoot->GetVisibility() != NewVisibility)
		{
			TimerRoot->SetVisibility(NewVisibility);
		}
	}

	// 남은 초는 시간이 지나며 바뀌므로 대기 중에만 주기적으로 갱신한다.
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TimerManager = World->GetTimerManager();
		if (bWaiting && !TimerManager.IsTimerActive(RefreshTimerHandle))
		{
			TimerManager.SetTimer(RefreshTimerHandle, this, &ThisClass::HandleRefreshTimer, RefreshInterval, true);
		}
		else if (!bWaiting)
		{
			TimerManager.ClearTimer(RefreshTimerHandle);
		}
	}

	BP_OnRespawnTimerRefresh();
}

void UDominationRespawnTimerWidget::HandleRefreshTimer()
{
	// 대기가 끝났으면 Refresh가 숨기고 타이머를 멈춘다.
	if (!IsWaitingForRespawn())
	{
		Refresh();
		return;
	}

	BP_OnRespawnTimerRefresh();
}

ADominationPlayerState* UDominationRespawnTimerWidget::GetBoundPlayerState() const
{
	return BoundPlayerState.Get();
}
