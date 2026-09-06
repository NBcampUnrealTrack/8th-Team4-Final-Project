// ShooterAugmentSelectScreen.cpp
#include "UI/InGame/ShooterAugmentSelectScreen.h"

#include "UI/Common/ShooterAugmentCard.h"
#include "UI/Common/ShooterRerollButton.h"
#include "UI/ShooterUIBundles.h"
#include "Augment/AugmentDefinition.h"
#include "Augment/AugmentRegistrySubsystem.h"
#include "Components/HorizontalBox.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/PlayerController.h"
#include "CommonButtonBase.h"
#include "UI/Common/ShooterAugmentSelectBanner.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterAugmentSelectScreen, Log, All);

void UShooterAugmentSelectScreen::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	
	CachedCards = { Card_0, Card_1, Card_2 };
	CachedRerollButtons = { RerollButton_0, RerollButton_1, RerollButton_2};
	
	for (UShooterAugmentCard* Card : CachedCards)
	{
		Card->OnAugmentCardClicked.AddUObject(this, &UShooterAugmentSelectScreen::HandleCardClicked);
	}
	for (UShooterRerollButton* Button : CachedRerollButtons)
	{
		Button->OnRerollButtonClicked.AddUObject(this, &ThisClass::HandleRerollButtonClicked);
	}
	
	if (Btn_PendingCard)
	{
		Btn_PendingCard->OnClicked().AddUObject(this, &ThisClass::HandlePendingCardClicked);
	}
}

void UShooterAugmentSelectScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (Banner)
	{
		Banner->SetPresentationActive(true);
	}
}

void UShooterAugmentSelectScreen::SetOffers(const TArray<FAugmentInventoryEntry>& InOffers,
										    const TArray<int32>& InRerollsRemaining)
{
	// 이전 아이콘 로드가 진행 중이면 취소
	if (IconLoadHandle.IsValid())
	{
		IconLoadHandle->CancelHandle();
		IconLoadHandle.Reset();
	}

	Offers = InOffers;
	RerollsRemaining = InRerollsRemaining;
	
	TryBuildCards();
}

void UShooterAugmentSelectScreen::SetEntryAnimation(EAugmentSelectEntry InEntry)
{
	EntryAnimation = InEntry;
}

void UShooterAugmentSelectScreen::SetDismissAvailable(bool bAvailable)
{
	if (Btn_PendingCard)
	{
		Btn_PendingCard->SetVisibility(bAvailable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

void UShooterAugmentSelectScreen::TryBuildCards()
{
	UAugmentRegistrySubsystem* Registry = GetRegistry();
	if (!IsValid(Registry))
	{
		return;
	}

	if (Registry->IsReady() == false)
	{
		Registry->OnRegistryReady.AddDynamic(this, &ThisClass::HandleRegistryReady);
		return;
	}

	TArray<FPrimaryAssetId> Ids;

	for (const auto& Offer : Offers)
	{
		Ids.Add(Offer.DefinitionId);
	}

	IconLoadHandle = UAssetManager::Get().ChangeBundleStateForPrimaryAssets(Ids, {FShooterUIBundles::Icon}, {});

	FStreamableDelegate OnLoaded = FStreamableDelegate::CreateUObject(this, &ThisClass::HandleIconsLoaded);

	// CallOrRegistry 구조
	if (!IconLoadHandle.IsValid() || IconLoadHandle->HasLoadCompleted())
	{
		FStreamableHandle::ExecuteDelegate(OnLoaded);
	}
	else
	{
		IconLoadHandle->BindCompleteDelegate(OnLoaded);
	}
}

void UShooterAugmentSelectScreen::RefreshOfferWidgets()
{
	UAugmentRegistrySubsystem* Registry = GetRegistry();
	if (!IsValid(Registry))
	{
		return;
	}
	
	const bool bWasEmpty = LastDisplayedOffers.IsEmpty();
	const bool bIsEmpty = Offers.IsEmpty();
	const bool bOfferCountChanged = LastDisplayedOffers.Num() != Offers.Num();
	
	TArray<int32> ChangedSlots;
	
	// Reroll 했을때 처리
	if (!bWasEmpty && !bIsEmpty && !bOfferCountChanged)
	{
		for (int32 i = 0; i < Offers.Num(); ++i)
		{
			if (LastDisplayedOffers[i].DefinitionId != Offers[i].DefinitionId)
			{
				ChangedSlots.Add(i);
			}
		}
	}
	
	for (int32 i = 0; i < CachedCards.Num(); ++i)
	{
		const bool bHasOffer = Offers.IsValidIndex(i);
		CachedCards[i]->SetVisibility(bHasOffer ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (CachedRerollButtons.IsValidIndex(i))
		{
			CachedRerollButtons[i]->SetVisibility(
				bHasOffer ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}

		if (!bHasOffer)
		{
			continue;
		}

		UAugmentDefinition* Def = Registry->FindDefinition(Offers[i].DefinitionId);
		if (IsValid(Def) == false)
		{
			CachedCards[i]->SetVisibility(ESlateVisibility::Collapsed);
			if (CachedRerollButtons.IsValidIndex(i))
			{
				CachedRerollButtons[i]->SetVisibility(ESlateVisibility::Collapsed);
			}
			continue;
		}
		
		CachedCards[i]->SetData(Def, i);
		if (CachedRerollButtons.IsValidIndex(i))
		{
			CachedRerollButtons[i]->SetSlotData(i, 
				RerollsRemaining.IsValidIndex(i) ? RerollsRemaining[i] : 0);
		}
	}
	
	if (!ensureMsgf(!Offers.IsEmpty(), TEXT("[ShooterAugmentSelectScreen] RefreshOfferWidgets가 빈 Offers로 호출되었습니다")))
	{
		return;
	}
	
	// 첫 묶음 상태인지 확인
	const bool bWholeBatchReplaced = !ChangedSlots.IsEmpty() && ChangedSlots.Num() != 1;
	
	if (bWasEmpty || bOfferCountChanged || bWholeBatchReplaced)
	{
		UWidgetAnimation* Anim = 
			(EntryAnimation == EAugmentSelectEntry::Resume && Anim_Resume)
			? Anim_Resume.Get()
			: Anim_Intro.Get();
		
		if (Anim)
		{
			PlayAnimation(Anim);
		}
	}
	else
	{
		for (const int32 ChangedSlot : ChangedSlots)
		{
			if (CachedCards.IsValidIndex(ChangedSlot))
			{
				CachedCards[ChangedSlot]->PlayRerollFeedback();
			}
		}
	}
	
	LastDisplayedOffers = Offers;
}

void UShooterAugmentSelectScreen::HandleRegistryReady()
{
	if (UAugmentRegistrySubsystem* Registry = GetRegistry())
	{
		Registry->OnRegistryReady.RemoveDynamic(this, &ThisClass::HandleRegistryReady);
	}

	TryBuildCards();
}

void UShooterAugmentSelectScreen::HandleIconsLoaded()
{
	RefreshOfferWidgets();
}

void UShooterAugmentSelectScreen::HandleCardClicked(int32 InOfferIndex)
{
	OnAugmentSelected.Broadcast(InOfferIndex);
}

void UShooterAugmentSelectScreen::HandleRerollButtonClicked(int32 InSlotIndex)
{
	OnAugmentRerollRequested.Broadcast(InSlotIndex);
}

void UShooterAugmentSelectScreen::HandlePendingCardClicked()
{
	OnDismissRequested.Broadcast();
}

UAugmentRegistrySubsystem* UShooterAugmentSelectScreen::GetRegistry() const
{
	return GetGameInstance() ? GetGameInstance()->GetSubsystem<UAugmentRegistrySubsystem>() : nullptr;
}

void UShooterAugmentSelectScreen::NativeDestruct()
{
	if (IconLoadHandle.IsValid())
	{
		IconLoadHandle->CancelHandle();
		IconLoadHandle.Reset();
	}

	if (UAugmentRegistrySubsystem* Registry = GetRegistry())
	{
		Registry->OnRegistryReady.RemoveDynamic(this, &ThisClass::HandleRegistryReady);
	}

	Super::NativeDestruct();
}

void UShooterAugmentSelectScreen::NativeOnDeactivated()
{
	if (Banner)
	{
		Banner->SetPresentationActive(false);
	}
	
	if (IconLoadHandle.IsValid())
	{
		IconLoadHandle->CancelHandle();
		IconLoadHandle.Reset();
	}

	LastDisplayedOffers.Reset();

	Super::NativeOnDeactivated();
}
