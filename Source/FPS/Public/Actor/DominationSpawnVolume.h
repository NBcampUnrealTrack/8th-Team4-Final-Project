#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DominationSpawnVolume.generated.h"

class UBoxComponent;
class USceneComponent;

/**
 * A~E 전선의 경계에 배치하는 중립 Domination 스폰 구역이다.
 * 공간 후보만 제공하며 실제 후보 선택과 점유 검사는 공용 TeamPlayerManager가 담당한다.
 */
UCLASS(Blueprintable)
class FPS_API ADominationSpawnVolume : public AActor
{
	GENERATED_BODY()

public:
	ADominationSpawnVolume();

	void GetSpawnCandidateTransforms(TArray<FTransform>& OutCandidateTransforms) const;

private:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Domination|Spawn", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Domination|Spawn", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UBoxComponent> SpawnArea;
};
