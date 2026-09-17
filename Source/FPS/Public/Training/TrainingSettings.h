#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Blueprint/UserWidget.h"
#include "Training/TrainingTypes.h"
#include "Training/TrainingModeDefinition.h"
#include "TrainingSettings.generated.h"


UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Training"))
class FPS_API UTrainingSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UTrainingSettings();

    float GetMinDistanceMeters() const { return FMath::Max(1.0f, MinDistanceMeters); }
    float GetMaxDistanceMeters() const { return FMath::Max(GetMinDistanceMeters(), MaxDistanceMeters); }
    float GetCountdownSeconds() const { return FMath::Max(0.0f, CountdownSeconds); }

    UPROPERTY(Config, EditAnywhere, Category = "Distance", meta = (ClampMin = "1.0", Units = "m"))
    float MinDistanceMeters = 5.0f;

    UPROPERTY(Config, EditAnywhere, Category = "Distance", meta = (ClampMin = "1.0", Units = "m"))
    float MaxDistanceMeters = 50.0f;

    UPROPERTY(Config, EditAnywhere, Category = "Distance", meta = (ClampMin = "0.0", Units = "m"))
    float DistanceHalfWidthMeters = 2.0f;

    UPROPERTY(Config, EditAnywhere, Category = "Distance", meta = (ClampMin = "1.0", Units = "cm"))
    float BotArcStepCentimeters = 150.0f;

    UPROPERTY(Config, EditAnywhere, Category = "Arena", meta = (ClampMin = "50.0", Units = "cm"))
    float MovementRadiusCentimeters = 300.0f;

    UPROPERTY(Config, EditAnywhere, Category = "Session|Target Acquisition", meta = (ClampMin = "0.05", Units = "s"))
    float TargetSpawnIntervalSeconds = 2.0f;

    UPROPERTY(Config, EditAnywhere, Category = "Session|Target Acquisition", meta = (ClampMin = "0.05", Units = "s"))
    float TargetMinSpawnIntervalSeconds = 1.0f;

    UPROPERTY(Config, EditAnywhere, Category = "Session|Target Acquisition", meta = (ClampMin = "0.0", Units = "s"))
    float TargetDurationSeconds = 60.0f;

    UPROPERTY(Config, EditAnywhere, Category = "Session|Combat Response", meta = (ClampMin = "0.05", Units = "s"))
    float CombatSpawnIntervalSeconds = 5.0f;

    UPROPERTY(Config, EditAnywhere, Category = "Session|Combat Response", meta = (ClampMin = "1"))
    int32 MaxLivingCombatAI = 10;

    UPROPERTY(Config, EditAnywhere, Category = "Session", meta = (ClampMin = "0.0", Units = "s"))
    float CountdownSeconds = 3.0f;

    UPROPERTY(Config, EditAnywhere, Category = "Health", meta = (ClampMin = "1.0"))
    float TargetHealth = 1.0f;

    UPROPERTY(Config, EditAnywhere, Category = "Health", meta = (ClampMin = "1.0"))
    float CombatHealth = 100.0f;

    UPROPERTY(Config, EditAnywhere, Category = "Health", meta = (ClampMin = "1.0"))
    float PopUpTargetHealth = 100.0f;

    // Optional data-driven overrides. If a mode is not assigned here, native defaults are used.
    UPROPERTY(Config, EditAnywhere, Category = "Session|Mode Definitions")
    TMap<ETrainingMode, TSoftObjectPtr<UTrainingModeDefinition>> ModeDefinitions;

    UPROPERTY(Config, EditAnywhere, Category = "Presentation", meta = (ClampMin = "0.1", Units = "s"))
    float DissolveDurationSeconds = 3.0f;

    UPROPERTY(Config, EditAnywhere, Category = "UI", meta = (ClampMin = "0"))
    int32 DamageFeedbackZOrder = 18;

    UPROPERTY(Config, EditAnywhere, Category = "UI", meta = (ClampMin = "0"))
    int32 SessionHUDZOrder = 19;

    UPROPERTY(Config, EditAnywhere, Category = "UI", meta = (ClampMin = "0"))
    int32 StartFeedbackZOrder = 20;

    UPROPERTY(Config, EditAnywhere, Category = "UI", meta = (ClampMin = "0"))
    int32 InteractionPromptZOrder = 25;

    UPROPERTY(Config, EditAnywhere, Category = "UI")
    TSoftClassPtr<UUserWidget> InteractionPromptWidgetClass;

    UPROPERTY(Config, EditAnywhere, Category = "UI|Mode Selection", meta = (ClampMin = "1.0", Units = "m"))
    float DefaultCustomDistanceMeters = 25.0f;

    UPROPERTY(Config, EditAnywhere, Category = "UI|Session HUD", meta = (ClampMin = "0.0", Units = "s"))
    float UrgentTimerThresholdSeconds = 10.0f;
};
