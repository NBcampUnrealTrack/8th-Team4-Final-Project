// ShooterUIInitComponent.cpp
#include "UI/Frontend/ShooterUIInitComponent.h"

#include "Augment/AugmentInventoryComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameMode/BaseRoundGameState.h"
#include "GameMode/SurrenderVoteTypes.h"
#include "Loading/LoadingScreenSubsystem.h"
#include "Player/BaseRoundPlayerController.h"
#include "Player/BaseTeamPlayerState.h"
#include "Player/ShooterPlayerState.h"
#include "Tags/UITags.h"
#include "TimerManager.h"
#include "UI/InGame/Menu/ShooterInGameMenuScreen.h"
#include "UI/InGame/Menu/ShooterSurrenderVoteScreen.h"
#include "UI/InGame/ShooterAugmentSelectScreen.h"
#include "UI/InGame/Shop/ShooterShopScreen.h"
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
		World->GetTimerManager().ClearTimer(ModalRefreshTimerHandle);
	}

	bInGameMenuRequested = false;
	bModalRefreshPending = false;
	HideInGameMenu();
	HideSurrenderVote();
	HideShop();
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

	BoundInventory = Inventory;
	BoundRoundGameState = RoundGameState;
	BoundRoundPlayerController = PlayerController;
	Inventory->OnOffersChanged.AddUniqueDynamic(this, &ThisClass::HandleAugmentOffersChanged);
	RoundGameState->OnRoundStateChanged.AddUniqueDynamic(this, &ThisClass::HandleRoundStateChanged);
	PlayerController->OnSurrenderVoteStateChanged.AddUObject(
		this,
		&ThisClass::HandleSurrenderVoteStateChanged);
	PlayerController->OnSurrenderVoteStartResult.AddUObject(
		this,
		&ThisClass::HandleSurrenderVoteStartResult);

	bInGameUIReady = true;
	bLastObservedVoteActive = PlayerController->GetSurrenderVoteState().IsActive();
	RefreshAugmentSelection();
	ScheduleModalRefresh();
	PlayerController->NotifyClientReadyForGameFlow();
}

void UShooterUIInitComponent::ToggleInGameMenu()
{
	if (!bInGameUIReady)
	{
		return;
	}

	const UWorld* World = GetWorld();
	const UGameInstance* GameInstance = IsValid(World)
		? World->GetGameInstance()
		: nullptr;
	const ULoadingScreenSubsystem* LoadingScreenSubsystem = IsValid(GameInstance)
		? GameInstance->GetSubsystem<ULoadingScreenSubsystem>()
		: nullptr;
	if (IsValid(LoadingScreenSubsystem) && LoadingScreenSubsystem->IsLoadingScreenVisible())
	{
		return;
	}

	if (IsInGameMenuOpen())
	{
		bInGameMenuRequested = false;
	}
	else
	{
		bInGameMenuRequested = true;
		HideShop();
	}

	ScheduleModalRefresh();
}

void UShooterUIInitComponent::ShowInGameMenu()
{
	bInGameMenuRequested = true;
	HideShop();

	if (IsValid(InGameMenuInstance))
	{
		InGameMenuInstance->SetSurrenderEnabled(CanRequestSurrender());
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	if (!IsValid(UIManager) || !IsValid(InGameMenuClass))
	{
		UE_LOG(LogShooterUIInitComp, Error, TEXT("InGameMenuClass is not configured."));
		return;
	}

	InGameMenuInstance = Cast<UShooterInGameMenuScreen>(
		UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, InGameMenuClass));
	if (!IsValid(InGameMenuInstance))
	{
		return;
	}

	InGameMenuInstance->OnResumeRequested.AddUObject(this, &ThisClass::HandleResumeRequested);
	InGameMenuInstance->OnReturnToLobbyRequested.AddUObject(
		this,
		&ThisClass::HandleReturnToLobbyRequested);
	InGameMenuInstance->OnQuitGameRequested.AddUObject(this, &ThisClass::HandleQuitGameRequested);
	InGameMenuInstance->OnSurrenderRequested.AddUObject(this, &ThisClass::HandleSurrenderRequested);
	InGameMenuInstance->SetStatusText(FText::GetEmpty());
	InGameMenuInstance->SetSurrenderEnabled(CanRequestSurrender());
}

void UShooterUIInitComponent::HideInGameMenu()
{
	bInGameMenuRequested = false;
	if (!IsValid(InGameMenuInstance))
	{
		return;
	}

	InGameMenuInstance->OnResumeRequested.RemoveAll(this);
	InGameMenuInstance->OnReturnToLobbyRequested.RemoveAll(this);
	InGameMenuInstance->OnQuitGameRequested.RemoveAll(this);
	InGameMenuInstance->OnSurrenderRequested.RemoveAll(this);
	RemoveScreenFromLayer(UITags::UI_Layer_Modal, InGameMenuInstance);
	InGameMenuInstance = nullptr;
}

bool UShooterUIInitComponent::IsInGameMenuOpen() const
{
	return IsValid(InGameMenuInstance);
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
	if (!IsValid(ShopScreenInstance))
	{
		return;
	}

	ShopScreenInstance->OnCloseRequested.AddUniqueDynamic(
		this,
		&ThisClass::HandleShopCloseRequested);
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

void UShooterUIInitComponent::ShowAugmentSelect(
	const TArray<FAugmentInventoryEntry>& InOffers)
{
	if (IsValid(AugmentSelectInstance) && AugmentSelectInstance->IsActivated())
	{
		AugmentSelectInstance->SetOffers(
			InOffers,
			BuildRerollsRemaining(InOffers.Num()));
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	if (!IsValid(UIManager) || !IsValid(AugmentSelectClass))
	{
		UE_LOG(LogShooterUIInitComp, Error, TEXT("AugmentSelectClass is not configured."));
		return;
	}

	AugmentSelectInstance = Cast<UShooterAugmentSelectScreen>(
		UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, AugmentSelectClass));
	if (!IsValid(AugmentSelectInstance))
	{
		return;
	}

	AugmentSelectInstance->OnAugmentSelected.RemoveDynamic(
		this,
		&ThisClass::HandleAugmentSelected);
	AugmentSelectInstance->OnAugmentSelected.AddUniqueDynamic(
		this,
		&ThisClass::HandleAugmentSelected);

	AugmentSelectInstance->OnAugmentRerollRequested.RemoveDynamic(
		this,
		&ThisClass::HandleAugmentRerollRequested);
	AugmentSelectInstance->OnAugmentRerollRequested.AddUniqueDynamic(
		this,
		&ThisClass::HandleAugmentRerollRequested);

	AugmentSelectInstance->SetOffers(
		InOffers,
		BuildRerollsRemaining(InOffers.Num()));
}

void UShooterUIInitComponent::HideAugmentSelect()
{
	if (!IsValid(AugmentSelectInstance))
	{
		return;
	}

	AugmentSelectInstance->OnAugmentSelected.RemoveDynamic(
		this,
		&ThisClass::HandleAugmentSelected);
	AugmentSelectInstance->OnAugmentRerollRequested.RemoveDynamic(
		this,
		&ThisClass::HandleAugmentRerollRequested);

	RemoveScreenFromLayer(UITags::UI_Layer_Modal, AugmentSelectInstance);
	AugmentSelectInstance = nullptr;
}

void UShooterUIInitComponent::ShowSurrenderVote()
{
	ABaseRoundPlayerController* PlayerController = BoundRoundPlayerController.Get();
	if (!IsValid(PlayerController))
	{
		return;
	}

	if (IsValid(SurrenderVoteInstance))
	{
		SurrenderVoteInstance->SetVoteState(PlayerController->GetSurrenderVoteState());
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	if (!IsValid(UIManager) || !IsValid(SurrenderVoteClass))
	{
		UE_LOG(LogShooterUIInitComp, Error, TEXT("SurrenderVoteClass is not configured."));
		return;
	}

	SurrenderVoteInstance = Cast<UShooterSurrenderVoteScreen>(
		UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, SurrenderVoteClass));
	if (!IsValid(SurrenderVoteInstance))
	{
		return;
	}

	SurrenderVoteInstance->OnVoteSubmitted.AddUObject(
		this,
		&ThisClass::HandleSurrenderVoteSubmitted);
	SurrenderVoteInstance->OnMenuRequested.AddUObject(
		this,
		&ThisClass::HandleSurrenderMenuRequested);
	SurrenderVoteInstance->SetVoteState(PlayerController->GetSurrenderVoteState());
}

void UShooterUIInitComponent::HideSurrenderVote()
{
	if (!IsValid(SurrenderVoteInstance))
	{
		return;
	}

	SurrenderVoteInstance->OnVoteSubmitted.RemoveAll(this);
	SurrenderVoteInstance->OnMenuRequested.RemoveAll(this);
	RemoveScreenFromLayer(UITags::UI_Layer_Modal, SurrenderVoteInstance);
	SurrenderVoteInstance = nullptr;
}

void UShooterUIInitComponent::HandleResumeRequested()
{
	bInGameMenuRequested = false;
	ScheduleModalRefresh();
}

void UShooterUIInitComponent::HandleReturnToLobbyRequested()
{
	if (ABaseRoundPlayerController* PlayerController = GetRoundPlayerController())
	{
		PlayerController->RequestReturnToFrontend();
	}
}

void UShooterUIInitComponent::HandleQuitGameRequested()
{
	if (ABaseRoundPlayerController* PlayerController = GetRoundPlayerController())
	{
		PlayerController->RequestQuitGame();
	}
}

void UShooterUIInitComponent::HandleSurrenderRequested()
{
	if (ABaseRoundPlayerController* PlayerController = GetRoundPlayerController())
	{
		PlayerController->RequestStartSurrenderVote();
	}
}

void UShooterUIInitComponent::HandleSurrenderVoteSubmitted(const bool bApprove)
{
	if (ABaseRoundPlayerController* PlayerController = GetRoundPlayerController())
	{
		PlayerController->SubmitSurrenderVote(bApprove);
	}
}

void UShooterUIInitComponent::HandleSurrenderMenuRequested()
{
	bInGameMenuRequested = true;
	ScheduleModalRefresh();
}

void UShooterUIInitComponent::HandleSurrenderVoteStateChanged()
{
	ScheduleModalRefresh();
}

void UShooterUIInitComponent::HandleSurrenderVoteStartResult(
	const ESurrenderVoteStartResult Result)
{
	if (!IsValid(InGameMenuInstance))
	{
		return;
	}

	switch (Result)
	{
	case ESurrenderVoteStartResult::AlreadyActive:
		InGameMenuInstance->SetStatusText(
			NSLOCTEXT("ShooterSurrender", "AlreadyActive", "이미 팀 투표가 진행 중입니다."));
		break;
	case ESurrenderVoteStartResult::Cooldown:
		InGameMenuInstance->SetStatusText(
			NSLOCTEXT("ShooterSurrender", "Cooldown", "잠시 후 다시 투표할 수 있습니다."));
		break;
	case ESurrenderVoteStartResult::NotAllowed:
		InGameMenuInstance->SetStatusText(
			NSLOCTEXT("ShooterSurrender", "NotAllowed", "현재는 항복 투표를 시작할 수 없습니다."));
		break;
	default:
		InGameMenuInstance->SetStatusText(FText::GetEmpty());
		break;
	}
}

void UShooterUIInitComponent::HandleAugmentSelected(const int32 OfferIndex)
{
	if (BoundInventory.IsValid())
	{
		// TODO: 액티브 증강 키 선택 UI가 없으므로, INDEX_NONE 을 임시로 넣었습니다.
		// 나중에 키 선택 UI가 생기면, 고른 슬롯 번호(0-based)를 INDEX_NONE 대신 넣어주면 됩니다.
		// Slot1(Q) = 0, Slot2(E) = 1 으로 정해두었습니다.
		// 액티브 증강이 아닌 경우, INDEX_NONE 을 넣어주면 됩니다.
		BoundInventory->ServerSelectOffer(OfferIndex, INDEX_NONE);
	}

	HideAugmentSelect();
}

void UShooterUIInitComponent::HandleAugmentOffersChanged()
{
	RefreshAugmentSelection();
}

void UShooterUIInitComponent::HandleRoundStateChanged()
{
	if (BoundRoundGameState.IsValid()
		&& BoundRoundGameState->GetGameFlowPhase() != EGameFlowPhase::AugmentSelecting)
	{
		HideShop();
	}

	RefreshAugmentSelection();
	if (IsValid(InGameMenuInstance))
	{
		InGameMenuInstance->SetSurrenderEnabled(CanRequestSurrender());
	}
	ScheduleModalRefresh();
}

void UShooterUIInitComponent::HandleShopCloseRequested()
{
	HideShop();
}

void UShooterUIInitComponent::ScheduleModalRefresh()
{
	if (bModalRefreshPending || !IsValid(GetWorld()))
	{
		return;
	}

	bModalRefreshPending = true;
	ModalRefreshTimerHandle = GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &ThisClass::RefreshModalPresentation));
}

void UShooterUIInitComponent::RefreshModalPresentation()
{
	bModalRefreshPending = false;

	const ABaseRoundPlayerController* PlayerController = BoundRoundPlayerController.Get();
	const bool bVoteActive = IsValid(PlayerController)
		&& PlayerController->GetSurrenderVoteState().IsActive();
	if (bVoteActive && !bLastObservedVoteActive)
	{
		bInGameMenuRequested = false;
		HideShop();
	}
	bLastObservedVoteActive = bVoteActive;

	if (!bInGameMenuRequested)
	{
		HideInGameMenu();
	}

	if (bVoteActive)
	{
		ShowSurrenderVote();
	}
	else
	{
		HideSurrenderVote();
	}

	if (bInGameMenuRequested)
	{
		ShowInGameMenu();
	}
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

	const bool bDraftPhaseIsOpen =
		BoundRoundGameState->GetGameFlowPhase() == EGameFlowPhase::AugmentSelecting;
	const TArray<FAugmentInventoryEntry> Offers = BoundInventory->GetOffers();
	if (!bDraftPhaseIsOpen || Offers.IsEmpty())
	{
		HideAugmentSelect();
		return;
	}

	ShowAugmentSelect(Offers);
}

bool UShooterUIInitComponent::CanRequestSurrender() const
{
	const ABaseRoundPlayerController* PlayerController = BoundRoundPlayerController.Get();
	const ABaseTeamPlayerState* PlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<ABaseTeamPlayerState>()
		: nullptr;

	return bInGameUIReady
		&& BoundRoundGameState.IsValid()
		&& BoundRoundGameState->HasMatchStarted()
		&& SurrenderVoteRules::IsVotingPhase(BoundRoundGameState->GetGameFlowPhase())
		&& IsValid(PlayerState)
		&& PlayerState->GetTeam() != EGameTeam::None
		&& !PlayerController->GetSurrenderVoteState().IsActive();
}

bool UShooterUIInitComponent::CanShowShop() const
{
	if (!bInGameUIReady
		|| IsInGameMenuOpen()
		|| IsValid(SurrenderVoteInstance)
		|| !BoundInventory.IsValid()
		|| !BoundRoundGameState.IsValid())
	{
		return false;
	}

	return BoundRoundGameState->GetGameFlowPhase() == EGameFlowPhase::AugmentSelecting
		&& BoundInventory->GetOffers().IsEmpty();
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

void UShooterUIInitComponent::UnbindRuntimeDelegates()
{
	if (BoundInventory.IsValid())
	{
		BoundInventory->OnOffersChanged.RemoveDynamic(this, &ThisClass::HandleAugmentOffersChanged);
	}
	if (BoundRoundGameState.IsValid())
	{
		BoundRoundGameState->OnRoundStateChanged.RemoveDynamic(this, &ThisClass::HandleRoundStateChanged);
	}
	if (BoundRoundPlayerController.IsValid())
	{
		BoundRoundPlayerController->OnSurrenderVoteStateChanged.RemoveAll(this);
		BoundRoundPlayerController->OnSurrenderVoteStartResult.RemoveAll(this);
	}

	BoundInventory.Reset();
	BoundRoundGameState.Reset();
	BoundRoundPlayerController.Reset();
}

void UShooterUIInitComponent::RemoveScreenFromLayer(
	const FGameplayTag LayerTag,
	UCommonActivatableWidget* Screen)
{
	if (UShooterUIManagerSubsystem* UIManager = GetUIManager())
	{
		UIManager->RemoveWidgetFromLayer(LayerTag, Screen);
	}
	else
	{
		Screen->DeactivateWidget();
	}
}

ABaseRoundPlayerController* UShooterUIInitComponent::GetRoundPlayerController() const
{
	return Cast<ABaseRoundPlayerController>(GetOwner());
}

UShooterUIManagerSubsystem* UShooterUIInitComponent::GetUIManager() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance)
		? GameInstance->GetSubsystem<UShooterUIManagerSubsystem>()
		: nullptr;
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
