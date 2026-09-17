#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Training/TrainingTypes.h"
#include "Training/TrainingScenario.h"
#include "TrainingModeDefinition.generated.h"


UENUM(BlueprintType)
enum class ETrainingBotSource : uint8
{
    Target,
    CombatAI
};

USTRUCT(BlueprintType)
struct FPS_API FTrainingModeConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training")
    TSubclassOf<UTrainingScenario> ScenarioClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training")
    ETrainingBotSource BotSource = ETrainingBotSource::Target;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|Presentation")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training|Presentation")
    FText HUDLabel;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training", meta = (ClampMin = "0.0", Units = "s"))
    float DurationSeconds = 60.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training", meta = (ClampMin = "0.05", Units = "s"))
    float SpawnIntervalSeconds = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training", meta = (ClampMin = "0.05", Units = "s"))
    float MinSpawnIntervalSeconds = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training", meta = (ClampMin = "0"))
    int32 MaxLivingActors = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training")
    bool bShowDamageFeedback = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training")
    bool bUseArenaDurationOverride = false;

    bool IsValid() const;
    float GetSpawnInterval(float ElapsedSeconds) const;
};

UCLASS(BlueprintType)
class FPS_API UTrainingModeDefinition : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training")
    FTrainingModeConfig Config;
};

namespace TrainingModeDefinitions
{
    FPS_API bool Resolve(ETrainingMode Mode, FTrainingModeConfig& OutConfig);
    FPS_API FText GetDisplayName(ETrainingMode Mode);
    FPS_API FText GetHUDLabel(ETrainingMode Mode);
}
