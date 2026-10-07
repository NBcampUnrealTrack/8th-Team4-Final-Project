#include "UI/InGame/Menu/ShooterSurrenderVoteScreen.h"

#include "CommonButtonBase.h"
#include "Components/Button.h"
#include "Components/PanelSlot.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"
#include "UI/InGame/Menu/ShooterSurrenderVoterRow.h"

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
	VoterRows.Reset();

	if (!ensure(VoterRowClass))
	{
		return;
	}

	VoterNames.Reserve(VoteState.Voters.Num());
	VoterRows.Reserve(VoteState.Voters.Num());

	for (const FSurrenderVoterState& VoterState : VoteState.Voters)
	{
		// 생성에 실패해도 Voters 와 인덱스를 맞추려고 null 도 넣는다
		UShooterSurrenderVoterRow* Row = CreateWidget<UShooterSurrenderVoterRow>(this, VoterRowClass);
		if (UVerticalBoxSlot* VerticalSlot = Cast<UVerticalBoxSlot>(Panel_Voters->AddChild(Row)))
		{
			VerticalSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 6.0f));
		}

		VoterNames.Add(VoterState.PlayerName);
		VoterRows.Add(Row);
	}
}

void UShooterSurrenderVoteScreen::RefreshVoterRows()
{
	const int32 RowCount = FMath::Min(VoteState.Voters.Num(), VoterRows.Num());

	for (int32 Index = 0; Index < RowCount; ++Index)
	{
		if (IsValid(VoterRows[Index]))
		{
			VoterRows[Index]->SetVoterState(VoteState.Voters[Index]);
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
