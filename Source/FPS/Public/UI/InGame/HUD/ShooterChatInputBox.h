// ShooterChatInputBox.h
#pragma once

#include "CoreMinimal.h"
#include "UI/Foundation/ShooterActivatableWidget.h"
#include "Chat/ChatTypes.h"
#include "ShooterChatInputBox.generated.h"

class UEditableTextBox;
class UTextBlock;
class UScrollBox;
class UImage;
class UTexture2D;
class UShooterChatSuggestionEntry;

// Enter로 열리는 채팅 입력창. 팀/전체 채널 토글(Tab), 전송/취소(Enter/Esc)를 이 위젯이 직접
// 가로챈다 - SEditableText가 Enter/Tab/Esc를 OnKeyDown에서 자체 소비해 Enhanced Input까지
// 버블링되지 않기 때문이다. 반면 일반 문자 키는 OnKeyChar로만 처리되어 OnKeyDown이 그대로
// 버블링되므로, 채팅창이 열려 있어도 캐릭터 이동(WASD)은 자연스럽게 계속 동작한다.
//
// UCommonUserWidget이 아니라 UShooterActivatableWidget인 이유: CommonUI의 Action Router는
// 레이어(UI.Layer.*)에 Push된 Activatable Widget만 입력 포커스 대상으로 인식한다. HUD에
// 고정 배치된 평범한 UserWidget에서 SetInputMode/SetKeyboardFocus를 직접 불러도 Router가
// HUD 자신의 GetDesiredInputConfig(Game 전용)를 계속 재적용해 포커스를 도로 뺏어간다.
// 그래서 이 위젯은 Enter를 누를 때마다 UTeamMatchUIInitComponent::OpenChat()으로
// UI.Layer.Modal에 Push되고, 닫힐 때 Pop되어 사라지는 방식으로 동작한다.
// (UI.Layer.GameMenu는 스코어보드처럼 포커스를 가져가지 않는 오버레이 전용이라
// Action Router가 리프 노드로 인식하지 않는다 - 실제로 채팅에 써봤다가 포커스가 전혀 안 걸려서
// Modal로 옮겼다.)
UCLASS()
class FPS_API UShooterChatInputBox : public UShooterActivatableWidget
{
	GENERATED_BODY()

public:
	UShooterChatInputBox();

	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// Tab 전환/귓말 수신용. 입력 중이던 텍스트는 유지한 채 채널 라벨/색상만 갱신한다.
	// Channel==Whisper면 WhisperTargetName을 라벨에 표시(보라색).
	void SetChannel(EChatChannel Channel, const FString& WhisperTargetName = FString());

	// Push 직후(Enter로 열 때) 호출. 컨테이너가 위젯을 재사용할 수 있어 이전 텍스트가 남을 수
	// 있으므로 열 때마다 명시적으로 비우고 채널을 지정한다.
	void ResetForOpen(EChatChannel Channel, const FString& WhisperTargetName = FString());

protected:
	virtual void NativeConstruct() override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UFUNCTION()
	void HandleMessageTextChanged(const FText& NewText);

	// "/w " 뒤에 아직 이름을 완성 안 한 상태(공백 없음)일 때 후보를 채워 넣는다.
	void RefreshWhisperSuggestions(const FString& TypedPrefix);
	void ClearSuggestions();
	void MoveSuggestionHighlight(int32 Delta);

	// Enter로 하이라이트된 후보 확정. 텍스트를 "/w 이름 "으로 채우고 후보 목록을 닫는다.
	void ApplySuggestedName(const FString& PlayerName);

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UEditableTextBox> Text_Message;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ChannelLabel;

	// 입력창 전체 네모의 왼쪽에 붙는 채널 표시 아이콘. WBP에서 이 이름으로 Image를 추가하면
	// Text_ChannelLabel과 같은 색으로 자동 틴트되고, 채널 종류에 따라 아이콘도 자동 교체된다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_ChannelIndicator;

	// 팀 채팅일 때 쓰는 아이콘
	UPROPERTY(EditDefaultsOnly, Category = "Chat")
	TObjectPtr<UTexture2D> TeamIconTexture;

	// 경기(전체) 채팅일 때 쓰는 아이콘
	UPROPERTY(EditDefaultsOnly, Category = "Chat")
	TObjectPtr<UTexture2D> MatchIconTexture;

	// 귓속말(1:1)일 때 쓰는 아이콘
	UPROPERTY(EditDefaultsOnly, Category = "Chat")
	TObjectPtr<UTexture2D> WhisperIconTexture;

	// 귓속말 자동완성 후보들이 채워지는 ScrollBox. 평소엔 Collapsed. 후보 수 제한이 없어서
	// 높이를 넘기면 스크롤되는데, 방향키로 하이라이트가 화면 밖으로 나가면
	// MoveSuggestionHighlight가 ScrollWidgetIntoView로 따라가게 만든다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> Box_Suggestions;

	UPROPERTY(EditDefaultsOnly, Category = "Chat")
	TSubclassOf<UShooterChatSuggestionEntry> SuggestionEntryWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Chat")
	FLinearColor TeamChannelColor = FLinearColor(0.2f, 0.55f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Chat")
	FLinearColor MatchChannelColor = FLinearColor(1.0f, 0.55f, 0.05f);

	UPROPERTY(EditDefaultsOnly, Category = "Chat")
	FLinearColor WhisperChannelColor = FLinearColor(0.75f, 0.3f, 0.9f);

private:
	TArray<FString> CurrentSuggestions;
	TArray<TObjectPtr<UShooterChatSuggestionEntry>> SuggestionWidgets;
	int32 HighlightedSuggestionIndex = INDEX_NONE;
};
