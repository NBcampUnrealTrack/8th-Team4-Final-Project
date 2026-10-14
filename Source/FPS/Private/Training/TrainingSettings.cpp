#include "Training/TrainingSettings.h"

#include "Training/UI/TrainingDamageFeedbackWidget.h"
#include "Blueprint/UserWidget.h"
#include "UObject/SoftObjectPath.h"
#include "UI/TrainingWidgetStyle.h"

UTrainingSettings::UTrainingSettings()
{
    AreaTransition.TitleFont = TrainingWidgetStyle::MakeFont(26);
    AreaTransition.TitleFont.TypefaceFontName = FName(TEXT("BoldItalic"));
    AreaTransition.TitleFont.LetterSpacing = 90;
    AreaTransition.SubtitleFont = TrainingWidgetStyle::MakeFont(10);
    AreaTransition.SubtitleFont.LetterSpacing = 180;

    FTrainingAreaTitle& FreePractice = AreaTitles.Add(TrainingAreaIds::FreePractice);
    FreePractice.Title = NSLOCTEXT("TrainingArea", "FreePractice", "FREE PRACTICE");
    FreePractice.Subtitle = NSLOCTEXT("TrainingArea", "FreePracticeSubtitle", "TRAINING GROUND");

    FTrainingAreaTitle& Arena = AreaTitles.Add(TrainingAreaIds::Arena);
    Arena.Title = NSLOCTEXT("TrainingArea", "Arena", "360° ARENA");
    Arena.Subtitle = NSLOCTEXT("TrainingArea", "ArenaSubtitle", "ACQUIRE. REACT. REPEAT.");

    FTrainingAreaTitle& CQB = AreaTitles.Add(TrainingAreaIds::CQB);
    CQB.Title = NSLOCTEXT("TrainingArea", "CQB", "CQB FACILITY");
    CQB.Subtitle = NSLOCTEXT("TrainingArea", "CQBSubtitle", "CLOSE-QUARTERS TRAINING");

    DamageFeedbackWidgetClass = TSoftClassPtr<UTrainingDamageFeedbackWidget>(FSoftObjectPath(
        TEXT("/Game/UI/Training/WBP_TrainingDamageFeedback.WBP_TrainingDamageFeedback_C")));

    InteractionPromptWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(
        TEXT("/Game/UI/Training/WBP_TrainingInteractionPrompt.WBP_TrainingInteractionPrompt_C")));
}
