// ShooterRoomPlayerSlot.cpp
#include "UI/OutGame/Room/ShooterRoomPlayerSlot.h"

#include "UI/Foundation/ShooterButtonBase.h"

void UShooterRoomPlayerSlot::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	Btn_Join->OnClicked().AddUObject(this, &ThisClass::HandleJoinClicked);
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

	switch (Presentation)
	{
	case EShooterRoomSlotPresentation::Snap:
		BP_ApplySlotState(true, View, SlotTeam);
		break;

	case EShooterRoomSlotPresentation::Animate:
		BP_PlayReadyTransition(View.bIsReady);
		break;

	case EShooterRoomSlotPresentation::None:
	default:
		break;
	}
}

void UShooterRoomPlayerSlot::SetEmpty(const EGameTeam InSlotTeam)
{
	SlotTeam = InSlotTeam;

	CachedPlayerId = INDEX_NONE;
	bCachedReady = false;
	bCachedIsHost = false;

	Btn_Join->SetVisibility(ESlateVisibility::Visible);
	BP_ApplySlotState(false, FShooterRoomSlotView(), SlotTeam);
}

void UShooterRoomPlayerSlot::HandleJoinClicked()
{
	OnJoinRequested.Broadcast(SlotTeam);
}
