// ShooterChatInputBox.cpp
#include "UI/InGame/HUD/ShooterChatInputBox.h"

#include "Blueprint/WidgetTree.h" // Modified: 보이는 바(Border) 찾기
#include "Chat/ShooterChatRouting.h"
#include "Components/Border.h" // Modified: 보이는 바(Border) 기준 정렬
#include "Components/EditableTextBox.h"
#include "Components/Image.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h" // 채팅 로그 폭에 맞춤
#include "Components/SizeBoxSlot.h" // 보이는 박스 슬롯 여백으로 폭 맞춤
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
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
		Text_Message->OnTextChanged.AddUniqueDynamic(this, &ThisClass::HandleMessageTextChanged);
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
		// PageUp/PageDown으로 채팅 내역을 크게 스크롤한다(휠은 PlayerController::InputKey에서 처리).
		if (InKeyEvent.GetKey() == EKeys::PageUp || InKeyEvent.GetKey() == EKeys::PageDown)
		{
			constexpr float PageSteps = 4.0f;
			PlayerController->RequestChatScroll(InKeyEvent.GetKey() == EKeys::PageUp ? -PageSteps : PageSteps);
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

	if (TypedPrefix.IsEmpty() || !Box_Suggestions)
	{
		return;
	}

	// Box_Suggestions를 배치해 놓고 엔트리 클래스를 안 넣은 건 WBP 설정 누락이므로 조용히 넘기지 않는다.
	if (!ensureMsgf(SuggestionEntryWidgetClass, TEXT("[ChatInputBox] %s: SuggestionEntryWidgetClass가 지정되지 않았습니다."), *GetNameSafe(this)))
	{
		return;
	}

	const AGameStateBase* GameState = GetWorld() ? GetWorld()->GetGameState() : nullptr;
	if (!GameState)
	{
		return;
	}

	const FString OwnName = GetOwningPlayerState() ? GetOwningPlayerState()->GetPlayerName() : FString();
	CurrentSuggestions = ShooterChatRouting::GetPlayerNamesMatchingPrefix(GameState, TypedPrefix, OwnName);
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
		if (!ensure(Entry))
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

// 위치·폭은 렌더 변환을 뺀 레이아웃 좌표로 계산해서, 이번에 준 이동값이
// 다음 틱 계산에 다시 섞여 흔들리지 않게 한다.
bool UShooterChatInputBox::AlignBelowChatLog(const FGeometry& ChatLogGeometry, const float Gap, float& OutWidth, FVector2D& OutTranslation)
{
	OutWidth = 0.0f;
	OutTranslation = FVector2D::ZeroVector;

	UWidget* Root = GetRootWidget();
	// 화면별 오프셋(OpenChatInput의 ScreenOffset, 예: 클래스 선택 화면)이 지정된 경우는 그 배치를 따른다.
	if (!Root || !GetRenderTransform().Translation.IsNearlyZero())
	{
		return false; // 맞출 대상이 아님 - 캐시하지 않는다
	}
	
	const UContentWidget* RootContentWidget = Cast<UContentWidget>(Root);
	const UWidget* VisibleBox = RootContentWidget && RootContentWidget->GetContent() ? RootContentWidget->GetContent() : Root;
	const FGeometry& VisibleGeometry = VisibleBox->GetCachedGeometry();

	const FSlateLayoutTransform LogLayout = ChatLogGeometry.GetAccumulatedLayoutTransform();
	// const FSlateLayoutTransform RootLayout = Root->GetCachedGeometry().GetAccumulatedLayoutTransform(); // Removed
	const FSlateLayoutTransform RootLayout = VisibleGeometry.GetAccumulatedLayoutTransform();
	const float RootScale = RootLayout.GetScale();
	const FVector2f LogLocalSize = FVector2f(ChatLogGeometry.GetLocalSize());
	if (RootScale <= KINDA_SMALL_NUMBER || LogLocalSize.IsNearlyZero() || FVector2f(VisibleGeometry.GetLocalSize()).IsNearlyZero())
	{
		return false; // 아직 한 번도 배치되지 않음(열린 직후) - 다음 틱에 다시 시도
	}

	// 폭: 채팅 로그 박스와 같은 실제 폭
	// OutWidth = LogLocalSize.X * LogLayout.GetScale() / RootScale; // Removed
	// Modified: 루트 SizeBox의 WidthOverride는 자식 배치에 쓰이지 않는다(SBox는 자식이 원하는 크기로 왼쪽 정렬 배치).
	// 그래서 보이는 박스 슬롯을 Fill로 두고, 화면 폭 - 원하는 폭만큼 오른쪽 여백을 줘서 폭을 정확히 맞춘다.
	// OutWidth에는 그 오른쪽 여백을 돌려준다(화면 폭이 같으면 그대로 재사용 가능).
	// Modified: 보이는 박스(SizeBox) 안의 실제 어두운 바(Border)는 위/왼쪽에 투명 여백이 있다(진단 로그: 위 15, 왼쪽 6).
	// 그 여백까지 채팅 로그 아래에 붙이면 바가 아래·오른쪽으로 밀리고 폭도 좁아지므로, 위치와 폭은 Border 기준으로 계산한다.
	const UBorder* VisibleBar = nullptr;
	if (WidgetTree)
	{
		TArray<UWidget*> AllWidgets;
		WidgetTree->GetAllWidgets(AllWidgets);
		for (const UWidget* Widget : AllWidgets)
		{
			if (const UBorder* Border = Cast<UBorder>(Widget))
			{
				VisibleBar = Border;
				break;
			}
		}
	}
	const FGeometry& BarGeometry = VisibleBar ? VisibleBar->GetCachedGeometry() : VisibleGeometry;
	const FSlateLayoutTransform BarLayout = BarGeometry.GetAccumulatedLayoutTransform();
	// 보이는 박스 폭 = 원하는 바 폭 + (박스와 바의 폭 차이)
	const float BarExtraWidth = FMath::Max(0.0f, VisibleGeometry.GetLocalSize().X - BarGeometry.GetLocalSize().X);

	const float DesiredWidth = LogLocalSize.X * LogLayout.GetScale() / RootScale + BarExtraWidth;
	const float RootLocalWidth = Root->GetCachedGeometry().GetLocalSize().X;
	const USizeBoxSlot* VisibleSlot = Cast<USizeBoxSlot>(VisibleBox->Slot);
	const float LeftPadding = VisibleSlot ? VisibleSlot->GetPadding().Left : 0.0f;
	OutWidth = FMath::Max(0.0f, RootLocalWidth - LeftPadding - DesiredWidth);

	// 위치: 어두운 바의 왼쪽 위를 채팅 로그 박스 왼쪽 아래(+Gap)에 맞춘다.
	const FVector2f Target = LogLayout.TransformPoint(FVector2f(0.0f, LogLocalSize.Y)) + FVector2f(0.0f, Gap * RootScale);
	// OutTranslation = FVector2D((Target - RootLayout.GetTranslation()) / RootScale); // Removed
	OutTranslation = FVector2D((Target - BarLayout.GetTranslation()) / RootScale);

	ApplyAlignment(OutWidth, OutTranslation);
	return true;
}

void UShooterChatInputBox::ApplyAlignment(const float Width, const FVector2D& Translation)
{
	UWidget* Root = GetRootWidget();
	// 화면별 오프셋이 지정된 경우는 그 배치를 따른다(AlignBelowChatLog와 같은 기준).
	if (!Root || !GetRenderTransform().Translation.IsNearlyZero())
	{
		return;
	}

	// Removed: 루트 SizeBox의 WidthOverride는 보이는 박스 폭에 반영되지 않음
	// if (USizeBox* RootSizeBox = Cast<USizeBox>(Root);
	// 	RootSizeBox && Width > 0.0f && !FMath::IsNearlyEqual(RootSizeBox->GetWidthOverride(), Width, 0.5f))
	// {
	// 	RootSizeBox->SetWidthOverride(Width);
	// }

	//  Width는 보이는 박스의 오른쪽 여백. 슬롯을 Fill로 바꾸고 여백으로 폭을 맞춘다.
	const UContentWidget* RootContentWidget = Cast<UContentWidget>(Root);
	UWidget* VisibleBox = RootContentWidget ? RootContentWidget->GetContent() : nullptr;
	if (USizeBoxSlot* VisibleSlot = VisibleBox ? Cast<USizeBoxSlot>(VisibleBox->Slot) : nullptr)
	{
		VisibleSlot->SetHorizontalAlignment(HAlign_Fill);
		FMargin SlotPadding = VisibleSlot->GetPadding();
		if (!FMath::IsNearlyEqual(SlotPadding.Right, Width, 0.5f))
		{
			SlotPadding.Right = Width;
			VisibleSlot->SetPadding(SlotPadding);
		}
	}
	if (!Root->GetRenderTransform().Translation.Equals(Translation, 0.5f))
	{
		Root->SetRenderTranslation(Translation);
	}
}

void UShooterChatInputBox::FocusMessageBoxDeferred()
{
	if (Text_Message)
	{
		Text_Message->SetKeyboardFocus();
	}
}
