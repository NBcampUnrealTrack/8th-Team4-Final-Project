#include "Player/DominationPlayerController.h"

#include "Audio/DominationSystemAnnouncer.h"
#include "Components/DominationSandstormComponent.h"
#include "EngineUtils.h"

#include "EnhancedInputComponent.h"
#include "Input/ShooterInputConfig.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameMode/DominationGameMode.h"
#include "GameMode/State/DominationGameState.h"
#include "Player/DominationPlayerState.h"
#include "UI/Frontend/DominationUIInitComponent.h"

ADominationPlayerController::ADominationPlayerController()
{
	UIInitComponent = CreateDefaultSubobject<UDominationUIInitComponent>(
		TEXT("DominationUIInitComponent"));
	SandstormComponent = CreateDefaultSubobject<UDominationSandstormComponent>(
		TEXT("DominationSandstormComponent"));
}

void ADominationPlayerController::PreClientTravel(const FString& PendingURL, const ETravelType TravelType, const bool bIsSeamlessTravel)
{
	SandstormComponent->ResetWorldBinding();
	UIInitComponent->ResetWorldBinding();
	Super::PreClientTravel(PendingURL, TravelType, bIsSeamlessTravel);
}

void ADominationPlayerController::NotifyLoadedWorld(const FName WorldPackageName, const bool bFinalDest)
{
	Super::NotifyLoadedWorld(WorldPackageName, bFinalDest);
	if (bFinalDest && IsLocalController() && SandstormComponent->HasBegunPlay())
	{
		SandstormComponent->InitializeForWorld();
		UIInitComponent->InitializeForWorld();
	}
}

bool ADominationPlayerController::CanProcessGameplayInput() const
{
	const ADominationGameState* DominationGameState = GetWorld()
		? GetWorld()->GetGameState<ADominationGameState>()
		: nullptr;
	const ADominationPlayerState* DominationPlayerState =
		GetPlayerState<ADominationPlayerState>();

	return Super::CanProcessGameplayInput()
		&& IsValid(DominationGameState)
		&& DominationGameState->GetDominationPhase() == EDominationMatchPhase::InProgress
		&& IsValid(GetPawn())
		&& IsValid(DominationPlayerState)
		&& DominationPlayerState->IsAlive();
}

void ADominationPlayerController::SelectCombatClass(
	const EDominationCombatClass RequestedClass)
{
	if (HasAuthority())
	{
		ServerSelectCombatClass_Implementation(RequestedClass);
		return;
	}

	ServerSelectCombatClass(RequestedClass);
}

void ADominationPlayerController::SetPreviewCombatClass(
	const EDominationCombatClass RequestedClass)
{
	if (HasAuthority())
	{
		ServerSetPreviewCombatClass_Implementation(RequestedClass);
		return;
	}

	ServerSetPreviewCombatClass(RequestedClass);
}

void ADominationPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	
	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EnhancedInputComponent || !InputConfig)
	{
		return;
	}
	
	if (InputConfig->ToggleAugmentSelectAction)
	{
		EnhancedInputComponent->BindAction(
			InputConfig->ToggleAugmentSelectAction,
			ETriggerEvent::Started, 
			this, 
			&ThisClass::Input_ToggleAugmentSelect);
	}
}

void ADominationPlayerController::Input_ToggleAugmentSelect()
{
	if (UDominationUIInitComponent* UIInit =
			FindComponentByClass<UDominationUIInitComponent>())
	{
		UIInit->ToggleAugmentSelect();
	}
}

void ADominationPlayerController::ServerSelectCombatClass_Implementation(
	const EDominationCombatClass RequestedClass)
{
	ADominationGameMode* DominationGameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<ADominationGameMode>()
		: nullptr;
	if (IsValid(DominationGameMode))
	{
		DominationGameMode->RequestCombatClass(this, RequestedClass);
	}
}

void ADominationPlayerController::ServerSetPreviewCombatClass_Implementation(
	const EDominationCombatClass RequestedClass)
{
	ADominationGameMode* DominationGameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<ADominationGameMode>()
		: nullptr;
	if (IsValid(DominationGameMode))
	{
		DominationGameMode->RequestPreviewCombatClass(this, RequestedClass);
	}
}

float ADominationPlayerController::GetCombatClassSelectionTimeRemaining() const
{
    const ADominationGameState* ModeState = GetWorld()
        ? GetWorld()->GetGameState<ADominationGameState>() : nullptr;
    return IsValid(ModeState) ? ModeState->GetPhaseTimeRemaining() : -1.0f;
}

void ADominationPlayerController::SandstormPreview(const float Intensity)
{
#if !UE_BUILD_SHIPPING
	if (!IsLocalController())
	{
		return;
	}
	SandstormComponent->SetPreviewIntensity(Intensity);
	for (TActorIterator<ADominationSystemAnnouncer> It(GetWorld()); It; ++It)
	{
		It->SetSandstormPreviewIntensity(Intensity);
	}
#endif
}
