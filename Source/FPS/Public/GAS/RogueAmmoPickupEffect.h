#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "RogueAmmoPickupEffect.generated.h"

// FieldPickup의 Data.Magnitude.Primary를 예비 탄약 지급 요청으로 전달한다.
UCLASS()
class FPS_API URogueAmmoPickupEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	URogueAmmoPickupEffect(const FObjectInitializer& ObjectInitializer);
};
