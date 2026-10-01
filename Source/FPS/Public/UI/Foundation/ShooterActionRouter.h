#pragma once

#include "CoreMinimal.h"
#include "Input/CommonUIActionRouterBase.h"
#include "ShooterActionRouter.generated.h"

UCLASS()
class FPS_API UShooterActionRouter : public UCommonUIActionRouterBase
{
	GENERATED_BODY()

public:
	void RecalculateActiveInputConfig();
};
