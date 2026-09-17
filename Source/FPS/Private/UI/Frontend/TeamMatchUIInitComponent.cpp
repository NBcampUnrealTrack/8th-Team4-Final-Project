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
#include "UI/InGame/HUD/ShooterChatInputBox.h"
#include "UI/InGame/Scoreboard/ShooterScoreboard.h"
#include "UI/Settings/ShooterOptionsScreen.h"

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
	RemoveOptionsScreen();
	RemoveInGameMenu();
	RemoveSurrenderVote();
	HideAugmentSelect();
	ResetAugmentRequestContext();
	HideScoreboard();

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
	InGameMenu->OnOptionsRequested.AddUObject(this, &ThisClass::HandleOptionsRequested);
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
	InGameMenu->OnOptionsRequested.RemoveAll(this);
	InGameMenu->OnReturnToLobbyRequested.RemoveAll(this);
	InGameMenu->OnQuitGameRequested.RemoveAll(this);
	InGameMenu->OnSurrenderRequested.RemoveAll(this);
	RemoveScreenFromLayer(UITags::UI_Layer_Modal, InGameMenu);
	SetInGameMenuInstance(nullptr);
}

void UTeamMatchUIInitComponent::PresentOptionsScreen()
{
	if (IsValid(OptionsScreenInstance))
	{
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	if (!IsValid(UIManager) || !IsValid(OptionsScreenClass))
	{
		UE_LOG(LogTeamMatchUIInit, Error, TEXT("OptionsScreenClass is not configured."));
		return;
	}

	OptionsScreenInstance = Cast<UShooterOptionsScreen>(
		UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, OptionsScreenClass));
	if (!IsValid(OptionsScreenInstance))
	{
		return;
	}

	OptionsScreenInstance->OnCloseRequested.AddUObject(
		this,
		&ThisClass::HandleOptionsCloseRequested);
}

void UTeamMatchUIInitComponent::RemoveOptionsScreen()
{
	if (!IsValid(OptionsScreenInstance))
	{
		return;
	}

	OptionsScreenInstance->FinishEditing();
	OptionsScreenInstance->OnCloseRequested.RemoveAll(this);
	RemoveScreenFromLayer(UITags::UI_Layer_Modal, OptionsScreenInstance);
	OptionsScreenInstance = nullptr;
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
		// 아래 Push 경로와 동일하게 진입 연출 상태를 먼저 갱신한다.
		OnAugmentOffersPresented(InOffers);
		AugmentScreen->SetEntryAnimation(GetAugmentEntryAnimation());
		
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
	AugmentScreen->OnSelectionOutroFinished.AddUniqueDynamic(
		this,
		&ThisClass::HandleSelectionOutroFinished);
	
	OnAugmentScreenCreated(AugmentScreen);
	
	OnAugmentOffersPresented(InOffers);
	AugmentScreen->SetEntryAnimation(GetAugmentEntryAnimation());
	
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
	AugmentScreen->OnSelectionOutroFinished.RemoveDynamic(
		this,
		&ThisClass::HandleSelectionOutroFinished);
	
	RemoveScreenFromLayer(UITags::UI_Layer_Modal, AugmentScreen);
	SetAugmentSelectInstance(nullptr);

	// 화면 제거는 요청을 취소하지 않는다.
	// 해제는 ResetAugmentRequestContext() 만 한다
}

void UTeamMatchUIInitComponent::ShowScoreboard()
{
	// Guard the creation entry point too: BP/direct callers may bypass input bindings.
	if (!SupportsScoreboard())
	{
		return;
	}

	if (IsValid(ScoreboardInstance))
	{
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	if (!ensure(UIManager) || !ensure(ScoreboardClass))
	{
		return;
	}

	ScoreboardInstance = Cast<UShooterScoreboard>(
		UIManager->PushWidgetToLayer(UITags::UI_Layer_GameMenu, ScoreboardClass));
}

void UTeamMatchUIInitComponent::HideScoreboard()
{
	if (!IsValid(ScoreboardInstance))
	{
		return;
	}

	RemoveScreenFromLayer(UITags::UI_Layer_GameMenu, ScoreboardInstance);
	ScoreboardInstance = nullptr;
}

UShooterChatInputBox* UTeamMatchUIInitComponent::OpenChat()
{
	if (IsValid(ChatInputBoxInstance))
	{
		return ChatInputBoxInstance;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	if (!ensure(UIManager) || !ensure(ChatInputBoxClass))
	{
		return nullptr;
	}

	// GameMenu 레이어는 스코어보드처럼 포커스를 가져가지 않는 오버레이용이라 Action Router가
	// 리프 노드로 취급하지 않는다(입력 포커스가 안 걸림). 채팅은 실제 키보드 포커스가 필요해서
	// InGameMenu/Options/AugmentSelect와 같은 Modal 레이어를 사용한다.
	ChatInputBoxInstance = Cast<UShooterChatInputBox>(
		UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, ChatInputBoxClass));
	return ChatInputBoxInstance;
}

void UTeamMatchUIInitComponent::CloseChat()
{
	if (!IsValid(ChatInputBoxInstance))
	{
		return;
	}

	RemoveScreenFromLayer(UITags::UI_Layer_Modal, ChatInputBoxInstance);
	ChatInputBoxInstance = nullptr;
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
	
	if (bSelectionRequestPending 
		&& !(Offers.IsValidIndex(RequestedOfferIndex)
		&& Offers[RequestedOfferIndex].DefinitionId == RequestDefinitionId))
	{
		ResetAugmentRequestContext();
	}
	const UShooterAugmentSelectScreen* AugmentScreen = GetAugmentSelectInstance();
	const bool bOutroPlaying = IsValid(AugmentScreen) && AugmentScreen->IsOutroPlaying();
	
	const bool bShowAugment = IsAugmentPresentationAllowed() 
		&& !Offers.IsEmpty()
		&& !bSelectionRequestPending
		&& !bOutroPlaying;

	if (bInGameMenuRequested)
	{
		CloseModeSpecificScreensForModal();
		RemoveSurrenderVote();
		HideAugmentSelect();

		// Options는 InGameMenu의 하위 화면이다. 열려 있으면 갱신 중 다시 메뉴를 덮어쓰지 않는다.
		if (IsValid(OptionsScreenInstance))
		{
			return;
		}

		PresentInGameMenu();
		return;
	}

	RemoveOptionsScreen();
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
	else if (!bOutroPlaying)
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

void UTeamMatchUIInitComponent::HandleOptionsRequested()
{
	// 같은 CommonUI stack 위에 올려 두면 옵션을 닫을 때 기존 메뉴의 상태와 포커스가 복구된다.
	PresentOptionsScreen();

	// 클래스 미지정이나 Push 실패 시에도 인게임 메뉴가 그대로 남아 빈 Modal 상태가 되지 않는다.
	if (!IsValid(OptionsScreenInstance))
	{
		ScheduleModalRefresh();
	}
}

void UTeamMatchUIInitComponent::HandleOptionsCloseRequested()
{
	RemoveOptionsScreen();
	bInGameMenuRequested = true;
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
	if (bSelectionRequestPending || !BoundInventory.IsValid())
	{
		return;
	}
	
	const TArray<FAugmentInventoryEntry>& CurrentOffers = BoundInventory->GetOffers();
	if (!CurrentOffers.IsValidIndex(OfferIndex))
	{
		return;
	}
	
	// 상태를 바꾸고 보냄
	bSelectionRequestPending =true;
	RequestedOfferIndex = OfferIndex;
	RequestDefinitionId = CurrentOffers[OfferIndex].DefinitionId;
	
	BoundInventory->ServerSelectOffer(OfferIndex);
	// 화면은 아웃트로 끝나고 닫음.
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

void UTeamMatchUIInitComponent::HandleSelectionOutroFinished()
{
	HideAugmentSelect();
	// 아웃트로 도중 도착한 다음 후보가 뜨기위함
	ScheduleModalRefresh();
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

void UTeamMatchUIInitComponent::ResetAugmentRequestContext()
{
	bSelectionRequestPending = false;
	RequestedOfferIndex = INDEX_NONE;
	RequestDefinitionId = FPrimaryAssetId();
}

bool UTeamMatchUIInitComponent::IsAugmentSelectionRequestPending() const
{
	return bSelectionRequestPending;
}

EAugmentSelectEntry UTeamMatchUIInitComponent::GetAugmentEntryAnimation() const
{
	return EAugmentSelectEntry::Intro;
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
