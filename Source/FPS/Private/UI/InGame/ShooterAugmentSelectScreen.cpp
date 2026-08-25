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
	
	TArray<int32> ChangedSlots;
	
	// Reroll 했을때 처리
	if (!bWasEmpty && !bIsEmpty)
	{
		ensureMsgf(LastDisplayedOffers.Num() == Offers.Num(), 
			TEXT("[ShooterAugmentSelectScreen]LastDisplayedOffers와 Offers의 슬롯 수가 다릅니다. Old = %d, New = %d"),
			LastDisplayedOffers.Num(),
			Offers.Num());
		
		const int32 CompareCount = FMath::Min(LastDisplayedOffers.Num(), Offers.Num());
		
		for (int32 i = 0; i < CompareCount; ++i)
		{
			if (LastDisplayedOffers[i].DefinitionId != Offers[i].DefinitionId || LastDisplayedOffers[i].Level != Offers[i].Level)
			{
				ChangedSlots.Add(i);
			}
		}
	}
	
	for (int32 i = 0; i < Offers.Num(); ++i)
	{
		UAugmentDefinition* Def = Registry->FindDefinition(Offers[i].DefinitionId);
		if (IsValid(Def) == false)
		{
			continue;
		}
		
		if (CachedCards.IsValidIndex(i))
		{
			CachedCards[i]->SetData(Def, Offers[i].Level, i);
		}
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
	
	if (bWasEmpty)
	{
		if (Anim_Intro)
		{
			PlayAnimation(Anim_Intro);
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

// 증강 선택 UI가 활성화될 때 마우스 커서를 표시 (이동 입력은 유지)
void UShooterAugmentSelectScreen::NativeOnActivated()
{
	Super::NativeOnActivated();

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		FInputModeGameAndUI InputModeData;
		InputModeData.SetWidgetToFocus(TakeWidget());
		InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputModeData.SetHideCursorDuringCapture(false);

		PlayerController->SetInputMode(InputModeData);
		PlayerController->SetShowMouseCursor(true);
	}
}

void UShooterAugmentSelectScreen::NativeOnDeactivated()
{
	if (IconLoadHandle.IsValid())
	{
		IconLoadHandle->CancelHandle();
		IconLoadHandle.Reset();
	}

	LastDisplayedOffers.Reset();

	// 증강 선택 후 커서를 숨기고 게임 입력 모드로 복귀
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		PlayerController->SetShowMouseCursor(false);
		PlayerController->SetInputMode(FInputModeGameOnly());
	}

	Super::NativeOnDeactivated();
}
