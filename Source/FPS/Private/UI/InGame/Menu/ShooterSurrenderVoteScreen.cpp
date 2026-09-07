#include "UI/InGame/Menu/ShooterSurrenderVoteScreen.h"

#include "Blueprint/WidgetTree.h"
#include "CommonButtonBase.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"

namespace SurrenderVoteUI
{
	static const FLinearColor RowBackgroundA(0.008f, 0.026f, 0.030f, 0.98f);
	static const FLinearColor RowBackgroundB(0.012f, 0.038f, 0.043f, 0.98f);
	static const FLinearColor PrimaryText(0.96f, 0.98f, 0.99f, 1.00f);
	static const FLinearColor PendingText(1.00f, 0.78f, 0.28f, 1.00f);
	static const FLinearColor PendingBadge(0.18f, 0.125f, 0.025f, 0.96f);
	static const FLinearColor ApproveText(0.12f, 1.00f, 0.74f, 1.00f);
	static const FLinearColor ApproveBadge(0.020f, 0.260f, 0.190f, 0.98f);
	static const FLinearColor RejectText(1.00f, 0.38f, 0.38f, 1.00f);
	static const FLinearColor RejectBadge(0.285f, 0.035f, 0.045f, 0.98f);

	static void ConfigureText(UTextBlock* Text, const int32 FontSize, const FLinearColor& Color, const bool bBold)
	{
		if (!IsValid(Text))
		{
			return;
		}

		Text->SetColorAndOpacity(FSlateColor(Color));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = FontSize;
		if (bBold)
		{
			Font.TypefaceFontName = TEXT("Bold");
		}
		Text->SetFont(Font);
	}
}

UShooterSurrenderVoteScreen::UShooterSurrenderVoteScreen()
{
	InputConfig = EShooterWidgetInputMode::GameAndMenu;
}

void UShooterSurrenderVoteScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (UCommonButtonBase* ApproveCommonButton = Cast<UCommonButtonBase>(Btn_Approve))
	{
		ApproveCommonButton->OnClicked().AddUObject(this, &ThisClass::HandleApproveClicked);
	}
	else if (UButton* ApproveButton = Cast<UButton>(Btn_Approve))
	{
		ApproveButton->OnClicked.AddDynamic(this, &ThisClass::HandleApproveClicked);
	}

	if (UCommonButtonBase* RejectCommonButton = Cast<UCommonButtonBase>(Btn_Reject))
	{
		RejectCommonButton->OnClicked().AddUObject(this, &ThisClass::HandleRejectClicked);
	}
	else if (UButton* RejectButton = Cast<UButton>(Btn_Reject))
	{
		RejectButton->OnClicked.AddDynamic(this, &ThisClass::HandleRejectClicked);
	}
}

void UShooterSurrenderVoteScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	RefreshRemainingTime();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			RemainingTimeTimerHandle,
			this,
			&ThisClass::RefreshRemainingTime,
			1.0f,
			true);
	}
}

void UShooterSurrenderVoteScreen::NativeOnDeactivated()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RemainingTimeTimerHandle);
	}
	Super::NativeOnDeactivated();
}

void UShooterSurrenderVoteScreen::SetVoteState(const FSurrenderVoteState& NewVoteState)
{
	const bool bRosterChanged = !HasSameVoterRoster(NewVoteState);
	VoteState = NewVoteState;

	if (bRosterChanged)
	{
		RebuildVoterRows();
	}

	RefreshVoterRows();
	RefreshApprovalSummary();
	RefreshRemainingTime();

	const bool bCanRespond = VoteState.IsActive() && !VoteState.bLocalPlayerHasResponded;
	if (IsValid(Btn_Approve))
	{
		Btn_Approve->SetIsEnabled(bCanRespond);
	}
	if (IsValid(Btn_Reject))
	{
		Btn_Reject->SetIsEnabled(bCanRespond);
	}
}

void UShooterSurrenderVoteScreen::HandleApproveClicked()
{
	OnVoteSubmitted.Broadcast(true);
}

void UShooterSurrenderVoteScreen::HandleRejectClicked()
{
	OnVoteSubmitted.Broadcast(false);
}

bool UShooterSurrenderVoteScreen::NativeOnHandleBackAction()
{
	OnMenuRequested.Broadcast();
	return true;
}

void UShooterSurrenderVoteScreen::RebuildVoterRows()
{
	Panel_Voters->ClearChildren();
	VoterNames.Reset();
	VoterNameTexts.Reset();
	VoterStatusTexts.Reset();
	VoterStatusBadges.Reset();

	if (!WidgetTree)
	{
		return;
	}

	VoterNames.Reserve(VoteState.Voters.Num());
	VoterNameTexts.Reserve(VoteState.Voters.Num());
	VoterStatusTexts.Reserve(VoteState.Voters.Num());
	VoterStatusBadges.Reserve(VoteState.Voters.Num());

	for (int32 Index = 0; Index < VoteState.Voters.Num(); ++Index)
	{
		const FSurrenderVoterState& VoterState = VoteState.Voters[Index];

		UBorder* RowBorder = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			*FString::Printf(TEXT("VoteRow_%d"), Index));
		RowBorder->SetBrushColor(
			Index % 2 == 0
				? SurrenderVoteUI::RowBackgroundA
				: SurrenderVoteUI::RowBackgroundB);
		RowBorder->SetPadding(FMargin(14.0f, 10.0f));

		UPanelSlot* RowPanelSlot = Panel_Voters->AddChild(RowBorder);
		if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(RowPanelSlot))
		{
			VerticalSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
			VerticalSlot->SetHorizontalAlignment(HAlign_Fill);
			VerticalSlot->SetVerticalAlignment(VAlign_Center);
		}

		UHorizontalBox* RowContent = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			*FString::Printf(TEXT("VoteRowContent_%d"), Index));
		RowBorder->SetContent(RowContent);

		UTextBlock* PlayerNameText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			*FString::Printf(TEXT("VotePlayerName_%d"), Index));
		PlayerNameText->SetText(FText::FromString(VoterState.PlayerName));
		SurrenderVoteUI::ConfigureText(PlayerNameText, 14, SurrenderVoteUI::PrimaryText, true);
		if (UHorizontalBoxSlot* NameSlot = Cast<UHorizontalBoxSlot>(RowContent->AddChild(PlayerNameText)))
		{
			FSlateChildSize Fill;
			Fill.SizeRule = ESlateSizeRule::Fill;
			Fill.Value = 1.0f;
			NameSlot->SetSize(Fill);
			NameSlot->SetVerticalAlignment(VAlign_Center);
			NameSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
		}

		UBorder* StatusBadge = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			*FString::Printf(TEXT("VoteStatusBadge_%d"), Index));
		StatusBadge->SetBrushColor(SurrenderVoteUI::PendingBadge);
		StatusBadge->SetPadding(FMargin(16.0f, 6.0f));
		if (UHorizontalBoxSlot* BadgeSlot = Cast<UHorizontalBoxSlot>(RowContent->AddChild(StatusBadge)))
		{
			BadgeSlot->SetVerticalAlignment(VAlign_Center);
		}

		UTextBlock* StatusText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			*FString::Printf(TEXT("VoteStatusText_%d"), Index));
		StatusText->SetText(NSLOCTEXT("ShooterSurrender", "Pending", "대기"));
		StatusText->SetJustification(ETextJustify::Center);
		SurrenderVoteUI::ConfigureText(StatusText, 12, SurrenderVoteUI::PendingText, true);
		StatusBadge->SetContent(StatusText);

		VoterNames.Add(VoterState.PlayerName);
		VoterNameTexts.Add(PlayerNameText);
		VoterStatusTexts.Add(StatusText);
		VoterStatusBadges.Add(StatusBadge);
	}
}

void UShooterSurrenderVoteScreen::RefreshVoterRows()
{
	const int32 RowCount = FMath::Min3(
		VoteState.Voters.Num(),
		VoterStatusTexts.Num(),
		VoterStatusBadges.Num());

	for (int32 Index = 0; Index < RowCount; ++Index)
	{
		const FSurrenderVoterState& VoterState = VoteState.Voters[Index];

		if (VoterNameTexts.IsValidIndex(Index) && IsValid(VoterNameTexts[Index]))
		{
			VoterNameTexts[Index]->SetText(FText::FromString(VoterState.PlayerName));
		}

		UTextBlock* StatusText = VoterStatusTexts[Index];
		UBorder* StatusBadge = VoterStatusBadges[Index];

		switch (VoterState.Choice)
		{
		case ESurrenderVoteChoice::Approve:
			StatusText->SetText(NSLOCTEXT("ShooterSurrender", "Approved", "찬성"));
			StatusText->SetColorAndOpacity(FSlateColor(SurrenderVoteUI::ApproveText));
			StatusBadge->SetBrushColor(SurrenderVoteUI::ApproveBadge);
			break;
		case ESurrenderVoteChoice::Reject:
			StatusText->SetText(NSLOCTEXT("ShooterSurrender", "Rejected", "반대"));
			StatusText->SetColorAndOpacity(FSlateColor(SurrenderVoteUI::RejectText));
			StatusBadge->SetBrushColor(SurrenderVoteUI::RejectBadge);
			break;
		default:
			StatusText->SetText(NSLOCTEXT("ShooterSurrender", "Pending", "대기"));
			StatusText->SetColorAndOpacity(FSlateColor(SurrenderVoteUI::PendingText));
			StatusBadge->SetBrushColor(SurrenderVoteUI::PendingBadge);
			break;
		}
	}
}

void UShooterSurrenderVoteScreen::RefreshApprovalSummary()
{
	Text_ApprovalSummary->SetText(FText::Format(
		NSLOCTEXT("ShooterSurrender", "ApprovalSummary", "찬성 {0} / 필요 {1}"),
		FText::AsNumber(VoteState.GetApprovalCount()),
		FText::AsNumber(VoteState.RequiredApprovals)));
}

void UShooterSurrenderVoteScreen::RefreshRemainingTime()
{
	const UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	const AGameStateBase* GameState = World->GetGameState<AGameStateBase>();
	const float ServerTime = IsValid(GameState)
		? GameState->GetServerWorldTimeSeconds()
		: World->GetTimeSeconds();
	const int32 RemainingSeconds = FMath::Max(
		0,
		FMath::CeilToInt(VoteState.EndServerTime - ServerTime));

	Text_RemainingTime->SetText(FText::Format(
		NSLOCTEXT("ShooterSurrender", "RemainingTime", "남은 시간 {0}초"),
		FText::AsNumber(RemainingSeconds)));
}

bool UShooterSurrenderVoteScreen::HasSameVoterRoster(
	const FSurrenderVoteState& NewVoteState) const
{
	if (VoterNames.Num() != NewVoteState.Voters.Num())
	{
		return false;
	}

	for (int32 Index = 0; Index < VoterNames.Num(); ++Index)
	{
		if (VoterNames[Index] != NewVoteState.Voters[Index].PlayerName)
		{
			return false;
		}
	}

	return true;
}
