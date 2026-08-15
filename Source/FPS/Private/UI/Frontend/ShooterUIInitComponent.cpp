// ShooterUIInitComponent.cpp
#include "UI/Frontend/ShooterUIInitComponent.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFlow/Round/BaseRoundGameState.h"
#include "GameFlow/Round/BaseRoundPlayerController.h"
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
		UE_LOG(LogShooterUIInitComp, Error,
			TEXT("Failed to create the initial in-game widget. Layer=%s WidgetClass=%s"),
			*InitialWidgetLayer.ToString(),
			*GetNameSafe(InitialWidgetClass));
		return;
	}

	World->GetTimerManager().ClearTimer(PlayerStateWaitTimerHandle);
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
	if (AugmentSelectInstance && AugmentSelectInstance->IsInViewport())
	{
		AugmentSelectInstance->SetOffers(InOffers);
		SetAugmentSelectionInput(true);
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

	AugmentSelectInstance->OnAugmentSelected.RemoveDynamic(this, &ThisClass::HandleAugmentSelected);
	AugmentSelectInstance->OnAugmentSelected.AddUniqueDynamic(this, &ThisClass::HandleAugmentSelected);
	AugmentSelectInstance->SetOffers(InOffers);
	SetAugmentSelectionInput(true);
}

void UShooterUIInitComponent::HideAugmentSelect()
{
	if (IsValid(AugmentSelectInstance))
	{
		AugmentSelectInstance->OnAugmentSelected.RemoveDynamic(this, &ThisClass::HandleAugmentSelected);
		AugmentSelectInstance->DeactivateWidget();
		AugmentSelectInstance = nullptr;
	}

	SetAugmentSelectionInput(false);
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

void UShooterUIInitComponent::SetAugmentSelectionInput(bool bSelecting)
{
	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	if (bSelecting)
	{
		FInputModeGameAndUI InputMode;
		if (IsValid(AugmentSelectInstance))
		{
			InputMode.SetWidgetToFocus(AugmentSelectInstance->TakeWidget());
		}
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);

		PlayerController->SetInputMode(InputMode);
		PlayerController->SetShowMouseCursor(true);
		return;
	}

	const FInputModeGameOnly InputMode;
	PlayerController->SetInputMode(InputMode);
	PlayerController->SetShowMouseCursor(false);
}

UShooterUIManagerSubsystem* UShooterUIInitComponent::GetUIManager() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GI = World ? World->GetGameInstance() : nullptr;

	return GI ? GI->GetSubsystem<UShooterUIManagerSubsystem>() : nullptr;
}
