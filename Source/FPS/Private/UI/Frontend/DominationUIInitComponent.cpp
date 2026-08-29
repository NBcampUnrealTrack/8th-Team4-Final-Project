#include "UI/Frontend/DominationUIInitComponent.h"

#include "Augment/AugmentInventoryComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameMode/DominationGameState.h"
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
	UnbindFromGameState();
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
	RefreshClassSelectionPresentation(DominationGameState);

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

	ADominationPlayerController* PlayerController =
		Cast<ADominationPlayerController>(GetOwner());
	ADominationPlayerState* PlayerState = IsValid(PlayerController)
		? PlayerController->GetPlayerState<ADominationPlayerState>()
		: nullptr;
	if (!IsValid(PlayerState) || PlayerState->GetTeam() == EGameTeam::None)
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
	ClearInitializationRetry();
}

void UDominationUIInitComponent::RefreshClassSelectionPresentation(
	ADominationGameState* DominationGameState)
{
	if (IsValid(DominationGameState)
		&& DominationGameState->GetDominationPhase()
			== EDominationMatchPhase::ClassSelecting)
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
}

void UDominationUIInitComponent::UnbindFromGameState()
{
	if (BoundGameState.IsValid())
	{
		BoundGameState->OnDominationFlowStateChanged.RemoveAll(this);
		BoundGameState->OnMatchStateChanged.RemoveAll(this);
	}
	BoundGameState.Reset();
}

bool UDominationUIInitComponent::IsAugmentPresentationAllowed() const
{
	return BoundGameState.IsValid()
		&& BoundGameState->GetDominationPhase() == EDominationMatchPhase::InProgress;
}

bool UDominationUIInitComponent::IsSurrenderRequestAllowed() const
{
	return BoundGameState.IsValid()
		&& BoundGameState->GetDominationPhase() == EDominationMatchPhase::InProgress;
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
