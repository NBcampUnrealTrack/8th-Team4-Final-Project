#include "UI/Settings/ShooterOptionsPreviewActor.h"

#include "Components/SceneCaptureComponent2D.h"

AShooterOptionsPreviewActor::AShooterOptionsPreviewActor()
{
    if (USceneCaptureComponent2D* Capture = GetCaptureComponent2D())
    {
        Capture->ComponentTags.AddUnique(TEXT("OptionsPreview"));
        Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
        Capture->bCaptureEveryFrame = false;
        Capture->bCaptureOnMovement = false;
        Capture->bAlwaysPersistRenderingState = false;
    }
}
