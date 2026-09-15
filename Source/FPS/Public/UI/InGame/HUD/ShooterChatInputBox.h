// ShooterChatInputBox.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Chat/ChatTypes.h"
#include "ShooterChatInputBox.generated.h"

class UEditableTextBox;
class UTextBlock;

// Enter로 열리는 채팅 입력창. 팀/전체 채널 토글(Tab), 전송/취소(Enter/Esc)를 이 위젯이 직접
// 가로챈다 - SEditableText가 Enter/Tab/Esc를 OnKeyDown에서 자체 소비해 Enhanced Input까지
// 버블링되지 않기 때문이다. 반면 일반 문자 키는 OnKeyChar로만 처리되어 OnKeyDown이 그대로
// 버블링되므로, 채팅창이 열려 있어도 캐릭터 이동(WASD)은 자연스럽게 계속 동작한다.
UCLASS()
class FPS_API UShooterChatInputBox : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// 열기: 텍스트를 비우고 지정 채널로 보이게 한 뒤 포커스를 준다. Enter로 열 때 항상 호출된다.
	void OpenForChannel(EChatChannel Channel);

	// Tab 전환용: 입력 중이던 텍스트는 유지한 채 채널 라벨/색상만 갱신한다.
	void SetChannel(EChatChannel Channel);

	// 닫기: 숨기고 텍스트를 비운다. 전송 여부와 무관하게 호출된다.
	void CloseAndClearText();

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UEditableTextBox> Text_Message;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Text_ChannelLabel;

	UPROPERTY(EditDefaultsOnly, Category = "Chat")
	FLinearColor TeamChannelColor = FLinearColor(0.2f, 0.55f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Chat")
	FLinearColor MatchChannelColor = FLinearColor(1.0f, 0.55f, 0.05f);
};
