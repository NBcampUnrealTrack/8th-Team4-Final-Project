#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DominationSpawnVolume.generated.h"

class UBoxComponent;
class USceneComponent;

/**
 * A~E 전선의 경계에 배치하는 중립 Domination 스폰 구역이다.
 * Box 범위 안의 네 위치를 순서대로 사용한다.
 */
UCLASS(Blueprintable)
class FPS_API ADominationSpawnVolume : public AActor
{
	GENERATED_BODY()

public:
	ADominationSpawnVolume();

	FTransform GetNextSpawnTransform();

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Domination|Spawn", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Domination|Spawn", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> SpawnArea;

	int32 NextSpawnPointIndex = 0;
};
