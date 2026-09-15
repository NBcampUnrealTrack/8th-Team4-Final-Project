// DominationPlayerSlot.cpp
#include "UI/InGame/Domination/DominationPlayerSlot.h"

#include "CommonTextBlock.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "Player/DominationPlayerState.h"

void UDominationPlayerSlot::SetPlayerState(
	ADominationPlayerState* InPlayerState,
	const bool bInIsLocalPlayer)
{
	BoundPlayerState = InPlayerState;
	bIsLocalPlayer = bInIsLocalPlayer;
	bForcedDisconnected = false;

	// 슬롯이 다른 플레이어에게 재할당될 수 있음
	LastPresentedState = EDominationSlotState::Disconnected;

	RefreshFromPlayerState();
}

void UDominationPlayerSlot::SetIconTexture(
	UTexture2D* IconTexture,
	UTexture2D* PortraitTexture)
{
	if (IsValid(Image_WeaponArt))
	{
		if (IsValid(IconTexture))
		{
			Image_WeaponArt->SetBrushFromTexture(IconTexture, true);
			Image_WeaponArt->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Image_WeaponArt->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (IsValid(Image_Portrait))
	{
		if (IsValid(PortraitTexture))
		{
			Image_Portrait->SetBrushFromTexture(PortraitTexture, true);
			Image_Portrait->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			Image_Portrait->SetVisibility(ESlateVisibility::Hidden);
		}
	}
}

void UDominationPlayerSlot::NotifyDisconnected()
{
	// PlayerState 객체가 아직 살아 있어도 PlayerArray에서 빠졌으면 이탈로 본다.
	bForcedDisconnected = true;

	RefreshFromPlayerState();
}

void UDominationPlayerSlot::RefreshFromPlayerState()
{
	const ADominationPlayerState* PlayerState = BoundPlayerState.Get();

	EDominationSlotState State = EDominationSlotState::Disconnected;
	if (IsValid(PlayerState) && !bForcedDisconnected)
	{
		CachedPlayerName = PlayerState->GetPlayerName();
		CachedPlayerId = PlayerState->GetPlayerId();

		if (PlayerState->HasSelectedCombatClass())
		{
			CachedDisplayClass = PlayerState->GetCombatClass();
			State = EDominationSlotState::Locked;
		}
		else if (PlayerState->HasPreviewCombatClass())
		{
			CachedDisplayClass = PlayerState->GetPreviewCombatClass();
			State = EDominationSlotState::Selecting;
		}
		else
		{
			CachedDisplayClass = EDominationCombatClass::Unselected;
			State = EDominationSlotState::Waiting;
		}
	}

	if (IsValid(Text_PlayerName))
	{
		Text_PlayerName->SetText(FText::FromString(CachedPlayerName));
	}

	RefreshClassNameText();

	BP_UpdateSlotPresentation(State, bIsLocalPlayer);

	if (State != LastPresentedState)
	{
		const EDominationSlotState OldState = LastPresentedState;
		LastPresentedState = State;

		BP_OnSlotStateChanged(State, OldState, bIsLocalPlayer);
	}
}

void UDominationPlayerSlot::RefreshClassNameText()
{
	if (!IsValid(Text_ClassName))
	{
		return;
	}

	const UDominationClassSettings* ClassSettings = GetDefault<UDominationClassSettings>();
	const FDominationCombatClassDefinition* Definition =
		(IsValid(ClassSettings) && CachedDisplayClass != EDominationCombatClass::Unselected)
			? ClassSettings->FindDefinition(CachedDisplayClass)
			: nullptr;

	const bool bHasClassName = Definition != nullptr && !Definition->DisplayName.IsEmpty();
	if (bHasClassName)
	{
		Text_ClassName->SetText(Definition->DisplayName);
	}

	Text_ClassName->SetVisibility(bHasClassName
		? ESlateVisibility::HitTestInvisible
		: ESlateVisibility::Collapsed);
}

EDominationCombatClass UDominationPlayerSlot::GetDisplayClass() const
{
	return CachedDisplayClass;
}

int32 UDominationPlayerSlot::GetCachedPlayerId() const
{
	return CachedPlayerId;
}
