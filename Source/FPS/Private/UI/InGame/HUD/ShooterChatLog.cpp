// ShooterChatLog.cpp
#include "UI/InGame/HUD/ShooterChatLog.h"

#include "Character/ShooterCharacter.h" // 조준 중 투명도
#include "Combat/CombatComponent.h" // 조준 중 투명도
#include "Components/Border.h" // 입력창 상태별 배경
#include "Blueprint/WidgetLayoutLibrary.h" // 화면 폭 비율로 채팅창 폭 계산
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h" // 화면 폭 비율로 채팅창 폭 계산
#include "Components/SizeBoxSlot.h" // 메시지 줄 폭 계산
#include "Components/VerticalBox.h"
#include "Player/ShooterPlayerController.h"
#include "Settings/ShooterGameUserSettings.h" // 채팅 불투명도/크기 옵션
#include "UI/InGame/HUD/ShooterChatEntry.h"
#include "UI/InGame/HUD/ShooterChatInputBox.h" // 입력창을 채팅 로그 아래에 맞춤

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
	PlayerController->OnChatScrollRequested.AddUObject(this, &ThisClass::HandleChatScrollRequested);

	if (ScrollBox_Entries)
	{
		ScrollBox_Entries->OnUserScrolled.AddUniqueDynamic(this, &ThisClass::HandleEntriesUserScrolled);
	}

	// 해상도 비율로 폭을 맞출 SizeBox(WBP: SizeBox > Border > ScrollBox_Entries)
	for (UWidget* Parent = ScrollBox_Entries ? ScrollBox_Entries->GetParent() : nullptr; Parent && Parent != this; Parent = Parent->GetParent())
	{
		if (USizeBox* SizeBox = Cast<USizeBox>(Parent))
		{
			WidthSizeBox = SizeBox;
			break;
		}
	}

	// 내역을 감싼 배경 Border를 찾아 원래 색을 기억한다(WBP: SizeBox > Border > ScrollBox_Entries).
	for (UWidget* Parent = ScrollBox_Entries ? ScrollBox_Entries->GetParent() : nullptr; Parent && Parent != this; Parent = Parent->GetParent())
	{
		if (UBorder* Border = Cast<UBorder>(Parent))
		{
			BackgroundBorder = Border;
			BackgroundOpenColor = Border->GetBrushColor();
			break;
		}
	}
	ApplyBackgroundForInputState();


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
		PlayerController->OnChatScrollRequested.RemoveAll(this); // 채팅 스크롤
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
	EntryReceivedTimes.Reset(); // 메시지별 페이드

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
	EntryWidget->ApplyFontScale(AppliedFontScale); // 채팅 크기 옵션
	EntryWidget->SetRowWidth(GetEntryRowWidth()); // 메시지 줄바꿈 폭을 채팅창 폭 기준으로 직접 지정
	Box_Entries->AddChildToVerticalBox(EntryWidget);
	ActiveEntries.Add(EntryWidget);
	// 메시지별 페이드 기준 시각. 일시정지/슬로모션과 무관하게 실시간을 쓴다.
	EntryReceivedTimes.Add(GetWorld() ? GetWorld()->GetRealTimeSeconds() : 0.0);

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
		// 채팅 불투명도 옵션이 "끄기"면 입력창을 열었을 때만 보인다(내역은 그대로 쌓인다).
		const UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get();
		if (bChatInputOpen || !IsValid(Settings) || Settings->GetChatOpacity() > 0.0f)
		{
			ShowPanelAndResetInactivityTimer();
		}
	}
}

void UShooterChatLog::HandleChatInputOpenChanged(const bool bIsOpen)
{
	bChatInputOpen = bIsOpen;

	if (!bAutoHide)
	{
		return;
	}

	// 배경판은 입력창이 열려 있을 때만 보인다.
	ApplyBackgroundForInputState();

	// 입력창을 닫으면 위쪽 내역을 보던 중이어도 최신 메시지로 돌아간다
	if (!bIsOpen && ScrollBox_Entries)
	{
		bFollowLatest = true;
		LastScrollEndOffset = -1.0f;
		ScrollBox_Entries->ScrollToEnd();
	}

	// 입력창을 채팅 로그 배경 박스 바로 아래에 같은 폭으로 붙인다.
	// 열린 직후에는 입력창이 아직 배치되지 않았으므로 다음 틱부터 맞춘다.
	// 해상도(뷰포트 크기)가 지난번 측정 때와 같으면 캐시한 폭/위치를 바로 넣고 끝낸다.
	// 입력창 위젯은 열 때마다 새로 만들어질 수 있어 값은 매번 넣어야 하지만, 측정은 해상도가 바뀌었을 때만 한다.
	if (bIsOpen && GetWorld())
	{
		const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
		const AShooterPlayerController* PlayerController = BoundPlayerController.Get();
		UShooterChatInputBox* ChatInput = PlayerController ? PlayerController->GetChatInputWidget() : nullptr;
		if (ChatInput && bHasCachedInputAlignment && ViewportSize.Equals(CachedInputAlignViewportSize))
		{
			ChatInput->ApplyAlignment(CachedInputWidth, CachedInputTranslation);
		}
		else
		{
			InputAlignAttempts = 0;
			bInputAlignedOnce = false;
			GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::AlignChatInputBelowLog);
		}
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
		// 마지막 메시지가 완전히 사라지는 시점에 패널도 숨긴다(메시지별 페이드와 동일한 기준).
		// PanelHideAfterInactivitySeconds
		GetPanelHideDelaySeconds(),
		false);
}

// 메시지 한 줄에 쓸 수 있는 폭(로컬 단위). 채팅창 SizeBox 폭에서 배경 Border의 바깥/안쪽 여백과
// 스크롤바 자리를 뺀다. 배치 결과(캐시된 크기)가 아니라 지정된 값으로 계산해 숨겨져 있던 중에도 정확하다.
float UShooterChatLog::GetEntryRowWidth() const
{
	// Modified: 대기방(bAutoHide == false)은 폭을 코드로 지정하지 않고 방 화면 레이아웃이 정한다.
	// WBP 기본 SizeBox 폭으로 계산하면 실제 채팅창 폭과 달라 줄바꿈이 어긋나므로, 실제 배치된 내역 폭을 쓴다.
	if (!bAutoHide && ScrollBox_Entries)
	{
		const float LaidOutWidth = ScrollBox_Entries->GetCachedGeometry().GetLocalSize().X;
		if (LaidOutWidth > 0.0f)
		{
			return FMath::Max(0.0f, LaidOutWidth - ScrollBox_Entries->GetScrollbarThickness().X);
		}
	}

	float Width = AppliedWidthOverride > 0.0f ? AppliedWidthOverride : 0.0f;
	if (Width <= 0.0f && WidthSizeBox.IsValid())
	{
		Width = WidthSizeBox->GetWidthOverride();
	}
	if (Width <= 0.0f)
	{
		// 폭 지정이 없으면 마지막 배치 결과를 쓴다.
		return ScrollBox_Entries ? ScrollBox_Entries->GetCachedGeometry().GetLocalSize().X : 0.0f;
	}

	if (const UBorder* Border = BackgroundBorder.Get())
	{
		if (const USizeBoxSlot* BorderSlot = Cast<USizeBoxSlot>(Border->Slot))
		{
			Width -= BorderSlot->GetPadding().Left + BorderSlot->GetPadding().Right;
		}
		Width -= Border->GetPadding().Left + Border->GetPadding().Right;
	}
	if (ScrollBox_Entries)
	{
		Width -= ScrollBox_Entries->GetScrollbarThickness().X;
	}
	return FMath::Max(0.0f, Width);
}

// Modified: 줄 폭이 바뀌었으면 모든 메시지의 줄바꿈 폭을 다시 계산한다(인게임/대기방 공용).
void UShooterChatLog::UpdateEntryRowWidths()
{
	const float EntryRowWidth = GetEntryRowWidth();
	if (EntryRowWidth > 0.0f && !FMath::IsNearlyEqual(EntryRowWidth, AppliedEntryRowWidth, 0.5f))
	{
		AppliedEntryRowWidth = EntryRowWidth;
		for (UShooterChatEntry* Entry : ActiveEntries)
		{
			if (Entry)
			{
				Entry->SetRowWidth(EntryRowWidth);
			}
		}
	}
}

float UShooterChatLog::GetPanelHideDelaySeconds() const
{
	return EntryVisibleSeconds + EntryFadeSeconds;
}

void UShooterChatLog::ApplyBackgroundForInputState()
{
	UBorder* Border = BackgroundBorder.Get();
	if (!Border)
	{
		return;
	}

	// 대기방처럼 상시 표시하는 인스턴스(bAutoHide == false)는 배경을 그대로 둔다.
	FLinearColor Color = BackgroundOpenColor;
	if (bAutoHide && !bChatInputOpen)
	{
		Color.A = 0.0f;
	}
	Border->SetBrushColor(Color);
}

bool UShooterChatLog::IsOwningPawnAiming() const
{
	const AShooterCharacter* Character = Cast<AShooterCharacter>(GetOwningPlayerPawn());
	const UCombatComponent* Combat = Character ? Character->GetCombatComponent() : nullptr;
	return Combat && Combat->IsAiming();
}

void UShooterChatLog::RefreshAutoHideVisuals()
{
	const UShooterGameUserSettings* Settings = UShooterGameUserSettings::Get();
	const float SettingsScale = IsValid(Settings) ? Settings->GetChatScale() : 1.0f;
	// 채팅 크기 옵션은 글자 크기로 적용해 줄바꿈이 새 크기 기준으로 다시 계산되게 한다.
	if (!FMath::IsNearlyEqual(SettingsScale, AppliedFontScale))
	{
		AppliedFontScale = SettingsScale;
		for (UShooterChatEntry* Entry : ActiveEntries)
		{
			if (Entry)
			{
				Entry->ApplyFontScale(AppliedFontScale);
			}
		}
	}

	// 채팅창 폭을 화면 폭 비율로 맞춘다(DPI 보정 후 값). 엔트리는 AutoWrapText.
	if (ChatWidthViewportFraction > 0.0f)
	{
		if (USizeBox* SizeBox = WidthSizeBox.Get())
		{
			const float ViewportScale = FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), KINDA_SMALL_NUMBER);
			const float DesiredWidth = UWidgetLayoutLibrary::GetViewportSize(this).X / ViewportScale * ChatWidthViewportFraction;
			if (DesiredWidth > 0.0f && !FMath::IsNearlyEqual(DesiredWidth, AppliedWidthOverride, 0.5f))
			{
				AppliedWidthOverride = DesiredWidth;
				SizeBox->SetWidthOverride(DesiredWidth);
			}
		}
	}

	// 채팅창 폭이 바뀌면(해상도/창 크기) 기존 메시지들의 줄바꿈 폭도 새 폭으로 다시 계산한다.
	// Modified: 대기방에서도 쓰도록 함수로 분리
	UpdateEntryRowWidths();

	// 입력창이 열려 있으면 전체 내역을 또렷하게, 닫혀 있으면 옵션 투명도와 조준 중 투명도를 적용한다.
	float PanelOpacity = 1.0f;
	if (!bChatInputOpen)
	{
		PanelOpacity = IsValid(Settings) ? Settings->GetChatOpacity() : 1.0f;
		if (IsOwningPawnAiming())
		{
			PanelOpacity *= AimingOpacity;
		}
	}
	SetRenderOpacity(PanelOpacity);

	const UWorld* World = GetWorld();
	const double Now = World ? World->GetRealTimeSeconds() : 0.0;
	const int32 EntryCount = ActiveEntries.Num();
	for (int32 Index = 0; Index < EntryCount; ++Index)
	{
		UShooterChatEntry* Entry = ActiveEntries[Index];
		if (!Entry)
		{
			continue;
		}

		float EntryOpacity = 1.0f;
		if (!bChatInputOpen)
		{
			// 최신 ClosedVisibleEntries줄만 남기고, 각 줄은 EntryVisibleSeconds 뒤 EntryFadeSeconds 동안 사라진다.
			const bool bWithinLineLimit = EntryCount - 1 - Index < ClosedVisibleEntries;
			const double Age = EntryReceivedTimes.IsValidIndex(Index) ? Now - EntryReceivedTimes[Index] : 0.0;
			const float FadeAlpha = Age <= EntryVisibleSeconds
				? 1.0f
				: (EntryFadeSeconds > 0.0f ? 1.0f - static_cast<float>(Age - EntryVisibleSeconds) / EntryFadeSeconds : 0.0f);
			EntryOpacity = bWithinLineLimit ? FMath::Clamp(FadeAlpha, 0.0f, 1.0f) : 0.0f;
		}

		const ESlateVisibility NewVisibility = EntryOpacity > 0.0f
			? ESlateVisibility::SelfHitTestInvisible
			: ESlateVisibility::Collapsed;
		if (Entry->GetVisibility() != NewVisibility)
		{
			Entry->SetVisibility(NewVisibility);
		}
		Entry->SetRenderOpacity(EntryOpacity);
	}
}

void UShooterChatLog::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// 메시지별 페이드, 줄 수 제한, 설정 투명도·크기, 조준 중 투명도. 대기방(상시 표시)은 그대로 둔다.
	if (bAutoHide)
	{
		RefreshAutoHideVisuals();
	}
	else
	{
		// Modified: 대기방은 자동 숨김을 안 쓰지만 줄바꿈 폭은 실제 채팅창 폭을 따라가야 한다.
		UpdateEntryRowWidths();
	}

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

// 채팅 입력 중 휠(PlayerController::InputKey)·PageUp/PageDown(ShooterChatInputBox)으로 내역을 스크롤한다.
// 맨 아래에 닿으면 다시 최신 메시지를 따라간다.
void UShooterChatLog::HandleChatScrollRequested(const float Steps)
{
	if (!ScrollBox_Entries || !bChatInputOpen)
	{
		return;
	}

	const float EndOffset = ScrollBox_Entries->GetScrollOffsetOfEnd();
	const float NewOffset = FMath::Clamp(ScrollBox_Entries->GetScrollOffset() + Steps * ScrollStepPixels, 0.0f, EndOffset);
	ScrollBox_Entries->SetScrollOffset(NewOffset);
	HandleEntriesUserScrolled(NewOffset);
}

// 입력창을 채팅 로그 아래에 맞춘다. 배치가 아직 안 끝났으면 다음 틱에 재시도하고,
// 숨겨져 있던 채팅 로그의 배치 정보가 한 프레임 늦을 수 있어 성공 후 한 번 더 맞춘다(최대 MaxInputAlignAttempts틱).
void UShooterChatLog::AlignChatInputBelowLog()
{
	constexpr int32 MaxInputAlignAttempts = 10;
	const AShooterPlayerController* PlayerController = BoundPlayerController.Get();
	UShooterChatInputBox* ChatInput = PlayerController ? PlayerController->GetChatInputWidget() : nullptr;
	if (!bChatInputOpen || !ChatInput || !GetWorld())
	{
		return;
	}
	
	// const UWidget* AnchorWidget = WidthSizeBox.IsValid() ? static_cast<const UWidget*>(WidthSizeBox.Get()) : this; // Removed
	const UWidget* AnchorWidget = BackgroundBorder.IsValid()
		? static_cast<const UWidget*>(BackgroundBorder.Get())
		: (WidthSizeBox.IsValid() ? static_cast<const UWidget*>(WidthSizeBox.Get()) : this);

	float Width = 0.0f;
	FVector2D Translation = FVector2D::ZeroVector;
	const bool bAligned = ChatInput->AlignBelowChatLog(AnchorWidget->GetCachedGeometry(), InputBoxGap, Width, Translation);
	const bool bNeedsAnotherPass = !bAligned || !bInputAlignedOnce;

	// 두 번째 확인 패스까지 끝난 값을 현재 해상도 기준으로 캐시한다(다음부터는 측정 없이 바로 적용).
	if (bAligned && bInputAlignedOnce)
	{
		bHasCachedInputAlignment = true;
		CachedInputAlignViewportSize = UWidgetLayoutLibrary::GetViewportSize(this);
		CachedInputWidth = Width;
		CachedInputTranslation = Translation;
	}
	bInputAlignedOnce |= bAligned;

	if (bNeedsAnotherPass && ++InputAlignAttempts < MaxInputAlignAttempts)
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::AlignChatInputBelowLog);
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
	// 메시지별 페이드 시각도 같은 순서로 제거
	if (EntryReceivedTimes.Num() > 0)
	{
		EntryReceivedTimes.RemoveAt(0, 1, EAllowShrinking::No);
	}
}
