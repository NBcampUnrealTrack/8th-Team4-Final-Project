#include "UI/Frontend/TrainingUIInitComponent.h"

#include "Engine/World.h"
#include "GameMode/State/BaseTeamGameState.h"
#include "Player/DominationPlayerState.h"
#include "Player/ShooterPlayerController.h"
#include "UI/InGame/Domination/DominationClassSelectScreen.h"

bool UTrainingUIInitComponent::OpenClassSelectionForTraining()
{
    AShooterPlayerController* LocalController = Cast<AShooterPlayerController>(GetOwner());
    ADominationPlayerState* ClassState = IsValid(LocalController)
        ? LocalController->GetPlayerState<ADominationPlayerState>()
        : nullptr;
    if (!IsValid(LocalController)
        || !LocalController->IsLocalController()
        || !IsValid(ClassState)
        || !ClassState->HasSelectedCombatClass()
        || !ClassState->IsAlive()
        || ClassState->GetParticipationState() != EPlayerParticipationState::Active)
    {
        return false;
    }

    PresentClassSelectionScreen();
    UDominationClassSelectScreen* ClassScreen = GetClassSelectionScreen();
    if (!IsValid(ClassScreen))
    {
        return false;
    }

    ClassScreen->SetAllowActiveClassReselection(true);
    return true;
}

void UTrainingUIInitComponent::CloseClassSelectionForTraining()
{
    RemoveClassSelectionScreen();
}

bool UTrainingUIInitComponent::PrepareModeBindings()
{
    return GetWorld() && IsValid(GetWorld()->GetGameState<ABaseTeamGameState>());
}

bool UTrainingUIInitComponent::CanInitializeCombatHUD(const ADominationPlayerState& ClassState) const
{
    return ClassState.HasSelectedCombatClass()
        && ClassState.GetParticipationState() == EPlayerParticipationState::Active;
}

bool UTrainingUIInitComponent::ShouldPresentClassSelection(const ADominationPlayerState& ClassState) const
{
    return ClassState.GetParticipationState() == EPlayerParticipationState::WaitingForMatchStart;
}

bool UTrainingUIInitComponent::IsAugmentModeActive() const
{
    const ADominationPlayerState* ClassState = GetCombatClassPlayerState();
    return IsValid(ClassState) && ClassState->HasSelectedCombatClass() && ClassState->IsAlive()
        && ClassState->GetParticipationState() == EPlayerParticipationState::Active;
}

bool UTrainingUIInitComponent::IsSurrenderRequestAllowed() const
{
    return false;
}

void UTrainingUIInitComponent::OnClassSelectionPresented()
{
    CompleteLocalLoading();
}

void UTrainingUIInitComponent::OnCombatHUDCreated(UShooterPlayerHUD*)
{
    CompleteLocalLoading();
}

void UTrainingUIInitComponent::CompleteLocalLoading()
{
    if (AShooterPlayerController* LocalController = Cast<AShooterPlayerController>(GetOwner()))
    {
        LocalController->HideLoadingScreen();
    }
}

void UTrainingUIInitComponent::RequestReturnToOutGame(AShooterPlayerController& PlayerController)
{
    PlayerController.RequestReturnToFrontend();
}

