// ShooterChatEntry.h
#pragma once

#include "CoreMinimal.h"
#include "CommonUserWidget.h"
#include "Chat/ChatTypes.h"
#include "ShooterChatEntry.generated.h"

class UCommonTextBlock;
class UImage;
class UTexture2D;

// 채팅 로그 한 줄 위젯. 발신자 이름 + 메시지 텍스트를 표시하고, 채널에 따라 팀(파랑)/전체(주황) 색을 적용한다.
UCLASS()
class FPS_API UShooterChatEntry : public UCommonUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Chat")
	void SetData(const FChatMessage& InMessage);

	// WBP에서 색상 외 추가 연출(테두리, 아이콘 등)을 붙일 수 있도록 킬피드의 RefreshTeamVisual과 같은 훅을 남긴다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Chat")
	void OnChannelApplied(EChatChannel Channel);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_SenderName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UCommonTextBlock> Text_Message;

	// 오버워치처럼 채널을 나타내는 작은 아이콘. WBP에 이 이름으로 Image를 추가만 하면 채널
	// 색으로 자동 틴트되고, 채널 종류에 따라 아래 두 텍스처 중 하나로 자동 교체된다.
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UImage> Image_ChannelIcon;

	// 팀 채팅일 때 쓰는 아이콘
	UPROPERTY(EditDefaultsOnly, Category = "Chat")
	TObjectPtr<UTexture2D> TeamIconTexture;

	// 경기(전체) 채팅일 때 쓰는 아이콘
	UPROPERTY(EditDefaultsOnly, Category = "Chat")
	TObjectPtr<UTexture2D> MatchIconTexture;

	// 귓속말(1:1)일 때 쓰는 아이콘
	UPROPERTY(EditDefaultsOnly, Category = "Chat")
	TObjectPtr<UTexture2D> WhisperIconTexture;

	UPROPERTY(EditDefaultsOnly, Category = "Chat")
	FLinearColor TeamChannelColor = FLinearColor(0.2f, 0.55f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Chat")
	FLinearColor MatchChannelColor = FLinearColor(1.0f, 0.55f, 0.05f);

	UPROPERTY(EditDefaultsOnly, Category = "Chat")
	FLinearColor WhisperChannelColor = FLinearColor(0.75f, 0.3f, 0.9f);
};
