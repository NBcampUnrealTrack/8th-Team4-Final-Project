#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TrainingDamageFeedbackWidget.generated.h"

class UCanvasPanel;
class UTextBlock;

UCLASS(Blueprintable)
class FPS_API UTrainingDamageFeedbackWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UTrainingDamageFeedbackWidget(const FObjectInitializer& ObjectInitializer);

    void PresentDamage(float Damage, bool bLethal, const FVector& WorldPosition);
    void ClearDamageNumbers();

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Damage Feedback")
    FVector DamageNumberWorldOffset = FVector(0.0, 0.0, 110.0);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Damage Feedback", meta = (ClampMin = "0.1", ClampMax = "3.0", Units = "s"))
    float DamageNumberLifetime = 0.85f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Damage Feedback", meta = (ClampMin = "1", ClampMax = "64"))
    int32 MaxVisibleDamageNumbers = 16;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Damage Feedback", meta = (ClampMin = "1", ClampMax = "96"))
    int32 NormalDamageFontSize = 24;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Damage Feedback", meta = (ClampMin = "1", ClampMax = "96"))
    int32 LethalDamageFontSize = 28;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Damage Feedback")
    FLinearColor NormalDamageColor = FLinearColor(1.0f, 0.97f, 0.76f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Damage Feedback")
    FLinearColor LethalDamageColor = FLinearColor(1.0f, 0.65f, 0.27f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Damage Feedback", meta = (ClampMin = "0.0"))
    float DamageRisePixels = 44.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Damage Feedback", meta = (ClampMin = "0.0"))
    float DamageHorizontalSpreadPixels = 18.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Damage Feedback|Style", meta = (ClampMin = "0", ClampMax = "4"))
    int32 DamageOutlineSize = 1;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Damage Feedback|Style")
    FLinearColor DamageOutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.82f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Damage Feedback|Style")
    FVector2D DamageShadowOffset = FVector2D(1.0f, 1.0f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Damage Feedback|Style")
    FLinearColor DamageShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.72f);

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Damage Feedback|Animation", meta = (ClampMin = "0.01", ClampMax = "0.5", Units = "s"))
    float DamageScaleInSeconds = 0.10f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Damage Feedback|Animation", meta = (ClampMin = "0.1", ClampMax = "2.0"))
    float NormalDamageStartScale = 0.86f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Training|Damage Feedback|Animation", meta = (ClampMin = "0.1", ClampMax = "2.0"))
    float LethalDamageStartScale = 1.12f;

protected:
    virtual void NativePreConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual void NativeDestruct() override;

private:
    UPROPERTY(Transient, meta = (BindWidgetOptional))
    TObjectPtr<UCanvasPanel> Canvas_Damage;

    struct FDamageNumber
    {
        TWeakObjectPtr<UTextBlock> Text;
        FVector WorldPosition = FVector::ZeroVector;
        float Age = 0.0f;
        float HorizontalOffset = 0.0f;
        bool bLethal = false;
    };

    TArray<FDamageNumber> DamageNumbers;
    int32 DamageSequence = 0;
};
