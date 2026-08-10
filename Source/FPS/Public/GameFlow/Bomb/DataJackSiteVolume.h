#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DataJackSiteVolume.generated.h"

class APawn;
class UBoxComponent;

UCLASS(Blueprintable)
class FPS_API ADataJackSiteVolume : public AActor
{
	GENERATED_BODY()

public:
	ADataJackSiteVolume();

	UFUNCTION(BlueprintPure, Category = "Game Flow|Data Jack|Site")
	FName GetSiteId() const;

	bool ContainsPawn(const APawn* Pawn) const;

	bool FindPlantSurface(const APawn* Pawn, FHitResult& OutSurfaceHit) const;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Game Flow|Data Jack|Site")
	TObjectPtr<UBoxComponent> SiteArea;

private:
	bool ContainsWorldLocation(const FVector& WorldLocation, float Tolerance = 0.0f) const;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Site", meta = (AllowPrivateAccess = "true"))
	FName SiteId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Site", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float FloorTraceHeight = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Site", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float FloorTraceDepth = 400.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Site", meta = (AllowPrivateAccess = "true"))
	TEnumAsByte<ECollisionChannel> PlantSurfaceTraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Site", meta = (AllowPrivateAccess = "true", ClampMin = "0.0"))
	float SiteBoundsTolerance = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Game Flow|Data Jack|Site", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumPlantSurfaceUpDot = 0.5f;
};
