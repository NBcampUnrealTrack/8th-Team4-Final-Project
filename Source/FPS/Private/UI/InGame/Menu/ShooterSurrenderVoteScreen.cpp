#include "UI/InGame/Menu/ShooterSurrenderVoteScreen.h"

#include "CommonButtonBase.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
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

	Btn_Approve->OnClicked().AddUObject(this, &ThisClass::HandleApproveClicked);
	Btn_Reject->OnClicked().AddUObject(this, &ThisClass::HandleRejectClicked);
}

void UShooterSurrenderVoteScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	RefreshRemainingTime();
	GetWorld()->GetTimerManager().SetTimer(
		RemainingTimeTimerHandle,
		this,
		&ThisClass::RefreshRemainingTime,
		1.0f,
		true);
}

void UShooterSurrenderVoteScreen::NativeOnDeactivated()
{
	GetWorld()->GetTimerManager().ClearTimer(RemainingTimeTimerHandle);
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
	Btn_Approve->SetIsEnabled(bCanRespond);
	Btn_Reject->SetIsEnabled(bCanRespond);
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
	VoterRows.Reset();
	VoterNames.Reset();
	if (!ensure(VoterRowClass))
	{
		return;
	}

	APlayerController* PlayerController = GetOwningPlayer();
	for (const FSurrenderVoterState& VoterState : VoteState.Voters)
	{
		UShooterSurrenderVoterRow* Row = CreateWidget<UShooterSurrenderVoterRow>(
			PlayerController,
			VoterRowClass);
		if (!ensure(Row))
		{
			return;
		}
		Panel_Voters->AddChild(Row);
		VoterRows.Add(Row);
		VoterNames.Add(VoterState.PlayerName);
	}
}

void UShooterSurrenderVoteScreen::RefreshVoterRows()
{
	for (int32 Index = 0; Index < VoterRows.Num(); ++Index)
	{
		VoterRows[Index]->SetVoterState(VoteState.Voters[Index]);
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
	const AGameStateBase* GameState = GetWorld()->GetGameState<AGameStateBase>();
	const float ServerTime = IsValid(GameState)
		? GameState->GetServerWorldTimeSeconds()
		: GetWorld()->GetTimeSeconds();
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
