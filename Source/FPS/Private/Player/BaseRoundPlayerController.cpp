#include "Player/BaseRoundPlayerController.h"

#include "EnhancedInputComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameMode/BaseRoundGameMode.h"
#include "GameMode/State/BaseRoundGameState.h"
#include "Input/ShooterInputConfig.h"
#include "Loading/LoadingScreenSubsystem.h"
#include "Player/BaseTeamPlayerState.h"
#include "UI/Frontend/ShooterUIInitComponent.h"

// Temp log
DEFINE_LOG_CATEGORY_STATIC(LogTempInput, Log, All);

void ABaseRoundPlayerController::BeginPlay()
{
	Super::BeginPlay();

	ApplyRoundInputEnabled(false);
}

void ABaseRoundPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetRoundInputLockApplied(false);

	if (IsLocalController())
	{
		SetGameplayMappingContextEnabled(false);
	}

	Super::EndPlay(EndPlayReason);
}

void ABaseRoundPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	UShooterInputConfig* ShooterInputConfig = InputConfig.Get();
	if (!IsValid(EnhancedInputComponent) || !IsValid(ShooterInputConfig))
	{
		return;
	}

	if (IsValid(ShooterInputConfig->ToggleShopAction))
	{
		EnhancedInputComponent->BindAction(
			ShooterInputConfig->ToggleShopAction,
			ETriggerEvent::Started,
			this,
			&ThisClass::Input_ToggleShop);
	}
}

void ABaseRoundPlayerController::RefreshLoadingScreenGameStateBinding(
	ULoadingScreenSubsystem* LoadingScreenSubsystem)
{
	if (!IsValid(LoadingScreenSubsystem))
	{
		return;
	}

	ABaseRoundGameState* RoundGameState = nullptr;
	if (UWorld* World = GetWorld())
	{
		RoundGameState = World->GetGameState<ABaseRoundGameState>();
	}

	LoadingScreenSubsystem->BindToRoundGameState(RoundGameState);
}
bool ABaseRoundPlayerController::IsRoundInputEnabled() const
{
	return bIsRoundInputEnabled;
}

bool ABaseRoundPlayerController::CanProcessGameplayInput() const
{
	const ABaseTeamPlayerState* TeamPlayerState = GetPlayerState<ABaseTeamPlayerState>();
	// Temp log
	const bool bSuper = Super::CanProcessGameplayInput();
	const bool bPawn = IsValid(GetPawn());
	const bool bState = IsValid(TeamPlayerState);
	const bool bAlive = bState && TeamPlayerState->IsAlive();
	const uint32 Mask = (bSuper ? 1u : 0u) | (bIsRoundInputEnabled ? 2u : 0u) | (bPawn ? 4u : 0u) | (bState ? 8u : 0u) | (bAlive ? 16u : 0u);
	if (Mask != LastTempInputMask)
	{
		LastTempInputMask = Mask;
		UE_LOG(LogTempInput, Warning, TEXT("[TEMP-INPUT] CanProcessGameplayInput changed: Super=%d RoundInputEnabled=%d Pawn=%d(%s) PlayerState=%d Alive=%d"),
			bSuper, bIsRoundInputEnabled, bPawn, *GetNameSafe(GetPawn()), bState, bAlive);
	}
	return Super::CanProcessGameplayInput()
		&& bIsRoundInputEnabled
		&& IsValid(GetPawn())
		&& IsValid(TeamPlayerState)
		&& TeamPlayerState->IsAlive();
}

bool ABaseRoundPlayerController::CanUseCombatInput() const
{
	return CanProcessActionInput();
}

void ABaseRoundPlayerController::SetRoundInputEnabled(const bool bEnabled)
{
	if (!HasAuthority())
	{
		return;
	}

	ApplyRoundInputEnabled(bEnabled);
	if (!IsLocalController())
	{
		ClientSetRoundInputEnabled(bEnabled);
	}
}

void ABaseRoundPlayerController::ClientSetRoundInputEnabled_Implementation(
	const bool bEnabled)
{
	ApplyRoundInputEnabled(bEnabled);
}

void ABaseRoundPlayerController::NotifyClientReadyForGameFlow()
{
	SetLoadingProgress(0.95f);

	if (!HasAuthority())
	{
		ServerNotifyClientReadyForGameFlow();
		return;
	}

	if (ABaseRoundGameMode* RoundGameMode = GetWorld()->GetAuthGameMode<ABaseRoundGameMode>())
	{
		RoundGameMode->NotifyPlayerReadyForGameFlow(this);
	}
}

void ABaseRoundPlayerController::ServerNotifyClientReadyForGameFlow_Implementation()
{
	if (ABaseRoundGameMode* RoundGameMode = GetWorld()->GetAuthGameMode<ABaseRoundGameMode>())
	{
		RoundGameMode->NotifyPlayerReadyForGameFlow(this);
	}
}

void ABaseRoundPlayerController::Input_ToggleShop()
{
	if (!IsLocalController())
	{
		return;
	}

	if (UShooterUIInitComponent* UIInitComponent = GetUIInitComponent())
	{
		UIInitComponent->ToggleShop();
	}
}

UShooterUIInitComponent* ABaseRoundPlayerController::GetUIInitComponent() const
{
	return FindComponentByClass<UShooterUIInitComponent>();
}

void ABaseRoundPlayerController::ApplyRoundInputEnabled(const bool bEnabled)
{
	// Temp log
	UE_LOG(LogTempInput, Warning, TEXT("[TEMP-INPUT] ApplyRoundInputEnabled(%s) current=%s Controller=%s Local=%d Authority=%d"),
		bEnabled ? TEXT("true") : TEXT("false"), bIsRoundInputEnabled ? TEXT("true") : TEXT("false"),
		*GetNameSafe(this), IsLocalController(), HasAuthority());

	if (bIsRoundInputEnabled == bEnabled)
	{
		return;
	}

	bIsRoundInputEnabled = bEnabled;
	SetRoundInputLockApplied(!bEnabled);
	SetGameplayMappingContextEnabled(bEnabled);

	if (!bEnabled)
	{
		if (APawn* ControlledPawn = GetPawn())
		{
			if (UPawnMovementComponent* MovementComponent = ControlledPawn->GetMovementComponent())
			{
				MovementComponent->StopMovementImmediately();
			}
		}
	}
}

void ABaseRoundPlayerController::SetRoundInputLockApplied(const bool bApplied)
{
	if (bIsRoundInputLockApplied == bApplied)
	{
		return;
	}

	SetIgnoreMoveInput(bApplied);
	SetIgnoreLookInput(bApplied);
	bIsRoundInputLockApplied = bApplied;
}

void ABaseRoundPlayerController::SetGameplayMappingContextEnabled(const bool bEnabled)
{
	UShooterInputConfig* ShooterInputConfig = InputConfig.Get();
	if (!IsValid(ShooterInputConfig))
	{
		return;
	}

	SetMappingContextEnabled(
		ShooterInputConfig->DefaultMappingContext,
		ShooterInputConfig->MappingPriority,
		bEnabled);
}
