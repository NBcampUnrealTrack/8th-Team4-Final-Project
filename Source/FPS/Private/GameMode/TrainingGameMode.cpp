#include "GameMode/TrainingGameMode.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Augment/AugmentDraftProfile.h"
#include "Combat/CombatComponent.h"
#include "GameFramework/Pawn.h"
#include "GameMode/Manager/AugmentDraftManager.h"
#include "GameMode/Manager/DominationLoadoutManager.h"
#include "GameMode/Manager/PlayerRespawnProtection.h"
#include "GameMode/Manager/TeamPlayerManager.h"
#include "GameMode/Settings/DominationClassSettings.h"
#include "GameMode/State/BaseTeamGameState.h"
#include "Player/DominationPlayerState.h"
#include "Player/TrainingPlayerController.h"
#include "Training/TrainingSessionSubsystem.h"
#include "Training/TrainingPlayerComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogTrainingGameMode, Log, All);
#define LOCTEXT_NAMESPACE "TrainingGameMode"

ATrainingGameMode::ATrainingGameMode()
{
    GameStateClass = ABaseTeamGameState::StaticClass();
    PlayerControllerClass = ATrainingPlayerController::StaticClass();
    PlayerStateClass = ADominationPlayerState::StaticClass();
    DefaultPawnClass = nullptr;
    bDelayedStart = true;
    bUseSeamlessTravel = false;
    LoadoutManager = CreateDefaultSubobject<UDominationLoadoutManager>(TEXT("LoadoutManager"));
    AugmentDraftManager = CreateDefaultSubobject<UAugmentDraftManager>(TEXT("AugmentDraftManager"));
    RespawnProtection = CreateDefaultSubobject<UPlayerRespawnProtection>(TEXT("RespawnProtection"));
}

void ATrainingGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    if (GetNetMode() != NM_Standalone || UGameplayStatics::HasOption(Options, TEXT("listen")))
    {
        ErrorMessage = TEXT("Training requires Standalone mode. Disable Listen Server / Dedicated Server.");
        UE_LOG(LogTrainingGameMode, Error, TEXT("%s"), *ErrorMessage);
    }
}

void ATrainingGameMode::PreLogin(const FString&, const FString&, const FUniqueNetIdRepl&, FString& ErrorMessage)
{
    ErrorMessage = TEXT("Training does not accept network connections.");
}

APlayerController* ATrainingGameMode::Login(UPlayer* NewPlayer, const ENetRole InRemoteRole,
    const FString& Portal, const FString& Options, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    const UGameInstance* Instance = GetGameInstance();
    const ULocalPlayer* LocalPlayer = Cast<ULocalPlayer>(NewPlayer);
    if (GetNetMode() != NM_Standalone || !Instance || !LocalPlayer
        || Instance->GetLocalPlayers().Num() != 1 || Instance->GetFirstGamePlayer() != LocalPlayer
        || TrainingPlayer.IsValid())
    {
        ErrorMessage = TEXT("Training requires exactly one local player in Standalone mode.");
        return nullptr;
    }
    return Super::Login(NewPlayer, InRemoteRole, Portal, Options, UniqueId, ErrorMessage);
}

bool ATrainingGameMode::IsTrainingPlayer(const APlayerController* Player) const
{
    return IsValid(Player) && TrainingPlayer.Get() == Player && Player->GetWorld() == GetWorld()
        && GetNetMode() == NM_Standalone && Player->IsLocalController();
}

void ATrainingGameMode::BeginPlay()
{
    Super::BeginPlay();
    if (GetNetMode() != NM_Standalone || !GetGameInstance()
        || GetGameInstance()->GetLocalPlayers().Num() != 1)
    {
        UE_LOG(LogTrainingGameMode, Error, TEXT("Training startup stopped: use Standalone with one local player."));
        return;
    }
    const UDominationClassSettings* Classes = GetDefault<UDominationClassSettings>();
    AugmentDraftManager->Initialize(this, bEnableAugmentDraft && !bUseManualAugmentSelection, AugmentDraftProfile);
    AugmentDraftManager->OnDependenciesReady.AddUObject(this, &ThisClass::HandleAugmentDependenciesReady);
    RespawnProtection->Initialize(GetWorld(), RespawnProtectionDuration);

    bConfigurationReady = Classes->HasValidDefinitions()
        && LoadoutManager->HasRequiredWeaponRegistryEntries()
        && PlayerControllerClass && PlayerControllerClass->IsChildOf(ATrainingPlayerController::StaticClass())
        && PlayerStateClass && PlayerStateClass->IsChildOf(ADominationPlayerState::StaticClass())
        && AugmentDraftManager->HasRequiredDependencies();
    if (!bConfigurationReady)
    {
        UE_LOG(LogTrainingGameMode, Error,
            TEXT("Training startup stopped: check Domination Classes (Pawn BPs), Weapon Registry, Training controller, class PlayerState, and Augment Registry."));
        return;
    }
    TryOpenClassSelection(TrainingPlayer.Get());
}

void ATrainingGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    ATrainingPlayerController* JoiningPlayer = Cast<ATrainingPlayerController>(NewPlayer);
    ADominationPlayerState* ClassState = IsValid(JoiningPlayer)
        ? JoiningPlayer->GetPlayerState<ADominationPlayerState>() : nullptr;
    if (GetNetMode() != NM_Standalone || !IsValid(ClassState) || !JoiningPlayer->IsLocalController()
        || !GetGameInstance() || GetGameInstance()->GetLocalPlayers().Num() != 1
        || JoiningPlayer->GetLocalPlayer() != GetGameInstance()->GetFirstGamePlayer()) { return; }
    if (TrainingPlayer.IsValid()) { return; }

    TrainingPlayer = JoiningPlayer;
    ClassState->SetTeam(EGameTeam::TeamA);
    ClassState->SetParticipationState(EPlayerParticipationState::WaitingForNextSpawn);
    UTeamPlayerManager::RemovePlayerPawn(JoiningPlayer);
    ClassState->ResetCombatClassSelection();
    AugmentDraftManager->ResetPlayerForNewMatch(ClassState);
    TryOpenClassSelection(JoiningPlayer);
}

void ATrainingGameMode::NotifyPlayerReadyForGameFlow(APlayerController* ReadyPlayer)
{
    TryOpenClassSelection(Cast<ATrainingPlayerController>(ReadyPlayer));
}

void ATrainingGameMode::TryOpenClassSelection(ATrainingPlayerController* ReadyPlayer)
{
    ADominationPlayerState* ClassState = IsValid(ReadyPlayer)
        ? ReadyPlayer->GetPlayerState<ADominationPlayerState>() : nullptr;
    if (!bConfigurationReady || !IsValid(ClassState)
        || !IsTrainingPlayer(ReadyPlayer) || !ClassState->IsLoadedIntoMatch()
        || ClassState->HasSelectedCombatClass()) { return; }
    ClassState->SetParticipationState(EPlayerParticipationState::WaitingForMatchStart);
}

bool ATrainingGameMode::RequestCombatClass(ATrainingPlayerController* Requester,
    const EDominationCombatClass RequestedClass, FText& OutFailure)
{
    OutFailure = FText::GetEmpty();
    ADominationPlayerState* ClassState = IsValid(Requester)
        ? Requester->GetPlayerState<ADominationPlayerState>() : nullptr;
    if (!bConfigurationReady || !IsValid(ClassState)
        || !IsTrainingPlayer(Requester)
        || !ClassState->IsLoadedIntoMatch()
        || ClassState->GetParticipationState() != EPlayerParticipationState::WaitingForMatchStart
        || !ClassState->TryConfirmCombatClass(RequestedClass))
    {
        OutFailure = LOCTEXT("InvalidSelection", "Class selection is not available. Wait for loading or choose a configured class.");
        return false;
    }
    if (!LoadoutManager->InitializeCombatClassInventory(ClassState)
        || !RestartTrainingPlayer(Requester, true))
    {
        UTeamPlayerManager::RemovePlayerPawn(Requester);
        ClassState->ResetCombatClassSelection();
        ClassState->SetParticipationState(EPlayerParticipationState::WaitingForMatchStart);
        OutFailure = LOCTEXT("SpawnFailed", "Could not spawn or equip the selected class. Check PlayerStart, class Pawn and weapon settings.");
        return false;
    }
    if (!HasMatchStarted()) { StartMatch(); }
    GrantInitialAugments(Requester);
    return true;
}

void ATrainingGameMode::RequestPreviewCombatClass(ATrainingPlayerController* Requester,
    const EDominationCombatClass RequestedClass)
{
    ADominationPlayerState* ClassState = IsValid(Requester)
        ? Requester->GetPlayerState<ADominationPlayerState>() : nullptr;
    if (bConfigurationReady && IsValid(ClassState)
        && IsTrainingPlayer(Requester)
        && ClassState->IsLoadedIntoMatch()
        && ClassState->GetParticipationState() == EPlayerParticipationState::WaitingForMatchStart)
    {
        ClassState->TrySetPreviewCombatClass(RequestedClass);
    }
}

UClass* ATrainingGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    const ADominationPlayerState* ClassState = IsValid(InController)
        ? InController->GetPlayerState<ADominationPlayerState>() : nullptr;
    return IsValid(ClassState) && ClassState->HasSelectedCombatClass()
        ? GetDefault<UDominationClassSettings>()->ResolvePawnClass(ClassState->GetCombatClass()) : nullptr;
}

bool ATrainingGameMode::PlayerCanRestart_Implementation(APlayerController* Requester)
{
    const ADominationPlayerState* ClassState = IsValid(Requester)
        ? Requester->GetPlayerState<ADominationPlayerState>() : nullptr;
    return bConfigurationReady && IsTrainingPlayer(Requester) && IsValid(ClassState) && ClassState->HasSelectedCombatClass()
        && ClassState->GetParticipationState() == EPlayerParticipationState::Active
        && Super::PlayerCanRestart_Implementation(Requester);
}

bool ATrainingGameMode::RestartTrainingPlayer(ATrainingPlayerController* WaitingPlayer, const bool bInitialSpawn)
{
    ADominationPlayerState* ClassState = IsValid(WaitingPlayer)
        ? WaitingPlayer->GetPlayerState<ADominationPlayerState>() : nullptr;
    if (!bConfigurationReady || !IsTrainingPlayer(WaitingPlayer)
        || !IsValid(ClassState) || !ClassState->HasSelectedCombatClass()) { return false; }

    UTeamPlayerManager::RemovePlayerPawn(WaitingPlayer);

    FTransform CQBRespawnTransform;
    const UTrainingPlayerComponent* TrainingBridge = WaitingPlayer->FindComponentByClass<UTrainingPlayerComponent>();
    if (!bInitialSpawn && TrainingBridge && TrainingBridge->TryGetCQBRespawnTransform(CQBRespawnTransform))
    {
        RestartPlayerAtTransform(WaitingPlayer, CQBRespawnTransform);
    }
    else
    {
        RestartPlayer(WaitingPlayer);
    }

    APawn* CombatPawn = WaitingPlayer->GetPawn();
    if (!IsValid(CombatPawn) || CombatPawn->GetController() != WaitingPlayer) { return false; }
    ClassState->ServerRespawnReset();
    if (!LoadoutManager->ApplyCombatClassLoadout(ClassState, CombatPawn)) { return false; }

    UCombatComponent* Combat = CombatPawn->FindComponentByClass<UCombatComponent>();
    if (!IsValid(Combat)) { return false; }
    Combat->SetInfiniteAmmo(true);

    WaitingPlayer->ChangeState(NAME_Playing);
    WaitingPlayer->ClientGotoState(NAME_Playing);
    WaitingPlayer->ClientExitSpectatorMode();
    ClassState->SetParticipationState(EPlayerParticipationState::Active);
    if (!bInitialSpawn)
    {
        RespawnProtection->BeginProtection(ClassState);
        WaitingPlayer->FinishRespawnFade(RespawnFadeInSeconds);
    }
    return true;
}

void ATrainingGameMode::GrantInitialAugments(ATrainingPlayerController* ReadyPlayer)
{
    ADominationPlayerState* ClassState = IsValid(ReadyPlayer)
        ? ReadyPlayer->GetPlayerState<ADominationPlayerState>() : nullptr;
    if (bUseManualAugmentSelection || !bEnableAugmentDraft || !AugmentDraftManager->AreDependenciesReady()
        || !IsValid(ClassState) || !ClassState->HasSelectedCombatClass()
        || ClassState->GetParticipationState() != EPlayerParticipationState::Active
        || !IsTrainingPlayer(ReadyPlayer) || bInitialAugmentsGranted) { return; }

    bInitialAugmentsGranted = true;
    AugmentDraftManager->GrantToPlayer(ClassState, EAugmentRewardPool::Active, 1,
        FGameplayTag::RequestGameplayTag(FName(TEXT("InputTag.Ability.Slot1"))));
    AugmentDraftManager->GrantToPlayer(ClassState, EAugmentRewardPool::Passive, 1);
    AugmentDraftManager->ServeNextReward(ClassState);
}

void ATrainingGameMode::HandleAugmentDependenciesReady()
{
    GrantInitialAugments(TrainingPlayer.Get());
}

void ATrainingGameMode::NotifyPlayerEliminatedBy(AController* EliminatedController, AShooterPlayerState*)
{
    ATrainingPlayerController* WaitingPlayer = Cast<ATrainingPlayerController>(EliminatedController);
    ADominationPlayerState* ClassState = IsValid(WaitingPlayer)
        ? WaitingPlayer->GetPlayerState<ADominationPlayerState>() : nullptr;
    if (!bConfigurationReady || !IsTrainingPlayer(WaitingPlayer) || !IsValid(ClassState)
        || !ClassState->HasSelectedCombatClass() || bRespawnPending) { return; }

    bRespawnPending = true;
    ClassState->SetParticipationState(EPlayerParticipationState::WaitingForNextSpawn);
    ClassState->SetAlive(false);
    RespawnProtection->EndProtection(ClassState);
    if (UTrainingSessionSubsystem* Session = GetWorld()->GetSubsystem<UTrainingSessionSubsystem>())
    {
        Session->EndSessionForParticipant(WaitingPlayer);
    }
    if (UTrainingPlayerComponent* TrainingBridge = WaitingPlayer->FindComponentByClass<UTrainingPlayerComponent>())
    {
        TrainingBridge->HandleParticipantDeath();
    }
    WaitingPlayer->BeginRespawnFade(RespawnFadeOutSeconds);

    const float FadeOutDelay = FMath::Max(0.0f, RespawnFadeOutSeconds);
    if (FadeOutDelay <= KINDA_SMALL_NUMBER)
    {
        HandleRespawnFadeOutComplete();
        return;
    }
    GetWorldTimerManager().SetTimer(RespawnTimer, this, &ThisClass::HandleRespawnFadeOutComplete,
        FadeOutDelay, false);
}

void ATrainingGameMode::HandleRespawnFadeOutComplete()
{
    ATrainingPlayerController* WaitingPlayer = TrainingPlayer.Get();
    if (!bConfigurationReady || !bRespawnPending || !IsTrainingPlayer(WaitingPlayer))
    {
        bRespawnPending = false;
        return;
    }
    MovePlayerToSpectator(WaitingPlayer);
    const float RemainingDelay = FMath::Max(0.1f, RespawnDelay - FMath::Max(0.0f, RespawnFadeOutSeconds));
    GetWorldTimerManager().SetTimer(RespawnTimer, this, &ThisClass::HandleRespawnTimer,
        RemainingDelay, false);
}

void ATrainingGameMode::HandleRespawnTimer()
{
    ATrainingPlayerController* WaitingPlayer = TrainingPlayer.Get();
    if (!bRespawnPending) { return; }
    bRespawnPending = false;
    if (!bConfigurationReady || !IsTrainingPlayer(WaitingPlayer)) { return; }
    if (!RestartTrainingPlayer(WaitingPlayer, false))
    {
        UTeamPlayerManager::RemovePlayerPawn(WaitingPlayer);
        MovePlayerToSpectator(WaitingPlayer);
        WaitingPlayer->FinishRespawnFade(RespawnFadeInSeconds);
        if (UTrainingPlayerComponent* TrainingBridge = WaitingPlayer->FindComponentByClass<UTrainingPlayerComponent>())
        {
            TrainingBridge->ResetTrainingState();
        }
        if (ADominationPlayerState* ClassState = WaitingPlayer->GetPlayerState<ADominationPlayerState>())
        {
            ClassState->SetAlive(false);
            AugmentDraftManager->ResetPlayerForNewMatch(ClassState);
            bInitialAugmentsGranted = false;
            ClassState->ResetCombatClassSelection();
            ClassState->SetParticipationState(EPlayerParticipationState::WaitingForMatchStart);
        }
        UE_LOG(LogTrainingGameMode, Error, TEXT("Training respawn failed. Check PlayerStart/Pawn/weapon configuration, then select a class again."));
        return;
    }
    GrantInitialAugments(WaitingPlayer);
    if (bEnableAugmentDraft && !bUseManualAugmentSelection)
    {
        AugmentDraftManager->ServeNextReward(WaitingPlayer->GetPlayerState<ADominationPlayerState>());
    }
}

void ATrainingGameMode::ReleaseTrainingPlayer()
{
    GetWorldTimerManager().ClearTimer(RespawnTimer);
    bRespawnPending = false;
    if (ATrainingPlayerController* LeavingPlayer = TrainingPlayer.Get())
    {
        if (UTrainingPlayerComponent* TrainingBridge = LeavingPlayer->FindComponentByClass<UTrainingPlayerComponent>())
        {
            TrainingBridge->ResetTrainingState();
        }
        ADominationPlayerState* ClassState = LeavingPlayer->GetPlayerState<ADominationPlayerState>();
        RespawnProtection->EndProtection(ClassState);
        AugmentDraftManager->ReleasePlayer(ClassState);
    }
    TrainingPlayer.Reset();
    bInitialAugmentsGranted = false;
}

void ATrainingGameMode::Logout(AController* Exiting)
{
    if (TrainingPlayer.Get() == Exiting)
    {
        ReleaseTrainingPlayer();
    }
    Super::Logout(Exiting);
}

void ATrainingGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearAllTimersForObject(this);
    bConfigurationReady = false;
    ReleaseTrainingPlayer();
    RespawnProtection->Shutdown();
    AugmentDraftManager->Shutdown();
    Super::EndPlay(EndPlayReason);
}

#undef LOCTEXT_NAMESPACE
