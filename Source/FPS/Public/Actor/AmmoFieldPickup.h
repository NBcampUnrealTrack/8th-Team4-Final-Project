#pragma once

#include "CoreMinimal.h"
#include "Actor/FieldPickup.h"
#include "AmmoFieldPickup.generated.h"

// 획득/연출/리스폰은 FieldPickup을 그대로 사용한다. BP에서는 외형만 지정하면 된다.
UCLASS(Blueprintable)
class FPS_API AAmmoFieldPickup : public AFieldPickup
{
	GENERATED_BODY()

public:
	AAmmoFieldPickup();
};
