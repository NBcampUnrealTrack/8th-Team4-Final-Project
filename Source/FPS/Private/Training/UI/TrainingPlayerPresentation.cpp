#include "Training/UI/TrainingPlayerPresentation.h"

#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Training/TrainingPlayerComponent.h"
#include "Training/TrainingSettings.h"
#include "Training/UI/TrainingDamageFeedbackWidget.h"
#include "Training/UI/TrainingSessionHUDWidget.h"
#include "Training/UI/TrainingStartFeedbackWidget.h"

void UTrainingPlayerPresentation::Initialize(UTrainingPlayerComponent& InOwner)
{
    Owner = &InOwner;
}

void UTrainingPlayerPresentation::HandlePhase(const int32 SessionId, const ETrainingFeedbackPhase Phase,
    const double PhaseStartServerSeconds, const ETrainingMode Mode, const float DurationSeconds)
{
    UTrainingPlayerComponent* Bridge = Owner.Get();
    APlayerController* LocalController = Bridge ? Bridge->GetParticipantController() : nullptr;
    if (!LocalController || !LocalController->IsLocalController())
    {
        return;
    }

    if (Phase == ETrainingFeedbackPhase::Stopped)
    {
        if (SessionId == FeedbackSessionId)
        {
            Clear();
        }
        return;
    }

    if (Phase == ETrainingFeedbackPhase::Countdown)
    {
        Clear();
        FeedbackSessionId = SessionId;
    }
    else if (FeedbackSessionId != SessionId)
    {
        return;
    }

    if (Phase == ETrainingFeedbackPhase::Started)
    {
        if (!Bridge->SessionHUDWidgetClass)
        {
            UE_LOG(LogTemp, Error, TEXT("[Training] SessionHUDWidgetClass is not configured. Use a WBP subclass of UTrainingSessionHUDWidget."));
        }
        else
        {
            SessionHUDWidget = CreateWidget<UTrainingSessionHUDWidget>(LocalController, Bridge->SessionHUDWidgetClass);
            if (SessionHUDWidget)
            {
                SessionHUDWidget->AddToPlayerScreen(GetDefault<UTrainingSettings>()->SessionHUDZOrder);
                SessionHUDWidget->StartSession(Mode, PhaseStartServerSeconds, DurationSeconds);
            }
        }
    }

    if (!Bridge->StartFeedbackWidgetClass)
    {
        UE_LOG(LogTemp, Error, TEXT("[Training] StartFeedbackWidgetClass is not configured. Use a WBP subclass of UTrainingStartFeedbackWidget."));
        return;
    }

    if (!StartFeedbackWidget)
    {
        StartFeedbackWidget = CreateWidget<UTrainingStartFeedbackWidget>(LocalController, Bridge->StartFeedbackWidgetClass);
    }
    if (!StartFeedbackWidget)
    {
        return;
    }
    if (!StartFeedbackWidget->IsInViewport())
    {
        StartFeedbackWidget->AddToPlayerScreen(GetDefault<UTrainingSettings>()->StartFeedbackZOrder);
    }

    const AGameStateBase* GameState = Bridge->GetWorld() ? Bridge->GetWorld()->GetGameState() : nullptr;
    const float Elapsed = GameState
        ? static_cast<float>(FMath::Max(0.0, GameState->GetServerWorldTimeSeconds() - PhaseStartServerSeconds))
        : 0.0f;
    if (Phase == ETrainingFeedbackPhase::Countdown)
    {
        StartFeedbackWidget->ShowCountdown(Elapsed);
    }
    else
    {
        StartFeedbackWidget->ShowStarted(Elapsed);
    }
}

void UTrainingPlayerPresentation::HandleHit(const int32 SessionId, const int32 Kills, const float Damage,
    const bool bKilled, const FVector& WorldPosition)
{
    if (SessionId != FeedbackSessionId)
    {
        return;
    }
    if (SessionHUDWidget)
    {
        SessionHUDWidget->PresentHit(Kills, bKilled);
    }
    PresentDamage(Damage, bKilled, WorldPosition);
}

void UTrainingPlayerPresentation::PresentStandaloneDamage(const float Damage, const bool bKilled,
    const FVector& WorldPosition)
{
    PresentDamage(Damage, bKilled, WorldPosition);
}

UTrainingDamageFeedbackWidget* UTrainingPlayerPresentation::EnsureDamageFeedbackWidget()
{
    UTrainingPlayerComponent* Bridge = Owner.Get();
    APlayerController* LocalController = Bridge ? Bridge->GetParticipantController() : nullptr;
    if (!LocalController || !LocalController->IsLocalController() || !Bridge->DamageFeedbackWidgetClass)
    {
        return nullptr;
    }
    if (!DamageFeedbackWidget)
    {
        DamageFeedbackWidget = CreateWidget<UTrainingDamageFeedbackWidget>(LocalController, Bridge->DamageFeedbackWidgetClass);
    }
    if (DamageFeedbackWidget && !DamageFeedbackWidget->IsInViewport())
    {
        DamageFeedbackWidget->AddToPlayerScreen(GetDefault<UTrainingSettings>()->DamageFeedbackZOrder);
    }
    return DamageFeedbackWidget;
}

void UTrainingPlayerPresentation::PresentDamage(const float Damage, const bool bKilled, const FVector& WorldPosition)
{
    if (!FMath::IsFinite(Damage) || Damage <= 0.0f)
    {
        return;
    }
    if (UTrainingDamageFeedbackWidget* Widget = EnsureDamageFeedbackWidget())
    {
        Widget->PresentDamage(Damage, bKilled, WorldPosition);
    }
}

void UTrainingPlayerPresentation::PlayBotSound(const int32 SessionId, USoundBase* Sound,
    USoundAttenuation* Attenuation, const FVector& WorldPosition, const float Volume)
{
    UTrainingPlayerComponent* Bridge = Owner.Get();
    APlayerController* LocalController = Bridge ? Bridge->GetParticipantController() : nullptr;
    if (!LocalController || !LocalController->IsLocalController() || SessionId != FeedbackSessionId || !Sound)
    {
        return;
    }

    if (!Attenuation)
    {
        if (!DefaultBotAttenuation)
        {
            DefaultBotAttenuation = NewObject<USoundAttenuation>(this);
            FSoundAttenuationSettings& Settings = DefaultBotAttenuation->Attenuation;
            Settings.bAttenuate = true;
            Settings.bSpatialize = true;
            Settings.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
            Settings.bEnableListenerFocus = false;
            Settings.bEnableOcclusion = false;
            Settings.bApplyNormalizationToStereoSounds = true;
            Settings.SpatializationAlgorithm = SPATIALIZATION_Default;
            Settings.AttenuationShape = EAttenuationShape::Sphere;
            Settings.AttenuationShapeExtents = FVector(1000.0, 0.0, 0.0);
            Settings.FalloffDistance = 6000.0f;
            Settings.NonSpatializedRadiusStart = 0.0f;
            Settings.NonSpatializedRadiusEnd = 0.0f;
            Settings.StereoSpread = 0.0f;
        }
        Attenuation = DefaultBotAttenuation;
    }
    UGameplayStatics::PlaySoundAtLocation(Bridge, Sound, WorldPosition, FRotator::ZeroRotator,
        FMath::Clamp(Volume, 0.0f, 4.0f), 1.0f, 0.0f, Attenuation);
}

void UTrainingPlayerPresentation::Clear()
{
    if (SessionHUDWidget)
    {
        SessionHUDWidget->RemoveFromParent();
        SessionHUDWidget = nullptr;
    }
    if (StartFeedbackWidget)
    {
        StartFeedbackWidget->ClearFeedback();
        StartFeedbackWidget->RemoveFromParent();
        StartFeedbackWidget = nullptr;
    }
    if (DamageFeedbackWidget)
    {
        DamageFeedbackWidget->RemoveFromParent();
        DamageFeedbackWidget = nullptr;
    }
    FeedbackSessionId = 0;
}
