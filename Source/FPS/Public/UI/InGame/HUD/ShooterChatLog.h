// ShooterChatLog.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Chat/ChatTypes.h"
#include "ShooterChatLog.generated.h"

class AShooterPlayerController;
class UShooterChatEntry;
class UVerticalBox;
class UScrollBox;

// 화면 좌측 상단에 상시 표시되는 채팅 로그. UShooterKillFeed와 동일한 구조(자가 구독 +
// VerticalBox 엔트리)를 따르되, 엔트리는 시간으로 만료되지 않고 MaxVisibleEntries를 넘을 때만
// 오래된 것부터 지워진다.
// 입력창이 닫혀 있으면 배경 없이 최근 ClosedVisibleEntries줄만 보이고 각 줄은 시간이 지나면
// 페이드된다(지워지지는 않음). 입력창을 열면 배경과 전체 내역이 다시 보인다.
UCLASS()
class FPS_API UShooterChatLog : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

protected:
	virtual void NativeDestruct() override;

	void RemoveOldestEntry();

	UFUNCTION()
	void HandleChatMessageReceived(const FChatMessage& Message);

	// Enter로 입력창이 열려 있는 동안은 아무것도 안 쳐도 내역 패널을 계속 보여주고,
	// 닫히면 그때부터 숨김 타이머를 건다.
	UFUNCTION()
	void HandleChatInputOpenChanged(bool bIsOpen);

	// 채팅(송신/수신) 활동이 있을 때마다 패널(배경+내역)을 보이게 하고 숨김 타이머를 다시 건다.
	// 엔트리는 그대로 남아있고 패널 전체의 Visibility만 토글된다. 다시 활동이 생기면 남아있던
	// 내역이 그대로 다시 보인다. 입력창이 열려 있으면 타이머를 걸지 않는다.
	void ShowPanelAndResetInactivityTimer();

	void HandlePanelInactivityTimeout();

	// 자동 줄바꿈 텍스트는 실제 폭을 알기 전까지 한 줄 높이로 계산되다가 몇 프레임 뒤에야 여러 줄
	// 높이로 늘어난다(숨겨져 있던 인게임 패널은 보이는 순간부터 배치가 시작돼서 더 늦다). 그래서 특정
	// 프레임에 한 번 스크롤하는 대신, 맨 아래를 따라가는 중이면 매 틱 끝 오프셋이 바뀔 때마다 다시 내린다.
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// 사용자가 휠/드래그로 위쪽 내역을 보고 있으면 따라가기를 멈추고, 다시 맨 아래로 내리면 재개한다.
	UFUNCTION()
	void HandleEntriesUserScrolled(float CurrentOffset);

	// false면 패널 자동 숨김을 끄고 상시 표시한다. 대기방(WBP_RoomScreen)에 배치한 인스턴스에서 끈다.
	UPROPERTY(EditAnywhere, Category = "Chat")
	bool bAutoHide = true;

	// 이 개수를 넘으면 가장 오래된 엔트리부터 지운다. 시간으로는 지워지지 않는다.
	// 입력창을 열었을 때 볼 수 있는 내역이라 8 -> 20으로 늘림 (닫혀 있을 때는 ClosedVisibleEntries만 보임)
	UPROPERTY(EditAnywhere, Category = "Chat", meta = (ClampMin = "1"))
	int32 MaxVisibleEntries = 20;

	// 입력창이 닫혀 있을 때 화면에 남기는 최대 줄 수(최신 메시지 기준)
	UPROPERTY(EditAnywhere, Category = "Chat|AutoHide", meta = (ClampMin = "1"))
	int32 ClosedVisibleEntries = 5;

	// 입력창이 닫혀 있을 때 각 메시지가 온전히 보이는 시간. 이후 EntryFadeSeconds 동안 사라진다.
	// 입력창을 다시 열면 지워지지 않은 내역(MaxVisibleEntries)이 모두 다시 보인다.
	UPROPERTY(EditDefaultsOnly, Category = "Chat|AutoHide", meta = (ClampMin = "0.1", Units = "s"))
	float EntryVisibleSeconds = 7.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Chat|AutoHide", meta = (ClampMin = "0.0", Units = "s"))
	float EntryFadeSeconds = 1.0f;

	// 입력창이 닫힌 채 조준(ADS/스코프) 중일 때 채팅 전체에 곱하는 투명도
	UPROPERTY(EditDefaultsOnly, Category = "Chat|AutoHide", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimingOpacity = 0.35f;

	// 채팅창 폭을 화면 폭(DPI 보정 후)의 이 비율로 맞춘다. 0이면 WBP SizeBox 폭을 그대로 쓴다.
	// 메시지는 AutoWrapText라 바뀐 폭에 맞춰 자동 줄바꿈된다.
	UPROPERTY(EditAnywhere, Category = "Chat|Layout", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ChatWidthViewportFraction = 0.28f;

	// 채팅 입력 중 휠 한 칸(PageUp/PageDown은 4칸)당 스크롤 거리
	UPROPERTY(EditDefaultsOnly, Category = "Chat|Layout", meta = (ClampMin = "1.0"))
	float ScrollStepPixels = 40.0f;

	// 채팅 로그 배경 박스와 그 아래 입력창 사이 간격
	UPROPERTY(EditDefaultsOnly, Category = "Chat|Layout", meta = (ClampMin = "0.0"))
	float InputBoxGap = 2.0f;

	UPROPERTY(EditDefaultsOnly, Category = "Chat")
	TSubclassOf<UShooterChatEntry> EntryWidgetClass;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UVerticalBox> Box_Entries;

	// Box_Entries를 감싸는 고정 높이 ScrollBox(있을 때만). 긴 메시지가 여러 줄을 차지해
	// 전체 내역 높이가 검정 배경(패널) 높이를 넘어도, 이 ScrollBox가 넘치는 만큼 클리핑하고
	// 항상 맨 아래(최신 메시지)로 스크롤시켜서 오래된 줄만 위로 밀려 안 보이게 한다.
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> ScrollBox_Entries;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UShooterChatEntry>> ActiveEntries;

	TWeakObjectPtr<AShooterPlayerController> BoundPlayerController;

	// true면 내역 높이가 바뀔 때마다 맨 아래(최신 메시지)로 자동 스크롤한다.
	bool bFollowLatest = true;

	// 입력창이 열려 있는 동안은 패널 숨김 타이머를 걸지 않는다.
	bool bChatInputOpen = false;

	// 마지막으로 맨 아래로 맞췄을 때의 끝 오프셋. 이 값이 바뀌었을 때만 다시 스크롤한다.
	float LastScrollEndOffset = -1.0f;

	FTimerHandle PanelHideTimerHandle;

	// ActiveEntries와 같은 순서로 각 메시지를 받은 시각(실시간 초)
	TArray<double> EntryReceivedTimes;

	// 내역을 감싼 검정 배경(Border). 입력창이 열려 있을 때만 원래 색으로 보인다.
	TWeakObjectPtr<class UBorder> BackgroundBorder;
	FLinearColor BackgroundOpenColor = FLinearColor::Black;

	// 메시지별 페이드/줄 수 제한, 설정 투명도·크기, 조준 중 투명도를 반영한다.
	void RefreshAutoHideVisuals();
	void ApplyBackgroundForInputState();
	float GetPanelHideDelaySeconds() const;
	bool IsOwningPawnAiming() const;

	// 채팅 입력 중 휠/PageUp·PageDown 스크롤
	void HandleChatScrollRequested(float Steps);

	// 입력창을 열 때 채팅 로그 아래에 맞춘다(배치 전이면 다음 틱 재시도)
	void AlignChatInputBelowLog();
	int32 InputAlignAttempts = 0;
	bool bInputAlignedOnce = false;

	// 마지막으로 측정한 입력창 폭/이동값과 그때의 뷰포트 크기. 같으면 측정 없이 재사용한다.
	bool bHasCachedInputAlignment = false;
	FVector2D CachedInputAlignViewportSize = FVector2D::ZeroVector;
	float CachedInputWidth = 0.0f;
	FVector2D CachedInputTranslation = FVector2D::ZeroVector;

	// 화면 폭 비율로 폭을 맞출 SizeBox와 마지막 적용 값, 채팅 크기 옵션 배율
	TWeakObjectPtr<class USizeBox> WidthSizeBox;
	float AppliedWidthOverride = -1.0f;
	float AppliedFontScale = 1.0f;

	// 메시지 한 줄 폭 계산과 마지막으로 엔트리들에 넘긴 값
	float GetEntryRowWidth() const;
	float AppliedEntryRowWidth = -1.0f;
};
