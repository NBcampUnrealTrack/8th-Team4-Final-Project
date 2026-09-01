#include "UI/Frontend/TeamMatchUIInitComponent.h"

#include "Augment/AugmentInventoryComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameMode/Types/SurrenderVoteTypes.h"
#include "Loading/LoadingScreenSubsystem.h"
#include "Player/BaseTeamPlayerState.h"
#include "Player/ShooterPlayerController.h"
#include "Tags/UITags.h"
#include "TimerManager.h"
#include "UI/InGame/Menu/ShooterInGameMenuScreen.h"
#include "UI/InGame/Menu/ShooterSurrenderVoteScreen.h"
#include "UI/InGame/ShooterAugmentSelectScreen.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogTeamMatchUIInit, Log, All);

void UTeamMatchUIInitComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ShutdownTeamMatchUI();
	Super::EndPlay(EndPlayReason);
}

void UTeamMatchUIInitComponent::InitializeTeamMatchUI(
	AShooterPlayerController* PlayerController,
	UAugmentInventoryComponent* Inventory)
{
	BoundPlayerController = PlayerController;
	BoundInventory = Inventory;

	Inventory->OnOffersChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleAugmentOffersChanged);
	PlayerController->OnSurrenderVoteStateChanged.AddUObject(
		this,
		&ThisClass::HandleSurrenderVoteStateChanged);
	PlayerController->OnSurrenderVoteStartResult.AddUObject(
		this,
		&ThisClass::HandleSurrenderVoteStartResult);

	bTeamMatchUIReady = true;
	bLastObservedVoteActive = PlayerController->GetSurrenderVoteState().IsActive();
	ScheduleModalRefresh();
}

void UTeamMatchUIInitComponent::ShutdownTeamMatchUI()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ModalRefreshTimerHandle);
	}

	bTeamMatchUIReady = false;
	bInGameMenuRequested = false;
	bLastObservedVoteActive = false;
	bModalRefreshPending = false;
	RemoveInGameMenu();
	RemoveSurrenderVote();
	HideAugmentSelect();

	if (BoundInventory.IsValid())
	{
		BoundInventory->OnOffersChanged.RemoveDynamic(
			this,
			&ThisClass::HandleAugmentOffersChanged);
	}
	if (BoundPlayerController.IsValid())
	{
		BoundPlayerController->OnSurrenderVoteStateChanged.RemoveAll(this);
		BoundPlayerController->OnSurrenderVoteStartResult.RemoveAll(this);
	}

	BoundInventory.Reset();
	BoundPlayerController.Reset();
}

void UTeamMatchUIInitComponent::NotifyModeUIStateChanged()
{
	if (UShooterInGameMenuScreen* InGameMenu = GetInGameMenuInstance())
	{
		InGameMenu->SetSurrenderEnabled(CanRequestSurrender());
	}
	ScheduleModalRefresh();
}

void UTeamMatchUIInitComponent::ToggleInGameMenu()
{
	if (!bTeamMatchUIReady)
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
	if (IsValid(LoadingScreenSubsystem)
		&& LoadingScreenSubsystem->IsLoadingScreenVisible())
	{
		return;
	}

	bInGameMenuRequested = !(bInGameMenuRequested || IsInGameMenuOpen());
	ScheduleModalRefresh();
}

void UTeamMatchUIInitComponent::ShowInGameMenu()
{
	bInGameMenuRequested = true;
	ScheduleModalRefresh();
}

void UTeamMatchUIInitComponent::HideInGameMenu()
{
	bInGameMenuRequested = false;
	ScheduleModalRefresh();
}

bool UTeamMatchUIInitComponent::IsInGameMenuOpen() const
{
	return IsValid(GetInGameMenuInstance());
}

void UTeamMatchUIInitComponent::PresentInGameMenu()
{
	if (UShooterInGameMenuScreen* InGameMenu = GetInGameMenuInstance())
	{
		InGameMenu->SetSurrenderEnabled(CanRequestSurrender());
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	const TSubclassOf<UShooterInGameMenuScreen> WidgetClass = GetInGameMenuWidgetClass();
	if (!IsValid(UIManager) || !IsValid(WidgetClass))
	{
		UE_LOG(LogTeamMatchUIInit, Error, TEXT("InGameMenuClass is not configured."));
		return;
	}

	UShooterInGameMenuScreen* InGameMenu = Cast<UShooterInGameMenuScreen>(
		UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, WidgetClass));
	if (!IsValid(InGameMenu))
	{
		return;
	}

	SetInGameMenuInstance(InGameMenu);
	InGameMenu->OnResumeRequested.AddUObject(this, &ThisClass::HandleResumeRequested);
	InGameMenu->OnReturnToLobbyRequested.AddUObject(
		this,
		&ThisClass::HandleReturnToLobbyRequested);
	InGameMenu->OnQuitGameRequested.AddUObject(this, &ThisClass::HandleQuitGameRequested);
	InGameMenu->OnSurrenderRequested.AddUObject(this, &ThisClass::HandleSurrenderRequested);
	InGameMenu->SetStatusText(FText::GetEmpty());
	InGameMenu->SetSurrenderEnabled(CanRequestSurrender());
}

void UTeamMatchUIInitComponent::RemoveInGameMenu()
{
	UShooterInGameMenuScreen* InGameMenu = GetInGameMenuInstance();
	if (!IsValid(InGameMenu))
	{
		return;
	}

	InGameMenu->OnResumeRequested.RemoveAll(this);
	InGameMenu->OnReturnToLobbyRequested.RemoveAll(this);
	InGameMenu->OnQuitGameRequested.RemoveAll(this);
	InGameMenu->OnSurrenderRequested.RemoveAll(this);
	RemoveScreenFromLayer(UITags::UI_Layer_Modal, InGameMenu);
	SetInGameMenuInstance(nullptr);
}

void UTeamMatchUIInitComponent::PresentSurrenderVote()
{
	AShooterPlayerController* PlayerController = BoundPlayerController.Get();
	if (!IsValid(PlayerController))
	{
		return;
	}

	if (UShooterSurrenderVoteScreen* VoteScreen = GetSurrenderVoteInstance())
	{
		VoteScreen->SetVoteState(PlayerController->GetSurrenderVoteState());
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	const TSubclassOf<UShooterSurrenderVoteScreen> WidgetClass = GetSurrenderVoteWidgetClass();
	if (!IsValid(UIManager) || !IsValid(WidgetClass))
	{
		UE_LOG(LogTeamMatchUIInit, Error, TEXT("SurrenderVoteClass is not configured."));
		return;
	}

	UShooterSurrenderVoteScreen* VoteScreen = Cast<UShooterSurrenderVoteScreen>(
		UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, WidgetClass));
	if (!IsValid(VoteScreen))
	{
		return;
	}

	SetSurrenderVoteInstance(VoteScreen);
	VoteScreen->OnVoteSubmitted.AddUObject(
		this,
		&ThisClass::HandleSurrenderVoteSubmitted);
	VoteScreen->OnMenuRequested.AddUObject(
		this,
		&ThisClass::HandleSurrenderMenuRequested);
	VoteScreen->SetVoteState(PlayerController->GetSurrenderVoteState());
}

void UTeamMatchUIInitComponent::RemoveSurrenderVote()
{
	UShooterSurrenderVoteScreen* VoteScreen = GetSurrenderVoteInstance();
	if (!IsValid(VoteScreen))
	{
		return;
	}

	VoteScreen->OnVoteSubmitted.RemoveAll(this);
	VoteScreen->OnMenuRequested.RemoveAll(this);
	RemoveScreenFromLayer(UITags::UI_Layer_Modal, VoteScreen);
	SetSurrenderVoteInstance(nullptr);
}

void UTeamMatchUIInitComponent::ShowAugmentSelect(
	const TArray<FAugmentInventoryEntry>& InOffers)
{
	if (UShooterAugmentSelectScreen* AugmentScreen = GetAugmentSelectInstance();
		IsValid(AugmentScreen) && AugmentScreen->IsActivated())
	{
		AugmentScreen->SetOffers(InOffers, BuildRerollsRemaining(InOffers.Num()));
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	const TSubclassOf<UShooterAugmentSelectScreen> WidgetClass = GetAugmentSelectWidgetClass();
	if (!IsValid(UIManager) || !IsValid(WidgetClass))
	{
		UE_LOG(LogTeamMatchUIInit, Error, TEXT("AugmentSelectClass is not configured."));
		return;
	}

	UShooterAugmentSelectScreen* AugmentScreen = Cast<UShooterAugmentSelectScreen>(
		UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, WidgetClass));
	if (!IsValid(AugmentScreen))
	{
		return;
	}

	SetAugmentSelectInstance(AugmentScreen);
	AugmentScreen->OnAugmentSelected.AddUniqueDynamic(
		this,
		&ThisClass::HandleAugmentSelected);
	AugmentScreen->OnAugmentRerollRequested.AddUniqueDynamic(
		this,
		&ThisClass::HandleAugmentRerollRequested);
	AugmentScreen->SetOffers(InOffers, BuildRerollsRemaining(InOffers.Num()));
}

void UTeamMatchUIInitComponent::HideAugmentSelect()
{
	UShooterAugmentSelectScreen* AugmentScreen = GetAugmentSelectInstance();
	if (!IsValid(AugmentScreen))
	{
		return;
	}

	AugmentScreen->OnAugmentSelected.RemoveDynamic(
		this,
		&ThisClass::HandleAugmentSelected);
	AugmentScreen->OnAugmentRerollRequested.RemoveDynamic(
		this,
		&ThisClass::HandleAugmentRerollRequested);
	RemoveScreenFromLayer(UITags::UI_Layer_Modal, AugmentScreen);
	SetAugmentSelectInstance(nullptr);
}

void UTeamMatchUIInitComponent::ScheduleModalRefresh()
{
	if (bModalRefreshPending || !IsValid(GetWorld()))
	{
		return;
	}

	bModalRefreshPending = true;
	ModalRefreshTimerHandle = GetWorld()->GetTimerManager().SetTimerForNextTick(
		FTimerDelegate::CreateUObject(this, &ThisClass::RefreshModalPresentation));
}

void UTeamMatchUIInitComponent::RefreshModalPresentation()
{
	bModalRefreshPending = false;

	const AShooterPlayerController* PlayerController = BoundPlayerController.Get();
	const bool bVoteActive = IsValid(PlayerController)
		&& PlayerController->GetSurrenderVoteState().IsActive();
	if (bVoteActive && !bLastObservedVoteActive)
	{
		bInGameMenuRequested = false;
	}
	bLastObservedVoteActive = bVoteActive;

	const TArray<FAugmentInventoryEntry> Offers = BoundInventory.IsValid()
		? BoundInventory->GetOffers()
		: TArray<FAugmentInventoryEntry>();
	const bool bShowAugment = IsAugmentPresentationAllowed() && !Offers.IsEmpty();

	if (bInGameMenuRequested)
	{
		CloseModeSpecificScreensForModal();
		RemoveSurrenderVote();
		HideAugmentSelect();
		PresentInGameMenu();
		return;
	}

	RemoveInGameMenu();
	if (bVoteActive)
	{
		CloseModeSpecificScreensForModal();
		HideAugmentSelect();
		PresentSurrenderVote();
		return;
	}

	RemoveSurrenderVote();
	if (bShowAugment)
	{
		CloseModeSpecificScreensForModal();
		ShowAugmentSelect(Offers);
	}
	else
	{
		HideAugmentSelect();
	}
}

bool UTeamMatchUIInitComponent::CanRequestSurrender() const
{
	const AShooterPlayerController* PlayerController = BoundPlayerController.Get();
	const ABaseTeamPlayerState* PlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<ABaseTeamPlayerState>()
		: nullptr;

	return bTeamMatchUIReady
		&& IsSurrenderRequestAllowed()
		&& IsValid(PlayerState)
		&& PlayerState->GetTeam() != EGameTeam::None
		&& !PlayerController->GetSurrenderVoteState().IsActive();
}

void UTeamMatchUIInitComponent::HandleResumeRequested()
{
	bInGameMenuRequested = false;
	ScheduleModalRefresh();
}

void UTeamMatchUIInitComponent::HandleReturnToLobbyRequested()
{
	if (AShooterPlayerController* PlayerController = BoundPlayerController.Get())
	{
		PlayerController->RequestReturnToLobby();
	}
}

void UTeamMatchUIInitComponent::HandleQuitGameRequested()
{
	if (AShooterPlayerController* PlayerController = BoundPlayerController.Get())
	{
		PlayerController->RequestQuitGame();
	}
}

void UTeamMatchUIInitComponent::HandleSurrenderRequested()
{
	if (AShooterPlayerController* PlayerController = BoundPlayerController.Get())
	{
		PlayerController->RequestStartSurrenderVote();
	}
}

void UTeamMatchUIInitComponent::HandleSurrenderVoteSubmitted(const bool bApprove)
{
	if (AShooterPlayerController* PlayerController = BoundPlayerController.Get())
	{
		PlayerController->SubmitSurrenderVote(bApprove);
	}
}

void UTeamMatchUIInitComponent::HandleSurrenderMenuRequested()
{
	bInGameMenuRequested = true;
	ScheduleModalRefresh();
}

void UTeamMatchUIInitComponent::HandleSurrenderVoteStateChanged()
{
	ScheduleModalRefresh();
}

void UTeamMatchUIInitComponent::HandleSurrenderVoteStartResult(
	const ESurrenderVoteStartResult Result)
{
	UShooterInGameMenuScreen* InGameMenu = GetInGameMenuInstance();
	if (!IsValid(InGameMenu))
	{
		return;
	}

	switch (Result)
	{
	case ESurrenderVoteStartResult::AlreadyActive:
		InGameMenu->SetStatusText(
			NSLOCTEXT("ShooterSurrender", "AlreadyActive", "이미 팀 투표가 진행 중입니다."));
		break;
	case ESurrenderVoteStartResult::Cooldown:
		InGameMenu->SetStatusText(
			NSLOCTEXT("ShooterSurrender", "Cooldown", "잠시 후 다시 투표할 수 있습니다."));
		break;
	case ESurrenderVoteStartResult::NotAllowed:
		InGameMenu->SetStatusText(
			NSLOCTEXT("ShooterSurrender", "NotAllowed", "현재는 항복 투표를 시작할 수 없습니다."));
		break;
	default:
		InGameMenu->SetStatusText(FText::GetEmpty());
		break;
	}
}

void UTeamMatchUIInitComponent::HandleAugmentSelected(const int32 OfferIndex)
{
	if (BoundInventory.IsValid())
	{
		BoundInventory->ServerSelectOffer(OfferIndex);
	}
	HideAugmentSelect();
}

void UTeamMatchUIInitComponent::HandleAugmentOffersChanged()
{
	ScheduleModalRefresh();
}

void UTeamMatchUIInitComponent::HandleAugmentRerollRequested(const int32 SlotIndex)
{
	if (BoundInventory.IsValid())
	{
		BoundInventory->ServerRerollOffer(SlotIndex);
	}
}

bool UTeamMatchUIInitComponent::IsTeamMatchUIReady() const
{
	return bTeamMatchUIReady;
}

bool UTeamMatchUIInitComponent::IsSurrenderVoteOpen() const
{
	return IsValid(GetSurrenderVoteInstance());
}

UAugmentInventoryComponent* UTeamMatchUIInitComponent::GetBoundAugmentInventory() const
{
	return BoundInventory.Get();
}

UShooterUIManagerSubsystem* UTeamMatchUIInitComponent::GetUIManager() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance)
		? GameInstance->GetSubsystem<UShooterUIManagerSubsystem>()
		: nullptr;
}

void UTeamMatchUIInitComponent::RemoveScreenFromLayer(
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

TArray<int32> UTeamMatchUIInitComponent::BuildRerollsRemaining(
	const int32 OfferCount) const
{
	TArray<int32> Result;
	Result.Reserve(OfferCount);
	for (int32 Index = 0; Index < OfferCount; ++Index)
	{
		Result.Add(BoundInventory.IsValid()
			? BoundInventory->GetSlotRerollsRemaining(Index)
			: 0);
	}
	return Result;
}
