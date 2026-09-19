#pragma once

#include "CoreMinimal.h"
#include "Engine/TargetPoint.h"
#include "CQBTrainingSpawnPoint.generated.h"

class ACQBTrainingPatrolPoint;

UCLASS(BlueprintType)
class FPS_API ACQBTrainingSpawnPoint : public ATargetPoint
{
    GENERATED_BODY()

public:
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "CQB|Spawn", meta = (ClampMin = "0"))
    int32 SpawnOrder = 0;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "CQB|Spawn")
    FName SpawnGroup = TEXT("Default");

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "CQB|Patrol")
    TArray<TObjectPtr<ACQBTrainingPatrolPoint>> PatrolPoints;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "CQB|Patrol")
    bool bPingPongPatrol = true;
};
