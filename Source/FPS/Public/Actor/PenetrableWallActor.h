#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/RoguePenetrableSurface.h"
#include "PenetrableWallActor.generated.h"

class UStaticMeshComponent;

UCLASS(Blueprintable)
class FPS_API APenetrableWallActor : public AActor, public IRoguePenetrableSurface
{
	GENERATED_BODY()

public:
	APenetrableWallActor();

	virtual bool CanPenetrate_Implementation() const override;
	virtual float GetPenetrationDamageMultiplier_Implementation() const override;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "FPS|Penetration")
	TObjectPtr<UStaticMeshComponent> WallMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Penetration")
	bool bCanPenetrate = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FPS|Penetration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PenetrationDamageMultiplier = 0.6f;
};
