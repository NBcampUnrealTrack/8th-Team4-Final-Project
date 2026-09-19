#include "Training/TrainingPlayerComponent.h"

#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Tags/UITags.h"
#include "Training/UI/TrainingPlayerPresentation.h"
#include "Training/UI/TrainingResultsWidget.h"
#include "UI/Subsystem/ShooterUIManagerSubsystem.h"
#include "UI/TrainingWidgetStyle.h"

#define LOCTEXT_NAMESPACE "TrainingPlayer"

void UTrainingPlayerComponent::ApplyButtonSounds(UUserWidget& Widget) const
{
    TrainingWidgetStyle::ApplyButtonSounds(Widget, ButtonHoverSound, ButtonClickSound);
}

void UTrainingPlayerComponent::NotifySessionHit(const int32 SessionId, const int32 Kills,
    const float Damage, const bool bKilled, const FVector& WorldPosition)
{
    if (GetOwner()->HasAuthority()) { ClientSessionHit(SessionId, Kills, Damage, bKilled, WorldPosition); }
}

void UTrainingPlayerComponent::ClientSessionHit_Implementation(const int32 SessionId, const int32 Kills,
    const float Damage, const bool bKilled, const FVector WorldPosition)
{
    if (Presentation)
    {
        Presentation->HandleHit(SessionId, Kills, Damage, bKilled, WorldPosition);
    }
}

void UTrainingPlayerComponent::NotifyTrainingDamage(const float Damage, const bool bKilled,
    const FVector& WorldPosition)
{
    if (GetOwner()->HasAuthority() && FMath::IsFinite(Damage) && Damage > 0.0f)
    {
        ClientTrainingDamage(Damage, bKilled, WorldPosition);
    }
}

void UTrainingPlayerComponent::ClientTrainingDamage_Implementation(const float Damage, const bool bKilled,
    const FVector WorldPosition)
{
    APlayerController* LocalController = GetParticipantController();
    if (!LocalController || !LocalController->IsLocalController())
    {
        return;
    }
    if (Presentation)
    {
        Presentation->PresentStandaloneDamage(Damage, bKilled, WorldPosition);
    }
}

void UTrainingPlayerComponent::NotifyBotSound(const int32 SessionId, USoundBase* Sound,
    USoundAttenuation* Attenuation, const FVector& WorldPosition, const float Volume)
{
    if (GetOwner()->HasAuthority() && Sound && Volume > 0.0f)
    {
        ClientPlayBotSound(SessionId, Sound, Attenuation, WorldPosition, Volume);
    }
}

void UTrainingPlayerComponent::ClientPlayBotSound_Implementation(const int32 SessionId, USoundBase* Sound,
    USoundAttenuation* Attenuation, const FVector WorldPosition, const float Volume)
{
    if (Presentation)
    {
        Presentation->PlayBotSound(SessionId, Sound, Attenuation, WorldPosition, Volume);
    }
}

void UTrainingPlayerComponent::NotifySessionFinished(const FTrainingSessionResult& Result)
{
    if (GetOwner()->HasAuthority()) { ClientSessionFinished(Result); }
}

void UTrainingPlayerComponent::ClientSessionFinished_Implementation(const FTrainingSessionResult& Result)
{
    APlayerController* LocalController = GetParticipantController();
    if (!LocalController || !LocalController->IsLocalController() || Result.SessionId != LastStoppedSessionId) { return; }
    CloseActiveDialog();
    CloseResults();
    UShooterUIManagerSubsystem* UIManager = GetUIManager();
    if (!UIManager || !UIManager->IsUIInitialized())
    {
        ClientNotify_Implementation(LOCTEXT("ResultsNoLayout", "Training results require the existing PrimaryGameLayout modal layer."));
        return;
    }
    if (!ResultsWidgetClass)
    {
        ClientNotify_Implementation(LOCTEXT("ResultsClassMissing", "Training Results Widget Class is not configured. Assign the existing WBP in the Training Player Component."));
        return;
    }
    UTrainingResultsWidget* NewResults = Cast<UTrainingResultsWidget>(
        UIManager->PushWidgetToLayer(UITags::UI_Layer_Modal, ResultsWidgetClass));
    if (!NewResults)
    {
        ClientNotify_Implementation(LOCTEXT("ResultsFailed", "Could not create Training Results. Check Results Widget Class and the Modal layer."));
        return;
    }
    ResultsWidget = NewResults;
    NewResults->InitializeResults(*this, Result);
    if (WidgetOpenSound) { UGameplayStatics::PlaySound2D(this, WidgetOpenSound); }
}

void UTrainingPlayerComponent::DismissResults(UTrainingResultsWidget& Widget)
{
    if (ResultsWidget.Get() == &Widget) { CloseResults(true); }
}

void UTrainingPlayerComponent::NotifyResultsRemoved(UTrainingResultsWidget& Widget)
{
    if (ResultsWidget.Get() == &Widget) { ResultsWidget.Reset(); }
    Widget.DetachBridge();
}

void UTrainingPlayerComponent::CloseResults(const bool bPlayCloseSound)
{
    UTrainingResultsWidget* CurrentResults = ResultsWidget.Get();
    ResultsWidget.Reset();
    if (!CurrentResults) { return; }
    CurrentResults->DetachBridge();
    if (bPlayCloseSound && WidgetCloseSound) { UGameplayStatics::PlaySound2D(this, WidgetCloseSound); }
    if (UShooterUIManagerSubsystem* UIManager = GetUIManager())
    {
        UIManager->RemoveWidgetFromLayer(UITags::UI_Layer_Modal, CurrentResults);
    }
    else { CurrentResults->DeactivateWidget(); }
}

#undef LOCTEXT_NAMESPACE
