// ShooterChatEntry.cpp
#include "UI/InGame/HUD/ShooterChatEntry.h"

#include "CommonTextBlock.h"
#include "Components/HorizontalBoxSlot.h" // 채널 아이콘 정렬
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h" // M채널 아이콘 정렬
#include "Framework/Application/SlateApplication.h" // 채널 아이콘 정렬

// 채팅 크기 옵션. 렌더 스케일은 배치 폭을 바꾸지 않아 줄바꿈 폭과 화면 크기가 어긋나므로
// 실제 글자 크기를 바꿔 레이아웃(줄바꿈)이 새 크기 기준으로 다시 계산되게 한다.
void UShooterChatEntry::ApplyFontScale(const float Scale)
{
	if (FMath::IsNearlyEqual(Scale, AppliedFontScale))
	{
		return;
	}
	AppliedFontScale = Scale;

	auto ApplyTo = [Scale](UCommonTextBlock* TextBlock, int32& BaseSize)
	{
		if (!TextBlock)
		{
			return;
		}
		FSlateFontInfo Font = TextBlock->GetFont();
		if (BaseSize <= 0)
		{
			BaseSize = Font.Size; // WBP에 지정된 크기를 기준으로 기억
		}
		Font.Size = FMath::Max(1, FMath::RoundToInt(BaseSize * Scale));
		TextBlock->SetFont(Font);
	};
	ApplyTo(Text_SenderName, BaseSenderFontSize);
	ApplyTo(Text_Message, BaseMessageFontSize);

	RefreshChannelIconLayout(); // 글자 크기가 바뀌면 아이콘 크기/위치도 다시 맞춘다
	RefreshMessageWrap(); // 글자/아이콘 크기가 바뀌면 메시지 줄바꿈 폭도 다시 계산
}

// 채널 아이콘을 첫 줄 글자 높이에 맞춘다.
// - 모든 칸을 위쪽 정렬로 두고, 아이콘은 한 줄 높이 안에서 가운데에 오도록 위 여백을 계산한다
//   (여러 줄 메시지에서도 아이콘이 첫 줄 옆에 붙어 있다).
// - 줄 높이 박스는 아래쪽에 디센더 공간이 있어 글자가 시각적으로 위에 붙으므로 IconVerticalNudge만큼 더 올린다.
// - 채팅 크기 옵션에 맞춰 아이콘 크기도 같은 배율로 바꾼다.
void UShooterChatEntry::RefreshChannelIconLayout()
{
	if (!Image_ChannelIcon || !Text_Message || !FSlateApplication::IsInitialized())
	{
		return;
	}

	for (UWidget* Widget : { static_cast<UWidget*>(Image_ChannelIcon.Get()), static_cast<UWidget*>(Text_SenderName.Get()), static_cast<UWidget*>(Text_Message.Get()) })
	{
		if (UHorizontalBoxSlot* BoxSlot = Widget ? Cast<UHorizontalBoxSlot>(Widget->Slot) : nullptr)
		{
			BoxSlot->SetVerticalAlignment(VAlign_Top);
		}
	}

	if (BaseIconSize.IsZero())
	{
		BaseIconSize = Image_ChannelIcon->GetBrush().ImageSize; // WBP에 지정된 크기를 기준으로 기억
	}
	const FVector2D IconSize = BaseIconSize * AppliedFontScale;
	Image_ChannelIcon->SetDesiredSizeOverride(IconSize);

	const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	// Removed: 줄 상자 높이 기준 가운데 정렬은 이름/메시지 글자 크기가 달라 아이콘·글자가 어긋났다.
	// const float LineHeight = FontMeasure->GetMaxCharacterHeight(Text_Message->GetFont());
	// if (UHorizontalBoxSlot* IconSlot = Cast<UHorizontalBoxSlot>(Image_ChannelIcon->Slot))
	// {
	// 	FMargin IconPadding = IconSlot->GetPadding();
	// 	IconPadding.Top = FMath::Max(0.0f, (LineHeight - IconSize.Y) * 0.5f);
	// 	IconSlot->SetPadding(IconPadding);
	// }
	
	const FSlateFontInfo MessageFont = Text_Message->GetFont();
	const FSlateFontInfo SenderFont = Text_SenderName ? Text_SenderName->GetFont() : MessageFont;
	const float SenderAscent = FontMeasure->GetMaxCharacterHeight(SenderFont) + FontMeasure->GetBaseline(SenderFont);
	const float MessageAscent = FontMeasure->GetMaxCharacterHeight(MessageFont) + FontMeasure->GetBaseline(MessageFont);
	const float BaselineY = FMath::Max(SenderAscent, MessageAscent);

	auto SetTopPadding = [](UWidget* Widget, const float Top)
	{
		if (UHorizontalBoxSlot* BoxSlot = Widget ? Cast<UHorizontalBoxSlot>(Widget->Slot) : nullptr)
		{
			FMargin SlotPadding = BoxSlot->GetPadding();
			SlotPadding.Top = FMath::Max(0.0f, Top);
			BoxSlot->SetPadding(SlotPadding);
		}
	};
	constexpr float GlyphBodyRatio = 0.7f;
	const float GlyphBodyCenterY = BaselineY - FMath::Max(SenderAscent, MessageAscent) * GlyphBodyRatio * 0.5f;

	const float IconTop = GlyphBodyCenterY - IconSize.Y * 0.5f;
	const float TextShift = FMath::Max(0.0f, -IconTop);
	SetTopPadding(Text_SenderName, BaselineY - SenderAscent + TextShift);
	SetTopPadding(Text_Message, BaselineY - MessageAscent + TextShift);
	SetTopPadding(Image_ChannelIcon, IconTop + TextShift);
	Image_ChannelIcon->SetRenderTranslation(FVector2D(0.0f, IconVerticalNudge * AppliedFontScale));
}

// 채팅 로그가 한 줄에 쓸 수 있는 폭(로컬 단위)을 넘겨준다. 폭이 바뀔 때마다 다시 불린다.
void UShooterChatEntry::SetRowWidth(const float InRowWidth)
{
	if (FMath::IsNearlyEqual(RowWidth, InRowWidth, 0.5f))
	{
		return;
	}
	RowWidth = InRowWidth;
	RefreshMessageWrap();
}

// 메시지 줄바꿈 폭 = 줄 폭 - 아이콘 - 이름(실제 글자 폭 측정) - 각 칸 여백.
// 배치 결과에 기대지 않고 폰트로 직접 재므로 컨테이너 종류나 폭 변경 순서와 무관하게 항상 같은 결과가 나온다.
void UShooterChatEntry::RefreshMessageWrap()
{
	if (!Text_Message || RowWidth <= 0.0f || !FSlateApplication::IsInitialized())
	{
		return;
	}

	auto HorizontalPadding = [](const UWidget* Widget)
	{
		const UHorizontalBoxSlot* BoxSlot = Widget ? Cast<UHorizontalBoxSlot>(Widget->Slot) : nullptr;
		return BoxSlot ? BoxSlot->GetPadding().Left + BoxSlot->GetPadding().Right : 0.0f;
	};

	float UsedWidth = HorizontalPadding(Text_Message);
	if (Image_ChannelIcon && Image_ChannelIcon->GetVisibility() != ESlateVisibility::Collapsed)
	{
		const FVector2D IconSize = (BaseIconSize.IsZero() ? Image_ChannelIcon->GetBrush().ImageSize : BaseIconSize) * AppliedFontScale;
		UsedWidth += IconSize.X + HorizontalPadding(Image_ChannelIcon);
	}
	if (Text_SenderName)
	{
		const TSharedRef<FSlateFontMeasure> FontMeasure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		UsedWidth += FontMeasure->Measure(Text_SenderName->GetText(), Text_SenderName->GetFont()).X
			+ FMath::Max(0.0f, static_cast<float>(TextShadowOffset.X))
			+ HorizontalPadding(Text_SenderName);
	}

	// 반올림·그림자 오차로 마지막 글자가 잘리지 않도록 약간 여유를 둔다.
	constexpr float SafetyMargin = 4.0f;
	constexpr float MinWrapWidth = 16.0f;
	Text_Message->SetWrapTextAt(FMath::Max(MinWrapWidth, RowWidth - UsedWidth - SafetyMargin));
}

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
		// 줄바꿈 폭을 고정값(WrapTextAt)이 아니라 실제 배치 폭으로 잡아, 해상도에 따라 채팅창 폭이
		// 바뀌어도 그 폭에 맞춰 자동으로 줄바꿈되게 한다(WBP 설정과 무관하게 보장).
		Text_Message->SetAutoWrapText(false);
		if (UHorizontalBoxSlot* MessageSlot = Cast<UHorizontalBoxSlot>(Text_Message->Slot))
		{
			MessageSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
		if (UHorizontalBoxSlot* SenderSlot = Text_SenderName ? Cast<UHorizontalBoxSlot>(Text_SenderName->Slot) : nullptr)
		{
			SenderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
		}
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

	// 입력창이 닫혀 있을 때는 배경판 없이 글자만 보이므로, 밝은 배경 위에서도 읽히도록
	// 그림자를 깔고 본문은 살짝 낮춘 흰색으로 둬서 이름(채널 색)이 먼저 눈에 들어오게 한다.
	for (UCommonTextBlock* TextBlock : { Text_SenderName.Get(), Text_Message.Get() })
	{
		if (TextBlock)
		{
			TextBlock->SetShadowOffset(TextShadowOffset);
			TextBlock->SetShadowColorAndOpacity(TextShadowColor);
		}
	}
	if (Text_Message)
	{
		Text_Message->SetColorAndOpacity(FSlateColor(MessageTextColor));
	}

	RefreshChannelIconLayout(); // 채널 아이콘을 첫 줄 글자 높이에 맞춤
	RefreshMessageWrap(); // 이름 길이에 맞춰 메시지 줄바꿈 폭 계산

	OnChannelApplied(InMessage.Channel);
}
