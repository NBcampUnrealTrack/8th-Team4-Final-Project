#include "UI/Frontend/DominationUIInitComponent.h"

#include "Augment/AugmentInventoryComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameMode/State/DominationGameState.h"
#include "Player/DominationPlayerController.h"
#include "Player/DominationPlayerState.h"
#include "Tags/UITags.h"
#include "TimerManager.h"
#include "UI/InGame/Domination/DominationClassSelectScreen.h"
#include "UI/InGame/HUD/ShooterPlayerHUD.h"
#include "UI/InGame/Menu/ShooterInGameMenuScreen.h"
#include "UI/InGame/Menu/ShooterSurrenderVoteScreen.h"
#include "UI/InGame/ShooterAugmentSelectScreen.h"
#include "UI/Layout/ShooterPrimaryGameLayout.h"
#include "UI/OutGame/MatchResult/ShooterMatchResultSubsystem.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

DEFINE_LOG_CATEGORY_STATIC(LogDominationUIInit, Log, All);

UDominationUIInitComponent::UDominationUIInitComponent()
{
	InitialWidgetLayer = UITags::UI_Layer_Game;
}

void UDominationUIInitComponent::BeginPlay()
{
	Super::BeginPlay();

	ADominationPlayerController* PlayerController =
		Cast<ADominationPlayerController>(GetOwner());
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	if (!RootLayoutClass
		|| !InitialWidgetClass
		|| !InitialWidgetLayer.IsValid()
		|| !ClassSelectionWidgetClass)
	{
		UE_LOG(LogDominationUIInit, Error,
			TEXT("Domination UI configuration is incomplete on %s."),
			*GetNameSafe(PlayerController));
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	if (!IsValid(UIManager))
	{
		UE_LOG(LogDominationUIInit, Error,
			TEXT("Shooter UI manager is unavailable for %s."),
			*GetNameSafe(PlayerController));
		return;
	}

	UIManager->InitializeUI(PlayerController, RootLayoutClass);
	if (!UIManager->IsUIInitialized())
	{
		UE_LOG(LogDominationUIInit, Error,
			TEXT("Failed to initialize the Domination root layout. RootLayoutClass=%s"),
			*GetNameSafe(RootLayoutClass));
		return;
	}

	RefreshDominationUI();
}

void UDominationUIInitComponent::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	ClearInitializationRetry();
	UnbindFromPlayerState();
	UnbindFromGameState();
	UnbindModalContainer();
	RemoveClassSelectionScreen();

	if (IsValid(InitialWidgetInstance))
	{
		if (UShooterUIManagerSubsystem* UIManager = GetUIManager())
		{
			UIManager->RemoveWidgetFromLayer(
				InitialWidgetLayer,
				InitialWidgetInstance);
		}
		InitialWidgetInstance = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UDominationUIInitComponent::ToggleAugmentSelect()
{
	if (!IsAugmentPresentationSettled())
	{
		return;
	}
	
	UAugmentInventoryComponent* Inv = GetBoundAugmentInventory();
	if (!IsValid(Inv)
		|| Inv->GetOffers().IsEmpty()
		|| !BoundGameState.IsValid()
		|| BoundGameState->GetDominationPhase() != EDominationMatchPhase::InProgress)
	{
		return;
	}
	
	SetAugmentSelectDismissed(!bAugmentSelectDismissed);
}

bool UDominationUIInitComponent::IsAugmentSelectDismissed() const
{
	return bAugmentSelectDismissed;
}

bool UDominationUIInitComponent::IsAugmentPresentationSettled() const
{
	UCommonActivatableWidgetContainerBase* Modal = GetModalContainer();
	if (!IsValid(Modal))
	{
		return false;
	}
	
	const UCommonActivatableWidget* StackTop = Modal->GetActiveWidget();
	const bool bStackShowsAugment = IsValid(StackTop) && StackTop->IsA<UShooterAugmentSelectScreen>();
	const UAugmentInventoryComponent* Inv = GetBoundAugmentInventory();
	const bool bShouldShowAugment = 
		IsAugmentPresentationAllowed() 
		&& IsValid(Inv)
		&& !Inv->GetOffers().IsEmpty();
	
	return bStackShowsAugment == bShouldShowAugment;
}

void UDominationUIInitComponent::RefreshDominationUI()
{
	ADominationGameState* DominationGameState = GetWorld()
		? GetWorld()->GetGameState<ADominationGameState>()
		: nullptr;
	if (!IsValid(DominationGameState))
	{
		ScheduleInitializationRetry();
		return;
	}

	BindToGameState(DominationGameState);

	ADominationPlayerController* PlayerController =
		Cast<ADominationPlayerController>(GetOwner());
	ADominationPlayerState* PlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<ADominationPlayerState>()
		: nullptr;
	if (!IsValid(PlayerState))
	{
		RemoveClassSelectionScreen();
		ScheduleInitializationRetry();
		return;
	}

	BindToPlayerState(PlayerState);
	RefreshClassSelectionPresentation(DominationGameState, PlayerState);

	const EDominationMatchPhase Phase = DominationGameState->GetDominationPhase();
	if (Phase != EDominationMatchPhase::InProgress
		&& Phase != EDominationMatchPhase::MatchEnded)
	{
		ClearInitializationRetry();
		return;
	}

	if (IsValid(InitialWidgetInstance) && IsTeamMatchUIReady())
	{
		ClearInitializationRetry();
		return;
	}

	if (PlayerState->GetTeam() == EGameTeam::None)
	{
		ScheduleInitializationRetry();
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	if (!IsValid(UIManager) || !UIManager->IsUIInitialized())
	{
		ClearInitializationRetry();
		UnbindFromGameState();
		UE_LOG(LogDominationUIInit, Error,
			TEXT("Domination root layout became unavailable before HUD initialization."));
		return;
	}

	if (!IsValid(InitialWidgetInstance))
	{
		InitialWidgetInstance = Cast<UShooterPlayerHUD>(
			UIManager->PushWidgetToLayer(InitialWidgetLayer, InitialWidgetClass));
		if (!IsValid(InitialWidgetInstance))
		{
			ClearInitializationRetry();
			UnbindFromGameState();
			UE_LOG(LogDominationUIInit, Error,
				TEXT("Failed to create the Domination player HUD. Layer=%s WidgetClass=%s"),
				*InitialWidgetLayer.ToString(),
				*GetNameSafe(InitialWidgetClass));
			return;
		}
	}

	UAugmentInventoryComponent* Inventory = PlayerState->GetAugmentInventory();
	if (!IsValid(Inventory))
	{
		ScheduleInitializationRetry();
		return;
	}

	InitializeTeamMatchUI(PlayerController, Inventory);
	BindModalContainer();
	ClearInitializationRetry();
}

void UDominationUIInitComponent::RefreshClassSelectionPresentation(
	ADominationGameState* DominationGameState,
	ADominationPlayerState* PlayerState)
{
	if (!IsValid(DominationGameState) || !IsValid(PlayerState))
	{
		RemoveClassSelectionScreen();
		return;
	}

	const EDominationMatchPhase Phase = DominationGameState->GetDominationPhase();
	const EPlayerParticipationState ParticipationState =
		PlayerState->GetParticipationState();
	const bool bInitialParticipantSelecting =
		Phase == EDominationMatchPhase::ClassSelecting
		&& ParticipationState == EPlayerParticipationState::WaitingForMatchStart;
	const bool bJoinInProgressSelecting =
		Phase == EDominationMatchPhase::InProgress
		&& ParticipationState == EPlayerParticipationState::WaitingForNextSpawn
		&& !PlayerState->HasSelectedCombatClass();

	if (bInitialParticipantSelecting || bJoinInProgressSelecting)
	{
		PresentClassSelectionScreen();
		return;
	}

	RemoveClassSelectionScreen();
}

void UDominationUIInitComponent::PresentClassSelectionScreen()
{
	if (IsValid(ClassSelectionWidgetInstance))
	{
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	if (!IsValid(UIManager) || !ClassSelectionWidgetClass)
	{
		return;
	}

	ClassSelectionWidgetInstance = Cast<UDominationClassSelectScreen>(
		UIManager->PushWidgetToLayer(
			UITags::UI_Layer_Menu,
			ClassSelectionWidgetClass));
	if (!IsValid(ClassSelectionWidgetInstance))
	{
		UE_LOG(LogDominationUIInit, Error,
			TEXT("Failed to create the Domination class selection screen. WidgetClass=%s"),
			*GetNameSafe(ClassSelectionWidgetClass));
	}
}

void UDominationUIInitComponent::RemoveClassSelectionScreen()
{
	if (!IsValid(ClassSelectionWidgetInstance))
	{
		return;
	}

	RemoveScreenFromLayer(
		UITags::UI_Layer_Menu,
		ClassSelectionWidgetInstance);
	ClassSelectionWidgetInstance = nullptr;
}

void UDominationUIInitComponent::HandleDominationFlowStateChanged()
{
	RefreshDominationUI();
	TryCaptureMatchResult();

	if (IsTeamMatchUIReady())
	{
		NotifyModeUIStateChanged();
	}
}

void UDominationUIInitComponent::HandleMatchStateChanged()
{
	TryCaptureMatchResult();
	
	// 새 매치가 InProgress 로 진입하는 순간, 이전 매치에서 남은
	// 증강 선택 보류 상태와 진입 연출 이력을 초기화한다.
	if (!BoundGameState.IsValid())
	{
		return;
	}
	
	const EDominationMatchPhase Phase = BoundGameState->GetDominationPhase();
	if (Phase == LastObservedMatchPhase)
	{
		return;
	}
	LastObservedMatchPhase = Phase;

	// 진입 순간에만 초기화
	if (Phase != EDominationMatchPhase::InProgress)
	{
		return;                     
	}

	SetAugmentSelectDismissed(false);
	bHasDismissedThisSelection = false;
	LastPresentedOfferIds.Reset();
	LastObservedRewardRound = 0;
}

void UDominationUIInitComponent::HandleParticipationStateChanged(
	const EPlayerParticipationState)
{
	RefreshDominationUI();
}

void UDominationUIInitComponent::HandleCombatClassChanged(
	const EDominationCombatClass)
{
	RefreshDominationUI();
}

void UDominationUIInitComponent::HandleRewardRoundChanged(EGameTeam Team, int32 NewScore)
{
	const int32 Round = BoundGameState.IsValid()
		? BoundGameState->GetCurrentRoundNumber()
		: 0;
	if (Round == LastObservedRewardRound)
	{
		return;
	}
	
	LastObservedRewardRound = Round;
	
	SetAugmentSelectDismissed(false);
}

void UDominationUIInitComponent::HandleAugmentDismissRequested()
{
	ToggleAugmentSelect();
}

void UDominationUIInitComponent::HandleModalDisplayedWidgetChanged(UCommonActivatableWidget* DisplayedWidget)
{
	OnAugmentPresentationChanged.Broadcast();
}

void UDominationUIInitComponent::BindModalContainer()
{
	UCommonActivatableWidgetContainerBase* Modal = GetModalContainer();
	
	if (BoundModalContainer.Get() == Modal)
	{
		return;
	}
	
	UnbindModalContainer();
	
	if (!IsValid(Modal))
	{
		return;
	}
	
	BoundModalContainer = Modal;
	
	Modal->OnDisplayedWidgetChanged().AddUObject(
		this,
		&ThisClass::HandleModalDisplayedWidgetChanged);
}

void UDominationUIInitComponent::UnbindModalContainer()
{
	if (UCommonActivatableWidgetContainerBase* Modal =
		BoundModalContainer.Get())
	{
		Modal->OnDisplayedWidgetChanged().RemoveAll(this);
	}

	BoundModalContainer.Reset();
}

void UDominationUIInitComponent::TryCaptureMatchResult()
{
	if (BoundGameState.IsValid()
		&& BoundGameState->GetDominationPhase() == EDominationMatchPhase::MatchEnded
		&& BoundGameState->GetMatchWinner() != EGameTeam::None)
	{
		UGameInstance* GameInstance = GetWorld()
			? GetWorld()->GetGameInstance()
			: nullptr;
		UShooterMatchResultSubsystem* MatchResultSubsystem = IsValid(GameInstance)
			? GameInstance->GetSubsystem<UShooterMatchResultSubsystem>()
			: nullptr;
		if (IsValid(MatchResultSubsystem))
		{
			MatchResultSubsystem->CaptureSnapshot(BoundGameState.Get());
		}
	}
}

void UDominationUIInitComponent::ScheduleInitializationRetry()
{
	UWorld* World = GetWorld();
	if (!IsValid(World)
		|| World->GetTimerManager().IsTimerActive(InitializationRetryTimerHandle))
	{
		return;
	}

	World->GetTimerManager().SetTimer(
		InitializationRetryTimerHandle,
		this,
		&ThisClass::RefreshDominationUI,
		0.1f,
		true);
}

void UDominationUIInitComponent::ClearInitializationRetry()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InitializationRetryTimerHandle);
	}
}

void UDominationUIInitComponent::BindToGameState(
	ADominationGameState* DominationGameState)
{
	if (BoundGameState.Get() == DominationGameState)
	{
		return;
	}

	UnbindFromGameState();
	BoundGameState = DominationGameState;
	DominationGameState->OnDominationFlowStateChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleDominationFlowStateChanged);
	DominationGameState->OnMatchStateChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleMatchStateChanged);
	DominationGameState->OnTeamScoreChanged.AddUniqueDynamic(
		this, 
		&ThisClass::HandleRewardRoundChanged);
}

void UDominationUIInitComponent::UnbindFromGameState()
{
	if (BoundGameState.IsValid())
	{
		BoundGameState->OnDominationFlowStateChanged.RemoveAll(this);
		BoundGameState->OnMatchStateChanged.RemoveAll(this);
		BoundGameState->OnTeamScoreChanged.RemoveAll(this);
	}
	BoundGameState.Reset();
}

void UDominationUIInitComponent::BindToPlayerState(
	ADominationPlayerState* PlayerState)
{
	if (BoundPlayerState.Get() == PlayerState)
	{
		return;
	}

	UnbindFromPlayerState();
	if (!IsValid(PlayerState))
	{
		return;
	}

	BoundPlayerState = PlayerState;
	PlayerState->OnParticipationStateChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleParticipationStateChanged);
	PlayerState->OnCombatClassChanged.AddUniqueDynamic(
		this,
		&ThisClass::HandleCombatClassChanged);
}

void UDominationUIInitComponent::UnbindFromPlayerState()
{
	if (ADominationPlayerState* PlayerState = BoundPlayerState.Get())
	{
		PlayerState->OnParticipationStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleParticipationStateChanged);
		PlayerState->OnCombatClassChanged.RemoveDynamic(
			this,
			&ThisClass::HandleCombatClassChanged);
	}

	BoundPlayerState.Reset();
}

bool UDominationUIInitComponent::IsAugmentPresentationAllowed() const
{
	return BoundGameState.IsValid()
		&& BoundGameState->GetDominationPhase() == EDominationMatchPhase::InProgress
		&& !bAugmentSelectDismissed;
}

bool UDominationUIInitComponent::IsSurrenderRequestAllowed() const
{
	return BoundGameState.IsValid()
		&& BoundGameState->GetDominationPhase() == EDominationMatchPhase::InProgress;
}

void UDominationUIInitComponent::OnAugmentScreenCreated(UShooterAugmentSelectScreen* Screen)
{
	Super::OnAugmentScreenCreated(Screen);
	
	if (!IsValid(Screen))
	{
		return;
	}
	
	Screen->OnDismissRequested.AddUniqueDynamic(this, &ThisClass::HandleAugmentDismissRequested);
	Screen->SetDismissAvailable(true);
}

void UDominationUIInitComponent::OnAugmentOffersPresented(const TArray<FAugmentInventoryEntry>& InOffers)
{
	Super::OnAugmentOffersPresented(InOffers);
	
	TArray<FPrimaryAssetId> CurrentOfferIds;
	CurrentOfferIds.Reserve(InOffers.Num());
	
	for (const FAugmentInventoryEntry& Offer : InOffers)
	{
		CurrentOfferIds.Add(Offer.DefinitionId);
	}
	
	bool bIsNewSelection = false;
	
	if (LastPresentedOfferIds.IsEmpty())
	{
		// 비었으면 첫 등장
		bIsNewSelection = true;
	}
	else if (LastPresentedOfferIds.Num() != CurrentOfferIds.Num())
	{
		// 후보 수가 달라도 첫 등장
		bIsNewSelection = true;
	}
	else
	{
		// 후보 개수는 같으므로 슬롯별 변경 개수로 리로/새 묶음을 구분한다.
		int32 DifferentCount = 0;
		for (int32 i = 0; i < CurrentOfferIds.Num(); ++i)
		{
			if (CurrentOfferIds[i] != LastPresentedOfferIds[i])
			{
				++DifferentCount;
			}
		}
		
		// 리롤은 한 슬롯만 바꾼다. 둘 이상이면 새 묶음이다
		bIsNewSelection = (DifferentCount >= 2);
	}
	
	if (bIsNewSelection)
	{
		bHasDismissedThisSelection = false;
	}
	
	LastPresentedOfferIds = MoveTemp(CurrentOfferIds);
}

EAugmentSelectEntry UDominationUIInitComponent::GetAugmentEntryAnimation() const
{
	return bHasDismissedThisSelection
		? EAugmentSelectEntry::Resume
		: EAugmentSelectEntry::Intro;
}

TSubclassOf<UShooterInGameMenuScreen>
UDominationUIInitComponent::GetInGameMenuWidgetClass() const
{
	return InGameMenuClass;
}

TSubclassOf<UShooterSurrenderVoteScreen>
UDominationUIInitComponent::GetSurrenderVoteWidgetClass() const
{
	return SurrenderVoteClass;
}

TSubclassOf<UShooterAugmentSelectScreen>
UDominationUIInitComponent::GetAugmentSelectWidgetClass() const
{
	return AugmentSelectClass;
}

UShooterInGameMenuScreen* UDominationUIInitComponent::GetInGameMenuInstance() const
{
	return InGameMenuInstance;
}

UCommonActivatableWidgetContainerBase* UDominationUIInitComponent::GetModalContainer() const
{
	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	UShooterPrimaryGameLayout* Layout = IsValid(UIManager) ? UIManager->GetRootLayout() : nullptr;
	
	return IsValid(Layout) ? Layout->GetLayerContainer(UITags::UI_Layer_Modal) : nullptr;
}

void UDominationUIInitComponent::SetAugmentSelectDismissed(bool bDismissed)
{
	if (bAugmentSelectDismissed == bDismissed)
	{
		return;
	}
	
	bAugmentSelectDismissed = bDismissed;
	
	if (bDismissed)
	{
		bHasDismissedThisSelection = true;
	}
	
	OnAugmentDismissStateChanged.Broadcast();
	NotifyModeUIStateChanged();
}

void UDominationUIInitComponent::SetInGameMenuInstance(
	UShooterInGameMenuScreen* Instance)
{
	InGameMenuInstance = Instance;
}

UShooterSurrenderVoteScreen* UDominationUIInitComponent::GetSurrenderVoteInstance() const
{
	return SurrenderVoteInstance;
}

void UDominationUIInitComponent::SetSurrenderVoteInstance(
	UShooterSurrenderVoteScreen* Instance)
{
	SurrenderVoteInstance = Instance;
}

UShooterAugmentSelectScreen* UDominationUIInitComponent::GetAugmentSelectInstance() const
{
	return AugmentSelectInstance;
}

void UDominationUIInitComponent::SetAugmentSelectInstance(
	UShooterAugmentSelectScreen* Instance)
{
	AugmentSelectInstance = Instance;
}
