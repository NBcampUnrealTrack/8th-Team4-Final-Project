// ShooterUIInitComponent.cpp
#include "UI/Frontend/ShooterUIInitComponent.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Player/ShooterPlayerState.h"
#include "Tags/UITags.h"
#include "UI/InGame/ShooterAugmentSelectScreen.h"
#include "Augment/AugmentInventoryComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterUIInitComp, Log, All);

void UShooterUIInitComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (!ensure(PC))
	{
		return;
	}

	if (!PC->IsLocalController())
	{
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	if (IsValid(UIManager) == false)
	{
		return;
	}
	
	UIManager->InitializeUI(PC, RootLayoutClass);

	// BP에서 지정
	if (!ensure(InitialWidgetClass))
	{
		return;
	}

	TryStartInGameUI();
}

void UShooterUIInitComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BoundInventory.IsValid())
	{
		BoundInventory->OnOffersChanged.RemoveDynamic(this, &ThisClass::HandleOffersChanged);
		BoundInventory.Reset();
	}
	
	Super::EndPlay(EndPlayReason);
}

void UShooterUIInitComponent::TryStartInGameUI()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	
	AShooterPlayerState* PS = GetOwner<APlayerController>()->GetPlayerState<AShooterPlayerState>();
	if (!PS)
	{
		if (!World->GetTimerManager().IsTimerActive(PlayerStateWaitTimerHandle))
		{
			GetWorld()->GetTimerManager().SetTimer(
			PlayerStateWaitTimerHandle,
			this,
			&ThisClass::TryStartInGameUI,
			0.1f,
			true
			);
		}
		
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(PlayerStateWaitTimerHandle);
	
	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	if (IsValid(UIManager) == false)
	{
		return;
	}
	
	UIManager->PushWidgetToLayer(InitialWidgetLayer, InitialWidgetClass);
	
	UAugmentInventoryComponent* Inventory = PS->GetAugmentInventory();
	if (Inventory == nullptr)
	{
		return;
	}

	BoundInventory = Inventory;
	Inventory->OnOffersChanged.AddDynamic(this, &ThisClass::HandleOffersChanged);

	// ⚠️ 구독하기 전에 후보가 이미 와 있을 수 있다.
	//    이 줄이 없으면 그 라운드만 화면이 안 뜨고, 다음 라운드부터 정상 동작해서 원인을 못 찾는다
	if (!Inventory->GetOffers().IsEmpty())
	{
		HandleOffersChanged();
	}
}

void UShooterUIInitComponent::ShowAugmentSelect(const TArray<FAugmentInventoryEntry>& InOffers)
{
	// 이미 증강이 떠있으면 return
	if (AugmentSelectInstance && AugmentSelectInstance->IsInViewport())
	{
		AugmentSelectInstance->SetOffers(InOffers);
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	if (IsValid(UIManager) == false)
	{
		return;
	}

	if (IsValid(AugmentSelectClass) == false)
	{
		UE_LOG(LogShooterUIInitComp, Error, TEXT("UIInitComponent | AugmentSelectClass가 지정되지 않았습니다"));
		return;
	}
	
	AugmentSelectInstance = Cast<UShooterAugmentSelectScreen>(UIManager->PushWidgetToLayer(UITags::UI_Layer_Menu, AugmentSelectClass));
	if (IsValid(AugmentSelectInstance) == false)
	{
		return;
	}
	
	AugmentSelectInstance->OnAugmentSelected.AddDynamic(this, &ThisClass::HandleAugmentSelected);
	AugmentSelectInstance->SetOffers(InOffers);
}

void UShooterUIInitComponent::HideAugmentSelect()
{
	if (AugmentSelectInstance)
	{
		AugmentSelectInstance->DeactivateWidget();
		AugmentSelectInstance = nullptr;
	}
}

void UShooterUIInitComponent::HandleAugmentSelected(int32 OfferIndex)
{
	if (BoundInventory.IsValid())
	{
		BoundInventory->ServerSelectOffer(OfferIndex);
	}
	AugmentSelectInstance = nullptr;
}

void UShooterUIInitComponent::HandleOffersChanged()
{
	if (!BoundInventory.IsValid())
	{
		return;
	}
	
	const TArray<FAugmentInventoryEntry> Offers = BoundInventory->GetOffers();
	if (Offers.IsEmpty())
	{
		HideAugmentSelect();
		return;
	}
	
	ShowAugmentSelect(Offers);
}

UShooterUIManagerSubsystem* UShooterUIInitComponent::GetUIManager() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;

	return GI ? GI->GetSubsystem<UShooterUIManagerSubsystem>() : nullptr;
}
