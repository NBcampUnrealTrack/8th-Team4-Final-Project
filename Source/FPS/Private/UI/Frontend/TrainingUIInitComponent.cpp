#include "UI/Frontend/TrainingUIInitComponent.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Widget.h"
#include "Engine/World.h"
#include "GameMode/State/BaseTeamGameState.h"
#include "Player/DominationPlayerState.h"
#include "Player/ShooterPlayerController.h"
#include "UI/InGame/HUD/ShooterPlayerHUD.h"
#include "UI/InGame/HUD/DominationCaptureProgressWidget.h"
#include "UI/InGame/HUD/DominationObjectiveCompassWidget.h"
#include "UI/InGame/HUD/DominationObjectiveFeedbackWidget.h"
#include "UI/InGame/HUD/DominationObjectiveTransitionTimerWidget.h"
#include "UI/InGame/HUD/DominationSiteTrackerWidget.h"

UTrainingUIInitComponent::UTrainingUIInitComponent()
{
    SuppressedHUDWidgetClasses = {
        UDominationCaptureProgressWidget::StaticClass(),
        UDominationObjectiveCompassWidget::StaticClass(),
        UDominationObjectiveFeedbackWidget::StaticClass(),
        UDominationObjectiveTransitionTimerWidget::StaticClass(),
        UDominationSiteTrackerWidget::StaticClass()
    };
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
    // Keep the same PICKED/lock-in presentation until the server has equipped the real pawn.
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
    return false; // No teams competing for a match result in Training.
}

void UTrainingUIInitComponent::OnClassSelectionPresented()
{
    CompleteLocalLoading();
}

void UTrainingUIInitComponent::OnCombatHUDCreated(UShooterPlayerHUD* HUD)
{
    if (IsValid(HUD)) { HideRuleOnlyWidgets(*HUD); }
    CompleteLocalLoading();
}

void UTrainingUIInitComponent::CompleteLocalLoading()
{
    if (AShooterPlayerController* LocalController = Cast<AShooterPlayerController>(GetOwner()))
    {
        // The presentation has actually been created; do not hide loading at an arbitrary delay.
        LocalController->HideLoadingScreen();
    }
}

void UTrainingUIInitComponent::HideRuleOnlyWidgets(UUserWidget& RootWidget)
{
    if (!RootWidget.WidgetTree) { return; }
    RootWidget.WidgetTree->ForEachWidget([this](UWidget* Candidate)
    {
        if (!IsValid(Candidate)) { return; }
        const bool bRuleWidget = SuppressedHUDWidgetNames.Contains(Candidate->GetFName())
            || SuppressedHUDWidgetClasses.ContainsByPredicate([Candidate](const TSubclassOf<UWidget>& WidgetClass)
            {
                return WidgetClass && Candidate->IsA(WidgetClass.Get());
            });
        if (bRuleWidget)
        {
            Candidate->SetVisibility(ESlateVisibility::Collapsed);
            return;
        }
        if (UUserWidget* NestedWidget = Cast<UUserWidget>(Candidate))
        {
            HideRuleOnlyWidgets(*NestedWidget);
        }
    });
}
