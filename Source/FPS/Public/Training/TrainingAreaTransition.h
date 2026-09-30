#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Training/TrainingAreaTransitionTypes.h"
#include "TrainingAreaTransition.generated.h"

class APlayerController;
class APawn;
class UTrainingAreaTransitionWidget;

// 연출의 수명과 이동 트랜잭션만 관리합니다. 구역별 규칙은 호출자가 소유합니다.
UCLASS()
class FPS_API UTrainingAreaTransition : public UObject
{
    GENERATED_BODY()

public:
    bool Begin(APlayerController& Player, FName AreaId,
        FTrainingAreaMove Move, FTrainingAreaMoveResolved Resolved);
    void Cancel();
    bool IsBlockingInput() const { return bBlockingInput; }
    bool IsCommittingMove() const { return bCommittingMove; }

private:
    bool HandleCovered();
    void HandleRevealed();

    UPROPERTY(Transient)
    TObjectPtr<UTrainingAreaTransitionWidget> Widget;

    TWeakObjectPtr<APlayerController> Controller;
    TWeakObjectPtr<APawn> OriginalPawn;
    FTrainingAreaMove PendingMove;
    FTrainingAreaMoveResolved MoveResolved;
    bool bBlockingInput = false;
    bool bCommittingMove = false;
};
