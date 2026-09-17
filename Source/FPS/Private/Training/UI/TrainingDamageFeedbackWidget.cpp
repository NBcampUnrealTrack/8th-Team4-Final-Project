#include "Training/UI/TrainingDamageFeedbackWidget.h"

#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "GameFramework/PlayerController.h"
#include "TrainingWidgetLayout.h"

UTrainingDamageFeedbackWidget::UTrainingDamageFeedbackWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetIsFocusable(false);
}

TSharedRef<SWidget> UTrainingDamageFeedbackWidget::RebuildWidget()
{
    if (!WidgetTree)
    {
        WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
    }

    if (!WidgetTree->RootWidget)
    {
        Canvas_Damage = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas_Damage"));
        WidgetTree->RootWidget = Canvas_Damage;
    }
    else
    {
        Canvas_Damage = Cast<UCanvasPanel>(WidgetTree->FindWidget(TEXT("Canvas_Damage")));
        if (!Canvas_Damage)
        {
            Canvas_Damage = Cast<UCanvasPanel>(WidgetTree->RootWidget);
        }
    }

    return Super::RebuildWidget();
}

void UTrainingDamageFeedbackWidget::NativePreConstruct()
{
    Super::NativePreConstruct();
    SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UTrainingDamageFeedbackWidget::PresentDamage(const float Damage, const bool bLethal,
    const FVector& WorldPosition)
{
    if (!Canvas_Damage || !WidgetTree || !FMath::IsFinite(Damage) || Damage <= 0.0f)
    {
        return;
    }

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
    DamageText->SetFont(TrainingWidgetLayout::MakeFont(bLethal ? LethalDamageFontSize : NormalDamageFontSize));
    DamageText->SetColorAndOpacity(FSlateColor(bLethal ? LethalDamageColor : NormalDamageColor));
    DamageText->SetShadowOffset(FVector2D(1.0, 1.0));
    DamageText->SetShadowColorAndOpacity(FLinearColor::Black);
    DamageText->SetVisibility(ESlateVisibility::Hidden);

    UCanvasPanelSlot* DamageSlot = Canvas_Damage->AddChildToCanvas(DamageText);
    DamageSlot->SetAutoSize(true);
    DamageSlot->SetAlignment(FVector2D(0.5, 1.0));

    FDamageNumber& Entry = DamageNumbers.AddDefaulted_GetRef();
    Entry.Text = DamageText;
    Entry.WorldPosition = WorldPosition + DamageNumberWorldOffset;
    Entry.HorizontalOffset = static_cast<float>(DamageSequence - 1) * FMath::Max(0.0f, DamageHorizontalSpreadPixels);
    DamageSequence = (DamageSequence + 1) % 3;
}

void UTrainingDamageFeedbackWidget::NativeTick(const FGeometry& MyGeometry, const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (DamageNumbers.IsEmpty())
    {
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
                const float Rise = FMath::Max(0.0f, DamageRisePixels) * Entry.Age / Lifetime;
                DamageSlot->SetPosition(Position + FVector2D(Entry.HorizontalOffset, -Rise));
            }
            DamageText->SetVisibility(ESlateVisibility::HitTestInvisible);
            DamageText->SetRenderOpacity(FMath::Clamp((Lifetime - Entry.Age) / (Lifetime * 0.4f), 0.0f, 1.0f));
        }
        else
        {
            DamageText->SetVisibility(ESlateVisibility::Hidden);
        }
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
}

void UTrainingDamageFeedbackWidget::NativeDestruct()
{
    ClearDamageNumbers();
    Super::NativeDestruct();
}
