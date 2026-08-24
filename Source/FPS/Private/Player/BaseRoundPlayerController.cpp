#include "Player/BaseRoundPlayerController.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameMode/BaseRoundGameMode.h"
#include "Input/ShooterInputConfig.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "Player/BaseTeamPlayerState.h"
#include "UI/Frontend/ShooterUIInitComponent.h"

void ABaseRoundPlayerController::BeginPlay()
{
	Super::BeginPlay();

	SetSystemMappingContextEnabled(true);
	ApplyRoundInputEnabled(false);
}

void ABaseRoundPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	SetRoundInputLockApplied(false);

	if (IsLocalController())
	{
		SetGameplayMappingContextEnabled(false);
		SetSystemMappingContextEnabled(false);
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

	if (IsValid(ShooterInputConfig->ToggleInGameMenuAction))
	{
		EnhancedInputComponent->BindAction(
			ShooterInputConfig->ToggleInGameMenuAction,
			ETriggerEvent::Started,
			this,
			&ThisClass::Input_ToggleInGameMenu);
	}
}

void ABaseRoundPlayerController::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(
		ABaseRoundPlayerController,
		SurrenderVoteState,
		COND_OwnerOnly,
		REPNOTIFY_Always);
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

void ABaseRoundPlayerController::ClientSetRoundInputEnabled_Implementation(
	const bool bEnabled)
{
	ApplyRoundInputEnabled(bEnabled);
}

void ABaseRoundPlayerController::NotifyClientReadyForGameFlow()
{
	// feat의 UI 준비 신호를 로딩 진행률의 단일 완료 경계로도 사용한다.
	SetLoadingProgress(0.9f);

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

const FSurrenderVoteState& ABaseRoundPlayerController::GetSurrenderVoteState() const
{
	return SurrenderVoteState;
}

void ABaseRoundPlayerController::RequestStartSurrenderVote()
{
	if (IsLocalController())
	{
		ServerRequestStartSurrenderVote();
	}
}

void ABaseRoundPlayerController::SubmitSurrenderVote(const bool bApprove)
{
	if (IsLocalController() && SurrenderVoteState.IsActive())
	{
		ServerSubmitSurrenderVote(SurrenderVoteState.VoteId, bApprove);
	}
}

void ABaseRoundPlayerController::ServerRequestStartSurrenderVote_Implementation()
{
	ABaseRoundGameMode* RoundGameMode = GetWorld()->GetAuthGameMode<ABaseRoundGameMode>();
	const ESurrenderVoteStartResult Result = IsValid(RoundGameMode)
		? RoundGameMode->RequestStartSurrenderVote(this)
		: ESurrenderVoteStartResult::NotAllowed;

	if (IsLocalController())
	{
		OnSurrenderVoteStartResult.Broadcast(Result);
	}
	else
	{
		ClientNotifySurrenderVoteStartResult(Result);
	}
}

void ABaseRoundPlayerController::ServerSubmitSurrenderVote_Implementation(
	const int32 VoteId,
	const bool bApprove)
{
	if (ABaseRoundGameMode* RoundGameMode = GetWorld()->GetAuthGameMode<ABaseRoundGameMode>())
	{
		RoundGameMode->SubmitSurrenderVote(this, VoteId, bApprove);
	}
}

void ABaseRoundPlayerController::ClientNotifySurrenderVoteStartResult_Implementation(
	const ESurrenderVoteStartResult Result)
{
	OnSurrenderVoteStartResult.Broadcast(Result);
}

void ABaseRoundPlayerController::OnRep_SurrenderVoteState()
{
	OnSurrenderVoteStateChanged.Broadcast();
}

void ABaseRoundPlayerController::SetSurrenderVoteState(
	const FSurrenderVoteState& NewState)
{
	SurrenderVoteState = NewState;
	ForceNetUpdate();

	if (IsLocalController())
	{
		OnRep_SurrenderVoteState();
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

void ABaseRoundPlayerController::Input_ToggleInGameMenu()
{
	if (!IsLocalController())
	{
		return;
	}

	if (UShooterUIInitComponent* UIInitComponent = GetUIInitComponent())
	{
		UIInitComponent->ToggleInGameMenu();
	}
}

UShooterUIInitComponent* ABaseRoundPlayerController::GetUIInitComponent() const
{
	return FindComponentByClass<UShooterUIInitComponent>();
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

void ABaseRoundPlayerController::SetSystemMappingContextEnabled(const bool bEnabled)
{
	UShooterInputConfig* ShooterInputConfig = InputConfig.Get();
	if (!IsValid(ShooterInputConfig))
	{
		return;
	}

	SetMappingContextEnabled(
		ShooterInputConfig->SystemMappingContext,
		ShooterInputConfig->SystemMappingPriority,
		bEnabled);
}

void ABaseRoundPlayerController::SetMappingContextEnabled(
	UInputMappingContext* MappingContext,
	const int32 Priority,
	const bool bEnabled)
{
	if (!IsLocalController() || !IsValid(MappingContext))
	{
		return;
	}

	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!IsValid(LocalPlayer))
	{
		return;
	}

	UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!IsValid(InputSubsystem))
	{
		return;
	}

	if (bEnabled)
	{
		InputSubsystem->AddMappingContext(MappingContext, Priority);
	}
	else
	{
		InputSubsystem->RemoveMappingContext(MappingContext);
	}
}
