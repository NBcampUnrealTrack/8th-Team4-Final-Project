#include "TrainingWidgetLayout.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Sound/SoundBase.h"
#include "Sound/SlateSound.h"
#include "Engine/Font.h"
#include "UI/Common/ShooterAugmentTierStyleSet.h"
#include "UObject/UObjectGlobals.h"

namespace TrainingWidgetLayout
{
FLinearColor GetAugmentTierColor(const EAugmentTier Tier, const UShooterAugmentTierStyleSet* Palette)
{
    if (const FShooterAugmentTierStyle* Style = Palette ? Palette->FindStyle(Tier) : nullptr)
    {
        FLinearColor Color = Style->AccentColor;
        Color.A = 1.0f;
        return Color;
    }
    switch (Tier)
    {
    case EAugmentTier::Rare: return FLinearColor(FColor(80, 167, 255));
    case EAugmentTier::Epic: return FLinearColor(FColor(191, 125, 255));
    case EAugmentTier::Legendary: return FLinearColor(FColor(255, 197, 80));
    default: return FLinearColor(FColor(168, 183, 197));
    }
}

FText GetAugmentTierName(const EAugmentTier Tier, const UShooterAugmentTierStyleSet* Palette)
{
    if (const FShooterAugmentTierStyle* Style = Palette ? Palette->FindStyle(Tier) : nullptr)
    {
        if (!Style->TierDisplayName.IsEmpty()) { return Style->TierDisplayName; }
    }
    switch (Tier)
    {
    case EAugmentTier::Rare: return NSLOCTEXT("TrainingAugmentStyle", "Rare", "RARE");
    case EAugmentTier::Epic: return NSLOCTEXT("TrainingAugmentStyle", "Epic", "EPIC");
    case EAugmentTier::Legendary: return NSLOCTEXT("TrainingAugmentStyle", "Legendary", "LEGENDARY");
    default: return NSLOCTEXT("TrainingAugmentStyle", "Common", "COMMON");
    }
}

namespace
{
    const FLinearColor Ink(0.018f, 0.025f, 0.041f, 0.96f);
    const FLinearColor Panel(0.037f, 0.055f, 0.077f, 1.0f);
    const FLinearColor Line(0.10f, 0.19f, 0.24f, 1.0f);
    const FLinearColor Accent(0.20f, 0.84f, 0.94f, 1.0f);
    const FLinearColor White(0.93f, 0.97f, 1.0f, 1.0f);
    const FLinearColor Muted(0.51f, 0.63f, 0.70f, 1.0f);

    UFont* DefaultFontAsset()
    {
        static TWeakObjectPtr<UFont> CachedFont;
        if (!CachedFont.IsValid())
        {
            CachedFont = LoadObject<UFont>(nullptr, TEXT("/Engine/EngineFonts/Roboto.Roboto"));
            ensureMsgf(CachedFont.IsValid(), TEXT("[TrainingUI] Cannot load /Engine/EngineFonts/Roboto.Roboto"));
        }
        return CachedFont.Get();
    }

    bool HasPersistentFont(const FSlateFontInfo& Font)
    {
        const UFont* Asset = Cast<UFont>(Font.FontObject.Get());
        return Asset && !Asset->HasAnyFlags(RF_Transient)
            && Asset->FontCacheType == EFontCacheType::Runtime;
    }

    FSlateBrush Brush(const FLinearColor& Fill, const float Radius = 0.0f)
    {
        FSlateBrush Result;
        Result.TintColor = Fill;
        Result.DrawAs = Radius > 0.0f ? ESlateBrushDrawType::RoundedBox : ESlateBrushDrawType::Box;
        Result.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
        Result.OutlineSettings.CornerRadii = FVector4(Radius, Radius, Radius, Radius);
        Result.OutlineSettings.Color = FSlateColor(Line);
        Result.OutlineSettings.Width = Radius > 0.0f ? 1.0f : 0.0f;
        return Result;
    }

    template<class T> T* Make(UWidgetTree& Tree, const TCHAR* Name)
    {
        return Tree.ConstructWidget<T>(T::StaticClass(), FName(Name));
    }

    UTextBlock* Text(UWidgetTree& Tree, const TCHAR* Name, const TCHAR* Value,
        const int32 Size, const FLinearColor& Color, const bool bCenter = false)
    {
        UTextBlock* Label = Make<UTextBlock>(Tree, Name);
        Label->SetText(FText::FromString(Value));
        Label->SetFont(MakeFont(Size));
        Label->SetColorAndOpacity(FSlateColor(Color));
        Label->SetJustification(bCenter ? ETextJustify::Center : ETextJustify::Left);
        return Label;
    }

    UBorder* Box(UWidgetTree& Tree, const TCHAR* Name, const FLinearColor& Fill, const FMargin& Padding)
    {
        UBorder* Container = Make<UBorder>(Tree, Name);
        Container->SetBrush(Brush(Fill, 12.0f));
        Container->SetPadding(Padding);
        return Container;
    }

    UCanvasPanelSlot* Place(UCanvasPanel& Canvas, UWidget& Widget, const FAnchors& Anchors,
        const FVector2D& Alignment, const FVector2D& Position)
    {
        UCanvasPanelSlot* Slot = Canvas.AddChildToCanvas(&Widget);
        Slot->SetAnchors(Anchors);
        Slot->SetAlignment(Alignment);
        Slot->SetPosition(Position);
        Slot->SetAutoSize(true);
        return Slot;
    }

    void Metric(UWidgetTree& Tree, UHorizontalBox& Row, const TCHAR* BoxName,
        const TCHAR* LabelName, const TCHAR* ValueName, const TCHAR* Label, const TCHAR* Value,
        const bool bLast)
    {
        UBorder* Card = Box(Tree, BoxName, Panel, FMargin(24.0f, 22.0f));
        UHorizontalBoxSlot* CardSlot = Row.AddChildToHorizontalBox(Card);
        CardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        CardSlot->SetPadding(FMargin(0.0f, 0.0f, bLast ? 0.0f : 16.0f, 0.0f));
        UVerticalBox* Content = Tree.ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
        Card->SetContent(Content);
        Content->AddChildToVerticalBox(Text(Tree, LabelName, Label, 11, Muted));
        Content->AddChildToVerticalBox(Text(Tree, ValueName, Value, 54, White))
            ->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 0.0f));
    }
}

FSlateFontInfo MakeFont(const int32 Size)
{
    return FSlateFontInfo(DefaultFontAsset(), Size, FName(TEXT("Bold")));
}

bool RepairFont(FSlateFontInfo& Font)
{
    if (HasPersistentFont(Font)) { return false; }
    UFont* DefaultFont = DefaultFontAsset();
    if (!DefaultFont) { return false; }
    Font.FontObject = DefaultFont;
    Font.CompositeFont.Reset();
    Font.TypefaceFontName = FName(TEXT("Bold"));
    return true;
}

void RepairTextFonts(UWidgetTree& Tree)
{
    Tree.ForEachWidget([](UWidget* Widget)
    {
        if (UTextBlock* Label = Cast<UTextBlock>(Widget))
        {
            FSlateFontInfo Font = Label->GetFont();
            if (RepairFont(Font))
            {
                Label->SetFont(Font);
            }
        }
    });
}

void BuildSessionHUD(UWidgetTree& Tree)
{
    UCanvasPanel* Root = Make<UCanvasPanel>(Tree, TEXT("Canvas_TrainingHUD"));
    Tree.RootWidget = Root;

    USizeBox* TimerSize = Make<USizeBox>(Tree, TEXT("Size_Timer"));
    TimerSize->SetWidthOverride(244.0f);
    UBorder* TimerPanel = Box(Tree, TEXT("Panel_Timer"), Ink, FMargin(20.0f, 12.0f));
    TimerSize->SetContent(TimerPanel);
    Place(*Root, *TimerSize, FAnchors(0.5f, 0.0f), FVector2D(0.5, 0.0), FVector2D(0.0, 28.0));
    UVerticalBox* TimerContent = Make<UVerticalBox>(Tree, TEXT("TimerContent"));
    TimerPanel->SetContent(TimerContent);
    TimerContent->AddChildToVerticalBox(Text(Tree, TEXT("Txt_Mode"), TEXT("360 TARGET ACQUISITION"), 10, Accent, true));
    TimerContent->AddChildToVerticalBox(Text(Tree, TEXT("Txt_Timer"), TEXT("60"), 42, White, true));
    TimerContent->AddChildToVerticalBox(Text(Tree, TEXT("Txt_TimerLabel"), TEXT("SECONDS REMAINING"), 9, Muted, true));
    USizeBox* BarSize = Make<USizeBox>(Tree, TEXT("Size_TimeProgress"));
    BarSize->SetHeightOverride(3.0f);
    UProgressBar* Progress = Make<UProgressBar>(Tree, TEXT("Bar_TimeRemaining"));
    FProgressBarStyle ProgressStyle;
    ProgressStyle.BackgroundImage = Brush(Line);
    ProgressStyle.FillImage = Brush(FLinearColor::White);
    Progress->SetWidgetStyle(ProgressStyle);
    Progress->SetFillColorAndOpacity(Accent);
    Progress->SetPercent(1.0f);
    BarSize->SetContent(Progress);
    TimerContent->AddChildToVerticalBox(BarSize)->SetPadding(FMargin(0.0f, 10.0f, 0.0f, 0.0f));

    UVerticalBox* Counter = Make<UVerticalBox>(Tree, TEXT("KillCounterContent"));
    Counter->SetVisibility(ESlateVisibility::Hidden);
    Counter->SetRenderOpacity(0.0f);
    Place(*Root, *Counter, FAnchors(0.5f, 0.5f), FVector2D(0.0, 0.5), FVector2D(92.0, 60.0));
    UTextBlock* KillText = Text(Tree, TEXT("Txt_Kills"), TEXT("+0"), 30, Accent);
    KillText->SetShadowOffset(FVector2D(1.0, 1.0));
    KillText->SetShadowColorAndOpacity(FLinearColor::Black);
    Counter->AddChildToVerticalBox(KillText);
    UTextBlock* KillLabel = Text(Tree, TEXT("Txt_KillLabel"), TEXT("ELIMINATIONS"), 9, White);
    KillLabel->SetShadowOffset(FVector2D(1.0, 1.0));
    KillLabel->SetShadowColorAndOpacity(FLinearColor::Black);
    Counter->AddChildToVerticalBox(KillLabel);

}

void BuildResults(UWidgetTree& Tree)
{
    UCanvasPanel* Root = Make<UCanvasPanel>(Tree, TEXT("Canvas_TrainingResults"));
    Tree.RootWidget = Root;
    UBorder* Backdrop = Make<UBorder>(Tree, TEXT("Backdrop"));
    Backdrop->SetBrush(Brush(FLinearColor(0.003f, 0.008f, 0.017f, 0.78f)));
    UCanvasPanelSlot* BackdropSlot = Root->AddChildToCanvas(Backdrop);
    BackdropSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
    BackdropSlot->SetOffsets(FMargin(0.0f));

    USizeBox* Size = Make<USizeBox>(Tree, TEXT("Size_Results"));
    Size->SetWidthOverride(660.0f);
    Place(*Root, *Size, FAnchors(0.5f, 0.5f), FVector2D(0.5, 0.5), FVector2D::ZeroVector);
    UBorder* Card = Box(Tree, TEXT("Panel_Results"), Ink, FMargin(32.0f));
    Size->SetContent(Card);
    UVerticalBox* Content = Make<UVerticalBox>(Tree, TEXT("ResultsContent"));
    Card->SetContent(Content);
    Content->AddChildToVerticalBox(Text(Tree, TEXT("Txt_ResultEyebrow"), TEXT("TRAINING  /  SESSION REPORT"), 11, Accent));
    Content->AddChildToVerticalBox(Text(Tree, TEXT("Txt_ResultTitle"), TEXT("SESSION COMPLETE"), 30, White))
        ->SetPadding(FMargin(0.0f, 12.0f, 0.0f, 8.0f));
    Content->AddChildToVerticalBox(Text(Tree, TEXT("Txt_ResultMode"), TEXT("360 Target Acquisition"), 14, Muted));
    Content->AddChildToVerticalBox(Text(Tree, TEXT("Txt_ResultDuration"), TEXT("60.0 SECONDS"), 12, Muted))
        ->SetPadding(FMargin(0.0f, 4.0f, 0.0f, 24.0f));
    UHorizontalBox* Metrics = Make<UHorizontalBox>(Tree, TEXT("ResultMetrics"));
    Content->AddChildToVerticalBox(Metrics);
    Metric(Tree, *Metrics, TEXT("Card_Kills"), TEXT("Lbl_ResultKills"), TEXT("Txt_ResultKills"),
        TEXT("ELIMINATIONS"), TEXT("0"), false);
    Metric(Tree, *Metrics, TEXT("Card_Accuracy"), TEXT("Lbl_ResultAccuracy"), TEXT("Txt_ResultAccuracy"),
        TEXT("ACCURACY"), TEXT("0.0%"), true);
    Content->AddChildToVerticalBox(Text(Tree, TEXT("Txt_ResultShots"), TEXT("HIT SHOTS  0 / 0"), 12, Muted))
        ->SetPadding(FMargin(0.0f, 14.0f, 0.0f, 24.0f));

    USizeBox* ButtonSize = Make<USizeBox>(Tree, TEXT("Size_Confirm"));
    ButtonSize->SetHeightOverride(52.0f);
    UButton* Confirm = Make<UButton>(Tree, TEXT("Btn_Confirm"));
    FButtonStyle Style;
    Style.SetNormal(Brush(Accent, 8.0f));
    Style.SetHovered(Brush(FLinearColor(0.43f, 0.95f, 1.0f), 8.0f));
    Style.SetPressed(Brush(FLinearColor(0.10f, 0.58f, 0.69f), 8.0f));
    Confirm->SetStyle(Style);
    Confirm->SetContent(Text(Tree, TEXT("Txt_ResultConfirm"), TEXT("CONFIRM"), 16, Ink, true));
    ButtonSize->SetContent(Confirm);
    Content->AddChildToVerticalBox(ButtonSize);
    Content->AddChildToVerticalBox(Text(Tree, TEXT("Txt_ResultHint"),
        TEXT("Close this report, then interact with the panel to train again."), 11, Muted, true))
        ->SetPadding(FMargin(0.0f, 16.0f, 0.0f, 0.0f));
}

void BuildCountdown(UWidgetTree& Tree)
{
    UCanvasPanel* Root = Make<UCanvasPanel>(Tree, TEXT("FeedbackCanvas"));
    Tree.RootWidget = Root;
    UVerticalBox* Content = Make<UVerticalBox>(Tree, TEXT("FeedbackContent"));
    Place(*Root, *Content, FAnchors(0.5f, 0.5f), FVector2D(0.0, 0.5), FVector2D(72.0, -12.0));
    Content->AddChildToVerticalBox(Text(Tree, TEXT("Txt_FeedbackLabel"), TEXT("TRAINING"), 11, Accent))
        ->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 5.0f));
    Content->AddChildToVerticalBox(Text(Tree, TEXT("Txt_FeedbackMessage"), TEXT("3초 뒤에 시작합니다"), 22, White));
}

void ApplyButtonSounds(UUserWidget& Widget, USoundBase* HoverSound, USoundBase* ClickSound)
{
    if (!Widget.WidgetTree || (!HoverSound && !ClickSound)) { return; }
    Widget.WidgetTree->ForEachWidget([HoverSound, ClickSound](UWidget* Candidate)
    {
        if (UButton* Button = Cast<UButton>(Candidate))
        {
            FButtonStyle Style = Button->GetStyle();
            if (HoverSound)
            {
                FSlateSound Hover;
                Hover.SetResourceObject(HoverSound);
                Style.SetHoveredSound(Hover);
            }
            if (ClickSound)
            {
                FSlateSound Click;
                Click.SetResourceObject(ClickSound);
                Style.SetPressedSound(Click);
            }
            Button->SetStyle(Style);
        }
        if (UUserWidget* Nested = Cast<UUserWidget>(Candidate))
        {
            ApplyButtonSounds(*Nested, HoverSound, ClickSound);
        }
    });
}
}
