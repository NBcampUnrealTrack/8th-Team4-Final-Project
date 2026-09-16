#include "UI/Frontend/CombatUIInitComponent.h"

#include "Augment/AugmentInventoryComponent.h"
#include "Character/ShooterCharacter.h"
#include "Engine/World.h"
#include "Player/ShooterPlayerController.h"
#include "Player/DominationPlayerState.h"
#include "Tags/UITags.h"
#include "UI/InGame/Domination/DominationClassSelectScreen.h"
#include "UI/InGame/HUD/ShooterPlayerHUD.h"
#include "UI/InGame/Menu/ShooterInGameMenuScreen.h"
#include "UI/InGame/Menu/ShooterSurrenderVoteScreen.h"
#include "UI/InGame/ShooterAugmentSelectScreen.h"
#include "UI/Layout/ShooterPrimaryGameLayout.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"
#include "Widgets/CommonActivatableWidgetContainer.h"

DEFINE_LOG_CATEGORY_STATIC(LogCombatUIInit, Log, All);

UCombatUIInitComponent::UCombatUIInitComponent()
{
    InitialWidgetLayer = UITags::UI_Layer_Game;
}

void UCombatUIInitComponent::BeginPlay()
{
	Super::BeginPlay();

	AShooterPlayerController* PlayerController =
		Cast<AShooterPlayerController>(GetOwner());
	if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
	{
		return;
	}

	if (!RootLayoutClass
		|| !InitialWidgetClass
		|| !InitialWidgetLayer.IsValid()
		|| !ClassSelectionWidgetClass)
	{
		UE_LOG(LogCombatUIInit, Error,
			TEXT("Combat UI configuration is incomplete on %s."),
			*GetNameSafe(PlayerController));
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	if (!IsValid(UIManager))
	{
		UE_LOG(LogCombatUIInit, Error,
			TEXT("Shooter UI manager is unavailable for %s."),
			*GetNameSafe(PlayerController));
		return;
	}

	UIManager->InitializeUI(PlayerController, RootLayoutClass);
	if (!UIManager->IsUIInitialized())
	{
		UE_LOG(LogCombatUIInit, Error,
			TEXT("Failed to initialize the combat root layout. RootLayoutClass=%s"),
			*GetNameSafe(RootLayoutClass));
		return;
	}

	RefreshCombatUI();
}

void UCombatUIInitComponent::ToggleAugmentSelect()
{
	if (!IsAugmentPresentationSettled())
	{
		return;
	}
	
	const UShooterAugmentSelectScreen* AS = GetAugmentSelectInstance();
	if (IsAugmentSelectionRequestPending()
		|| (IsValid(AS) && AS->IsOutroPlaying()))
	{
		return;
	}
		
	UAugmentInventoryComponent* Inv = GetBoundAugmentInventory();
	if (!IsValid(Inv)
		|| Inv->GetOffers().IsEmpty()
		|| !IsAugmentModeActive())
	{
		return;
	}
	
	SetAugmentSelectDismissed(!bAugmentSelectDismissed);
}

bool UCombatUIInitComponent::IsAugmentSelectDismissed() const
{
	return bAugmentSelectDismissed;
}

bool UCombatUIInitComponent::IsAugmentPresentationSettled() const
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

void UCombatUIInitComponent::PresentClassSelectionScreen()
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
		UE_LOG(LogCombatUIInit, Error,
			TEXT("Failed to create the combat class selection screen. WidgetClass=%s"),
			*GetNameSafe(ClassSelectionWidgetClass));
	}
    else
    {
        OnClassSelectionPresented();
    }
}

void UCombatUIInitComponent::RemoveClassSelectionScreen()
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

void UCombatUIInitComponent::HandleParticipationStateChanged(
	const EPlayerParticipationState)
{
	RefreshCombatUI();
}

void UCombatUIInitComponent::HandleCombatClassChanged(
	const EDominationCombatClass)
{
	RefreshCombatUI();
}

void UCombatUIInitComponent::HandleAugmentDismissRequested()
{
	ToggleAugmentSelect();
}

void UCombatUIInitComponent::HandleModalDisplayedWidgetChanged(UCommonActivatableWidget* DisplayedWidget)
{
	OnAugmentPresentationChanged.Broadcast();
}

void UCombatUIInitComponent::BindModalContainer()
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

void UCombatUIInitComponent::UnbindModalContainer()
{
	if (UCommonActivatableWidgetContainerBase* Modal =
		BoundModalContainer.Get())
	{
		Modal->OnDisplayedWidgetChanged().RemoveAll(this);
	}

	BoundModalContainer.Reset();
}

void UCombatUIInitComponent::ScheduleInitializationRetry()
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
		&ThisClass::RefreshCombatUI,
		0.1f,
		true);
}

void UCombatUIInitComponent::ClearInitializationRetry()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InitializationRetryTimerHandle);
	}
}

void UCombatUIInitComponent::BindToPlayerState(
	ADominationPlayerState* PlayerState)
{
	if (BoundPlayerState.Get() == PlayerState)
	{
		return;
	}

	if (IsTeamMatchUIReady()) { ShutdownTeamMatchUI(); }
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
    PlayerState->OnPawnSet.AddUniqueDynamic(this, &ThisClass::HandleCombatPawnSet);
}

void UCombatUIInitComponent::UnbindFromPlayerState()
{
	if (ADominationPlayerState* PlayerState = BoundPlayerState.Get())
	{
		PlayerState->OnPawnSet.RemoveDynamic(this, &ThisClass::HandleCombatPawnSet);
		PlayerState->OnParticipationStateChanged.RemoveDynamic(
			this,
			&ThisClass::HandleParticipationStateChanged);
		PlayerState->OnCombatClassChanged.RemoveDynamic(
			this,
			&ThisClass::HandleCombatClassChanged);
	}

	BoundPlayerState.Reset();
}

void UCombatUIInitComponent::OnAugmentScreenCreated(UShooterAugmentSelectScreen* Screen)
{
	Super::OnAugmentScreenCreated(Screen);
	
	if (!IsValid(Screen))
	{
		return;
	}
	
	Screen->OnDismissRequested.AddUniqueDynamic(this, &ThisClass::HandleAugmentDismissRequested);
	Screen->SetDismissAvailable(true);
}

void UCombatUIInitComponent::OnAugmentOffersPresented(const TArray<FAugmentInventoryEntry>& InOffers)
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

EAugmentSelectEntry UCombatUIInitComponent::GetAugmentEntryAnimation() const
{
	return bHasDismissedThisSelection
		? EAugmentSelectEntry::Resume
		: EAugmentSelectEntry::Intro;
}

TSubclassOf<UShooterInGameMenuScreen>
UCombatUIInitComponent::GetInGameMenuWidgetClass() const
{
	return InGameMenuClass;
}

TSubclassOf<UShooterSurrenderVoteScreen>
UCombatUIInitComponent::GetSurrenderVoteWidgetClass() const
{
	return SurrenderVoteClass;
}

TSubclassOf<UShooterAugmentSelectScreen>
UCombatUIInitComponent::GetAugmentSelectWidgetClass() const
{
	return AugmentSelectClass;
}

UShooterInGameMenuScreen* UCombatUIInitComponent::GetInGameMenuInstance() const
{
	return InGameMenuInstance;
}

UCommonActivatableWidgetContainerBase* UCombatUIInitComponent::GetModalContainer() const
{
	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	UShooterPrimaryGameLayout* Layout = IsValid(UIManager) ? UIManager->GetRootLayout() : nullptr;
	
	return IsValid(Layout) ? Layout->GetLayerContainer(UITags::UI_Layer_Modal) : nullptr;
}

void UCombatUIInitComponent::SetAugmentSelectDismissed(bool bDismissed)
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

void UCombatUIInitComponent::SetInGameMenuInstance(
	UShooterInGameMenuScreen* Instance)
{
	InGameMenuInstance = Instance;
}

UShooterSurrenderVoteScreen* UCombatUIInitComponent::GetSurrenderVoteInstance() const
{
	return SurrenderVoteInstance;
}

void UCombatUIInitComponent::SetSurrenderVoteInstance(
	UShooterSurrenderVoteScreen* Instance)
{
	SurrenderVoteInstance = Instance;
}

UShooterAugmentSelectScreen* UCombatUIInitComponent::GetAugmentSelectInstance() const
{
	return AugmentSelectInstance;
}

void UCombatUIInitComponent::SetAugmentSelectInstance(
	UShooterAugmentSelectScreen* Instance)
{
	AugmentSelectInstance = Instance;
}

void UCombatUIInitComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearInitializationRetry();
    UnbindFromPlayerState();
    UnbindModalContainer();
    RemoveClassSelectionScreen();
    RemoveCombatHUD();
    Super::EndPlay(EndPlayReason);
}

void UCombatUIInitComponent::RefreshCombatUI()
{
    if (!PrepareModeBindings())
    {
        ScheduleInitializationRetry();
        return;
    }
    AShooterPlayerController* LocalController = Cast<AShooterPlayerController>(GetOwner());
    ADominationPlayerState* ClassState = IsValid(LocalController)
        ? LocalController->GetPlayerState<ADominationPlayerState>() : nullptr;
    if (!IsValid(ClassState))
    {
        RemoveClassSelectionScreen();
        ScheduleInitializationRetry();
        return;
    }
    BindToPlayerState(ClassState);
    if (ShouldPresentClassSelection(*ClassState))
    {
        PresentClassSelectionScreen();
    }
    else
    {
        RemoveClassSelectionScreen();
    }
    if (!CanInitializeCombatHUD(*ClassState))
    {
        ClearInitializationRetry();
        return;
    }
    if (IsValid(InitialWidgetInstance) && IsTeamMatchUIReady())
    {
        NotifyModeUIStateChanged();
        ClearInitializationRetry();
        return;
    }
    if (ClassState->GetTeam() == EGameTeam::None)
    {
        ScheduleInitializationRetry();
        return;
    }
    UShooterUIManagerSubsystem* UIManager = GetUIManager();
    if (!IsValid(UIManager) || !UIManager->IsUIInitialized())
    {
        ClearInitializationRetry();
        UE_LOG(LogCombatUIInit, Error, TEXT("Combat root layout became unavailable before HUD initialization."));
        return;
    }
    if (!IsValid(InitialWidgetInstance))
    {
        InitialWidgetInstance = Cast<UShooterPlayerHUD>(
            UIManager->PushWidgetToLayer(InitialWidgetLayer, InitialWidgetClass));
        if (!IsValid(InitialWidgetInstance))
        {
            ClearInitializationRetry();
            UE_LOG(LogCombatUIInit, Error, TEXT("Could not create combat HUD: %s (layer %s)."),
                *GetNameSafe(InitialWidgetClass), *InitialWidgetLayer.ToString());
            return;
        }
        OnCombatHUDCreated(InitialWidgetInstance);
    }
    UAugmentInventoryComponent* Inventory = ClassState->GetAugmentInventory();
    if (!IsValid(Inventory))
    {
        ScheduleInitializationRetry();
        return;
    }
    // Pawn changes rebuild only the HUD, not menu/offer delegates.
    if (!IsTeamMatchUIReady())
    {
        InitializeTeamMatchUI(LocalController, Inventory);
        BindModalContainer();
    }
    NotifyModeUIStateChanged();
    ClearInitializationRetry();
}

ADominationPlayerState* UCombatUIInitComponent::GetCombatClassPlayerState() const
{
    return BoundPlayerState.Get();
}

bool UCombatUIInitComponent::IsAugmentPresentationAllowed() const
{
    return IsAugmentModeActive() && !bAugmentSelectDismissed;
}

void UCombatUIInitComponent::ResetAugmentPresentationHistory()
{
    SetAugmentSelectDismissed(false);
    bHasDismissedThisSelection = false;
    LastPresentedOfferIds.Reset();
}

void UCombatUIInitComponent::RemoveCombatHUD()
{
    if (IsValid(InitialWidgetInstance))
    {
        RemoveScreenFromLayer(InitialWidgetLayer, InitialWidgetInstance);
        InitialWidgetInstance = nullptr;
    }
}

void UCombatUIInitComponent::HandleCombatPawnSet(APlayerState*, APawn* NewPawn, APawn* OldPawn)
{
    // The previous HUD held the previous CombatComponent's ADS delegate.
    // Ignore spectator/null transitions; rebuild when a new combat avatar exists.
    if (NewPawn != OldPawn && IsValid(Cast<AShooterCharacter>(NewPawn)))
    {
        RemoveCombatHUD();
        RefreshCombatUI();
    }
}

void UCombatUIInitComponent::NotifyClassSelectionRequestFailed(const FText& Reason)
{
    if (IsValid(ClassSelectionWidgetInstance))
    {
        ClassSelectionWidgetInstance->NotifySelectionFailed(Reason);
    }
}
