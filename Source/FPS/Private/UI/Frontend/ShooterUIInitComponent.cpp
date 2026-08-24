// ShooterUIInitComponent.cpp
#include "UI/Frontend/ShooterUIInitComponent.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameMode/BaseRoundGameState.h"
#include "Player/BaseRoundPlayerController.h"
#include "Player/ShooterPlayerState.h"
#include "Tags/UITags.h"
#include "UI/InGame/ShooterAugmentSelectScreen.h"
#include "Augment/AugmentInventoryComponent.h"
#include "Input/CommonUIActionRouterBase.h"
#include "Engine/GameViewportClient.h"

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
	if (!IsValid(UIManager)
		|| !ensure(RootLayoutClass)
		|| !ensure(InitialWidgetClass)
		|| !ensure(InitialWidgetLayer.IsValid()))
	{
		return;
	}

	UIManager->InitializeUI(PC, RootLayoutClass);
	TryStartInGameUI();
}

void UShooterUIInitComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PlayerStateWaitTimerHandle);
	}

	HideAugmentSelect();
	UnbindRuntimeDelegates();
	Super::EndPlay(EndPlayReason);
}

void UShooterUIInitComponent::TryStartInGameUI()
{
	if (bInGameUIReady)
	{
		return;
	}

	UWorld* World = GetWorld();
	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	AShooterPlayerState* PlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<AShooterPlayerState>()
		: nullptr;
	ABaseRoundGameState* RoundGameState = IsValid(World) ? World->GetGameState<ABaseRoundGameState>() : nullptr;
	UAugmentInventoryComponent* Inventory = IsValid(PlayerState) ? PlayerState->GetAugmentInventory() : nullptr;
	if (!IsValid(World) || !IsValid(PlayerState) || !IsValid(RoundGameState) || !IsValid(Inventory))
	{
		ScheduleInGameUIRetry();
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	if (!IsValid(UIManager) || !UIManager->IsUIInitialized())
	{
		// RootLayout 준비 시간은 환경마다 다르므로 고정 Delay 대신 준비 완료까지 재시도한다.
		ScheduleInGameUIRetry();
		return;
	}

	if (!IsValid(UIManager->PushWidgetToLayer(InitialWidgetLayer, InitialWidgetClass)))
	{
		if (!bLoggedInitialWidgetPushFailure)
		{
			bLoggedInitialWidgetPushFailure = true;
			UE_LOG(LogShooterUIInitComp, Warning,
				TEXT("Failed to create the initial in-game widget; initialization will retry. Layer=%s WidgetClass=%s"),
				*InitialWidgetLayer.ToString(),
				*GetNameSafe(InitialWidgetClass));
		}

		ScheduleInGameUIRetry();
		return;
	}

	World->GetTimerManager().ClearTimer(PlayerStateWaitTimerHandle);
	bLoggedInitialWidgetPushFailure = false;
	UnbindRuntimeDelegates();
	BoundInventory = Inventory;
	BoundRoundGameState = RoundGameState;
	Inventory->OnOffersChanged.AddUniqueDynamic(this, &ThisClass::HandleAugmentSelectionStateChanged);
	RoundGameState->OnRoundStateChanged.AddUniqueDynamic(this, &ThisClass::HandleAugmentSelectionStateChanged);

	bInGameUIReady = true;
	RefreshAugmentSelection();

	if (ABaseRoundPlayerController* RoundPlayerController = Cast<ABaseRoundPlayerController>(PlayerController))
	{
		RoundPlayerController->NotifyClientReadyForGameFlow();
	}
}

void UShooterUIInitComponent::ShowAugmentSelect(const TArray<FAugmentInventoryEntry>& InOffers)
{
	// 이미 증강이 떠있으면 return
	if (IsValid(AugmentSelectInstance) && AugmentSelectInstance->IsActivated())
	{
		AugmentSelectInstance->SetOffers(InOffers, BuildRerollsRemaining(InOffers.Num()));

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

	AugmentSelectInstance = Cast<UShooterAugmentSelectScreen>(UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, AugmentSelectClass));
	if (IsValid(AugmentSelectInstance) == false)
	{
		return;
	}

	AugmentSelectInstance->OnAugmentSelected.RemoveDynamic(this, &ThisClass::HandleAugmentSelected);
	AugmentSelectInstance->OnAugmentSelected.AddUniqueDynamic(this, &ThisClass::HandleAugmentSelected);
	AugmentSelectInstance->OnAugmentRerollRequested.RemoveDynamic(this, &ThisClass::HandleAugmentRerollRequested);
	AugmentSelectInstance->OnAugmentRerollRequested.AddUniqueDynamic(this, &ThisClass::HandleAugmentRerollRequested);
	AugmentSelectInstance->SetOffers(InOffers, BuildRerollsRemaining(InOffers.Num()));

}

void UShooterUIInitComponent::HideAugmentSelect()
{
	if (IsValid(AugmentSelectInstance))
	{
		AugmentSelectInstance->OnAugmentSelected.RemoveDynamic(this, &ThisClass::HandleAugmentSelected);
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

	HideAugmentSelect();
}

void UShooterUIInitComponent::HandleAugmentSelectionStateChanged()
{
	RefreshAugmentSelection();
}

void UShooterUIInitComponent::HandleAugmentRerollRequested(int32 SlotIndex)
{
	if (BoundInventory.IsValid())
	{
		BoundInventory->ServerRerollOffer(SlotIndex);
	}
}

void UShooterUIInitComponent::RefreshAugmentSelection()
{
	if (!BoundInventory.IsValid() || !BoundRoundGameState.IsValid())
	{
		HideAugmentSelect();
		return;
	}

	const bool bDraftPhaseIsOpen = BoundRoundGameState->GetGameFlowPhase() == EGameFlowPhase::AugmentSelecting;
	const TArray<FAugmentInventoryEntry> Offers = BoundInventory->GetOffers();
	if (!bDraftPhaseIsOpen || Offers.IsEmpty())
	{
		HideAugmentSelect();
		return;
	}

	ShowAugmentSelect(Offers);
}

void UShooterUIInitComponent::UnbindRuntimeDelegates()
{
	if (BoundInventory.IsValid())
	{
		BoundInventory->OnOffersChanged.RemoveDynamic(this, &ThisClass::HandleAugmentSelectionStateChanged);
	}
	if (BoundRoundGameState.IsValid())
	{
		BoundRoundGameState->OnRoundStateChanged.RemoveDynamic(this, &ThisClass::HandleAugmentSelectionStateChanged);
	}

	BoundInventory.Reset();
	BoundRoundGameState.Reset();
}

void UShooterUIInitComponent::ScheduleInGameUIRetry()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || World->GetTimerManager().IsTimerActive(PlayerStateWaitTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		PlayerStateWaitTimerHandle,
		this,
		&ThisClass::TryStartInGameUI,
		0.1f,
		true);
}

UShooterUIManagerSubsystem* UShooterUIInitComponent::GetUIManager() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;

	return GI ? GI->GetSubsystem<UShooterUIManagerSubsystem>() : nullptr;
}

TArray<int32> UShooterUIInitComponent::BuildRerollsRemaining(int32 OfferCount) const
{
	TArray<int32> Result;
	Result.Reserve(OfferCount);
	
	for (int32 i = 0; i < OfferCount; ++i)
	{
		const int32 Remaining = BoundInventory.IsValid() ? BoundInventory->GetSlotRerollsRemaining(i) : 0;
		
		Result.Add(Remaining);
	}
	
	return Result;
}
