#pragma once

#include "CoreMinimal.h"
#include "Engine/SceneCapture2D.h"
#include "ShooterOptionsPreviewActor.generated.h"

/** Drop this actor into a level to feed the Graphics page live preview. */
UCLASS(Blueprintable, meta = (DisplayName = "Shooter Options Preview Camera"))
class FPS_API AShooterOptionsPreviewActor : public ASceneCapture2D
{
    GENERATED_BODY()

public:
    AShooterOptionsPreviewActor();
};
