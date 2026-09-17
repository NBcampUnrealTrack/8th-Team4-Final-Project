// ShooterChatInputBox.cpp
#include "UI/InGame/HUD/ShooterChatInputBox.h"

#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameMode/State/BaseTeamGameState.h"
#include "Player/ShooterPlayerController.h"
#include "TimerManager.h"
#include "UI/InGame/HUD/ShooterChatSuggestionEntry.h"

UShooterChatInputBox::UShooterChatInputBox()
{
	InputConfig = EShooterWidgetInputMode::GameAndMenu;
}

void UShooterChatInputBox::NativeConstruct()
{
	Super::NativeConstruct();

	if (Text_Message)
	{
		Text_Message->OnTextChanged.AddDynamic(this, &ThisClass::HandleMessageTextChanged);
	}
}

UWidget* UShooterChatInputBox::NativeGetDesiredFocusTarget() const
{
	return Text_Message;
}

FReply UShooterChatInputBox::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	AShooterPlayerController* PlayerController = GetOwningPlayer<AShooterPlayerController>();
	if (PlayerController && Text_Message)
	{
		if (InKeyEvent.GetKey() == EKeys::Enter)
		{
			// 추천 목록이 떠 있으면 Enter는 전송이 아니라 하이라이트된 이름 확정(오버워치 친구
			// 검색과 동일한 방향키+Enter 방식). 안 떠 있으면 평소처럼 메시지 전송.
			if (CurrentSuggestions.IsValidIndex(HighlightedSuggestionIndex))
			{
				ApplySuggestedName(CurrentSuggestions[HighlightedSuggestionIndex]);
			}
			else
			{
				PlayerController->HandleChatSubmit(Text_Message->GetText().ToString());
			}
			return FReply::Handled();
		}
		if (InKeyEvent.GetKey() == EKeys::Tab)
		{
			PlayerController->HandleChatChannelSwitchRequested();
			return FReply::Handled();
		}
		if ((InKeyEvent.GetKey() == EKeys::Up || InKeyEvent.GetKey() == EKeys::Down) && !CurrentSuggestions.IsEmpty())
		{
			MoveSuggestionHighlight(InKeyEvent.GetKey() == EKeys::Up ? -1 : 1);
			return FReply::Handled();
		}
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			if (!CurrentSuggestions.IsEmpty())
			{
				ClearSuggestions();
				return FReply::Handled();
			}
			PlayerController->HandleChatCancel();
			return FReply::Handled();
		}
	}

	return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

void UShooterChatInputBox::SetChannel(const EChatChannel Channel, const FString& WhisperTargetName)
{
	FLinearColor ChannelColor = MatchChannelColor;
	FString LabelText = TEXT("ALL");

	switch (Channel)
	{
	case EChatChannel::Team:
		LabelText = TEXT("TEAM");
		ChannelColor = TeamChannelColor;
		break;
	case EChatChannel::Match:
		LabelText = TEXT("ALL");
		ChannelColor = MatchChannelColor;
		break;
	case EChatChannel::Whisper:
		LabelText = WhisperTargetName;
		ChannelColor = WhisperChannelColor;
		break;
	}

	if (Text_ChannelLabel)
	{
		// 채팅 내역의 "[이름]:" 표기와 통일감을 주기 위해 채널 라벨에도 콜론을 붙인다.
		Text_ChannelLabel->SetText(FText::FromString(FString::Printf(TEXT("%s:"), *LabelText)));
		Text_ChannelLabel->SetColorAndOpacity(FSlateColor(ChannelColor));
	}

	if (Image_ChannelIndicator)
	{
		UTexture2D* IconTexture = MatchIconTexture;
		if (Channel == EChatChannel::Team)
		{
			IconTexture = TeamIconTexture;
		}
		else if (Channel == EChatChannel::Whisper)
		{
			IconTexture = WhisperIconTexture;
		}

		if (IconTexture)
		{
			Image_ChannelIndicator->SetBrushFromTexture(IconTexture);
		}
	}
}

void UShooterChatInputBox::ResetForOpen(const EChatChannel Channel, const FString& WhisperTargetName)
{
	if (Text_Message)
	{
		Text_Message->SetText(FText::GetEmpty());
	}

	ClearSuggestions();
	SetChannel(Channel, WhisperTargetName);
}

void UShooterChatInputBox::HandleMessageTextChanged(const FText& NewText)
{
	const FString Text = NewText.ToString();
	FString Rest;
	if (!ShooterChatLimits::StripWhisperCommandPrefix(Text, Rest))
	{
		ClearSuggestions();
		return;
	}

	FString Name, Message;
	if (Rest.Split(TEXT(" "), &Name, &Message))
	{
		// 이름 뒤에 공백까지 쳤다 - 이름은 완성된 걸로 보고 후보는 닫은 뒤 채널 라벨을
		// 미리 이 사람으로 보여준다(실제 전송/큐 등록은 HandleChatSubmit에서).
		ClearSuggestions();
		if (!Name.IsEmpty())
		{
			SetChannel(EChatChannel::Whisper, Name);
		}
		return;
	}

	RefreshWhisperSuggestions(Rest);
}

void UShooterChatInputBox::RefreshWhisperSuggestions(const FString& TypedPrefix)
{
	ClearSuggestions();

	if (TypedPrefix.IsEmpty() || !Box_Suggestions || !SuggestionEntryWidgetClass)
	{
		return;
	}

	const ABaseTeamGameState* TeamGameState = GetWorld() ? GetWorld()->GetGameState<ABaseTeamGameState>() : nullptr;
	if (!TeamGameState)
	{
		return;
	}

	const FString OwnName = GetOwningPlayerState() ? GetOwningPlayerState()->GetPlayerName() : FString();
	CurrentSuggestions = TeamGameState->GetPlayerNamesMatchingPrefix(TypedPrefix, OwnName);
	if (CurrentSuggestions.IsEmpty())
	{
		return;
	}

	HighlightedSuggestionIndex = 0;
	SuggestionWidgets.SetNum(CurrentSuggestions.Num());

	// Box_Suggestions는 입력창 바로 위에서 위로 자라나는 배치를 전제로 한다. 추가한 순서대로
	// 위->아래로 쌓이므로, 가장 좋은 후보(인덱스 0)가 입력창에 가장 가까운 맨 아래에 오도록
	// 역순으로 추가한다. 후보 수 제한이 없어서 ScrollBox 높이를 넘으면 스크롤된다.
	for (int32 Index = CurrentSuggestions.Num() - 1; Index >= 0; --Index)
	{
		UShooterChatSuggestionEntry* Entry = CreateWidget<UShooterChatSuggestionEntry>(this, SuggestionEntryWidgetClass);
		if (!Entry)
		{
			continue;
		}

		Entry->SetPlayerName(CurrentSuggestions[Index]);
		Entry->SetHighlighted(Index == HighlightedSuggestionIndex);

		Box_Suggestions->AddChild(Entry);
		SuggestionWidgets[Index] = Entry;
	}

	Box_Suggestions->SetVisibility(ESlateVisibility::Visible);
	if (SuggestionWidgets.IsValidIndex(HighlightedSuggestionIndex))
	{
		Box_Suggestions->ScrollWidgetIntoView(SuggestionWidgets[HighlightedSuggestionIndex], false);
	}
}

void UShooterChatInputBox::ClearSuggestions()
{
	for (UShooterChatSuggestionEntry* Entry : SuggestionWidgets)
	{
		if (Entry)
		{
			Entry->RemoveFromParent();
		}
	}

	SuggestionWidgets.Reset();
	CurrentSuggestions.Reset();
	HighlightedSuggestionIndex = INDEX_NONE;

	if (Box_Suggestions)
	{
		Box_Suggestions->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UShooterChatInputBox::MoveSuggestionHighlight(const int32 Delta)
{
	if (SuggestionWidgets.IsEmpty())
	{
		return;
	}

	if (SuggestionWidgets.IsValidIndex(HighlightedSuggestionIndex))
	{
		SuggestionWidgets[HighlightedSuggestionIndex]->SetHighlighted(false);
	}

	HighlightedSuggestionIndex =
		(HighlightedSuggestionIndex + Delta + SuggestionWidgets.Num()) % SuggestionWidgets.Num();
	SuggestionWidgets[HighlightedSuggestionIndex]->SetHighlighted(true);

	if (Box_Suggestions)
	{
		Box_Suggestions->ScrollWidgetIntoView(SuggestionWidgets[HighlightedSuggestionIndex], true);
	}
}

void UShooterChatInputBox::ApplySuggestedName(FString PlayerName)
{
	// "/w 이름"을 텍스트에 남기는 대신 채널을 바로 Whisper로 잠근다 - 입력창은 실제 메시지만
	// 남도록 비워서, 이전에 쓰던 글자가 이어져 파싱이 꼬이는 일이 없게 한다.
	if (AShooterPlayerController* PlayerController = GetOwningPlayer<AShooterPlayerController>())
	{
		PlayerController->RegisterWhisperTarget(PlayerName);
	}

	if (Text_Message)
	{
		// SetText가 OnTextChanged -> HandleMessageTextChanged -> ClearSuggestions()를 동기
		// 호출해 CurrentSuggestions를 Reset()한다. PlayerName은 함수 시작 시 값으로 복사받은
		// 별도 FString이라 이 시점에 원본 배열이 지워져도 안전하다(참조로 받으면 댕글링).
		Text_Message->SetText(FText::GetEmpty());
	}

	ClearSuggestions();
	SetChannel(EChatChannel::Whisper, PlayerName);

	// 포커스 요청을 라벨 텍스트 갱신과 다른 틱으로 분리해 둔다(무해한 방어 코드).
	if (Text_Message && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::FocusMessageBoxDeferred);
	}
}

void UShooterChatInputBox::FocusMessageBoxDeferred()
{
	if (Text_Message)
	{
		Text_Message->SetKeyboardFocus();
	}
}
