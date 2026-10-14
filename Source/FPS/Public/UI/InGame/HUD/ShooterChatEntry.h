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

	// 채팅 크기 옵션. WBP 글자 크기에 Scale을 곱해 적용
	void ApplyFontScale(float Scale);

	// 한 줄에 쓸 수 있는 폭(로컬 단위)을 받아 메시지 줄바꿈 폭을 직접 계산한다
	void SetRowWidth(float InRowWidth);

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

	// 신규 글자 스타일
	UPROPERTY(EditDefaultsOnly, Category = "Chat|Style")
	FLinearColor MessageTextColor = FLinearColor(1.0f, 1.0f, 1.0f, 0.9f);

	UPROPERTY(EditDefaultsOnly, Category = "Chat|Style")
	FVector2D TextShadowOffset = FVector2D(1.0f, 1.0f);

	UPROPERTY(EditDefaultsOnly, Category = "Chat|Style")
	FLinearColor TextShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f);

	// 채널 아이콘을 줄 높이 가운데에서 위아래로 미세 조정하는 값(px, 음수면 위로). 글자 크기 배율이 곱해진다.
	UPROPERTY(EditDefaultsOnly, Category = "Chat|Style")
	float IconVerticalNudge = 0.0f;

private:
	// ApplyFontScale 기준값(WBP 원래 글자 크기)과 마지막으로 적용한 배율
	int32 BaseSenderFontSize = 0;
	int32 BaseMessageFontSize = 0;
	float AppliedFontScale = 1.0f;

	// 채널 아이콘을 첫 줄 글자 높이에 맞춘다(크기 배율 포함). BaseIconSize는 WBP 원래 아이콘 크기.
	void RefreshChannelIconLayout();
	FVector2D BaseIconSize = FVector2D::ZeroVector;

	// 채팅 로그가 넘겨준 한 줄 폭과, 그 기준으로 메시지 줄바꿈 폭을 다시 계산하는 함수
	void RefreshMessageWrap();
	float RowWidth = 0.0f;
};
