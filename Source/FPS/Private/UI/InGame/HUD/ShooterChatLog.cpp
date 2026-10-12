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
	PlayerController->OnChatInputOpenChanged.AddUniqueDynamic(this, &ThisClass::HandleChatInputOpenChanged);

	if (ScrollBox_Entries)
	{
		ScrollBox_Entries->OnUserScrolled.AddUniqueDynamic(this, &ThisClass::HandleEntriesUserScrolled);
	}

	// 활동이 생기기 전까지는 배경+내역을 아예 숨겨둔다. 상시 표시 모드는 WBP에 지정된 Visibility를 그대로 둔다.
	if (bAutoHide)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}

	// 입력창이 이미 열린 상태에서 생성됐으면(HUD 재생성 등) 바로 보여준다.
	if (PlayerController->IsChatInputOpen())
	{
		HandleChatInputOpenChanged(true);
	}
}

void UShooterChatLog::NativeDestruct()
{
	if (AShooterPlayerController* PlayerController = BoundPlayerController.Get())
	{
		PlayerController->OnChatMessageReceived.RemoveDynamic(this, &ThisClass::HandleChatMessageReceived);
		PlayerController->OnChatInputOpenChanged.RemoveDynamic(this, &ThisClass::HandleChatInputOpenChanged);
	}
	BoundPlayerController.Reset();

	if (ScrollBox_Entries)
	{
		ScrollBox_Entries->OnUserScrolled.RemoveDynamic(this, &ThisClass::HandleEntriesUserScrolled);
	}

	// 이 위젯이 건 타이머(패널 숨김)를 정리한다.
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
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
	if (!ensure(EntryWidget))
	{
		return;
	}

	EntryWidget->SetData(Message);
	Box_Entries->AddChildToVerticalBox(EntryWidget);
	ActiveEntries.Add(EntryWidget);

	// 내역은 시간으로 만료되지 않고 개수가 꽉 찼을 때만 오래된 것부터 지운다.
	while (ActiveEntries.Num() > MaxVisibleEntries)
	{
		RemoveOldestEntry();
	}

	// 여러 줄짜리 메시지 때문에 내역 전체 높이가 패널 높이를 넘어도, 항상 최신 메시지가
	// 보이는 맨 아래로 스크롤시킨다 - 넘친 만큼 위쪽(오래된 줄)만 화면 밖으로 밀려난다.
	// 새 메시지는 위쪽 내역을 보던 중이어도 맨 아래로 돌아온다. 줄바꿈으로 늘어나는 높이는 NativeTick이 따라간다.
	if (ScrollBox_Entries)
	{
		bFollowLatest = true;
		LastScrollEndOffset = -1.0f;
		ScrollBox_Entries->ScrollToEnd();
	}

	if (bAutoHide)
	{
		ShowPanelAndResetInactivityTimer();
	}
}

void UShooterChatLog::HandleChatInputOpenChanged(const bool bIsOpen)
{
	bChatInputOpen = bIsOpen;

	if (!bAutoHide)
	{
		return;
	}

	// 열릴 때는 패널을 보이고 숨김 타이머를 끈다. 닫힐 때는 그때부터 카운트다운을 시작한다.
	ShowPanelAndResetInactivityTimer();
}

void UShooterChatLog::ShowPanelAndResetInactivityTimer()
{
	SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// 입력창이 열려 있는 동안은 숨기지 않는다 - 닫힐 때(HandleChatInputOpenChanged) 다시 타이머를 건다.
	if (bChatInputOpen)
	{
		World->GetTimerManager().ClearTimer(PanelHideTimerHandle);
		return;
	}

	// 같은 핸들에 SetTimer를 다시 걸면 기존 예약은 자동으로 취소되고 새로 카운트다운된다 -
	// 활동이 계속 이어지는 동안은 패널이 숨겨지지 않는다.
	World->GetTimerManager().SetTimer(
		PanelHideTimerHandle,
		this,
		&ThisClass::HandlePanelInactivityTimeout,
		PanelHideAfterInactivitySeconds,
		false);
}

void UShooterChatLog::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!ScrollBox_Entries || !bFollowLatest)
	{
		return;
	}

	const float EndOffset = ScrollBox_Entries->GetScrollOffsetOfEnd();
	if (!FMath::IsNearlyEqual(EndOffset, LastScrollEndOffset))
	{
		LastScrollEndOffset = EndOffset;
		ScrollBox_Entries->ScrollToEnd();
	}
}

void UShooterChatLog::HandleEntriesUserScrolled(float CurrentOffset)
{
	// 맨 아래에서 몇 픽셀 이내면 끝에 붙어 있는 것으로 본다.
	constexpr float EndTolerance = 2.0f;
	bFollowLatest = CurrentOffset >= ScrollBox_Entries->GetScrollOffsetOfEnd() - EndTolerance;
	if (bFollowLatest)
	{
		LastScrollEndOffset = -1.0f;
	}
}

void UShooterChatLog::HandlePanelInactivityTimeout()
{
	// 엔트리는 지우지 않는다 - 다음 활동이 생기면 남아있던 내역이 그대로 다시 보인다.
	SetVisibility(ESlateVisibility::Collapsed);
}

void UShooterChatLog::RemoveOldestEntry()
{
	if (ActiveEntries.Num() == 0)
	{
		return;
	}

	if (UShooterChatEntry* Oldest = ActiveEntries[0])
	{
		Oldest->RemoveFromParent();
	}
	// 채팅은 순서가 중요해서 RemoveAtSwap을 쓸 수 없다. 대신 줄이 하나 빠질 때마다 메모리를
	// 재할당하지 않도록 축소를 막는다 - 최대 MaxVisibleEntries개라 원소 이동 비용은 무시할 수준이다.
	ActiveEntries.RemoveAt(0, 1, EAllowShrinking::No);
}
