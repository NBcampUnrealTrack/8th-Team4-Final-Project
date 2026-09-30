#include "Training/UI/TrainingAreaTransitionWidget.h"

#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace TrainingAreaAnimation
{
    float Progress(const double Elapsed, const float Duration)
    {
        return Duration <= 0.0f ? 1.0f : FMath::Clamp(static_cast<float>(Elapsed / Duration), 0.0f, 1.0f);
    }

    float Smooth(const float Alpha)
    {
        return Alpha * Alpha * (3.0f - 2.0f * Alpha);
    }
}

class STrainingAreaTransition final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(STrainingAreaTransition) {}
        SLATE_ARGUMENT(FTrainingAreaTransitionStyle, Style)
        SLATE_ARGUMENT(FTrainingAreaTitle, AreaTitle)
        SLATE_EVENT(FTrainingAreaCovered, OnCovered)
        SLATE_EVENT(FSimpleDelegate, OnRevealed)
        SLATE_EVENT(FSimpleDelegate, OnFinished)
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs)
    {
        Style = InArgs._Style;
        OnCovered = InArgs._OnCovered;
        OnRevealed = InArgs._OnRevealed;
        OnFinished = InArgs._OnFinished;
        bHasTitle = !InArgs._AreaTitle.Title.IsEmpty();
        PhaseStartSeconds = FSlateApplication::Get().GetCurrentTime();
        SetCanTick(false);
        SetVisibility(EVisibility::HitTestInvisible);
        ActiveTimerHandle = RegisterActiveTimer(0.0f,
            FWidgetActiveTimerDelegate::CreateSP(this, &STrainingAreaTransition::UpdateTransition));

        ChildSlot
        [
            SNew(SOverlay)
            + SOverlay::Slot()
            [
                SAssignNew(ScreenCover, SImage)
                .Image(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
                .ColorAndOpacity(FLinearColor::Black)
            ]
            + SOverlay::Slot()
            [
                SNew(SConstraintCanvas)
                + SConstraintCanvas::Slot()
                .Anchors(FAnchors(0.5f, 0.5f))
                .Alignment(FVector2D(0.0, 0.5))
                .Offset(FMargin(Style.OffsetFromAim.X, Style.OffsetFromAim.Y, 0.0, 0.0))
                .AutoSize(true)
                [
                    SAssignNew(TitleContent, SVerticalBox)
                    + SVerticalBox::Slot().AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(InArgs._AreaTitle.Title)
                        .Font(Style.TitleFont)
                        .ColorAndOpacity(Style.TitleColor)
                        .ShadowColorAndOpacity(Style.ShadowColor)
                        .ShadowOffset(Style.ShadowOffset)
                    ]
                    + SVerticalBox::Slot().AutoHeight().Padding(0.0f, Style.SubtitleGap, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(InArgs._AreaTitle.Subtitle)
                        .Font(Style.SubtitleFont)
                        .ColorAndOpacity(Style.SubtitleColor)
                        .ShadowColorAndOpacity(Style.ShadowColor)
                        .ShadowOffset(Style.ShadowOffset)
                        .Visibility(InArgs._AreaTitle.Subtitle.IsEmpty()
                            ? EVisibility::Collapsed : EVisibility::HitTestInvisible)
                    ]
                ]
            ]
        ];
        ScreenCover->SetRenderOpacity(0.0f);
        TitleContent->SetRenderOpacity(0.0f);
    }

    void Stop()
    {
        Phase = EPhase::Stopped;
        if (TSharedPtr<FActiveTimerHandle> Handle = ActiveTimerHandle.Pin())
        {
            UnRegisterActiveTimer(Handle.ToSharedRef());
        }
        ActiveTimerHandle.Reset();
        OnCovered.Unbind();
        OnRevealed.Unbind();
        OnFinished.Unbind();
    }

    EActiveTimerReturnType UpdateTransition(const double InCurrentTime, const float)
    {
        const double Elapsed = FMath::Max(0.0, InCurrentTime - PhaseStartSeconds);
        switch (Phase)
        {
        case EPhase::Covering:
        {
            const float Alpha = TrainingAreaAnimation::Progress(Elapsed, Style.FadeToBlackSeconds);
            ScreenCover->SetRenderOpacity(TrainingAreaAnimation::Smooth(Alpha));
            if (Alpha >= 1.0f)
            {
                Phase = EPhase::AwaitingBlackFrame;
                Invalidate(EInvalidateWidgetReason::Paint);
            }
            break;
        }
        case EPhase::AwaitingBlackFrame:
            // 검은 화면이 실제로 그려진 다음 프레임에만 위치를 바꿉니다.
            if (bBlackFramePainted)
            {
                Phase = EPhase::Holding;
                const FTrainingAreaCovered Move = OnCovered;
                OnCovered.Unbind();
                bShowTitle = Move.IsBound() && Move.Execute() && bHasTitle;
                PhaseStartSeconds = FSlateApplication::Get().GetCurrentTime();
            }
            break;
        case EPhase::Holding:
            if (Elapsed >= FMath::Max(0.0f, Style.BlackHoldSeconds))
            {
                Phase = EPhase::Revealing;
                PhaseStartSeconds = InCurrentTime;
            }
            break;
        case EPhase::Revealing:
        {
            const float Alpha = TrainingAreaAnimation::Progress(Elapsed, Style.RevealSeconds);
            ScreenCover->SetRenderOpacity(1.0f - TrainingAreaAnimation::Smooth(Alpha));
            if (Alpha >= 1.0f && !bRevealNotified)
            {
                bRevealNotified = true;
                const FSimpleDelegate Notify = OnRevealed;
                OnRevealed.Unbind();
                Notify.ExecuteIfBound();
                if (Phase == EPhase::Stopped) return EActiveTimerReturnType::Stop;
            }
            const bool bTitleComplete = UpdateTitle(Elapsed);
            if (bRevealNotified && bTitleComplete)
            {
                const FSimpleDelegate Notify = OnFinished;
                Phase = EPhase::Stopped;
                ActiveTimerHandle.Reset();
                OnCovered.Unbind();
                OnRevealed.Unbind();
                OnFinished.Unbind();
                Notify.ExecuteIfBound();
                return EActiveTimerReturnType::Stop;
            }
            break;
        }
        case EPhase::Stopped:
            return EActiveTimerReturnType::Stop;
        }
        return Phase == EPhase::Stopped
            ? EActiveTimerReturnType::Stop : EActiveTimerReturnType::Continue;
    }

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
        const int32 LayerId, const FWidgetStyle& InWidgetStyle, const bool bParentEnabled) const override
    {
        const int32 Result = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect,
            OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
        if (Phase == EPhase::AwaitingBlackFrame)
        {
            bBlackFramePainted = true;
        }
        return Result;
    }

private:
    bool UpdateTitle(const double RevealElapsed)
    {
        if (!bShowTitle) return true;
        const double Age = RevealElapsed - FMath::Max(0.0f, Style.TitleDelaySeconds);
        if (Age < 0.0) return false;

        const float FadeIn = FMath::Max(0.0f, Style.TitleFadeInSeconds);
        const float FadeOutStart = FadeIn + FMath::Max(0.0f, Style.TitleHoldSeconds);
        float Opacity = 1.0f;
        float Slide = 0.0f;
        bool bComplete = false;
        if (Age < FadeIn)
        {
            Opacity = TrainingAreaAnimation::Smooth(TrainingAreaAnimation::Progress(Age, FadeIn));
            Slide = (1.0f - Opacity) * Style.SlideInDistance;
        }
        else if (Age >= FadeOutStart)
        {
            const float Alpha = TrainingAreaAnimation::Progress(Age - FadeOutStart, Style.TitleFadeOutSeconds);
            Opacity = 1.0f - TrainingAreaAnimation::Smooth(Alpha);
            Slide = -(1.0f - Opacity) * Style.SlideOutDistance;
            bComplete = Alpha >= 1.0f;
        }
        TitleContent->SetRenderOpacity(Opacity);
        TitleContent->SetRenderTransform(FSlateRenderTransform(FVector2f(Slide, 0.0f)));
        return bComplete;
    }

    enum class EPhase : uint8 { Covering, AwaitingBlackFrame, Holding, Revealing, Stopped };
    EPhase Phase = EPhase::Covering;
    FTrainingAreaTransitionStyle Style;
    TSharedPtr<SImage> ScreenCover;
    TSharedPtr<SVerticalBox> TitleContent;
    FTrainingAreaCovered OnCovered;
    FSimpleDelegate OnRevealed;
    FSimpleDelegate OnFinished;
    TWeakPtr<FActiveTimerHandle> ActiveTimerHandle;
    double PhaseStartSeconds = 0.0;
    mutable bool bBlackFramePainted = false;
    bool bHasTitle = false;
    bool bShowTitle = false;
    bool bRevealNotified = false;
};

UTrainingAreaTransitionWidget::UTrainingAreaTransitionWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetIsFocusable(false);
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UTrainingAreaTransitionWidget::Configure(const FTrainingAreaTransitionStyle& InStyle,
    const FTrainingAreaTitle& InTitle, FTrainingAreaCovered InCovered, FSimpleDelegate InRevealed,
    FSimpleDelegate InFinished, FSimpleDelegate InInterrupted)
{
    Style = InStyle;
    AreaTitle = InTitle;
    Covered = MoveTemp(InCovered);
    Revealed = MoveTemp(InRevealed);
    Finished = MoveTemp(InFinished);
    Interrupted = MoveTemp(InInterrupted);
}

TSharedRef<SWidget> UTrainingAreaTransitionWidget::RebuildWidget()
{
    return SAssignNew(TransitionView, STrainingAreaTransition)
        .Style(Style)
        .AreaTitle(AreaTitle)
        .OnCovered(Covered)
        .OnRevealed(Revealed)
        .OnFinished(Finished);
}

void UTrainingAreaTransitionWidget::Stop()
{
    if (TransitionView) TransitionView->Stop();
    Covered.Unbind();
    Revealed.Unbind();
    Finished.Unbind();
    Interrupted.Unbind();
}

void UTrainingAreaTransitionWidget::NativeDestruct()
{
    const FSimpleDelegate Notify = Interrupted;
    Stop();
    Notify.ExecuteIfBound();
    Super::NativeDestruct();
}

void UTrainingAreaTransitionWidget::ReleaseSlateResources(const bool bReleaseChildren)
{
    Stop();
    TransitionView.Reset();
    Super::ReleaseSlateResources(bReleaseChildren);
}
