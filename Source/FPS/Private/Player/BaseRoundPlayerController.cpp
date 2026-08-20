#include "Player/BaseRoundPlayerController.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Player/BaseTeamPlayerState.h"
#include "GameMode/BaseRoundGameMode.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Input/ShooterInputConfig.h"
#include "InputMappingContext.h"

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

bool ABaseRoundPlayerController::IsRoundInputEnabled() const
{
	return bIsRoundInputEnabled;
}

bool ABaseRoundPlayerController::CanProcessGameplayInput() const
{
	const ABaseTeamPlayerState* TeamPlayerState = GetPlayerState<ABaseTeamPlayerState>();
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

void ABaseRoundPlayerController::ClientSetRoundInputEnabled_Implementation(const bool bEnabled)
{
	ApplyRoundInputEnabled(bEnabled);
}

void ABaseRoundPlayerController::NotifyClientReadyForGameFlow()
{
	SetLoadingProgress(0.95f);

	if (HasAuthority())
	{
		if (ABaseRoundGameMode* RoundGameMode = GetWorld() != nullptr
			? GetWorld()->GetAuthGameMode<ABaseRoundGameMode>()
			: nullptr)
		{
			RoundGameMode->NotifyPlayerReadyForGameFlow(this);
		}
		return;
	}

	ServerNotifyClientReadyForGameFlow();
}

void ABaseRoundPlayerController::ServerNotifyClientReadyForGameFlow_Implementation()
{
	if (ABaseRoundGameMode* RoundGameMode = GetWorld() != nullptr
		? GetWorld()->GetAuthGameMode<ABaseRoundGameMode>()
		: nullptr)
	{
		RoundGameMode->NotifyPlayerReadyForGameFlow(this);
	}
}

void ABaseRoundPlayerController::ApplyRoundInputEnabled(const bool bEnabled)
{
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
	if (!IsLocalController())
	{
		return;
	}

	UShooterInputConfig* ShooterInputConfig = InputConfig.Get();
	if (!IsValid(ShooterInputConfig))
	{
		return;
	}

	UInputMappingContext* DefaultMappingContext = ShooterInputConfig->DefaultMappingContext.Get();
	if (!IsValid(DefaultMappingContext))
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!IsValid(LocalPlayer))
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
		InputSubsystem->AddMappingContext(DefaultMappingContext, ShooterInputConfig->MappingPriority);
	}
	else
	{
		InputSubsystem->RemoveMappingContext(DefaultMappingContext);
	}

}
