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
// VerticalBox 엔트리 + 타이머 체인 만료)를 따른다.
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

	// 채팅(송신/수신) 활동이 있을 때마다 패널(배경+내역)을 보이게 하고 숨김 타이머를 다시 건다.
	// 개별 엔트리의 만료(RemoveOldestEntry)와는 별개 - 엔트리는 그대로 남아있고 패널 전체의
	// Visibility만 토글된다. 다시 활동이 생기면 남아있던 내역이 그대로 다시 보인다.
	void ShowPanelAndResetInactivityTimer();

	void HandlePanelInactivityTimeout();

	// false면 엔트리 만료(EntryLifetimeSeconds)와 패널 자동 숨김을 모두 끄고 MaxVisibleEntries만큼
	// 내역을 계속 쌓아둔다. 대기방(WBP_RoomScreen)에 배치한 인스턴스에서 끈다.
	UPROPERTY(EditAnywhere, Category = "Chat")
	bool bAutoHide = true;

	UPROPERTY(EditAnywhere, Category = "Chat", meta = (ClampMin = "1"))
	int32 MaxVisibleEntries = 8;

	UPROPERTY(EditDefaultsOnly, Category = "Chat", meta = (ClampMin = "0.1"))
	float EntryLifetimeSeconds = 8.0f;

	// 채팅 관련 활동(송신/수신)이 이 시간 동안 없으면 배경+내역 패널 전체를 숨긴다.
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

	FTimerHandle ExpiryTimerHandle;
	FTimerHandle PanelHideTimerHandle;
};
