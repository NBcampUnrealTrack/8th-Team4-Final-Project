#include "GameFlow/Bomb/BombPlayerController.h"

#include "Blueprint/UserWidget.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFlow/Bomb/BombGameMode.h"
#include "GameFlow/Bomb/DataJackInputConfig.h"
#include "GameFlow/Common/BaseTeamPlayerState.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UI/GameFlow/BaseBombGameFlowWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogBombPlayerController, Log, All);

void ABombPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocalController())
	{
		UDataJackInputConfig* ObjectiveInputConfig = DataJackInputConfig.Get();
		const bool bHasValidObjectiveInput = IsValid(ObjectiveInputConfig)
			&& IsValid(ObjectiveInputConfig->MappingContext.Get())
			&& IsValid(ObjectiveInputConfig->InteractAction.Get())
			&& IsValid(ObjectiveInputConfig->DropAction.Get());
		if (!bHasValidObjectiveInput)
		{
			UE_LOG(LogBombPlayerController, Error, TEXT("%s requires a complete DataJackInputConfig."), *GetNameSafe(this));
		}

		if (!GameFlowWidgetClass)
		{
			UE_LOG(LogBombPlayerController, Error, TEXT("%s requires a GameFlowWidgetClass."), *GetNameSafe(this));
		}
	}

	CreateGameFlowWidget();
}

void ABombPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ApplyObjectiveInteractionActive(false);
	SetDataJackMappingContextEnabled(false);
	RemoveGameFlowWidget();
	Super::EndPlay(EndPlayReason);
}

bool ABombPlayerController::IsObjectiveInteractionActive() const
{
	return bIsObjectiveInteractionActive;
}

bool ABombPlayerController::CanUseCombatInput() const
{
	return Super::CanUseCombatInput() && !bIsObjectiveInteractionActive;
}

void ABombPlayerController::SetObjectiveInteractionActive(const bool bActive)
{
	if (!HasAuthority())
	{
		return;
	}

	ApplyObjectiveInteractionActive(bActive);
	if (!IsLocalController())
	{
		ClientSetObjectiveInteractionActive(bActive);
	}
}

void ABombPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	UDataJackInputConfig* ObjectiveInputConfig = DataJackInputConfig.Get();
	if (!IsValid(EnhancedInputComponent) || !IsValid(ObjectiveInputConfig))
	{
		return;
	}

	if (UInputAction* InteractAction = ObjectiveInputConfig->InteractAction.Get())
	{
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ABombPlayerController::Input_DataJackInteractionStarted);
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Completed, this, &ABombPlayerController::Input_DataJackInteractionStopped);
		EnhancedInputComponent->BindAction(InteractAction, ETriggerEvent::Canceled, this, &ABombPlayerController::Input_DataJackInteractionStopped);
	}

	if (UInputAction* DropAction = ObjectiveInputConfig->DropAction.Get())
	{
		EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Started, this, &ABombPlayerController::Input_DataJackDropStarted);
		EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Completed, this, &ABombPlayerController::Input_DataJackDropStopped);
		EnhancedInputComponent->BindAction(DropAction, ETriggerEvent::Canceled, this, &ABombPlayerController::Input_DataJackDropStopped);
	}
}

void ABombPlayerController::HandleRoundInputStateChanged(const bool bEnabled)
{
	Super::HandleRoundInputStateChanged(bEnabled);

	if (!bEnabled)
	{
		if (HasAuthority())
		{
			if (ABombGameMode* BombGameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ABombGameMode>() : nullptr)
			{
				BombGameMode->StopDataJackInteraction(this);
				BombGameMode->StopDataJackDrop(this);
			}
		}

		ApplyObjectiveInteractionActive(false);
	}

	SetDataJackMappingContextEnabled(bEnabled);
}

void ABombPlayerController::ClientSetObjectiveInteractionActive_Implementation(const bool bActive)
{
	ApplyObjectiveInteractionActive(bActive);
}

void ABombPlayerController::ServerStartDataJackInteraction_Implementation()
{
	ABombGameMode* BombGameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ABombGameMode>() : nullptr;
	const bool bInteractionStarted = IsValid(BombGameMode) && BombGameMode->StartDataJackInteraction(this);
	SetObjectiveInteractionActive(bInteractionStarted);
}

void ABombPlayerController::ServerStopDataJackInteraction_Implementation()
{
	if (ABombGameMode* BombGameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ABombGameMode>() : nullptr)
	{
		BombGameMode->StopDataJackInteraction(this);
	}

	SetObjectiveInteractionActive(false);
}

void ABombPlayerController::ServerStartDataJackDrop_Implementation()
{
	if (ABombGameMode* BombGameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ABombGameMode>() : nullptr)
	{
		BombGameMode->StartDataJackDrop(this);
	}
}

void ABombPlayerController::ServerStopDataJackDrop_Implementation()
{
	if (ABombGameMode* BombGameMode = GetWorld() != nullptr ? GetWorld()->GetAuthGameMode<ABombGameMode>() : nullptr)
	{
		BombGameMode->StopDataJackDrop(this);
	}
}

void ABombPlayerController::CreateGameFlowWidget()
{
	if (!IsLocalController() || IsValid(GameFlowWidget) || !GameFlowWidgetClass)
	{
		return;
	}

	GameFlowWidget = CreateWidget<UBaseBombGameFlowWidget>(this, GameFlowWidgetClass);
	if (IsValid(GameFlowWidget))
	{
		GameFlowWidget->AddToPlayerScreen();
	}
}

void ABombPlayerController::RemoveGameFlowWidget()
{
	if (!IsValid(GameFlowWidget))
	{
		return;
	}

	GameFlowWidget->RemoveFromParent();
	GameFlowWidget = nullptr;
}

void ABombPlayerController::ApplyObjectiveInteractionActive(const bool bActive)
{
	const bool bInteractionStateChanged = bIsObjectiveInteractionActive != bActive;
	bIsObjectiveInteractionActive = bActive;
	SetObjectiveMovementInputLockApplied(bActive);

	if (bActive)
	{
		if (APawn* ControlledPawn = GetPawn())
		{
			if (UPawnMovementComponent* MovementComponent = ControlledPawn->GetMovementComponent())
			{
				MovementComponent->StopMovementImmediately();
			}
		}
	}

	if (bInteractionStateChanged)
	{
		OnObjectiveInteractionStateChanged.Broadcast(bIsObjectiveInteractionActive);
	}
}

void ABombPlayerController::SetObjectiveMovementInputLockApplied(const bool bApplied)
{
	if (bIsObjectiveMovementInputLockApplied == bApplied)
	{
		return;
	}

	SetIgnoreMoveInput(bApplied);
	bIsObjectiveMovementInputLockApplied = bApplied;
}

void ABombPlayerController::SetDataJackMappingContextEnabled(const bool bEnabled)
{
	if (!IsLocalController() || bIsDataJackMappingContextEnabled == bEnabled)
	{
		return;
	}

	UDataJackInputConfig* ObjectiveInputConfig = DataJackInputConfig.Get();
	if (!IsValid(ObjectiveInputConfig))
	{
		return;
	}

	UInputMappingContext* MappingContext = ObjectiveInputConfig->MappingContext.Get();
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!IsValid(MappingContext) || !IsValid(LocalPlayer))
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!IsValid(InputSubsystem))
	{
		return;
	}

	if (bEnabled)
	{
		InputSubsystem->AddMappingContext(MappingContext, ObjectiveInputConfig->MappingPriority);
	}
	else
	{
		InputSubsystem->RemoveMappingContext(MappingContext);
	}

	bIsDataJackMappingContextEnabled = bEnabled;
}

void ABombPlayerController::Input_DataJackInteractionStarted()
{
	if (!IsRoundInputEnabled() || bIsObjectiveInteractionActive)
	{
		return;
	}

	ServerStartDataJackInteraction();
}

void ABombPlayerController::Input_DataJackInteractionStopped()
{
	ApplyObjectiveInteractionActive(false);
	ServerStopDataJackInteraction();
}

void ABombPlayerController::Input_DataJackDropStarted()
{
	if (IsRoundInputEnabled() && !bIsObjectiveInteractionActive)
	{
		ServerStartDataJackDrop();
	}
}

void ABombPlayerController::Input_DataJackDropStopped()
{
	ServerStopDataJackDrop();
}
