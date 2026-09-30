#pragma once

#include "CoreMinimal.h"
#include "Fonts/SlateFontInfo.h"
#include "TrainingAreaTransitionTypes.generated.h"

namespace TrainingAreaIds
{
    inline const FName FreePractice(TEXT("FreePractice"));
    inline const FName Arena(TEXT("Arena360"));
    inline const FName CQB(TEXT("CQBFacility"));
}

DECLARE_DELEGATE_RetVal_OneParam(bool, FTrainingAreaMove, FText&);
DECLARE_DELEGATE_TwoParams(FTrainingAreaMoveResolved, bool, const FText&);
DECLARE_DELEGATE_RetVal(bool, FTrainingAreaCovered);

USTRUCT(BlueprintType)
struct FPS_API FTrainingAreaTitle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
    FText Title;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area")
    FText Subtitle;
};

USTRUCT(BlueprintType)
struct FPS_API FTrainingAreaTransitionStyle
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen", meta = (ClampMin = "0.0", Units = "s"))
    float FadeToBlackSeconds = 0.38f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen", meta = (ClampMin = "0.0", Units = "s"))
    float BlackHoldSeconds = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen", meta = (ClampMin = "0.0", Units = "s"))
    float RevealSeconds = 0.60f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Title", meta = (ClampMin = "0.0", Units = "s"))
    float TitleDelaySeconds = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Title", meta = (ClampMin = "0.0", Units = "s"))
    float TitleFadeInSeconds = 0.55f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Title", meta = (ClampMin = "0.0", Units = "s"))
    float TitleHoldSeconds = 1.65f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Title", meta = (ClampMin = "0.0", Units = "s"))
    float TitleFadeOutSeconds = 0.70f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    FVector2D OffsetFromAim = FVector2D(88.0, -18.0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0.0"))
    float SlideInDistance = 20.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0.0"))
    float SlideOutDistance = 12.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0.0"))
    float SubtitleGap = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typography")
    FSlateFontInfo TitleFont;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typography")
    FSlateFontInfo SubtitleFont;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typography")
    FLinearColor TitleColor = FLinearColor(1.0f, 0.77f, 0.32f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typography")
    FLinearColor SubtitleColor = FLinearColor(0.89f, 0.88f, 0.82f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typography")
    FLinearColor ShadowColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.85f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Typography")
    FVector2D ShadowOffset = FVector2D(1.0, 2.0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Screen", meta = (ClampMin = "0"))
    int32 ZOrder = 1000;
};
