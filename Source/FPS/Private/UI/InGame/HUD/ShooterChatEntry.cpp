// ShooterChatEntry.cpp
#include "UI/InGame/HUD/ShooterChatEntry.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

void UShooterChatEntry::SetData(const FChatMessage& InMessage)
{
	FLinearColor ChannelColor = MatchChannelColor;
	if (InMessage.Channel == EChatChannel::Team)
	{
		ChannelColor = TeamChannelColor;
	}
	else if (InMessage.Channel == EChatChannel::Whisper)
	{
		ChannelColor = WhisperChannelColor;
	}

	if (Text_SenderName)
	{
		// 오버워치 채팅처럼 닉네임을 "[이름]:" 형태로 감싸서 보여준다.
		Text_SenderName->SetText(FText::FromString(FString::Printf(TEXT("[%s]:"), *InMessage.SenderName)));
		Text_SenderName->SetColorAndOpacity(FSlateColor(ChannelColor));
	}

	if (Text_Message)
	{
		// 기본(단어 단위) 줄바꿈은 스페이스 없는 긴 영단어를 못 끊어 박스 밖으로 넘친다.
		// 한글은 음절 단위로 항상 끊기므로 문제없지만, 영어도 동일하게 보이도록 글자 단위 줄바꿈을 강제한다.
		Text_Message->SetWrappingPolicy(ETextWrappingPolicy::AllowPerCharacterWrapping);
		Text_Message->SetText(FText::FromString(InMessage.MessageText));
	}

	if (Image_ChannelIcon)
	{
		UTexture2D* IconTexture = MatchIconTexture;
		if (InMessage.Channel == EChatChannel::Team)
		{
			IconTexture = TeamIconTexture;
		}
		else if (InMessage.Channel == EChatChannel::Whisper)
		{
			IconTexture = WhisperIconTexture;
		}

		if (IconTexture)
		{
			Image_ChannelIcon->SetBrushFromTexture(IconTexture);
		}
	}

	OnChannelApplied(InMessage.Channel);
}
