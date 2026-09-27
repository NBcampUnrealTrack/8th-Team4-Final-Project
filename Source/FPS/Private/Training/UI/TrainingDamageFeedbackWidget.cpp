#include "Training/UI/TrainingDamageFeedbackWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "TrainingWidgetStyle.h"

UTrainingDamageFeedbackWidget::UTrainingDamageFeedbackWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetIsFocusable(false);
}

void UTrainingDamageFeedbackWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    SetVisibility(IsDesignTime() || !DamageNumbers.IsEmpty()
        ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (!IsDesignTime() && !Canvas_Damage)
    {
        UE_LOG(LogTemp, Error, TEXT("[Training] Damage Feedback WBP requires a Canvas Panel named Canvas_Damage."));
    }
}

void UTrainingDamageFeedbackWidget::PresentDamage(const float Damage, const bool bLethal,
    const FVector& WorldPosition)
{
    if (!Canvas_Damage || !WidgetTree || !FMath::IsFinite(Damage) || Damage <= 0.0f
        || WorldPosition.ContainsNaN())
    {
        return;
    }

    SetVisibility(ESlateVisibility::HitTestInvisible);
    const int32 Limit = FMath::Clamp(MaxVisibleDamageNumbers, 1, 64);
    while (DamageNumbers.Num() >= Limit)
    {
        if (UTextBlock* OldText = DamageNumbers[0].Text.Get())
        {
            OldText->RemoveFromParent();
        }
        DamageNumbers.RemoveAt(0, 1, EAllowShrinking::No);
    }

    UTextBlock* DamageText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    DamageText->SetText(FText::AsNumber(FMath::RoundToInt(Damage)));

    FSlateFontInfo DamageFont = TrainingWidgetStyle::MakeFont(bLethal ? LethalDamageFontSize : NormalDamageFontSize);
    DamageFont.OutlineSettings.OutlineSize = FMath::Clamp(DamageOutlineSize, 0, 4);
    DamageFont.OutlineSettings.OutlineColor = DamageOutlineColor;
    DamageText->SetFont(DamageFont);
    DamageText->SetColorAndOpacity(FSlateColor(bLethal ? LethalDamageColor : NormalDamageColor));
    DamageText->SetJustification(ETextJustify::Center);
    DamageText->SetShadowOffset(DamageShadowOffset);
    DamageText->SetShadowColorAndOpacity(DamageShadowColor);
    DamageText->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
    const float StartScale = bLethal ? LethalDamageStartScale : NormalDamageStartScale;
    DamageText->SetRenderScale(FVector2D(StartScale, StartScale));
    DamageText->SetVisibility(ESlateVisibility::Hidden);

    UCanvasPanelSlot* DamageSlot = Canvas_Damage->AddChildToCanvas(DamageText);
    DamageSlot->SetAutoSize(true);
    DamageSlot->SetAlignment(FVector2D(0.5, 1.0));

    FDamageNumber& Entry = DamageNumbers.AddDefaulted_GetRef();
    Entry.Text = DamageText;
    Entry.WorldPosition = WorldPosition + DamageNumberWorldOffset;
    Entry.HorizontalOffset = static_cast<float>(DamageSequence - 1) * FMath::Max(0.0f, DamageHorizontalSpreadPixels);
    Entry.bLethal = bLethal;
    DamageSequence = (DamageSequence + 1) % 3;
}

void UTrainingDamageFeedbackWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (DamageNumbers.IsEmpty())
    {
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    const float Lifetime = FMath::Max(0.1f, DamageNumberLifetime);
    APlayerController* ParticipantController = GetOwningPlayer();
    for (int32 Index = DamageNumbers.Num() - 1; Index >= 0; --Index)
    {
        FDamageNumber& Entry = DamageNumbers[Index];
        UTextBlock* DamageText = Entry.Text.Get();
        Entry.Age += InDeltaTime;
        if (!DamageText || Entry.Age >= Lifetime)
        {
            if (DamageText)
            {
                DamageText->RemoveFromParent();
            }
            DamageNumbers.RemoveAt(Index, 1, EAllowShrinking::No);
            continue;
        }

        FVector2D Position;
        if (ParticipantController && UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
            ParticipantController, Entry.WorldPosition, Position, true))
        {
            if (UCanvasPanelSlot* DamageSlot = Cast<UCanvasPanelSlot>(DamageText->Slot))
            {
                const float Progress = FMath::Clamp(Entry.Age / Lifetime, 0.0f, 1.0f);
                const float RiseAlpha = 1.0f - FMath::Square(1.0f - Progress);
                const float Rise = FMath::Max(0.0f, DamageRisePixels) * RiseAlpha;
                DamageSlot->SetPosition(Position + FVector2D(Entry.HorizontalOffset, -Rise));
            }

            const float ScaleDuration = FMath::Max(0.01f, DamageScaleInSeconds);
            const float ScaleAlpha = FMath::Clamp(Entry.Age / ScaleDuration, 0.0f, 1.0f);
            const float StartScale = Entry.bLethal ? LethalDamageStartScale : NormalDamageStartScale;
            const float Scale = FMath::Lerp(StartScale, 1.0f, FMath::InterpEaseOut(0.0f, 1.0f, ScaleAlpha, 3.0f));
            DamageText->SetRenderScale(FVector2D(Scale, Scale));
            DamageText->SetVisibility(ESlateVisibility::HitTestInvisible);
            DamageText->SetRenderOpacity(FMath::Clamp((Lifetime - Entry.Age) / (Lifetime * 0.36f), 0.0f, 1.0f));
        }
        else
        {
            DamageText->SetVisibility(ESlateVisibility::Hidden);
        }
    }
    if (DamageNumbers.IsEmpty())
    {
        SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UTrainingDamageFeedbackWidget::ClearDamageNumbers()
{
    for (const FDamageNumber& Entry : DamageNumbers)
    {
        if (UTextBlock* DamageText = Entry.Text.Get())
        {
            DamageText->RemoveFromParent();
        }
    }
    DamageNumbers.Reset();
    DamageSequence = 0;
    SetVisibility(ESlateVisibility::Collapsed);
}

void UTrainingDamageFeedbackWidget::NativeDestruct()
{
    ClearDamageNumbers();
    Super::NativeDestruct();
}
