#include "Training/TrainingAreaTransition.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Training/TrainingSettings.h"
#include "Training/UI/TrainingAreaTransitionWidget.h"

bool UTrainingAreaTransition::Begin(APlayerController& Player, const FName AreaId,
    FTrainingAreaMove Move, FTrainingAreaMoveResolved Resolved)
{
    if (bBlockingInput || !Player.IsLocalController() || !IsValid(Player.GetPawn()) || !Move.IsBound())
    {
        return false;
    }
    Cancel();
    Widget = CreateWidget<UTrainingAreaTransitionWidget>(&Player);
    if (!Widget) return false;

    const UTrainingSettings* Settings = GetDefault<UTrainingSettings>();
    const FTrainingAreaTitle* Title = Settings->AreaTitles.Find(AreaId);
    if (!Title && !AreaId.IsNone())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Training] Area title '%s' is missing in Training settings."), *AreaId.ToString());
    }

    Controller = &Player;
    OriginalPawn = Player.GetPawn();
    PendingMove = MoveTemp(Move);
    MoveResolved = MoveTemp(Resolved);
    bBlockingInput = true;
    if (UPawnMovementComponent* Movement = OriginalPawn->GetMovementComponent())
    {
        Movement->StopMovementImmediately();
    }
    Widget->Configure(Settings->AreaTransition, Title ? *Title : FTrainingAreaTitle(),
        FTrainingAreaCovered::CreateUObject(this, &ThisClass::HandleCovered),
        FSimpleDelegate::CreateUObject(this, &ThisClass::HandleRevealed),
        FSimpleDelegate::CreateUObject(this, &ThisClass::Cancel),
        FSimpleDelegate::CreateUObject(this, &ThisClass::Cancel));
    Widget->AddToViewport(Settings->AreaTransition.ZOrder);
    if (!Widget->IsInViewport())
    {
        MoveResolved.Unbind();
        Cancel();
        return false;
    }
    return true;
}

bool UTrainingAreaTransition::HandleCovered()
{
    // 요청은 한 번만 소비하며, 연출 도중 교체된 폰에는 적용하지 않습니다.
    FTrainingAreaMove Move = MoveTemp(PendingMove);
    FTrainingAreaMoveResolved Resolved = MoveTemp(MoveResolved);
    PendingMove.Unbind();
    MoveResolved.Unbind();
    FText Failure;
    const bool bSamePawn = Controller.IsValid() && OriginalPawn.IsValid()
        && Controller->GetPawn() == OriginalPawn.Get();
    bool bSuccess = false;
    if (bSamePawn && Move.IsBound())
    {
        const TGuardValue<bool> CommitScope(bCommittingMove, true);
        bSuccess = Move.Execute(Failure);
    }
    if (!bSuccess && Failure.IsEmpty())
    {
        Failure = NSLOCTEXT("TrainingAreaTransition", "MoveUnavailable", "The training move is no longer available.");
    }
    Resolved.ExecuteIfBound(bSuccess, Failure);
    return bSuccess;
}

void UTrainingAreaTransition::HandleRevealed()
{
    // 자막이 남아 있는 동안에는 정상적으로 플레이할 수 있습니다.
    bBlockingInput = false;
}

void UTrainingAreaTransition::Cancel()
{
    FTrainingAreaMoveResolved Cancelled = MoveTemp(MoveResolved);
    bBlockingInput = false;
    bCommittingMove = false;
    PendingMove.Unbind();
    MoveResolved.Unbind();
    Controller.Reset();
    OriginalPawn.Reset();
    UTrainingAreaTransitionWidget* PreviousWidget = Widget;
    Widget = nullptr;
    if (PreviousWidget)
    {
        PreviousWidget->Stop();
        PreviousWidget->RemoveFromParent();
    }
    Cancelled.ExecuteIfBound(false,
        NSLOCTEXT("TrainingAreaTransition", "Interrupted", "The training move was interrupted. Interact again."));
}
