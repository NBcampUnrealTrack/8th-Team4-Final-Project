#include "GameMode/TrainingGameMode.h"

#include "Engine/World.h"
#include "Augment/AugmentDraftProfile.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogTrainingGameMode, Log, All);
#define LOCTEXT_NAMESPACE "TrainingGameMode"

ATrainingGameMode::ATrainingGameMode()
{
    GameStateClass = ABaseTeamGameState::StaticClass();
    PlayerControllerClass = ATrainingPlayerController::StaticClass();
    // This existing state contains only combat-class/preview data on top of ShooterPlayerState.
    PlayerStateClass = ADominationPlayerState::StaticClass();
    DefaultPawnClass = nullptr; // Selected class resolves the actual existing character BP.
    bDelayedStart = true;
    LoadoutManager = CreateDefaultSubobject<UDominationLoadoutManager>(TEXT("LoadoutManager"));
    AugmentDraftManager = CreateDefaultSubobject<UAugmentDraftManager>(TEXT("AugmentDraftManager"));
    RespawnProtection = CreateDefaultSubobject<UPlayerRespawnProtection>(TEXT("RespawnProtection"));
}

void ATrainingGameMode::BeginPlay()
{
    Super::BeginPlay();
    const UDominationClassSettings* Classes = GetDefault<UDominationClassSettings>();
    AugmentDraftManager->Initialize(this, bEnableAugmentDraft, AugmentDraftProfile);
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
    // Ready RPCs can arrive before BeginPlay during a level transition.
    for (const TWeakObjectPtr<ATrainingPlayerController>& Entry : InitializedPlayers)
    {
        TryOpenClassSelection(Entry.Get());
    }
}

void ATrainingGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
    ATrainingPlayerController* JoiningPlayer = Cast<ATrainingPlayerController>(NewPlayer);
    ADominationPlayerState* ClassState = IsValid(JoiningPlayer)
        ? JoiningPlayer->GetPlayerState<ADominationPlayerState>() : nullptr;
    if (!IsValid(ClassState) || InitializedPlayers.Contains(JoiningPlayer)) { return; }

    // Do not call the auto-spawning parent path: class selection is the spawn gate.
    // Reuse existing state, inventory reset and pawn-removal operations.
    InitializedPlayers.Add(JoiningPlayer);
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
        || !InitializedPlayers.Contains(ReadyPlayer) || !ClassState->IsLoadedIntoMatch()
        || ClassState->HasSelectedCombatClass()) { return; }
    ClassState->SetParticipationState(EPlayerParticipationState::WaitingForMatchStart);
    // The local UI hides loading only after its class-selection screen is actually created.
}

bool ATrainingGameMode::RequestCombatClass(ATrainingPlayerController* Requester,
    const EDominationCombatClass RequestedClass, FText& OutFailure)
{
    OutFailure = FText::GetEmpty();
    ADominationPlayerState* ClassState = IsValid(Requester)
        ? Requester->GetPlayerState<ADominationPlayerState>() : nullptr;
    if (!HasAuthority() || !bConfigurationReady || !IsValid(ClassState)
        || Requester->GetWorld() != GetWorld() || !InitializedPlayers.Contains(Requester)
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
    if (HasAuthority() && bConfigurationReady && IsValid(ClassState)
        && Requester->GetWorld() == GetWorld() && InitializedPlayers.Contains(Requester)
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
    // AGameMode::StartMatch must not spawn other players before they have selected a class.
    return bConfigurationReady && IsValid(ClassState) && ClassState->HasSelectedCombatClass()
        && ClassState->GetParticipationState() == EPlayerParticipationState::Active
        && Super::PlayerCanRestart_Implementation(Requester);
}

bool ATrainingGameMode::RestartTrainingPlayer(ATrainingPlayerController* WaitingPlayer, const bool bInitialSpawn)
{
    ADominationPlayerState* ClassState = IsValid(WaitingPlayer)
        ? WaitingPlayer->GetPlayerState<ADominationPlayerState>() : nullptr;
    if (!IsValid(ClassState) || !ClassState->HasSelectedCombatClass()) { return false; }

    UTeamPlayerManager::RemovePlayerPawn(WaitingPlayer);
    RestartPlayer(WaitingPlayer); // Normal PlayerStart. No capture-point/spawn-volume dependency.
    APawn* CombatPawn = WaitingPlayer->GetPawn();
    if (!IsValid(CombatPawn) || CombatPawn->GetController() != WaitingPlayer) { return false; }
    ClassState->ServerRespawnReset();
    if (!LoadoutManager->ApplyCombatClassLoadout(ClassState, CombatPawn)) { return false; }

    WaitingPlayer->ChangeState(NAME_Playing);
    WaitingPlayer->ClientGotoState(NAME_Playing);
    WaitingPlayer->ClientExitSpectatorMode();
    ClassState->SetParticipationState(EPlayerParticipationState::Active);
    if (!bInitialSpawn) { RespawnProtection->BeginProtection(ClassState); }
    return true;
}

void ATrainingGameMode::GrantInitialAugments(ATrainingPlayerController* ReadyPlayer)
{
    ADominationPlayerState* ClassState = IsValid(ReadyPlayer)
        ? ReadyPlayer->GetPlayerState<ADominationPlayerState>() : nullptr;
    if (!bEnableAugmentDraft || !AugmentDraftManager->AreDependenciesReady()
        || !IsValid(ClassState) || !ClassState->HasSelectedCombatClass()
        || ClassState->GetParticipationState() != EPlayerParticipationState::Active
        || InitialAugmentsGranted.Contains(ReadyPlayer)) { return; }

    InitialAugmentsGranted.Add(ReadyPlayer);
    // Same opening loadout as Domination: Slot1 active + passive, once per joining player.
    // No capture/round timer is introduced just to grant these rewards.
    AugmentDraftManager->GrantToPlayer(ClassState, EAugmentRewardPool::Active, 1,
        FGameplayTag::RequestGameplayTag(FName(TEXT("InputTag.Ability.Slot1"))));
    AugmentDraftManager->GrantToPlayer(ClassState, EAugmentRewardPool::Passive, 1);
    AugmentDraftManager->ServeNextReward(ClassState);
}

void ATrainingGameMode::HandleAugmentDependenciesReady()
{
    for (const TWeakObjectPtr<ATrainingPlayerController>& Entry : InitializedPlayers)
    {
        GrantInitialAugments(Entry.Get());
    }
}

void ATrainingGameMode::NotifyPlayerEliminatedBy(AController* EliminatedController, AShooterPlayerState*)
{
    ATrainingPlayerController* WaitingPlayer = Cast<ATrainingPlayerController>(EliminatedController);
    ADominationPlayerState* ClassState = IsValid(WaitingPlayer)
        ? WaitingPlayer->GetPlayerState<ADominationPlayerState>() : nullptr;
    if (!HasAuthority() || !IsValid(ClassState) || !ClassState->HasSelectedCombatClass()
        || PendingRespawns.Contains(WaitingPlayer)) { return; }

    ClassState->SetParticipationState(EPlayerParticipationState::WaitingForNextSpawn);
    ClassState->SetAlive(false);
    RespawnProtection->EndProtection(ClassState);
    MovePlayerToSpectator(WaitingPlayer);
    ClassState->SetAlive(false);
    FTimerHandle& RespawnTimer = PendingRespawns.Add(WaitingPlayer);
    GetWorldTimerManager().SetTimer(RespawnTimer,
        FTimerDelegate::CreateUObject(this, &ThisClass::HandleRespawnTimer,
            TWeakObjectPtr<ATrainingPlayerController>(WaitingPlayer)),
        FMath::Max(0.1f, RespawnDelay), false);
    // The training session deliberately survives player death and pawn replacement.
}

void ATrainingGameMode::HandleRespawnTimer(const TWeakObjectPtr<ATrainingPlayerController> WaitingPlayer)
{
    PendingRespawns.Remove(WaitingPlayer);
    if (!WaitingPlayer.IsValid()) { return; }
    if (!RestartTrainingPlayer(WaitingPlayer.Get(), false))
    {
        UTeamPlayerManager::RemovePlayerPawn(WaitingPlayer.Get());
        MovePlayerToSpectator(WaitingPlayer.Get());
        if (ADominationPlayerState* ClassState = WaitingPlayer->GetPlayerState<ADominationPlayerState>())
        {
            ClassState->SetAlive(false);
        }
        UE_LOG(LogTrainingGameMode, Error, TEXT("Training respawn failed for %s. Check PlayerStart/Pawn/weapon configuration."),
            *GetNameSafe(WaitingPlayer.Get()));
        return;
    }
    GrantInitialAugments(WaitingPlayer.Get());
    if (bEnableAugmentDraft)
    {
        AugmentDraftManager->ServeNextReward(WaitingPlayer->GetPlayerState<ADominationPlayerState>());
    }
}

void ATrainingGameMode::Logout(AController* Exiting)
{
    if (ATrainingPlayerController* LeavingPlayer = Cast<ATrainingPlayerController>(Exiting))
    {
        if (FTimerHandle* RespawnTimer = PendingRespawns.Find(LeavingPlayer))
        {
            GetWorldTimerManager().ClearTimer(*RespawnTimer);
            PendingRespawns.Remove(LeavingPlayer);
        }
        ADominationPlayerState* ClassState = LeavingPlayer->GetPlayerState<ADominationPlayerState>();
        RespawnProtection->EndProtection(ClassState);
        AugmentDraftManager->ReleasePlayer(ClassState);
        InitializedPlayers.Remove(LeavingPlayer);
        InitialAugmentsGranted.Remove(LeavingPlayer);
        if (UTrainingSessionSubsystem* Session = GetWorld()->GetSubsystem<UTrainingSessionSubsystem>())
        {
            Session->EndSessionForParticipant(LeavingPlayer);
        }
    }
    Super::Logout(Exiting);
}

void ATrainingGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bConfigurationReady = false;
    for (TPair<TWeakObjectPtr<ATrainingPlayerController>, FTimerHandle>& Entry : PendingRespawns)
    {
        GetWorldTimerManager().ClearTimer(Entry.Value);
    }
    PendingRespawns.Reset();
    RespawnProtection->Shutdown();
    AugmentDraftManager->Shutdown();
    InitializedPlayers.Reset();
    InitialAugmentsGranted.Reset();
    Super::EndPlay(EndPlayReason);
}

#undef LOCTEXT_NAMESPACE
