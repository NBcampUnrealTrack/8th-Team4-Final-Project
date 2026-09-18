#include "Training/TrainingSettings.h"

#include "Training/UI/TrainingDamageFeedbackWidget.h"
#include "Blueprint/UserWidget.h"
#include "UObject/SoftObjectPath.h"

UTrainingSettings::UTrainingSettings()
{
    DamageFeedbackWidgetClass = TSoftClassPtr<UTrainingDamageFeedbackWidget>(FSoftObjectPath(
        TEXT("/Game/UI/Training/WBP_TrainingDamageFeedback.WBP_TrainingDamageFeedback_C")));

    InteractionPromptWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(
        TEXT("/Game/UI/Training/WBP_TrainingInteractionPrompt.WBP_TrainingInteractionPrompt_C")));
}
