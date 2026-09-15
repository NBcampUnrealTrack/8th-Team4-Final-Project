#include "Training/UI/TrainingSessionHUDWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "TrainingWidgetLayout.h"

UTrainingSessionHUDWidget::UTrainingSessionHUDWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetIsFocusable(false);
}

TSharedRef<SWidget> UTrainingSessionHUDWidget::RebuildWidget()
{
    if (!WidgetTree) { WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient); }
    if (!WidgetTree->RootWidget) { TrainingWidgetLayout::BuildSessionHUD(*WidgetTree); }
    TrainingWidgetLayout::RepairTextFonts(*WidgetTree);
    Txt_Mode = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_Mode")));
    Txt_Timer = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_Timer")));
    Txt_TimerLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_TimerLabel")));
    Txt_Kills = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_Kills")));
    Txt_KillLabel = Cast<UTextBlock>(WidgetTree->FindWidget(TEXT("Txt_KillLabel")));
    KillCounterContent = WidgetTree->FindWidget(TEXT("KillCounterContent"));
    ApplyKillFeedbackAppearance();
    Bar_TimeRemaining = Cast<UProgressBar>(WidgetTree->FindWidget(TEXT("Bar_TimeRemaining")));
    Canvas_Damage = Cast<UCanvasPanel>(WidgetTree->FindWidget(TEXT("Canvas_Damage")));
    return Super::RebuildWidget();
}

void UTrainingSessionHUDWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    if (WidgetTree) { TrainingWidgetLayout::RepairTextFonts(*WidgetTree); }
    ApplyKillFeedbackAppearance();
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UTrainingSessionHUDWidget::StartSession(const ETrainingMode Mode,
    const double InStartServerSeconds, const float DurationSeconds)
{
    ActiveMode = Mode;
    StartServerSeconds = InStartServerSeconds;
    ActiveDurationSeconds = FMath::Max(0.0f, DurationSeconds);
    bSessionActive = true;
    DisplayedSeconds = INDEX_NONE;
    DisplayedKills = 0;
    ResetKillFeedback();
    ClearDamageNumbers();
    if (Txt_Kills)
    {
        Txt_Kills->SetText(FText::GetEmpty());
        Txt_Kills->SetRenderScale(FVector2D(1.0, 1.0));
    }
    if (Txt_Mode)
    {
        Txt_Mode->SetText(Mode == ETrainingMode::TargetAcquisition
            ? NSLOCTEXT("TrainingHUD", "Target", "360 TARGET ACQUISITION")
            : NSLOCTEXT("TrainingHUD", "Combat", "360 COMBAT RESPONSE"));
    }
    if (Txt_TimerLabel)
    {
        Txt_TimerLabel->SetText(ActiveDurationSeconds > 0.0f
            ? NSLOCTEXT("TrainingHUD", "Remaining", "SECONDS REMAINING")
            : NSLOCTEXT("TrainingHUD", "Elapsed", "ELAPSED  /  MANUAL END"));
    }
    if (Bar_TimeRemaining)
    {
        Bar_TimeRemaining->SetVisibility(ActiveDurationSeconds > 0.0f
            ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    RefreshClock();
}

void UTrainingSessionHUDWidget::RefreshClock()
{
    if (!bSessionActive || !GetWorld()) { return; }
    const AGameStateBase* GameState = GetWorld()->GetGameState();
    const double ServerNow = GameState ? GameState->GetServerWorldTimeSeconds() : GetWorld()->GetTimeSeconds();
    const float Elapsed = static_cast<float>(FMath::Max(0.0, ServerNow - StartServerSeconds));
    const int32 Seconds = ActiveDurationSeconds > 0.0f
        ? TrainingRules::RemainingWholeSeconds(ActiveDurationSeconds, Elapsed) : FMath::FloorToInt(Elapsed);
    if (Seconds != DisplayedSeconds)
    {
        DisplayedSeconds = Seconds;
        if (Txt_Timer)
        {
            Txt_Timer->SetText(FText::FromString(ActiveDurationSeconds > 0.0f
                ? FString::Printf(TEXT("%02d"), Seconds)
                : FString::Printf(TEXT("%02d:%02d"), Seconds / 60, Seconds % 60)));
            Txt_Timer->SetColorAndOpacity(FSlateColor(ActiveDurationSeconds > 0.0f && Seconds <= 10
                ? FLinearColor(1.0f, 0.43f, 0.25f) : FLinearColor(0.93f, 0.97f, 1.0f)));
        }
    }
    if (Bar_TimeRemaining && ActiveDurationSeconds > 0.0f)
    {
        Bar_TimeRemaining->SetPercent(FMath::Clamp(1.0f - Elapsed / ActiveDurationSeconds, 0.0f, 1.0f));
    }
}

void UTrainingSessionHUDWidget::PresentHit(const int32 TotalKills, const float Damage,
    const bool bKilled, const FVector& WorldPosition)
{
    if (!bSessionActive) { return; }
    if (bKilled && TotalKills > DisplayedKills)
    {
        DisplayedKills = TotalKills;
        KillFadeStartOpacity = bKillFeedbackActive ? KillFeedbackOpacity : 0.0f;
        KillFeedbackAge = 0.0f;
        bKillFeedbackActive = true;
        if (Txt_Kills) { Txt_Kills->SetText(FText::FromString(FString::Printf(TEXT("+%d"), TotalKills))); }
        UpdateKillFeedback(0.0f);
    }
    if (ActiveMode != ETrainingMode::CombatResponse || !Canvas_Damage || !FMath::IsFinite(Damage) || Damage <= 0.0f)
    {
        return;
    }
    const int32 Limit = FMath::Clamp(MaxVisibleDamageNumbers, 1, 64);
    while (DamageNumbers.Num() >= Limit)
    {
        if (UTextBlock* OldText = DamageNumbers[0].Text.Get()) { OldText->RemoveFromParent(); }
        DamageNumbers.RemoveAt(0);
    }
    UTextBlock* DamageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    DamageText->SetText(FText::AsNumber(FMath::RoundToInt(Damage)));
    DamageText->SetFont(TrainingWidgetLayout::MakeFont(bKilled ? 28 : 24));
    DamageText->SetColorAndOpacity(FSlateColor(bKilled
        ? FLinearColor(1.0f, 0.65f, 0.27f) : FLinearColor(1.0f, 0.97f, 0.76f)));
    DamageText->SetShadowOffset(FVector2D(1.0, 1.0));
    DamageText->SetShadowColorAndOpacity(FLinearColor::Black);
    DamageText->SetVisibility(ESlateVisibility::Hidden);
    UCanvasPanelSlot* DamageSlot = Canvas_Damage->AddChildToCanvas(DamageText);
    DamageSlot->SetAutoSize(true);
    DamageSlot->SetAlignment(FVector2D(0.5, 1.0));
    FDamageNumber& Entry = DamageNumbers.AddDefaulted_GetRef();
    Entry.Text = DamageText;
    Entry.WorldPosition = WorldPosition + DamageNumberWorldOffset;
    Entry.HorizontalOffset = static_cast<float>((DamageSequence++ % 3) - 1) * 18.0f;
}

void UTrainingSessionHUDWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!bSessionActive) { return; }
    RefreshClock();
    UpdateKillFeedback(InDeltaTime);
    const float Lifetime = FMath::Max(0.1f, DamageNumberLifetime);
    APlayerController* ParticipantController = GetOwningPlayer();
    for (int32 Index = DamageNumbers.Num() - 1; Index >= 0; --Index)
    {
        FDamageNumber& Entry = DamageNumbers[Index];
        UTextBlock* DamageText = Entry.Text.Get();
        Entry.Age += InDeltaTime;
        if (!DamageText || Entry.Age >= Lifetime)
        {
            if (DamageText) { DamageText->RemoveFromParent(); }
            DamageNumbers.RemoveAtSwap(Index);
            continue;
        }
        FVector2D Position;
        if (ParticipantController && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
            ParticipantController, Entry.WorldPosition, Position, true))
        {
            if (UCanvasPanelSlot* DamageSlot = Cast<UCanvasPanelSlot>(DamageText->Slot))
            {
                DamageSlot->SetPosition(Position + FVector2D(Entry.HorizontalOffset, -44.0f * Entry.Age / Lifetime));
            }
            DamageText->SetVisibility(ESlateVisibility::HitTestInvisible);
            DamageText->SetRenderOpacity(FMath::Clamp((Lifetime - Entry.Age) / (Lifetime * 0.4f), 0.0f, 1.0f));
        }
        else { DamageText->SetVisibility(ESlateVisibility::Hidden); }
    }
}

void UTrainingSessionHUDWidget::UpdateKillFeedback(const float DeltaSeconds)
{
    if (!bKillFeedbackActive) { return; }
    KillFeedbackAge += FMath::Max(0.0f, DeltaSeconds);
    const float FadeIn = FMath::Max(0.0f, KillFadeInSeconds);
    const float Hold = FMath::Max(0.0f, KillHoldSeconds);
    const float FadeOut = FMath::Max(0.0f, KillFadeOutSeconds);
    const float FadeOutStart = FadeIn + Hold;
    if (KillFeedbackAge >= FadeOutStart + FadeOut)
    {
        ResetKillFeedback();
        return;
    }
    const auto Smooth = [](const float Alpha)
    {
        const float T = FMath::Clamp(Alpha, 0.0f, 1.0f);
        return T * T * (3.0f - 2.0f * T);
    };
    if (FadeIn > KINDA_SMALL_NUMBER && KillFeedbackAge < FadeIn)
    {
        KillFeedbackOpacity = FMath::Lerp(KillFadeStartOpacity, 1.0f, Smooth(KillFeedbackAge / FadeIn));
    }
    else if (KillFeedbackAge < FadeOutStart)
    {
        KillFeedbackOpacity = 1.0f;
    }
    else
    {
        KillFeedbackOpacity = FadeOut > KINDA_SMALL_NUMBER
            ? 1.0f - Smooth((KillFeedbackAge - FadeOutStart) / FadeOut) : 0.0f;
    }
    ApplyKillFeedbackAppearance();
}

void UTrainingSessionHUDWidget::ApplyKillFeedbackAppearance()
{
    const ESlateVisibility CounterVisibility = bKillFeedbackActive
        ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Hidden;
    if (KillCounterContent)
    {
        KillCounterContent->SetRenderOpacity(KillFeedbackOpacity);
        KillCounterContent->SetVisibility(CounterVisibility);
    }
    else
    {
        for (UTextBlock* Label : {Txt_Kills.Get(), Txt_KillLabel.Get()})
        {
            if (Label)
            {
                Label->SetRenderOpacity(KillFeedbackOpacity);
                Label->SetVisibility(CounterVisibility);
            }
        }
    }
}

void UTrainingSessionHUDWidget::ResetKillFeedback()
{
    bKillFeedbackActive = false;
    KillFeedbackAge = 0.0f;
    KillFeedbackOpacity = 0.0f;
    KillFadeStartOpacity = 0.0f;
    ApplyKillFeedbackAppearance();
}

void UTrainingSessionHUDWidget::ClearDamageNumbers()
{
    for (const FDamageNumber& Entry : DamageNumbers)
    {
        if (UTextBlock* DamageText = Entry.Text.Get()) { DamageText->RemoveFromParent(); }
    }
    DamageNumbers.Reset();
}

void UTrainingSessionHUDWidget::NativeDestruct()
{
    bSessionActive = false;
    ResetKillFeedback();
    ClearDamageNumbers();
    Super::NativeDestruct();
}
