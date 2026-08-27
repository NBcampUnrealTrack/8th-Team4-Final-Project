// ShooterUIInitComponent.cpp
#include "UI/Frontend/ShooterUIInitComponent.h"

#include "Augment/AugmentInventoryComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/BaseRoundGameState.h"
#include "Player/BaseRoundPlayerController.h"
#include "Player/ShooterPlayerState.h"
#include "Tags/UITags.h"
#include "TimerManager.h"
#include "UI/InGame/Menu/ShooterInGameMenuScreen.h"
#include "UI/InGame/Menu/ShooterSurrenderVoteScreen.h"
#include "UI/InGame/ShooterAugmentSelectScreen.h"
#include "UI/InGame/Shop/ShooterShopScreen.h"
#include "UI/OutGame/MatchResult/ShooterMatchResultSubsystem.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterUIInitComp, Log, All);

void UShooterUIInitComponent::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!ensure(PlayerController) || !PlayerController->IsLocalController())
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

	UIManager->InitializeUI(PlayerController, RootLayoutClass);
	TryStartInGameUI();
}

void UShooterUIInitComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PlayerStateWaitTimerHandle);
	}

	HideShop();
	UnbindRuntimeDelegates();
	Super::EndPlay(EndPlayReason);
}

void UShooterUIInitComponent::TryStartInGameUI()
{
	if (IsTeamMatchUIReady())
	{
		return;
	}

	UWorld* World = GetWorld();
	ABaseRoundPlayerController* PlayerController = GetRoundPlayerController();
	AShooterPlayerState* PlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<AShooterPlayerState>()
		: nullptr;
	ABaseRoundGameState* RoundGameState = IsValid(World)
		? World->GetGameState<ABaseRoundGameState>()
		: nullptr;
	UAugmentInventoryComponent* Inventory = IsValid(PlayerState)
		? PlayerState->GetAugmentInventory()
		: nullptr;

	if (!IsValid(World)
		|| !IsValid(PlayerController)
		|| !IsValid(PlayerState)
		|| !IsValid(RoundGameState)
		|| !IsValid(Inventory))
	{
		ScheduleInGameUIRetry();
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	if (!IsValid(UIManager) || !UIManager->IsUIInitialized())
	{
		ScheduleInGameUIRetry();
		return;
	}

	if (!IsValid(UIManager->PushWidgetToLayer(InitialWidgetLayer, InitialWidgetClass)))
	{
		if (!bLoggedInitialWidgetPushFailure)
		{
			bLoggedInitialWidgetPushFailure = true;
			UE_LOG(
				LogShooterUIInitComp,
				Warning,
				TEXT("Failed to create the initial in-game widget. Layer=%s WidgetClass=%s"),
				*InitialWidgetLayer.ToString(),
				*GetNameSafe(InitialWidgetClass));
		}
		ScheduleInGameUIRetry();
		return;
	}

	World->GetTimerManager().ClearTimer(PlayerStateWaitTimerHandle);
	bLoggedInitialWidgetPushFailure = false;
	UnbindRuntimeDelegates();

	BoundRoundGameState = RoundGameState;
	RoundGameState->OnRoundStateChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleRoundStateChanged);
	InitializeTeamMatchUI(PlayerController, Inventory);
	PlayerController->NotifyClientReadyForGameFlow();
}

void UShooterUIInitComponent::ToggleShop()
{
	if (IsShopOpen())
	{
		HideShop();
		return;
	}
	ShowShop();
}

void UShooterUIInitComponent::ShowShop()
{
	if (IsShopOpen() || !CanShowShop())
	{
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	if (!IsValid(UIManager) || !IsValid(ShopScreenClass))
	{
		UE_LOG(LogShooterUIInitComp, Error, TEXT("ShopScreenClass is not configured."));
		return;
	}

	HideAugmentSelect();
	ShopScreenInstance = Cast<UShooterShopScreen>(
		UIManager->PushWidgetToLayer(UITags::UI_Layer_Menu, ShopScreenClass));
	if (IsValid(ShopScreenInstance))
	{
		ShopScreenInstance->OnCloseRequested.AddUniqueDynamic(
			this,
			&ThisClass::HandleShopCloseRequested);
	}
}

void UShooterUIInitComponent::HideShop()
{
	if (!IsValid(ShopScreenInstance))
	{
		return;
	}

	ShopScreenInstance->OnCloseRequested.RemoveDynamic(
		this,
		&ThisClass::HandleShopCloseRequested);
	RemoveScreenFromLayer(UITags::UI_Layer_Menu, ShopScreenInstance);
	ShopScreenInstance = nullptr;
}

bool UShooterUIInitComponent::IsShopOpen() const
{
	return IsValid(ShopScreenInstance);
}

void UShooterUIInitComponent::HandleRoundStateChanged()
{
	if (BoundRoundGameState.IsValid()
		&& BoundRoundGameState->GetGameFlowPhase() == EGameFlowPhase::MatchEnded)
	{
		UShooterMatchResultSubsystem* MatchResultSubsystem =
			GetWorld()->GetGameInstance()->GetSubsystem<UShooterMatchResultSubsystem>();
		MatchResultSubsystem->CaptureSnapshot(BoundRoundGameState.Get());
	}

	if (BoundRoundGameState.IsValid()
		&& BoundRoundGameState->GetGameFlowPhase() != EGameFlowPhase::AugmentSelecting)
	{
		HideShop();
	}
	NotifyModeUIStateChanged();
}

void UShooterUIInitComponent::HandleShopCloseRequested()
{
	HideShop();
}

bool UShooterUIInitComponent::CanShowShop() const
{
	UAugmentInventoryComponent* Inventory = GetBoundAugmentInventory();
	if (!IsTeamMatchUIReady()
		|| IsInGameMenuOpen()
		|| IsSurrenderVoteOpen()
		|| !IsValid(Inventory)
		|| !BoundRoundGameState.IsValid())
	{
		return false;
	}

	return BoundRoundGameState->GetGameFlowPhase() == EGameFlowPhase::AugmentSelecting
		&& Inventory->GetOffers().IsEmpty();
}

void UShooterUIInitComponent::ScheduleInGameUIRetry()
{
	UWorld* World = GetWorld();
	if (!IsValid(World)
		|| World->GetTimerManager().IsTimerActive(PlayerStateWaitTimerHandle))
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

void UShooterUIInitComponent::UnbindRuntimeDelegates()
{
	if (BoundRoundGameState.IsValid())
	{
		BoundRoundGameState->OnRoundStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleRoundStateChanged);
	}
	BoundRoundGameState.Reset();
}

ABaseRoundPlayerController* UShooterUIInitComponent::GetRoundPlayerController() const
{
	return Cast<ABaseRoundPlayerController>(GetOwner());
}

bool UShooterUIInitComponent::IsAugmentPresentationAllowed() const
{
	return BoundRoundGameState.IsValid()
		&& BoundRoundGameState->GetGameFlowPhase() == EGameFlowPhase::AugmentSelecting;
}

bool UShooterUIInitComponent::IsSurrenderRequestAllowed() const
{
	return BoundRoundGameState.IsValid()
		&& BoundRoundGameState->HasMatchStarted()
		&& BoundRoundGameState->GetGameFlowPhase() != EGameFlowPhase::MatchEnded;
}

void UShooterUIInitComponent::CloseModeSpecificScreensForModal()
{
	HideShop();
}

TSubclassOf<UShooterInGameMenuScreen>
UShooterUIInitComponent::GetInGameMenuWidgetClass() const
{
	return InGameMenuClass;
}

TSubclassOf<UShooterSurrenderVoteScreen>
UShooterUIInitComponent::GetSurrenderVoteWidgetClass() const
{
	return SurrenderVoteClass;
}

TSubclassOf<UShooterAugmentSelectScreen>
UShooterUIInitComponent::GetAugmentSelectWidgetClass() const
{
	return AugmentSelectClass;
}

UShooterInGameMenuScreen* UShooterUIInitComponent::GetInGameMenuInstance() const
{
	return InGameMenuInstance;
}

void UShooterUIInitComponent::SetInGameMenuInstance(
	UShooterInGameMenuScreen* Instance)
{
	InGameMenuInstance = Instance;
}

UShooterSurrenderVoteScreen* UShooterUIInitComponent::GetSurrenderVoteInstance() const
{
	return SurrenderVoteInstance;
}

void UShooterUIInitComponent::SetSurrenderVoteInstance(
	UShooterSurrenderVoteScreen* Instance)
{
	SurrenderVoteInstance = Instance;
}

UShooterAugmentSelectScreen* UShooterUIInitComponent::GetAugmentSelectInstance() const
{
	return AugmentSelectInstance;
}

void UShooterUIInitComponent::SetAugmentSelectInstance(
	UShooterAugmentSelectScreen* Instance)
{
	AugmentSelectInstance = Instance;
}
