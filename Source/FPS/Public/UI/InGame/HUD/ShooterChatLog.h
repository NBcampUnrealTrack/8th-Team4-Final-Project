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
	UPROPERTY(EditAnywhere, Category = "Chat", meta = (ClampMin = "1"))
	int32 MaxVisibleEntries = 8;

	// 채팅 관련 활동(송신/수신/입력창 닫힘)이 이 시간 동안 없으면 배경+내역 패널 전체를 숨긴다.
	UPROPERTY(EditDefaultsOnly, Category = "Chat", meta = (ClampMin = "0.1"))
	float PanelHideAfterInactivitySeconds = 4.0f;

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
};
