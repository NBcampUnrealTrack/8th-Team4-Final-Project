#include "Training/TrainingModeDefinition.h"

#include "Training/TrainingScenario.h"
#include "Training/TrainingSettings.h"
#include "Training/TrainingArena.h"

bool FTrainingModeConfig::IsValid() const
{
    UClass* Scenario = ScenarioClass.Get();
    return Scenario && !Scenario->HasAnyClassFlags(CLASS_Abstract)
        && FMath::IsFinite(DurationSeconds) && DurationSeconds >= 0.0f
        && FMath::IsFinite(SpawnIntervalSeconds) && SpawnIntervalSeconds >= 0.05f
        && FMath::IsFinite(MinSpawnIntervalSeconds) && MinSpawnIntervalSeconds >= 0.05f
        && MaxLivingActors >= 0;
}

float FTrainingModeConfig::GetSpawnInterval(const float ElapsedSeconds) const
{
    const float StartInterval = FMath::Max(0.05f, SpawnIntervalSeconds);
    const float MinInterval = FMath::Clamp(MinSpawnIntervalSeconds, 0.05f, StartInterval);
    if (DurationSeconds <= 0.0f || FMath::IsNearlyEqual(StartInterval, MinInterval))
    {
        return StartInterval;
    }
    const float Progress = FMath::Clamp(ElapsedSeconds / DurationSeconds, 0.0f, 1.0f);
    return FMath::Lerp(StartInterval, MinInterval, Progress);
}

namespace
{
bool ResolveModeDefinition(const ETrainingMode Mode, FTrainingModeConfig& OutConfig)
{
    const UTrainingSettings* Settings = GetDefault<UTrainingSettings>();
    if (!Settings)
    {
        return false;
    }

    if (const TSoftObjectPtr<UTrainingModeDefinition>* DefinitionRef = Settings->ModeDefinitions.Find(Mode))
    {
        if (!DefinitionRef->IsNull())
        {
            if (const UTrainingModeDefinition* Definition = DefinitionRef->LoadSynchronous())
            {
                OutConfig = Definition->Config;
                return OutConfig.IsValid();
            }
            return false;
        }
    }

    // Native defaults keep existing projects working while allowing designers to migrate mode-by-mode to Data Assets.
    OutConfig = FTrainingModeConfig();
    switch (Mode)
    {
    case ETrainingMode::TargetAcquisition:
        OutConfig.ScenarioClass = UTargetAcquisitionScenario::StaticClass();
        OutConfig.DisplayName = NSLOCTEXT("TrainingMode", "TargetDisplay", "360 Target Acquisition");
        OutConfig.HUDLabel = NSLOCTEXT("TrainingMode", "TargetHUD", "360 TARGET ACQUISITION");
        OutConfig.DurationSeconds = FMath::Max(0.0f, Settings->TargetDurationSeconds);
        OutConfig.SpawnIntervalSeconds = FMath::Max(0.05f, Settings->TargetSpawnIntervalSeconds);
        OutConfig.MinSpawnIntervalSeconds = FMath::Clamp(
            Settings->TargetMinSpawnIntervalSeconds, 0.05f, OutConfig.SpawnIntervalSeconds);
        OutConfig.MaxLivingActors = 0;
        OutConfig.bShowDamageFeedback = false;
        OutConfig.bUseArenaDurationOverride = false;
        break;
    case ETrainingMode::CombatResponse:
        OutConfig.ScenarioClass = UCombatResponseScenario::StaticClass();
        OutConfig.DisplayName = NSLOCTEXT("TrainingMode", "CombatDisplay", "360 Combat Response");
        OutConfig.HUDLabel = NSLOCTEXT("TrainingMode", "CombatHUD", "360 COMBAT RESPONSE");
        OutConfig.DurationSeconds = 0.0f;
        OutConfig.SpawnIntervalSeconds = FMath::Max(0.05f, Settings->CombatSpawnIntervalSeconds);
        OutConfig.MinSpawnIntervalSeconds = OutConfig.SpawnIntervalSeconds;
        OutConfig.MaxLivingActors = FMath::Max(1, Settings->MaxLivingCombatAI);
        OutConfig.bShowDamageFeedback = true;
        OutConfig.bUseArenaDurationOverride = true;
        break;
    default:
        return false;
    }
    return OutConfig.IsValid();
}

}

bool TrainingModeDefinitions::Resolve(const ETrainingMode Mode, FTrainingModeConfig& OutConfig,
    const ATrainingArena* Arena)
{
    if (!ResolveModeDefinition(Mode, OutConfig)) { return false; }
    OutConfig.BotSource = OutConfig.ScenarioClass.GetDefaultObject()->GetBotSource();
    if (Arena && OutConfig.bUseArenaDurationOverride)
    {
        if (!FMath::IsFinite(Arena->CombatDurationSeconds)) { return false; }
        OutConfig.DurationSeconds = FMath::Max(0.0f, Arena->CombatDurationSeconds);
    }
    return OutConfig.IsValid();
}

FText TrainingModeDefinitions::GetDisplayName(const ETrainingMode Mode)
{
    FTrainingModeConfig Config;
    if (Resolve(Mode, Config) && !Config.DisplayName.IsEmpty())
    {
        return Config.DisplayName;
    }
    if (const UEnum* ModeEnum = StaticEnum<ETrainingMode>())
    {
        return ModeEnum->GetDisplayNameTextByValue(static_cast<int64>(Mode));
    }
    return NSLOCTEXT("TrainingMode", "Fallback", "Training");
}

FText TrainingModeDefinitions::GetHUDLabel(const ETrainingMode Mode)
{
    FTrainingModeConfig Config;
    return Resolve(Mode, Config) && !Config.HUDLabel.IsEmpty()
        ? Config.HUDLabel
        : GetDisplayName(Mode);
}
