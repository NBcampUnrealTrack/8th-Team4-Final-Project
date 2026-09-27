#include "Training/UI/TrainingPlayerPresentation.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Training/TrainingPlayerComponent.h"
#include "Training/CQB/CQBTrainingSpawner.h"
#include "Training/TrainingSettings.h"
#include "Training/UI/TrainingDamageFeedbackWidget.h"
#include "Training/UI/TrainingSessionHUDWidget.h"
#include "Training/UI/TrainingStartFeedbackWidget.h"

void UTrainingPlayerPresentation::Initialize(UTrainingPlayerComponent& InOwner)
{
    Owner = &InOwner;
}

void UTrainingPlayerPresentation::HandlePhase(const int32 SessionId, const ETrainingFeedbackPhase Phase,
    const double PhaseStartSeconds, const ETrainingMode Mode, const float DurationSeconds)
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
                SessionHUDWidget->StartSession(Mode, PhaseStartSeconds, DurationSeconds);
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

    const UWorld* World = Bridge->GetWorld();
    const float Elapsed = World
        ? static_cast<float>(FMath::Max(0.0, World->GetTimeSeconds() - PhaseStartSeconds))
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
    if (!LocalController || !LocalController->IsLocalController())
    {
        return nullptr;
    }

    const UTrainingSettings* Settings = GetDefault<UTrainingSettings>();
    TSubclassOf<UTrainingDamageFeedbackWidget> WidgetClass = Bridge->DamageFeedbackWidgetClass;
    if (!WidgetClass && Settings)
    {
        WidgetClass = Settings->DamageFeedbackWidgetClass.LoadSynchronous();
    }
    if (!WidgetClass)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[Training] Damage feedback widget is not configured. Set Training > UI > Damage Feedback Widget Class or a component override."));
        return nullptr;
    }

    if (!DamageFeedbackWidget)
    {
        DamageFeedbackWidget = CreateWidget<UTrainingDamageFeedbackWidget>(LocalController, WidgetClass);
    }
    if (DamageFeedbackWidget && !DamageFeedbackWidget->IsInViewport())
    {
        DamageFeedbackWidget->AddToPlayerScreen(Settings ? Settings->DamageFeedbackZOrder : 18);
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
            const UTrainingSettings* TrainingSettings = GetDefault<UTrainingSettings>();
            Settings.AttenuationShapeExtents = FVector(FMath::Max(0.0f, TrainingSettings->BotSoundInnerRadiusCentimeters), 0.0, 0.0);
            Settings.FalloffDistance = FMath::Max(0.0f, TrainingSettings->BotSoundFalloffDistanceCentimeters);
            Settings.NonSpatializedRadiusStart = 0.0f;
            Settings.NonSpatializedRadiusEnd = 0.0f;
            Settings.StereoSpread = 0.0f;
        }
        Attenuation = DefaultBotAttenuation;
    }
    UGameplayStatics::PlaySoundAtLocation(Bridge, Sound, WorldPosition, FRotator::ZeroRotator,
        FMath::Clamp(Volume, 0.0f, 4.0f), 1.0f, 0.0f, Attenuation);
}

void UTrainingPlayerPresentation::StartCQBSession(ACQBTrainingSpawner& Spawner,
    const int32 TotalTroopers, TSubclassOf<UTrainingSessionHUDWidget> WidgetClass)
{
    UTrainingPlayerComponent* Bridge = Owner.Get();
    APlayerController* LocalController = Bridge ? Bridge->GetParticipantController() : nullptr;
    if (!LocalController) { return; }
    Clear();
    if (!WidgetClass)
    {
        Bridge->NotifyMessage(NSLOCTEXT("TrainingPlayer", "CQBHUDMissing",
            "CQB HUD is not configured. Assign HUD Widget Class on BP_CQBTrainingSpawner."));
        return;
    }

    UTrainingSessionHUDWidget* HUD = CreateWidget<UTrainingSessionHUDWidget>(LocalController, WidgetClass);
    if (!HUD) { return; }
    if (!Bridge->IsCQBTrainingActiveWith(Spawner) || !Spawner.IsTrainingRunning())
    {
        HUD->RemoveFromParent();
        return;
    }
    bCQBHUD = true;
    SessionHUDWidget = HUD;
    HUD->AddToPlayerScreen(GetDefault<UTrainingSettings>()->SessionHUDZOrder);
    if (!Bridge->IsCQBTrainingActiveWith(Spawner) || !Spawner.IsTrainingRunning())
    {
        HUD->RemoveFromParent();
        if (SessionHUDWidget == HUD) { SessionHUDWidget = nullptr; bCQBHUD = false; }
        return;
    }
    HUD->StartCQBSession(TotalTroopers);
}

void UTrainingPlayerPresentation::UpdateCQBProgress(const int32 KilledTroopers,
    const int32 TotalTroopers, const bool bComplete)
{
    if (bCQBHUD && SessionHUDWidget)
    {
        SessionHUDWidget->PresentCQBKill(KilledTroopers, TotalTroopers);
        if (bComplete) { SessionHUDWidget->FinishCQBSession(); }
    }
}

void UTrainingPlayerPresentation::StopCQBSession()
{
    if (!bCQBHUD) { return; }
    if (SessionHUDWidget)
    {
        SessionHUDWidget->RemoveFromParent();
        SessionHUDWidget = nullptr;
    }
    bCQBHUD = false;
}

void UTrainingPlayerPresentation::Clear(const bool bKeepCQBHUD)
{
    if (!bKeepCQBHUD || !bCQBHUD)
    {
        if (SessionHUDWidget) { SessionHUDWidget->RemoveFromParent(); }
        SessionHUDWidget = nullptr;
        bCQBHUD = false;
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
