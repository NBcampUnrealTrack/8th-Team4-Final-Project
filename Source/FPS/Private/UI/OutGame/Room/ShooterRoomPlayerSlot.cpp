// ShooterRoomPlayerSlot.cpp
#include "UI/OutGame/Room/ShooterRoomPlayerSlot.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "UI/Foundation/ShooterButtonBase.h"

void UShooterRoomPlayerSlot::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Btn_Join->OnClicked().AddUObject(this, &ThisClass::HandleJoinClicked);
	DefaultProfileBrush = Img_Profile->GetBrush();
	SetTalking(false);
}

void UShooterRoomPlayerSlot::SetPlayer(const FShooterRoomSlotView& View)
{
	const EShooterRoomSlotPresentation Presentation =
		FShooterRoomRules::DecideReadyPresentation(CachedPlayerId, bCachedReady, bCachedIsHost, View);

	SlotTeam = View.Team;
	CachedPlayerId = View.PlayerId;
	bCachedReady = View.bIsReady;
	bCachedIsHost = View.bIsHost;

	Btn_Join->SetVisibility(ESlateVisibility::Collapsed);
	if (IsValid(Panel_Join))
	{
		Panel_Join->SetVisibility(ESlateVisibility::Collapsed);
	}

	// 연출 판정과 무관하게 매번 덮어씀
	Img_Profile->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (View.ProfileImage)
	{
		Img_Profile->SetBrushFromTexture(View.ProfileImage);
	}
	else
	{
		Img_Profile->SetBrush(DefaultProfileBrush);
	}

	const EShooterRoomSlotState State = View.bIsReady
		? EShooterRoomSlotState::Ready
		: EShooterRoomSlotState::Waiting;

	switch (Presentation)
	{
	case EShooterRoomSlotPresentation::Snap:
		StopReadyTransition();
		ApplyStateBackground(State);
		BP_ApplySlotState(true, View, SlotTeam);
		BP_ApplyReadyColor(View.bIsReady);
		break;

	case EShooterRoomSlotPresentation::Animate:
		StopReadyTransition();
		ApplyStateBackground(State);
		BP_ApplyReadyColor(View.bIsReady);
		BP_PlayReadyTransition(View.bIsReady);
		break;

	case EShooterRoomSlotPresentation::None:
	default:
		break;
	}
}

void UShooterRoomPlayerSlot::SetEmpty(const EGameTeam InSlotTeam, const bool bJoinable)
{
	SlotTeam = InSlotTeam;

	CachedPlayerId = INDEX_NONE;
	bCachedReady = false;
	bCachedIsHost = false;

	Btn_Join->SetVisibility(ESlateVisibility::Visible);
	Btn_Join->SetIsEnabled(bJoinable);
	if (IsValid(Panel_Join))
	{
		Panel_Join->SetVisibility(ESlateVisibility::Visible);
		Panel_Join->SetIsEnabled(bJoinable);
	}
	Img_Profile->SetVisibility(ESlateVisibility::Collapsed);
	SetTalking(false);
	StopReadyTransition();
	ApplyStateBackground(EShooterRoomSlotState::Empty);
	BP_ApplySlotState(false, FShooterRoomSlotView(), SlotTeam);
	BP_ApplyReadyColor(false);
}

void UShooterRoomPlayerSlot::SetTalking(const bool bIsTalking)
{
	if (IsValid(Img_VoiceTalking))
	{
		Img_VoiceTalking->SetVisibility(bIsTalking ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UShooterRoomPlayerSlot::HandleJoinClicked()
{
	OnJoinRequested.Broadcast(SlotTeam);
}

void UShooterRoomPlayerSlot::ApplyStateBackground(const EShooterRoomSlotState State)
{
	const auto SetShown = [](UWidget* Widget, const bool bShown)
	{
		if (IsValid(Widget))
		{
			Widget->SetVisibility(bShown ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
		}
	};

	const bool bHasPlayer = State != EShooterRoomSlotState::Empty;
	const bool bIsReady = State == EShooterRoomSlotState::Ready;
	const bool bTeamA = bHasPlayer && SlotTeam == EGameTeam::TeamA;
	const bool bTeamB = bHasPlayer && SlotTeam == EGameTeam::TeamB;

	// 팀 배경이 기본 레이어, 레디 배경은 그 위 상태 레이어
	SetShown(Image_Background_TeamA, bTeamA);
	SetShown(Image_Background_TeamB, bTeamB);
	SetShown(Image_BG_Ready_A, bTeamA && bIsReady);
	SetShown(Image_BG_Ready_B, bTeamB && bIsReady);
}

void UShooterRoomPlayerSlot::StopReadyTransition()
{
	// 슬롯 애니가 레디 연출뿐이라 전부 멈춤
	StopAllAnimations();
}
