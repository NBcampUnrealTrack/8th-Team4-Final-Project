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

	switch (Presentation)
	{
	case EShooterRoomSlotPresentation::Snap:
		BP_ApplySlotState(true, View, SlotTeam);
		BP_ApplyReadyColor(View.bIsReady);
		break;

	case EShooterRoomSlotPresentation::Animate:
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
	BP_ApplySlotState(false, FShooterRoomSlotView(), SlotTeam);
	BP_ApplyReadyColor(false);
}

void UShooterRoomPlayerSlot::HandleJoinClicked()
{
	OnJoinRequested.Broadcast(SlotTeam);
}
