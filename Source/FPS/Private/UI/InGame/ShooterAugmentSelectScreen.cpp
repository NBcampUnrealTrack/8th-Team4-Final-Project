// ShooterAugmentSelectScreen.cpp
#include "UI/InGame/ShooterAugmentSelectScreen.h"
#include "UI/Common/ShooterAugmentCard.h"
#include "UI/ShooterUIBundles.h"
#include "Augment/AugmentDefinition.h"
#include "Augment/AugmentRegistrySubsystem.h"
#include "Components/HorizontalBox.h"
#include "Engine/AssetManager.h"
#include "Engine/GameInstance.h"
#include "Engine/StreamableManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterAugmentSelectScreen, Log, All);

void UShooterAugmentSelectScreen::SetOffers(const TArray<FAugmentInventoryEntry>& InOffers)
{
	// 이전 아이콘 로드가 진행 중이면 취소
	if (IconLoadHandle.IsValid())
	{
		IconLoadHandle->CancelHandle();
		IconLoadHandle.Reset();
	}

	Offers = InOffers;
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
	Box_Cards->ClearChildren();

	UAugmentRegistrySubsystem* Registry = GetRegistry();
	if (!IsValid(Registry))
	{
		return;
	}

	for (int32 i = 0; i < Offers.Num(); i++)
	{
		UAugmentDefinition* Def = Registry->FindDefinition(Offers[i].DefinitionId);
		if (IsValid(Def) == false)
		{
			UE_LOG(LogShooterAugmentSelectScreen, Error, TEXT("HandleIconsLoaded | %s는 레지스트리에 없습니다."),
			       *Offers[i].DefinitionId.ToString());
			continue;
		}

		UShooterAugmentCard* CardInstance = CreateWidget<UShooterAugmentCard>(this, CardClass);
		if (IsValid(CardInstance) == false)
		{
			UE_LOG(LogShooterAugmentSelectScreen, Error, TEXT("HandleIconsLoaded | AugmentCardClass가 지정되지 않았습니다."));
			continue;
		}

		CardInstance->SetData(Def, Offers[i].Level, i);
		CardInstance->OnAugmentCardClicked.AddUObject(this, &ThisClass::HandleCardClicked);
		Box_Cards->AddChildToHorizontalBox(CardInstance);
	}

	if (Anim_Intro)
	{
		PlayAnimation(Anim_Intro);
	}
}

void UShooterAugmentSelectScreen::HandleCardClicked(int32 InOfferIndex)
{
	OnAugmentSelected.Broadcast(InOfferIndex);
	DeactivateWidget();
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
