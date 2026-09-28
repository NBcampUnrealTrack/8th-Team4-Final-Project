#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Training/TrainingAreaTransitionTypes.h"
#include "TrainingAreaTransitionWidget.generated.h"

class STrainingAreaTransition;

UCLASS(NotBlueprintable)
class FPS_API UTrainingAreaTransitionWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UTrainingAreaTransitionWidget(const FObjectInitializer& ObjectInitializer);
    void Configure(const FTrainingAreaTransitionStyle& InStyle, const FTrainingAreaTitle& InTitle,
        FTrainingAreaCovered InCovered, FSimpleDelegate InRevealed, FSimpleDelegate InFinished,
        FSimpleDelegate InInterrupted);
    void Stop();
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeDestruct() override;

private:
    UPROPERTY(Transient)
    FTrainingAreaTransitionStyle Style;

    UPROPERTY(Transient)
    FTrainingAreaTitle AreaTitle;

    TSharedPtr<STrainingAreaTransition> TransitionView;
    FTrainingAreaCovered Covered;
    FSimpleDelegate Revealed;
    FSimpleDelegate Finished;
    FSimpleDelegate Interrupted;
};
