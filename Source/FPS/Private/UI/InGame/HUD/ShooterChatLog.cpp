// ShooterChatLog.cpp
#include "UI/InGame/HUD/ShooterChatLog.h"

#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Player/ShooterPlayerController.h"
#include "UI/InGame/HUD/ShooterChatEntry.h"

void UShooterChatLog::NativeConstruct()
{
	Super::NativeConstruct();

	AShooterPlayerController* PlayerController = GetOwningPlayer<AShooterPlayerController>();
	if (!ensure(PlayerController))
	{
		return;
	}

	BoundPlayerController = PlayerController;
	PlayerController->OnChatMessageReceived.AddUniqueDynamic(this, &ThisClass::HandleChatMessageReceived);

	// 활동이 생기기 전까지는 배경+내역을 아예 숨겨둔다.
	SetVisibility(ESlateVisibility::Collapsed);
}

void UShooterChatLog::NativeDestruct()
{
	if (AShooterPlayerController* PlayerController = BoundPlayerController.Get())
	{
		PlayerController->OnChatMessageReceived.RemoveDynamic(this, &ThisClass::HandleChatMessageReceived);
	}
	BoundPlayerController.Reset();

	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExpiryTimerHandle);
		World->GetTimerManager().ClearTimer(PanelHideTimerHandle);
	}
	ExpiryTimerHandle.Invalidate();
	PanelHideTimerHandle.Invalidate();

	ActiveEntries.Reset();

	Super::NativeDestruct();
}

void UShooterChatLog::HandleChatMessageReceived(const FChatMessage& Message)
{
	if (!ensure(EntryWidgetClass) || !ensure(Box_Entries))
	{
		return;
	}

	UShooterChatEntry* EntryWidget = CreateWidget<UShooterChatEntry>(this, EntryWidgetClass);
	if (EntryWidget == nullptr)
	{
		return;
	}

	EntryWidget->SetData(Message);
	Box_Entries->AddChildToVerticalBox(EntryWidget);
	ActiveEntries.Add(EntryWidget);

	while (ActiveEntries.Num() > MaxVisibleEntries)
	{
		RemoveOldestEntry();
	}

	// 여러 줄짜리 메시지 때문에 내역 전체 높이가 패널 높이를 넘어도, 항상 최신 메시지가
	// 보이는 맨 아래로 스크롤시킨다 - 넘친 만큼 위쪽(오래된 줄)만 화면 밖으로 밀려난다.
	if (ScrollBox_Entries)
	{
		ScrollBox_Entries->ScrollToEnd();
	}

	// 최초 예약 등록
	if (ExpiryTimerHandle.IsValid() == false)
	{
		GetWorld()->GetTimerManager().SetTimer(
			ExpiryTimerHandle,
			this,
			&ThisClass::RemoveOldestEntry,
			EntryLifetimeSeconds,
			false);
	}

	ShowPanelAndResetInactivityTimer();
}

void UShooterChatLog::ShowPanelAndResetInactivityTimer()
{
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// 같은 핸들에 SetTimer를 다시 걸면 기존 예약은 자동으로 취소되고 새로 카운트다운된다 -
	// 활동이 계속 이어지는 동안은 패널이 숨겨지지 않는다.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			PanelHideTimerHandle,
			this,
			&ThisClass::HandlePanelInactivityTimeout,
			PanelHideAfterInactivitySeconds,
			false);
	}
}

void UShooterChatLog::HandlePanelInactivityTimeout()
{
	// 엔트리는 지우지 않는다 - 다음 활동이 생기면 남아있던 내역이 그대로 다시 보인다.
	SetVisibility(ESlateVisibility::Collapsed);
}

void UShooterChatLog::RemoveOldestEntry()
{
	if (ActiveEntries.Num() > 0)
	{
		if (UShooterChatEntry* Oldest = ActiveEntries[0])
		{
			Oldest->RemoveFromParent();
		}
		ActiveEntries.RemoveAt(0);
	}

	// 남은 줄이 있으면 만료 예약
	ExpiryTimerHandle.Invalidate();

	if (ActiveEntries.Num() > 0)
	{
		GetWorld()->GetTimerManager().SetTimer(
			ExpiryTimerHandle,
			this,
			&ThisClass::RemoveOldestEntry,
			EntryLifetimeSeconds,
			false);
	}
}
