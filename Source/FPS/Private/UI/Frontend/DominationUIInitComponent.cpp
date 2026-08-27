#include "UI/Frontend/DominationUIInitComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Player/DominationPlayerController.h"
#include "Player/DominationPlayerState.h"
#include "Tags/UITags.h"
#include "TimerManager.h"
#include "UI/InGame/HUD/ShooterPlayerHUD.h"
#include "UI/Layout/ShooterPrimaryGameLayout.h"
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

	if (!RootLayoutClass || !InitialWidgetClass || !InitialWidgetLayer.IsValid())
	{
		UE_LOG(LogDominationUIInit, Error,
			TEXT("Domination HUD configuration is incomplete on %s."),
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

	TryInitializePlayerHUD();
}

void UDominationUIInitComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InitializationRetryTimerHandle);
	}

	if (IsValid(InitialWidgetInstance))
	{
		if (UShooterUIManagerSubsystem* UIManager = GetUIManager())
		{
			UIManager->RemoveWidgetFromLayer(InitialWidgetLayer, InitialWidgetInstance);
		}
		InitialWidgetInstance = nullptr;
	}

	Super::EndPlay(EndPlayReason);
}

void UDominationUIInitComponent::TryInitializePlayerHUD()
{
	if (IsValid(InitialWidgetInstance))
	{
		return;
	}

	ADominationPlayerController* PlayerController =
		Cast<ADominationPlayerController>(GetOwner());
	if (!IsValid(PlayerController)
		|| !IsValid(PlayerController->GetPlayerState<ADominationPlayerState>()))
	{
		ScheduleInitializationRetry();
		return;
	}

	UShooterUIManagerSubsystem* UIManager = GetUIManager();
	if (!IsValid(UIManager) || !UIManager->IsUIInitialized())
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(InitializationRetryTimerHandle);
		}
		UE_LOG(LogDominationUIInit, Error,
			TEXT("Domination root layout became unavailable before HUD initialization."));
		return;
	}

	InitialWidgetInstance = Cast<UShooterPlayerHUD>(
		UIManager->PushWidgetToLayer(InitialWidgetLayer, InitialWidgetClass));
	if (!IsValid(InitialWidgetInstance))
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(InitializationRetryTimerHandle);
		}
		UE_LOG(LogDominationUIInit, Error,
			TEXT("Failed to create the Domination player HUD. Layer=%s WidgetClass=%s"),
			*InitialWidgetLayer.ToString(),
			*GetNameSafe(InitialWidgetClass));
		return;
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(InitializationRetryTimerHandle);
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
		&ThisClass::TryInitializePlayerHUD,
		0.1f,
		true);
}

UShooterUIManagerSubsystem* UDominationUIInitComponent::GetUIManager() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = IsValid(World) ? World->GetGameInstance() : nullptr;
	return IsValid(GameInstance)
		? GameInstance->GetSubsystem<UShooterUIManagerSubsystem>()
		: nullptr;
}
