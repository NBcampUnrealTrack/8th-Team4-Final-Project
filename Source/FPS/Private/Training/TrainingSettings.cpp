#include "Training/TrainingSettings.h"

#include "Blueprint/UserWidget.h"
#include "UObject/SoftObjectPath.h"

UTrainingSettings::UTrainingSettings()
{
    InteractionPromptWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(
        TEXT("/Game/UI/Training/WBP_TrainingInteractionPrompt.WBP_TrainingInteractionPrompt_C")));
}
