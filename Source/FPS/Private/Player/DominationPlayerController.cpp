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

	// [INPUTDBG] 조작 불능 원인 추적용 임시 로그. 입력 가능 조건이 하나라도 바뀔 때만 한 번 출력한다.
	const bool bSuper = Super::CanProcessGameplayInput();
	const bool bHasGameState = IsValid(DominationGameState);
	const bool bInProgress = bHasGameState && DominationGameState->GetDominationPhase() == EDominationMatchPhase::InProgress;
	const bool bHasPawn = IsValid(GetPawn());
	const bool bHasPlayerState = IsValid(DominationPlayerState);
	const bool bAlive = bHasPlayerState && DominationPlayerState->IsAlive();
	const uint32 DebugMask = (bSuper ? 1u : 0u) | (bHasGameState ? 2u : 0u) | (bInProgress ? 4u : 0u)
		| (bHasPawn ? 8u : 0u) | (bHasPlayerState ? 16u : 0u) | (bAlive ? 32u : 0u);
	if (DebugMask != LastInputDebugMask)
	{
		LastInputDebugMask = DebugMask;
		UE_LOG(LogTemp, Warning, TEXT("[INPUTDBG] %s CanProcessGameplayInput=%d (Super[InputBlocked tag 없음]=%d GameState=%d InProgress=%d Pawn=%d(%s) PlayerState=%d Alive=%d) Local=%d"),
			*GetNameSafe(this), DebugMask == 63u, bSuper, bHasGameState, bInProgress, bHasPawn, *GetNameSafe(GetPawn()),
			bHasPlayerState, bAlive, IsLocalController());
	}

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
