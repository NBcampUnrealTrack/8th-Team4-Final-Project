#include "Player/DominationPlayerController.h"

#include "Audio/DominationSystemAnnouncer.h"
#include "Components/DominationSandstormComponent.h"
#include "EngineUtils.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

#include "EnhancedInputComponent.h"
#include "Input/ShooterInputConfig.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameMode/DominationGameMode.h"
#include "GameMode/Manager/DominationLoadoutManager.h"
#include "GameMode/State/DominationGameState.h"
#include "Player/DominationPlayerState.h"
#include "UI/Frontend/DominationUIInitComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogDominationClientAssets, Log, All);

ADominationPlayerController::ADominationPlayerController()
{
	UIInitComponent = CreateDefaultSubobject<UDominationUIInitComponent>(
		TEXT("DominationUIInitComponent"));
	SandstormComponent = CreateDefaultSubobject<UDominationSandstormComponent>(
		TEXT("DominationSandstormComponent"));
}

void ADominationPlayerController::PreClientTravel(const FString& PendingURL, const ETravelType TravelType, const bool bIsSeamlessTravel)
{
	ResetInitialCombatAssetLoad();
	SandstormComponent->ResetWorldBinding();
	UIInitComponent->ResetWorldBinding();
	Super::PreClientTravel(PendingURL, TravelType, bIsSeamlessTravel);
}

void ADominationPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetInitialCombatAssetLoad();
	Super::EndPlay(EndPlayReason);
}

void ADominationPlayerController::ClientPrepareInitialCombatAssets_Implementation(
	const FGuid Generation, const TArray<EDominationCombatClass>& CombatClasses)
{
	ResetInitialCombatAssetLoad();
	InitialCombatAssetGeneration = Generation;
	InitialCombatClasses = CombatClasses;
	InitialCombatAssetStartTime = FPlatformTime::Seconds();

	TArray<FSoftObjectPath> Paths;
	bool bValidRequest = Generation.IsValid() && !CombatClasses.IsEmpty();
	for (const EDominationCombatClass CombatClass : CombatClasses)
	{
		bValidRequest &= UDominationLoadoutManager::GatherCombatAssetPaths(CombatClass, Paths);
	}
	if (!bValidRequest)
	{
		bInitialCombatAssetResultSent = true;
		ServerInitialCombatAssetsReady(Generation, false);
		return;
	}

	UE_LOG(LogDominationClientAssets, Log,
		TEXT("[DominationTravel] Client selected roster assets requested: Generation=%s Classes=%d Assets=%d"),
		*Generation.ToString(), CombatClasses.Num(), Paths.Num());
	for (const FSoftObjectPath& Path : Paths)
	{
		UE_LOG(LogDominationClientAssets, Verbose,
			TEXT("[DominationTravel] Client combat asset root: %s"), *Path.ToString());
	}
	// 상대 플레이어의 첫 복제 스폰도 동기 로드를 유발하지 않도록 선택된 전체 명단을 준비한다.
	InitialCombatAssetHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Paths, FStreamableDelegate::CreateUObject(
			this, &ThisClass::HandleInitialCombatAssetsLoaded, Generation));
	if (!InitialCombatAssetHandle.IsValid() && !bInitialCombatAssetResultSent)
	{
		bInitialCombatAssetResultSent = true;
		ServerInitialCombatAssetsReady(Generation, false);
	}
}

void ADominationPlayerController::HandleInitialCombatAssetsLoaded(const FGuid Generation)
{
	if (Generation != InitialCombatAssetGeneration || bInitialCombatAssetResultSent)
	{
		return;
	}
	bool bReady = !InitialCombatClasses.IsEmpty();
	for (const EDominationCombatClass CombatClass : InitialCombatClasses)
	{
		bReady &= UDominationLoadoutManager::AreCombatAssetsReady(CombatClass);
	}
	bInitialCombatAssetResultSent = true;
	UE_LOG(LogDominationClientAssets, Log,
		TEXT("[DominationTravel] Client selected roster assets completed: Generation=%s Success=%d Elapsed=%.3fs"),
		*Generation.ToString(), bReady, FPlatformTime::Seconds() - InitialCombatAssetStartTime);
	ServerInitialCombatAssetsReady(Generation, bReady);
}

void ADominationPlayerController::ClientCancelInitialCombatAssets_Implementation(const FGuid Generation)
{
	if (Generation == InitialCombatAssetGeneration)
	{
		ResetInitialCombatAssetLoad();
	}
}

void ADominationPlayerController::ResetInitialCombatAssetLoad()
{
	InitialCombatAssetGeneration.Invalidate();
	if (InitialCombatAssetHandle.IsValid())
	{
		InitialCombatAssetHandle->CancelHandle();
		InitialCombatAssetHandle.Reset();
	}
	InitialCombatClasses.Reset();
	bInitialCombatAssetResultSent = false;
}

void ADominationPlayerController::ServerInitialCombatAssetsReady_Implementation(
	const FGuid Generation, const bool bSucceeded)
{
	ADominationGameMode* GameMode = GetWorld()
		? GetWorld()->GetAuthGameMode<ADominationGameMode>() : nullptr;
	if (IsValid(GameMode))
	{
		GameMode->NotifyClientInitialCombatAssetsReady(this, Generation, bSucceeded);
	}
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
