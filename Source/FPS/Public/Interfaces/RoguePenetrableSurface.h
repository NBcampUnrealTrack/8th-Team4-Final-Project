#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "RoguePenetrableSurface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType, Blueprintable)
class URoguePenetrableSurface : public UInterface
{
	GENERATED_BODY()
};

class FPS_API IRoguePenetrableSurface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Rogue|Penetration")
	bool CanPenetrate() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Rogue|Penetration")
	float GetPenetrationDamageMultiplier() const;
};
