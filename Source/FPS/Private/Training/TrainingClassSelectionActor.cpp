#include "Training/TrainingClassSelectionActor.h"

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "TrainingInteractionRegistration.h"
#include "Player/DominationPlayerState.h"
#include "Player/TrainingPlayerController.h"

ATrainingClassSelectionActor::ATrainingClassSelectionActor()
{
    PrimaryActorTick.bCanEverTick = false;

    InteractionCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionCollision"));
    SetRootComponent(InteractionCollision);
    TrainingInteractionRegistration::ConfigureCollision(*InteractionCollision, FVector(45.0, 45.0, 70.0));
    InteractionCollision->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleInteractionBeginOverlap);
    InteractionCollision->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleInteractionEndOverlap);

    DisplayMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh"));
    TrainingInteractionRegistration::ConfigureDisplayMesh(*DisplayMesh, *InteractionCollision);
}

void ATrainingClassSelectionActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    TrainingInteractionRegistration::Cleanup(*this, LocalOverlappingController);
    Super::EndPlay(EndPlayReason);
}

bool ATrainingClassSelectionActor::CanInteract(const APlayerController& RequestingPlayer) const
{
    const ATrainingPlayerController* TrainingController = Cast<ATrainingPlayerController>(&RequestingPlayer);
    const ADominationPlayerState* PlayerState = TrainingController
        ? TrainingController->GetPlayerState<ADominationPlayerState>()
        : nullptr;
    return GetNetMode() == NM_Standalone && RequestingPlayer.IsLocalController()
        && IsValid(TrainingController)
        && IsValid(RequestingPlayer.GetPawn())
        && IsValid(PlayerState)
        && PlayerState->HasSelectedCombatClass()
        && PlayerState->IsAlive()
        && PlayerState->GetParticipationState() == EPlayerParticipationState::Active;
}

void ATrainingClassSelectionActor::Interact(APlayerController& RequestingPlayer)
{
    if (!CanInteract(RequestingPlayer))
    {
        return;
    }

    if (ATrainingPlayerController* TrainingController = Cast<ATrainingPlayerController>(&RequestingPlayer))
    {
        TrainingController->RequestTrainingClassSelection();
    }
}

void ATrainingClassSelectionActor::HandleInteractionBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    TrainingInteractionRegistration::Register(*this, OtherActor, LocalOverlappingController);
}

void ATrainingClassSelectionActor::HandleInteractionEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32)
{
    TrainingInteractionRegistration::Unregister(*this, OtherActor, LocalOverlappingController);
}
